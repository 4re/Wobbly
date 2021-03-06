#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegExpValidator>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <VSScript.h>

#include "WobblyException.h"
#include "WobblyWindow.h"


WobblyWindow::WobblyWindow()
    : project(nullptr)
    , current_frame(0)
    , match_pattern("ccnnc")
    , decimation_pattern("kkkkd")
    , preview(false)
    , vsapi(nullptr)
    , vsscript(nullptr)
    , vscore(nullptr)
    , vsnode{nullptr, nullptr}
    , vsframe(nullptr)
{
    createUI();

    try {
        initialiseVapourSynth();

        checkRequiredFilters();
    } catch (WobblyException &e) {
        show();
        errorPopup(e.what());
        exit(1); // Seems a bit heavy-handed, but close() doesn't close the window if called here, so...
    }
}


void WobblyWindow::errorPopup(const char *msg) {
    QMessageBox::information(this, QStringLiteral("Error"), msg);
}


void WobblyWindow::closeEvent(QCloseEvent *event) {
    // XXX Only ask if the project was modified.

    if (project) {
        QMessageBox::StandardButton answer = QMessageBox::question(this, QStringLiteral("Save?"), QStringLiteral("Save project?"), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

        if (answer == QMessageBox::Yes) {
            saveProject();
        } else if (answer == QMessageBox::No) {
            ;
        } else {
            event->ignore();
            return;
        }
    }

    cleanUpVapourSynth();

    if (project) {
        delete project;
        project = nullptr;
    }

    event->accept();
}


void WobblyWindow::createMenu() {
    QMenuBar *bar = menuBar();

    QMenu *p = bar->addMenu("&Project");

    QAction *projectOpen = new QAction("&Open project", this);
    QAction *projectSave = new QAction("&Save project", this);
    QAction *projectSaveAs = new QAction("&Save project as", this);
    QAction *projectQuit = new QAction("&Quit", this);

    projectOpen->setShortcut(QKeySequence::Open);
    projectSave->setShortcut(QKeySequence::Save);
    projectSaveAs->setShortcut(QKeySequence::SaveAs);
    projectQuit->setShortcut(QKeySequence("Ctrl+Q"));

    connect(projectOpen, &QAction::triggered, this, &WobblyWindow::openProject);
    connect(projectSave, &QAction::triggered, this, &WobblyWindow::saveProject);
    connect(projectSaveAs, &QAction::triggered, this, &WobblyWindow::saveProjectAs);
    connect(projectQuit, &QAction::triggered, this, &QWidget::close);

    p->addAction(projectOpen);
    p->addAction(projectSave);
    p->addAction(projectSaveAs);
    p->addSeparator();
    p->addAction(projectQuit);


    tools_menu = bar->addMenu("&Tools");
}


struct Shortcut {
    const char *keys;
    void (WobblyWindow::* func)();
};


void WobblyWindow::createShortcuts() {
    Shortcut shortcuts[] = {
        { "Left", &WobblyWindow::jump1Backward },
        { "Right", &WobblyWindow::jump1Forward },
        { "Ctrl+Left", &WobblyWindow::jump5Backward },
        { "Ctrl+Right", &WobblyWindow::jump5Forward },
        { "Alt+Left", &WobblyWindow::jump50Backward },
        { "Alt+Right", &WobblyWindow::jump50Forward },
        { "Home", &WobblyWindow::jumpToStart },
        { "End", &WobblyWindow::jumpToEnd },
        { "PgDown", &WobblyWindow::jumpALotBackward },
        { "PgUp", &WobblyWindow::jumpALotForward },
        { "Ctrl+Up", &WobblyWindow::jumpToNextSectionStart },
        { "Ctrl+Down", &WobblyWindow::jumpToPreviousSectionStart },
        { "S", &WobblyWindow::cycleMatchPCN },
        { "Ctrl+F", &WobblyWindow::freezeForward },
        { "Shift+F", &WobblyWindow::freezeBackward },
        { "F", &WobblyWindow::freezeRange },
        { "Delete,F", &WobblyWindow::deleteFreezeFrame },
        { "D", &WobblyWindow::toggleDecimation },
        { "I", &WobblyWindow::addSection },
        { "Delete,I", &WobblyWindow::deleteSection },
        { "P", &WobblyWindow::toggleCombed },
        //{ "R", &WobblyWindow::resetRange },
        { "R,S", &WobblyWindow::resetSection },
        { "Ctrl+R", &WobblyWindow::rotateAndSetPatterns },
        { "Ctrl+P", &WobblyWindow::togglePreview },
        { nullptr, nullptr }
    };

    for (int i = 0; shortcuts[i].func; i++) {
        QShortcut *s = new QShortcut(QKeySequence(shortcuts[i].keys), this);
        connect(s, &QShortcut::activated, this, shortcuts[i].func);
    }
}


void WobblyWindow::createFrameDetailsViewer() {
    frame_num_label = new QLabel;
    frame_num_label->setTextFormat(Qt::RichText);
    time_label = new QLabel;
    matches_label = new QLabel;
    matches_label->setTextFormat(Qt::RichText);
    section_label = new QLabel;
    custom_list_label = new QLabel;
    freeze_label = new QLabel;
    decimate_metric_label = new QLabel;
    mic_label = new QLabel;
    mic_label->setTextFormat(Qt::RichText);
    combed_label = new QLabel;

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(frame_num_label);
    vbox->addWidget(time_label);
    vbox->addWidget(matches_label);
    vbox->addWidget(section_label);
    vbox->addWidget(custom_list_label);
    vbox->addWidget(freeze_label);
    vbox->addWidget(decimate_metric_label);
    vbox->addWidget(mic_label);
    vbox->addWidget(combed_label);
    vbox->addStretch(1);

    QWidget *details_widget = new QWidget;
    details_widget->setLayout(vbox);

    QDockWidget *details_dock = new QDockWidget("Frame details", this);
    details_dock->setFloating(false);
    details_dock->setWidget(details_widget);
    addDockWidget(Qt::LeftDockWidgetArea, details_dock);
    tools_menu->addAction(details_dock->toggleViewAction());
    //connect(details_dock, &QDockWidget::visibilityChanged, details_dock, &QDockWidget::setEnabled);
}


void WobblyWindow::createCropAssistant() {
    const char *crop_prefixes[4] = {
        "Left: ",
        "Top: ",
        "Right: ",
        "Bottom: "
    };

    const char *resize_prefixes[2] = {
        "Width: ",
        "Height: "
    };

    QVBoxLayout *vbox = new QVBoxLayout;

    for (int i = 0; i < 4; i++) {
        crop_spin[i] = new QSpinBox;
        crop_spin[i]->setRange(0, 99999);
        crop_spin[i]->setPrefix(crop_prefixes[i]);
        crop_spin[i]->setSuffix(QStringLiteral(" px"));
        connect(crop_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &WobblyWindow::cropChanged);

        vbox->addWidget(crop_spin[i]);
    }

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    crop_box = new QGroupBox(QStringLiteral("Crop"));
    crop_box->setCheckable(true);
    crop_box->setChecked(true);
    crop_box->setLayout(hbox);
    connect(crop_box, &QGroupBox::clicked, this, &WobblyWindow::cropToggled);

    vbox = new QVBoxLayout;

    for (int i = 0; i < 2; i++) {
        resize_spin[i] = new QSpinBox;
        resize_spin[i]->setRange(1, 999999);
        resize_spin[i]->setPrefix(resize_prefixes[i]);
        resize_spin[i]->setSuffix(QStringLiteral(" px"));
        connect(resize_spin[i], static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &WobblyWindow::resizeChanged);

        vbox->addWidget(resize_spin[i]);
    }

    hbox = new QHBoxLayout;
    hbox->addLayout(vbox);
    hbox->addStretch(1);

    resize_box = new QGroupBox(QStringLiteral("Resize"));
    resize_box->setCheckable(true);
    resize_box->setChecked(false);
    resize_box->setLayout(hbox);
    connect(resize_box, &QGroupBox::clicked, this, &WobblyWindow::resizeToggled);

    vbox = new QVBoxLayout;
    vbox->addWidget(crop_box);
    vbox->addWidget(resize_box);
    vbox->addStretch(1);

    QWidget *crop_widget = new QWidget;
    crop_widget->setLayout(vbox);

    crop_dock = new QDockWidget("Cropping/Resizing", this);
    crop_dock->setVisible(false);
    crop_dock->setFloating(true);
    crop_dock->setWidget(crop_widget);
    addDockWidget(Qt::RightDockWidgetArea, crop_dock);
    tools_menu->addAction(crop_dock->toggleViewAction());
    connect(crop_dock, &QDockWidget::visibilityChanged, crop_dock, &QDockWidget::setEnabled);
    connect(crop_dock, &QDockWidget::visibilityChanged, [this] {
        if (!project)
            return;

        try {
            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }
    });
}


void WobblyWindow::createPresetEditor() {
    preset_combo = new QComboBox;
    connect(preset_combo, static_cast<void (QComboBox::*)(const QString &)>(&QComboBox::activated), this, &WobblyWindow::presetChanged);

    preset_edit = new PresetTextEdit;
    preset_edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    preset_edit->setTabChangesFocus(true);
    connect(preset_edit, &PresetTextEdit::focusLost, this, &WobblyWindow::presetEdited);

    QPushButton *new_button = new QPushButton(QStringLiteral("New"));
    QPushButton *rename_button = new QPushButton(QStringLiteral("Rename"));
    QPushButton *delete_button = new QPushButton(QStringLiteral("Delete"));

    connect(new_button, &QPushButton::clicked, this, &WobblyWindow::presetNew);
    connect(rename_button, &QPushButton::clicked, this, &WobblyWindow::presetRename);
    connect(delete_button, &QPushButton::clicked, this, &WobblyWindow::presetDelete);


    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new_button);
    hbox->addWidget(rename_button);
    hbox->addWidget(delete_button);
    hbox->addStretch(1);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(preset_combo);
    vbox->addWidget(preset_edit);
    vbox->addLayout(hbox);

    QWidget *preset_widget = new QWidget;
    preset_widget->setLayout(vbox);


    QDockWidget *preset_dock = new QDockWidget("Preset editor", this);
    preset_dock->setVisible(false);
    preset_dock->setFloating(true);
    preset_dock->setWidget(preset_widget);
    addDockWidget(Qt::RightDockWidgetArea, preset_dock);
    tools_menu->addAction(preset_dock->toggleViewAction());
    connect(preset_dock, &QDockWidget::visibilityChanged, preset_dock, &QDockWidget::setEnabled);
}


void WobblyWindow::createPatternEditor() {
    match_pattern_edit = new QLineEdit(match_pattern);
    decimation_pattern_edit = new QLineEdit(decimation_pattern);

    QRegExpValidator *match_validator = new QRegExpValidator(QRegExp("[pcn]{5,}"), this);
    QRegExpValidator *decimation_validator = new QRegExpValidator(QRegExp("[dk]{5,}"), this);

    match_pattern_edit->setValidator(match_validator);
    decimation_pattern_edit->setValidator(decimation_validator);

    connect(match_pattern_edit, &QLineEdit::textEdited, this, &WobblyWindow::matchPatternEdited);
    connect(decimation_pattern_edit, &QLineEdit::textEdited, this, &WobblyWindow::decimationPatternEdited);

    QVBoxLayout *vbox = new QVBoxLayout;
    vbox->addWidget(new QLabel(QStringLiteral("Match pattern:")));
    vbox->addWidget(match_pattern_edit);
    vbox->addWidget(new QLabel(QStringLiteral("Decimation pattern:")));
    vbox->addWidget(decimation_pattern_edit);
    vbox->addStretch(1);

    QWidget *pattern_widget = new QWidget;
    pattern_widget->setLayout(vbox);


    QDockWidget *pattern_dock = new QDockWidget("Pattern editor", this);
    pattern_dock->setVisible(false);
    pattern_dock->setFloating(true);
    pattern_dock->setWidget(pattern_widget);
    addDockWidget(Qt::RightDockWidgetArea, pattern_dock);
    tools_menu->addAction(pattern_dock->toggleViewAction());
    connect(pattern_dock, &QDockWidget::visibilityChanged, pattern_dock, &QDockWidget::setEnabled);
}


void WobblyWindow::createUI() {
    createMenu();
    createShortcuts();

    setWindowTitle(QStringLiteral("Wobbly IVTC Assistant"));

    statusBar()->setSizeGripEnabled(true);

    frame_label = new QLabel;
    frame_label->setTextFormat(Qt::PlainText);
    frame_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    /*
    QWidget *central_widget = new QWidget;
    central_widget->setLayout(hbox);

    setCentralWidget(central_widget);
    */
    setCentralWidget(frame_label);


    createFrameDetailsViewer();
    createCropAssistant();
    createPresetEditor();
    createPatternEditor();
}


void WobblyWindow::initialiseVapourSynth() {
    if (!vsscript_init())
        throw WobblyException("Fatal error: failed to initialise VSScript. Your VapourSynth installation is probably broken.");
    
    
    vsapi = vsscript_getVSApi();
    if (!vsapi)
        throw WobblyException("Fatal error: failed to acquire VapourSynth API struct. Did you update the VapourSynth library but not the Python module (or the other way around)?");
    
    if (vsscript_createScript(&vsscript))
        throw WobblyException(std::string("Fatal error: failed to create VSScript object. Error message: ") + vsscript_getError(vsscript));

    vscore = vsscript_getCore(vsscript);
    if (!vscore)
        throw WobblyException("Fatal error: failed to retrieve VapourSynth core object.");
}


void WobblyWindow::cleanUpVapourSynth() {
    frame_label->setPixmap(QPixmap()); // Does it belong here?
    vsapi->freeFrame(vsframe);
    vsframe = nullptr;

    for (int i = 0; i < 2; i++) {
        vsapi->freeNode(vsnode[i]);
        vsnode[i] = nullptr;
    }

    vsscript_freeScript(vsscript);
    vsscript = nullptr;
}


void WobblyWindow::checkRequiredFilters() {
    const char *filters[][4] = {
        {
            "com.sources.d2vsource",
            "Source",
            "d2vsource plugin not found.",
            "I don't know."
        },
        {
            "com.nodame.fieldhint",
            "FieldHint",
            "FieldHint plugin not found.",
            "FieldHint plugin is too old."
        },
        {
            "com.vapoursynth.std",
            "FreezeFrames",
            "VapourSynth standard filter library not found. This should never happen.",
            "VapourSynth version is too old."
        },
        {
            "com.vapoursynth.std",
            "DeleteFrames",
            "VapourSynth standard filter library not found. This should never happen.",
            "VapourSynth version is too old."
        },
        {
            "the.weather.channel",
            "Colorspace",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            "the.weather.channel",
            "Depth",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            "the.weather.channel",
            "Resize",
            "zimg plugin not found.",
            "Arwen broke it."
        },
        {
            nullptr
        }
    };

    for (int i = 0; filters[i][0]; i++) {
        VSPlugin *plugin = vsapi->getPluginById(filters[i][0], vscore);
        if (!plugin)
            throw WobblyException(std::string("Fatal error: ") + filters[i][2]);

        VSMap *map = vsapi->getFunctions(plugin);
        if (vsapi->propGetType(map, filters[i][1]) == ptUnset)
            throw WobblyException(std::string("Fatal error: plugin found but it lacks filter '") + filters[i][1] + "'" + (filters[i][3] ? std::string(". Likely reason: ") + filters[i][3] : ""));
    }
}


void WobblyWindow::initialiseUIFromProject() {
    // Crop.
    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(true);

    crop_spin[0]->setValue(project->crop.left);
    crop_spin[1]->setValue(project->crop.top);
    crop_spin[2]->setValue(project->crop.right);
    crop_spin[3]->setValue(project->crop.bottom);

    for (int i = 0; i < 4; i++)
        crop_spin[i]->blockSignals(false);

    crop_box->setChecked(project->isCropEnabled());


    // Resize.
    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(true);

    resize_spin[0]->setValue(project->resize.width);
    resize_spin[1]->setValue(project->resize.height);

    for (int i = 0; i < 2; i++)
        resize_spin[i]->blockSignals(false);

    resize_box->setChecked(project->isResizeEnabled());


    // Presets.
    for (auto it = project->presets.cbegin(); it != project->presets.cend(); it++)
        preset_combo->addItem(QString::fromStdString(it->second.name));

    if (preset_combo->count()) {
        preset_combo->setCurrentIndex(0);
        presetChanged(preset_combo->currentText());
    }
}


void WobblyWindow::openProject() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

    if (!path.isNull()) {
        WobblyProject *tmp = new WobblyProject(true);

        try {
            tmp->readProject(path.toStdString());

            project_path = path;

            if (project)
                delete project;
            project = tmp;

            initialiseUIFromProject();

            vsscript_clearOutput(vsscript, 1);

            evaluateMainDisplayScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());

            if (project == tmp)
                project = nullptr;
            delete tmp;
        }
    }
}


void WobblyWindow::realSaveProject(const QString &path) {
    if (!project)
        return;

    // The currently selected preset might not have been stored in the project yet.
    if (preset_combo->currentIndex() != -1)
        project->setPresetContents(preset_combo->currentText().toStdString(), preset_edit->toPlainText().toStdString());

    project->writeProject(path.toStdString());

    project_path = path;
}


void WobblyWindow::saveProject() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        if (project_path.isEmpty())
            saveProjectAs();
        else
            realSaveProject(project_path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::saveProjectAs() {
    try {
        if (!project)
            throw WobblyException("Can't save the project because none has been loaded.");

        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Wobbly project"), QString(), QString(), nullptr, QFileDialog::DontUseNativeDialog);

        if (!path.isNull())
            realSaveProject(path);
    } catch (WobblyException &e) {
        errorPopup(e.what());
    }
}


void WobblyWindow::evaluateMainDisplayScript() {
    std::string script = project->generateMainDisplayScript(crop_dock->isVisible());

    if (vsscript_evaluateScript(&vsscript, script.c_str(), QFileInfo(project->project_path.c_str()).dir().path().toUtf8().constData(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.erase(traceback);

        throw WobblyException("Failed to evaluate main display script. Error message:\n" + error);
    }

    vsapi->freeNode(vsnode[0]);

    vsnode[0] = vsscript_getOutput(vsscript, 0);
    if (!vsnode[0])
        throw WobblyException("Main display script evaluated successfully, but no node found at output index 0.");

    displayFrame(current_frame);
}


void WobblyWindow::evaluateFinalScript() {
    std::string script = project->generateFinalScript(true);

    if (vsscript_evaluateScript(&vsscript, script.c_str(), QFileInfo(project->project_path.c_str()).dir().path().toUtf8().constData(), efSetWorkingDir)) {
        std::string error = vsscript_getError(vsscript);
        // The traceback is mostly unnecessary noise.
        size_t traceback = error.find("Traceback");
        if (traceback != std::string::npos)
            error.erase(traceback);

        throw WobblyException("Failed to evaluate final script. Error message:\n" + error);
    }

    vsapi->freeNode(vsnode[1]);

    vsnode[1] = vsscript_getOutput(vsscript, 0);
    if (!vsnode[1])
        throw WobblyException("Final script evaluated successfully, but no node found at output index 0.");

    displayFrame(current_frame);
}


void WobblyWindow::displayFrame(int n) {
    if (!vsnode[(int)preview])
        return;

    if (n < 0)
        n = 0;
    if (n >= project->num_frames[PostSource])
        n = project->num_frames[PostSource] - 1;

    std::vector<char> error(1024);
    const VSFrameRef *frame = vsapi->getFrame(preview ? project->frameNumberAfterDecimation(n): n, vsnode[(int)preview], error.data(), 1024);

    if (!frame)
        throw WobblyException(std::string("Failed to retrieve frame. Error message: ") + error.data());

    const uint8_t *ptr = vsapi->getReadPtr(frame, 0);
    int width = vsapi->getFrameWidth(frame, 0);
    int height = vsapi->getFrameHeight(frame, 0);
    int stride = vsapi->getStride(frame, 0);
    frame_label->setPixmap(QPixmap::fromImage(QImage(ptr, width, height, stride, QImage::Format_RGB32)));
    // Must free the frame only after replacing the pixmap.
    vsapi->freeFrame(vsframe);
    vsframe = frame;

    current_frame = n;

    updateFrameDetails();
}


void WobblyWindow::updateFrameDetails() {
    QString frame("Frame: ");

    if (!preview)
        frame += "<b>";

    frame += QString::number(current_frame);

    if (!preview)
        frame += "</b>";

    frame += " | ";

    if (preview)
        frame += "<b>";

    frame += QString::number(project->frameNumberAfterDecimation(current_frame));

    if (preview)
        frame += "</b>";

    frame_num_label->setText(frame);


    time_label->setText(QString::fromStdString("Time: " + project->frameToTime(current_frame)));


    int matches_start = std::max(0, current_frame - 10);
    int matches_end = std::min(current_frame + 10, project->num_frames[PostSource] - 1);

    QString matches("Matches: ");
    for (int i = matches_start; i <= matches_end; i++) {
        char match = project->matches[i];

        bool is_decimated = project->isDecimatedFrame(i);

        if (i % 5 == 0)
            matches += "<u>";

        if (is_decimated)
            matches += "<s>";

        if (i == current_frame)
            match += 'C' - 'c';
        matches += match;

        if (is_decimated)
            matches += "</s>";

        if (i % 5 == 0)
            matches += "</u>";
    }
    matches_label->setText(matches);


    if (project->isCombedFrame(current_frame))
        combed_label->setText(QStringLiteral("Combed"));
    else
        combed_label->clear();


    decimate_metric_label->setText(QStringLiteral("DMetric: ") + QString::number(project->decimate_metrics[current_frame]));


    int match_index = matchCharToIndex(project->matches[current_frame]);
    QString mics("Mics: ");
    for (int i = 0; i < 5; i++) {
        if (i == match_index)
            mics += "<b>";

        mics += QStringLiteral("%1 ").arg((int)project->mics[current_frame][i]);

        if (i == match_index)
            mics += "</b>";
    }
    mic_label->setText(mics);


    const Section *current_section = project->findSection(current_frame);
    int section_start = current_section->start;
    int section_end = project->getSectionEnd(section_start) - 1;

    QString presets;
    for (auto it = current_section->presets.cbegin(); it != current_section->presets.cend(); it++)
        presets.append(QString::fromStdString(*it));

    if (presets.isNull())
        presets = "<none>";

    section_label->setText(QStringLiteral("Section: [%1,%2]\nPresets:\n%3").arg(section_start).arg(section_end).arg(presets));


    QString custom_lists;
    for (auto it = project->custom_lists.cbegin(); it != project->custom_lists.cend(); it++) {
        const FrameRange *range = it->findFrameRange(current_frame);
        if (range)
            custom_lists += QStringLiteral("%1: [%2,%3]\n").arg(QString::fromStdString(it->name)).arg(range->first).arg(range->last);
    }

    if (custom_lists.isNull())
        custom_lists = "<none>";

    custom_list_label->setText(QStringLiteral("Custom lists:\n%1").arg(custom_lists));


    const FreezeFrame *freeze = project->findFreezeFrame(current_frame);
    if (freeze)
        freeze_label->setText(QStringLiteral("Frozen: [%1,%2,%3]").arg(freeze->first).arg(freeze->last).arg(freeze->replacement));
    else
        freeze_label->clear();
}


void WobblyWindow::jumpRelative(int offset) {
    int target = current_frame + offset;

    if (target < 0)
        target = 0;
    if (target >= project->num_frames[PostSource])
        target = project->num_frames[PostSource] - 1;

    if (preview) {
        int skip = offset < 0 ? -1 : 1;

        while (true) {
            if (target == 0 || target == project->num_frames[PostSource] - 1)
                skip = -skip;

            if (!project->isDecimatedFrame(target))
                break;

            target += skip;
        }
    }

    displayFrame(target);
}


void WobblyWindow::jump1Backward() {
    jumpRelative(-1);
}


void WobblyWindow::jump1Forward() {
    jumpRelative(1);
}


void WobblyWindow::jump5Backward() {
    jumpRelative(-5);
}


void WobblyWindow::jump5Forward() {
    jumpRelative(5);
}


void WobblyWindow::jump50Backward() {
    jumpRelative(-50);
}


void WobblyWindow::jump50Forward() {
    jumpRelative(50);
}


void WobblyWindow::jumpALotBackward() {
    if (!project)
        return;

    int twenty_percent = project->num_frames[PostSource] * 20 / 100;

    jumpRelative(-twenty_percent);
}


void WobblyWindow::jumpALotForward() {
    if (!project)
        return;

    int twenty_percent = project->num_frames[PostSource] * 20 / 100;

    jumpRelative(twenty_percent);
}


void WobblyWindow::jumpToStart() {
    jumpRelative(0 - current_frame);
}


void WobblyWindow::jumpToEnd() {
    if (!project)
        return;

    jumpRelative(project->num_frames[PostSource] - current_frame);
}


void WobblyWindow::jumpToNextSectionStart() {
    if (!project)
        return;

    const Section *next_section = project->findNextSection(current_frame);

    if (next_section)
        jumpRelative(next_section->start - current_frame);
}


void WobblyWindow::jumpToPreviousSectionStart() {
    if (!project)
        return;

    if (current_frame == 0)
        return;

    const Section *section = project->findSection(current_frame);
    if (section->start == current_frame)
        section = project->findSection(current_frame - 1);

    jumpRelative(section->start - current_frame);
}


void WobblyWindow::cycleMatchPCN() {
    // N -> C -> P. This is the order Yatta uses, so we use it.

    char &match = project->matches[current_frame];

    if (match == 'n')
        match = 'c';
    else if (match == 'c') {
        if (current_frame == 0)
            match = 'n';
        else
            match = 'p';
    } else if (match == 'p') {
        if (current_frame == project->num_frames[PostSource] - 1)
            match = 'c';
        else
            match = 'n';
    }

    evaluateMainDisplayScript();
}


void WobblyWindow::freezeForward() {
    if (current_frame == project->num_frames[PostSource] - 1)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame + 1);

        evaluateMainDisplayScript();
    } catch (WobblyException &) {
        // XXX Maybe don't be silent.
    }
}


void WobblyWindow::freezeBackward() {
    if (current_frame == 0)
        return;

    try {
        project->addFreezeFrame(current_frame, current_frame, current_frame - 1);

        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::freezeRange() {
    static FreezeFrame ff = { -1, -1, -1 };

    // XXX Don't bother if first or last are part of a freezeframe.
    if (ff.first == -1)
        ff.first = current_frame;
    else if (ff.last == -1)
        ff.last = current_frame;
    else if (ff.replacement == -1) {
        ff.replacement = current_frame;
        try {
            project->addFreezeFrame(ff.first, ff.last, ff.replacement);

            evaluateMainDisplayScript();
        } catch (WobblyException &) {

        }

        ff = { -1, -1, -1 };
    }
}


void WobblyWindow::deleteFreezeFrame() {
    const FreezeFrame *ff = project->findFreezeFrame(current_frame);
    if (ff) {
        project->deleteFreezeFrame(ff->first);

        evaluateMainDisplayScript();
    }
}


void WobblyWindow::toggleDecimation() {
    if (project->isDecimatedFrame(current_frame))
        project->deleteDecimatedFrame(current_frame);
    else
        project->addDecimatedFrame(current_frame);

    updateFrameDetails();
}


void WobblyWindow::toggleCombed() {
    if (project->isCombedFrame(current_frame))
        project->deleteCombedFrame(current_frame);
    else
        project->addCombedFrame(current_frame);

    updateFrameDetails();
}


void WobblyWindow::addSection() {
    const Section *section = project->findSection(current_frame);
    if (section->start != current_frame) {
        project->addSection(current_frame);

        updateFrameDetails();
    }
}


void WobblyWindow::deleteSection() {
    const Section *section = project->findSection(current_frame);
    project->deleteSection(section->start);

    updateFrameDetails();
}


void WobblyWindow::cropChanged(int value) {
    (void)value;

    if (!project)
        return;

    project->setCrop(crop_spin[0]->value(), crop_spin[1]->value(), crop_spin[2]->value(), crop_spin[3]->value());

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::cropToggled(bool checked) {
    if (!project)
        return;

    project->setCropEnabled(checked);

    try {
        evaluateMainDisplayScript();
    } catch (WobblyException &) {

    }
}


void WobblyWindow::resizeChanged(int value) {
    (void)value;

    if (!project)
        return;

    project->setResize(resize_spin[0]->value(), resize_spin[1]->value());
}


void WobblyWindow::resizeToggled(bool checked) {
    if (!project)
        return;

    project->setResizeEnabled(checked);
}


void WobblyWindow::presetChanged(const QString &text) {
    if (!project)
        return;

    if (text.isEmpty())
        preset_edit->setPlainText(QString());
    else
        preset_edit->setPlainText(QString::fromStdString(project->getPresetContents(text.toStdString())));
}


void WobblyWindow::presetEdited() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    project->setPresetContents(preset_combo->currentText().toStdString(), preset_edit->toPlainText().toStdString());
}


void WobblyWindow::presetNew() {
    if (!project)
        return;

    QString preset_name = QInputDialog::getText(this, QStringLiteral("New preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."));

    if (!preset_name.isEmpty()) {
        try {
            project->addPreset(preset_name.toStdString());

            preset_combo->addItem(preset_name);
            preset_combo->setCurrentText(preset_name);

            presetChanged(preset_name);
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }
}


void WobblyWindow::presetRename() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    QString preset_name = QInputDialog::getText(this, QStringLiteral("Rename preset"), QStringLiteral("Use only letters, numbers, and the underscore character.\nThe first character cannot be a number."), QLineEdit::Normal, preset_combo->currentText());

    if (!preset_name.isEmpty() && preset_name != preset_combo->currentText()) {
        try {
            project->renamePreset(preset_combo->currentText().toStdString(), preset_name.toStdString());

            preset_combo->setItemText(preset_combo->currentIndex(), preset_name);

            //presetChanged(preset_name);

            // If the preset's name was displayed in other places, update them.
        } catch (WobblyException &e) {
            errorPopup(e.what());
        }
    }

}


void WobblyWindow::presetDelete() {
    if (!project)
        return;

    if (preset_combo->currentIndex() == -1)
        return;

    project->deletePreset(preset_combo->currentText().toStdString());

    preset_combo->removeItem(preset_combo->currentIndex());

    presetChanged(preset_combo->currentText());
}


void WobblyWindow::resetRange() {
    if (!project)
        return;

    //evaluateMainDisplayScript();
}


void WobblyWindow::resetSection() {
    if (!project)
        return;

    const Section *section = project->findSection(current_frame);

    project->resetSectionMatches(section->start);

    evaluateMainDisplayScript();
}


void WobblyWindow::rotateAndSetPatterns() {
    if (!project)
        return;

    int size = match_pattern.size();
    match_pattern.prepend(match_pattern[size - 1]);
    match_pattern.truncate(size);
    match_pattern_edit->setText(match_pattern);

    size = decimation_pattern.size();
    decimation_pattern.prepend(decimation_pattern[size - 1]);
    decimation_pattern.truncate(size);
    decimation_pattern_edit->setText(decimation_pattern);

    const Section *section = project->findSection(current_frame);

    project->setSectionMatchesFromPattern(section->start, match_pattern.toStdString());
    project->setSectionDecimationFromPattern(section->start, decimation_pattern.toStdString());

    evaluateMainDisplayScript();
}


void WobblyWindow::matchPatternEdited(const QString &text) {
    match_pattern = text;
}


void WobblyWindow::decimationPatternEdited(const QString &text) {
    decimation_pattern = text;
}


void WobblyWindow::togglePreview() {
    preview = !preview;

    if (preview) {
        try {
            evaluateFinalScript();
        } catch (WobblyException &e) {
            errorPopup(e.what());
            preview = !preview;
        }
    } else
        evaluateMainDisplayScript();
}
