#include <cstdio>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "WobblyException.h"
#include "WobblyProject.h"


void WobblyProject::writeProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open project file. Error message: " + file.errorString());

    project_path = path;

    QJsonObject json_project;

    json_project.insert("wibbly wobbly version", 42); // XXX use real version


    json_project.insert("input file", QString::fromStdString(input_file));


    QJsonArray json_trims;

    for (auto it = trims.cbegin(); it != trims.cend(); it++) {
        QJsonArray json_trim;
        json_trim.append(it->second.first);
        json_trim.append(it->second.last);
        json_trims.append(json_trim);
    }
    json_project.insert("trim", json_trims);


    QJsonObject json_vfm_parameters;

    for (auto it = vfm_parameters.cbegin(); it != vfm_parameters.cend(); it++)
        json_vfm_parameters.insert(QString::fromStdString(it->first), it->second);

    json_project.insert("vfm parameters", json_vfm_parameters);


    QJsonObject json_vdecimate_parameters;

    for (auto it = vdecimate_parameters.cbegin(); it != vdecimate_parameters.cend(); it++)
        json_vdecimate_parameters.insert(QString::fromStdString(it->first), it->second);

    json_project.insert("vdecimate parameters", json_vdecimate_parameters);


    QJsonArray json_mics, json_matches, json_combed_frames, json_decimated_frames, json_decimate_metrics;

    for (size_t i = 0; i < mics.size(); i++) {
        QJsonArray json_mic;
        for (int j = 0; j < 5; j++)
            json_mic.append(mics[i][j]);
        json_mics.append(json_mic);
    }

    for (size_t i = 0; i < matches.size(); i++)
        json_matches.append(QString(matches[i]));

    for (size_t i = 0; i < combed_frames.size(); i++)
        json_combed_frames.append(combed_frames[i]);

    for (size_t i = 0; i < decimated_frames.size(); i++)
        json_decimated_frames.append(decimated_frames[i]);

    for (size_t i = 0; i < decimate_metrics.size(); i++)
        json_decimate_metrics.append(decimate_metrics[i]);

    json_project.insert("mics", json_mics);
    json_project.insert("matches", json_matches);
    json_project.insert("combed frames", json_combed_frames);
    json_project.insert("decimated frames", json_decimated_frames);
    json_project.insert("decimate metrics", json_decimate_metrics);


    QJsonObject json_section_types;
    QJsonArray json_sections[3];

    for (int i = 0; i < 3; i++) { // XXX Magic numbers are bad.
        if (!is_wobbly && (i == PostSource || i == PostDecimate))
            continue;

        for (auto it = sections[i].cbegin(); it != sections[i].cend(); it++) {
            QJsonObject json_section;
            json_section.insert("start", it->second.start);
            QJsonArray json_presets;
            for (size_t j = 0; j < it->second.presets.size(); j++)
                json_presets.append(QString::fromStdString(it->second.presets[i]));
            json_section.insert("presets", json_presets);
            json_section.insert("fps_num", (qint64)it->second.fps_num);
            json_section.insert("fps_den", (qint64)it->second.fps_den);
            json_section.insert("num_frames", it->second.num_frames);

            json_sections[i].append(json_section);
        }
    }

    json_section_types.insert("post field match", json_sections[PostFieldMatch]);
    if (is_wobbly) {
        json_section_types.insert("post source", json_sections[PostSource]);
        json_section_types.insert("post decimate", json_sections[PostDecimate]);
    }
    json_project.insert("sections", json_section_types);


    if (is_wobbly) {
        QJsonArray json_presets, json_frozen_frames;

        for (auto it = presets.cbegin(); it != presets.cend(); it++) {
            QJsonObject json_preset;
            json_preset.insert("name", QString::fromStdString(it->second.name));
            json_preset.insert("contents", QString::fromStdString(it->second.contents));

            json_presets.append(json_preset);
        }

        for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++) {
            QJsonArray json_ff;
            json_ff.append(it->second.first);
            json_ff.append(it->second.last);
            json_ff.append(it->second.replacement);

            json_frozen_frames.append(json_ff);
        }

        json_project.insert("presets", json_presets);
        json_project.insert("frozen frames", json_frozen_frames);


        QJsonObject json_custom_list_types;
        QJsonArray json_custom_lists[3];

        for (int i = 0; i < 3; i++) {
            for (size_t j = 0; j < custom_lists[i].size(); j++) {
                QJsonObject json_custom_list;
                json_custom_list.insert("name", QString::fromStdString(custom_lists[i][j].name));
                json_custom_list.insert("preset", QString::fromStdString(custom_lists[i][j].preset));
                QJsonArray json_frames;
                for (auto it = custom_lists[i][j].frames.cbegin(); it != custom_lists[i][j].frames.cend(); it++) {
                    QJsonArray json_pair;
                    json_pair.append(it->second.first);
                    json_pair.append(it->second.last);
                    json_frames.append(json_pair);
                }
                json_custom_list.insert("frames", json_frames);

                json_custom_lists[i].append(json_custom_list);
            }
        }

        json_custom_list_types.insert("post source", json_custom_lists[PostSource]);
        json_custom_list_types.insert("post field match", json_custom_lists[PostFieldMatch]);
        json_custom_list_types.insert("post decimate", json_custom_lists[PostDecimate]);
        json_project.insert("custom lists", json_custom_list_types);


        QJsonObject json_resize, json_crop;

        json_resize.insert("width", resize.width);
        json_resize.insert("height", resize.height);
        json_project.insert("resize", json_resize);

        json_crop.insert("left", crop.left);
        json_crop.insert("top", crop.top);
        json_crop.insert("right", crop.right);
        json_crop.insert("bottom", crop.bottom);
        json_project.insert("crop", json_crop);
    }

    QJsonDocument json_doc(json_project);

    file.write(json_doc.toJson(QJsonDocument::Indented));
}

void WobblyProject::readProject(const std::string &path) {
    // XXX Make sure the things only written by Wobbly get sane defaults. Actually, make sure everything has sane defaults, since Wibbly doesn't always write all the categories.

    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::ReadOnly))
        throw WobblyException("Couldn't open project file. Error message: " + file.errorString());

    project_path = path;

    QByteArray data = file.readAll();

    QJsonDocument json_doc(QJsonDocument::fromJson(data));

    QJsonObject json_project = json_doc.object();


    //int version = (int)json_project["wibbly wobbly version"].toDouble();


    input_file = json_project["input file"].toString().toStdString();


    num_frames_after_trim = 0;

    QJsonArray json_trims = json_project["trim"].toArray();
    for (int i = 0; i < json_trims.size(); i++) {
        QJsonArray json_trim = json_trims[i].toArray();
        FrameRange range;
        range.first = (int)json_trim[0].toDouble();
        range.last = (int)json_trim[1].toDouble();
        trims.insert(std::make_pair(range.first, range));
        num_frames_after_trim += range.last - range.first + 1;
    }


    QJsonObject json_vfm_parameters = json_project["vfm parameters"].toObject();

    QStringList keys = json_vfm_parameters.keys();
    for (int i = 0; i < keys.size(); i++)
        vfm_parameters.insert(std::make_pair(keys.at(i).toStdString(), json_vfm_parameters[keys.at(i)].toDouble()));


    QJsonObject json_vdecimate_parameters = json_project["vdecimate parameters"].toObject();

    keys = json_vdecimate_parameters.keys();
    for (int i = 0; i < keys.size(); i++)
        vdecimate_parameters.insert(std::make_pair(keys.at(i).toStdString(), json_vdecimate_parameters[keys.at(i)].toDouble()));


    QJsonArray json_mics, json_matches, json_original_matches, json_combed_frames, json_decimated_frames, json_decimate_metrics;


    json_mics = json_project["mics"].toArray();
    mics.resize(json_mics.size());
    for (int i = 0; i < json_mics.size(); i++) {
        QJsonArray json_mic = json_mics[i].toArray();
        for (int j = 0; j < 5; j++)
            mics[i][j] = (int16_t)json_mic[j].toDouble();
    }


    json_matches = json_project["matches"].toArray();
    matches.resize(json_matches.size());
    for (int i = 0; i < json_matches.size(); i++)
        matches[i] = json_matches[i].toString().toStdString()[0];


    json_original_matches = json_project["original matches"].toArray();
    original_matches.resize(json_original_matches.size());
    for (int i = 0; i < json_original_matches.size(); i++)
        original_matches[i] = json_original_matches[i].toString().toStdString()[0];

    if (original_matches.size() == 0) {
        original_matches.resize(num_frames_after_trim);
        memset(original_matches.data(), 'c', original_matches.size());
    }

    if (matches.size() == 0) {
        matches.resize(original_matches.size());
        memcpy(matches.data(), original_matches.data(), matches.size());
    }


    json_combed_frames = json_project["combed frames"].toArray();
    combed_frames.resize(json_combed_frames.size());
    for (int i = 0; i < json_combed_frames.size(); i++)
        combed_frames[i] = (int)json_combed_frames[i].toDouble();


    json_decimated_frames = json_project["decimated frames"].toArray();
    decimated_frames.resize(json_decimated_frames.size());
    for (int i = 0; i < json_decimated_frames.size(); i++)
        decimated_frames[i] = (int)json_decimated_frames[i].toDouble();


    json_decimate_metrics = json_project["decimate metrics"].toArray();
    decimate_metrics.resize(json_decimate_metrics.size());
    for (int i = 0; i < json_decimate_metrics.size(); i++)
        decimate_metrics[i] = (int)json_decimate_metrics[i].toDouble();


    QJsonArray json_presets, json_frozen_frames;

    json_presets = json_project["presets"].toArray();
    for (int i = 0; i < json_presets.size(); i++) {
        QJsonObject json_preset = json_presets[i].toObject();
        Preset preset = {
            .name = json_preset["name"].toString().toStdString(),
            .contents = json_preset["contents"].toString().toStdString()
        };
        presets.insert(std::make_pair(preset.name, preset));
    }


    json_frozen_frames = json_project["frozen frames"].toArray();
    for (int i = 0; i < json_frozen_frames.size(); i++) {
        QJsonArray json_ff = json_frozen_frames[i].toArray();
        FreezeFrame ff = {
            .first = (int)json_ff[0].toDouble(),
            .last = (int)json_ff[1].toDouble(),
            .replacement = (int)json_ff[2].toDouble()
        };
        frozen_frames.insert(std::make_pair(ff.first, ff));
    }


    QJsonArray json_sections[3], json_custom_lists[3];

    json_sections[0] = json_project["sections"].toObject()["post source"].toArray();
    json_sections[1] = json_project["sections"].toObject()["post field match"].toArray();
    json_sections[2] = json_project["sections"].toObject()["post decimate"].toArray();

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < json_sections[i].size(); j++) {
            Section section;
            QJsonObject json_section = json_sections[i][j].toObject();
            section.start = (int)json_section["start"].toDouble();
            json_presets = json_section["presets"].toArray();
            section.presets.resize(json_presets.size());
            for (int k = 0; k < json_presets.size(); k++)
                section.presets[k] = json_presets[k].toString().toStdString();
            section.fps_num = (int)json_section["fps_num"].toDouble();
            section.fps_den = (int)json_section["fps_den"].toDouble();
            section.num_frames = (int)json_section["num_frames"].toDouble();

            sections[i].insert(std::make_pair(section.start, section));
        }

        if (json_sections[i].size() == 0) {
            Section section;
            section.start = 0;
            section.fps_num = 0;
            section.fps_den = 0;
            section.num_frames = 0;

            sections[i].insert(std::make_pair(section.start, section));
        }
    }

    json_custom_lists[0] = json_project["custom lists"].toObject()["post source"].toArray();
    json_custom_lists[1] = json_project["custom lists"].toObject()["post field match"].toArray();
    json_custom_lists[2] = json_project["custom lists"].toObject()["post decimate"].toArray();

    for (int i = 0; i < 3; i++) {
        custom_lists[i].resize(json_custom_lists[i].size());

        for (int j = 0; j < json_custom_lists[i].size(); j++) {
            CustomList list;
            QJsonObject json_list = json_custom_lists[i][j].toObject();
            list.name = json_list["name"].toString().toStdString();
            list.preset = json_list["preset"].toString().toStdString();
            QJsonArray json_frames = json_list["frames"].toArray();
            for (int k = 0; k < json_frames.size(); k++) {
                QJsonArray json_range = json_frames[k].toArray();
                list.addFrameRange((int)json_range[0].toDouble(), (int)json_range[1].toDouble());
            }

            custom_lists[i][j] = list;
        }
    }


    QJsonObject json_resize, json_crop;

    json_resize = json_project["resize"].toObject();
    resize.width = (int)json_resize["width"].toDouble();
    resize.height = (int)json_resize["height"].toDouble();

    json_crop = json_project["crop"].toObject();
    crop.left = (int)json_crop["left"].toDouble();
    crop.top = (int)json_crop["top"].toDouble();
    crop.right = (int)json_crop["right"].toDouble();
    crop.bottom = (int)json_crop["bottom"].toDouble();
}

void WobblyProject::addFreezeFrame(int first, int last, int replacement) {
    // XXX Make sure it doesn't overlap with any existing freezeframe. If findFreezeFrame(first) and findFreezeFrame(last) both return -1, it's all good.
    FreezeFrame ff = {
        .first = first,
        .last = last,
        .replacement = replacement
    };
    frozen_frames.insert(std::make_pair(first, ff));
}

void WobblyProject::deleteFreezeFrame(int frame) {
    frozen_frames.erase(frame);
}

// Find the FreezeFrame this frame belongs to.
int WobblyProject::findFreezeFrame(int frame) {
    // XXX A more efficient search algorithm is in order. This function will likely be called every time a frame is to be displayed. lower_bound() maybe.
    for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++)
        if (it->second.first <= frame && frame <= it->second.last)
            return it->first;

    return -1;
}


void WobblyProject::addPreset(const std::string &preset_name) {
    std::string contents = "def ";
    contents += preset_name;
    contents +=
            "(clip):\n"
            "\n"
            "\n"
            "    return clip\n";
    addPreset(preset_name, contents);
}

void WobblyProject::addPreset(const std::string &preset_name, const std::string &preset_contents) {
    Preset preset;
    preset.name = preset_name;
    preset.contents = preset_contents;
    presets.insert(std::make_pair(preset_name, preset));
}

void WobblyProject::deletePreset(const std::string &preset_name) {
    presets.erase(preset_name);
}

void WobblyProject::assignPresetToSection(const std::string &preset_name, PositionInFilterChain position, int section_start) {
    // The user may want to assign the same preset twice.
    sections[position].at(section_start).presets.push_back(preset_name);
}


void WobblyProject::setMatch(int frame, char match) {
    matches[frame] = match;
}


void WobblyProject::addSection(int section_start, PositionInFilterChain position) {
    Section section;
    section.start = section_start;
    sections[position].insert(std::make_pair(section_start, section));
}

void WobblyProject::deleteSection(int section_start, PositionInFilterChain position) {
    sections[position].erase(section_start);
}


void WobblyProject::addCustomList(const std::string &list_name, PositionInFilterChain position) {
    // XXX no duplicates
    CustomList list;
    list.name = list_name;
    custom_lists[position].push_back(list);
}

void WobblyProject::deleteCustomList(const std::string &list_name, PositionInFilterChain position) { // XXX Maybe overload to take an index instead of name.
    for (auto it = custom_lists[position].cbegin(); it != custom_lists[position].cend(); it++)
        if (it->name == list_name) {
            custom_lists[position].erase(it);
            break;
        }
}


void WobblyProject::sectionsToScript(std::string &script, PositionInFilterChain position) {
    // XXX Make a temporary copy of the sections map and merge sections with identical presets, to generate as few trims as possible.
    std::string splice = "src = c.std.Splice(mismatch=True, clips=[";
    for (auto it = sections[position].cbegin(); it != sections[position].cend(); it++) {
        std::string section_name = "section";
        section_name += std::to_string(it->second.start);
        script += section_name + " = src";

        for (size_t i = 0; i < it->second.presets.size(); i++) {
            script += "\n";
            script += section_name + " = ";
            script += it->second.presets[i] + "(";
            script += section_name + ")";
        }

        script += "[";
        script += std::to_string(it->second.start);
        script += ":";

        auto it_next = it;
        it_next++;
        if (it_next != sections[position].cend())
            script += it_next->second.start;
        script += "]\n";

        splice += section_name + ",";
    }
    splice +=
            "])\n"
            "\n";

    script += splice;
}

void WobblyProject::customListsToScript(std::string &script, PositionInFilterChain position) {
    const std::vector<CustomList> &lists = custom_lists[position];

    for (size_t i = 0; i < lists.size(); i++) {
        std::string list_name = "cl_";
        list_name += lists[i].name;

        script += list_name + " = " + lists[i].preset + "(src)\n";

        std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

        auto it = lists[i].frames.cbegin();
        auto it_prev = it;

        if (it->second.first > 0) {
            splice += "src[0:";
            splice += std::to_string(it->second.first) + "],";
        }
        splice += list_name + "[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";

        it++;
        for ( ; it != lists[i].frames.cend(); it++, it_prev++) {
            splice += "src[";
            splice += std::to_string(it_prev->second.last + 1) + ":" + std::to_string(it->second.first) + "],";

            splice += list_name + "[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
        }

        // it_prev is cend()-1 at the end of the loop.

        if (it_prev->second.last < num_frames_after_trim - 1) {
            splice += "src[";
            splice += std::to_string(it_prev->second.last + 1) + ":]";
        }

        splice += "])\n\n";

        script += splice;
    }
}

void WobblyProject::headerToScript(std::string &script) {
    script +=
            "import vapoursynth as vs\n"
            "\n"
            "c = vs.get_core()\n"
            "\n";
}

void WobblyProject::presetsToScript(std::string &script) {
    for (auto it = presets.cbegin(); it != presets.cend(); it++)
        script += it->second.contents + "\n\n";
}

void WobblyProject::sourceToScript(std::string &script) {
    script += "src = c.d2v.Source(input='";
    script += input_file;
    script +=
            "')\n"
            "\n";
}

void WobblyProject::trimToScript(std::string &script) {
    script += "src = c.std.Splice(clips=[";
    for (auto it = trims.cbegin(); it != trims.cend(); it++)
        script += "src[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
    script +=
            "])\n"
            "\n";
}

void WobblyProject::fieldHintToScript(std::string &script) {
    script += "src = c.fh.FieldHint(clip=src, tff=";
    script += std::to_string((int)vfm_parameters["order"]);
    script += ", matches='";
    script.append(matches.data(), matches.size());
    script +=
            "')\n"
            "\n";
}

void WobblyProject::freezeFramesToScript(std::string &script) {
    std::string ff_first = ", first=[";
    std::string ff_last = ", last=[";
    std::string ff_replacement = ", replacement=[";

    for (auto it = frozen_frames.cbegin(); it != frozen_frames.cend(); it++) {
        ff_first += std::to_string(it->second.first) + ",";
        ff_last += std::to_string(it->second.last) + ",";
        ff_replacement += std::to_string(it->second.replacement) + ",";
    }
    ff_first += "]";
    ff_last += "]";
    ff_replacement += "]";

    script += "src = c.std.FreezeFrames(clip=src";
    script += ff_first;
    script += ff_last;
    script += ff_replacement;
    script +=
            ")\n"
            "\n";
}

void WobblyProject::decimatedFramesToScript(std::string &script) {
    script += "src = c.std.DeleteFrames(clip=src, frames=[";

    for (size_t i = 0; i < decimated_frames.size(); i++)
        script += std::to_string(decimated_frames[i]) + ",";

    script +=
            "])\n"
            "\n";
}

void WobblyProject::cropToScript(std::string &script) {
    script += "src = c.std.CropRel(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ")\n\n";
}

void WobblyProject::resizeToScript(std::string &script) {
    script += "src = c.resize.Bicubic(clip=src, width=";
    script += std::to_string(resize.width) + ", height=";
    script += std::to_string(resize.height) + ")\n\n";
}

void WobblyProject::rgbConversionToScript(std::string &script) {
    // XXX use zimg
    script +=
            "src = c.std.FlipVertical(clip=src)\n"
            "src = c.resize.Bicubic(clip=src, format=vs.COMPATBGR32)\n"
            "\n";
}

void WobblyProject::setOutputToScript(std::string &script) {
    script += "src.set_output()\n";
}

std::string WobblyProject::generateFinalScript() {
    // XXX Insert comments before and after each part.
    std::string script;

    headerToScript(script);

    presetsToScript(script);

    sourceToScript(script);

    trimToScript(script);

    // XXX Put them in the same order as Yatta does.
    sectionsToScript(script, PostSource);

    customListsToScript(script, PostSource);

    fieldHintToScript(script);

    // XXX Put them and FreezeFrames in the same order as Yatta does.
    sectionsToScript(script, PostFieldMatch);

    customListsToScript(script, PostFieldMatch);

    if (frozen_frames.size())
        freezeFramesToScript(script);

    if (decimated_frames.size())
        decimatedFramesToScript(script);

    // XXX DeleteFrames doesn't change the frame rate or the frame durations. This must be done separately.

    // XXX Put them in the same order as Yatta does.
    sectionsToScript(script, PostDecimate);

    customListsToScript(script, PostDecimate);

    cropToScript(script);

    resizeToScript(script);

    setOutputToScript(script);

    return script;
}

std::string WobblyProject::generateMainDisplayScript() {
    // I guess use text.Text to print matches, frame number, metrics, etc. Or just QLabels.

    std::string script;

    headerToScript(script);

    sourceToScript(script);

    trimToScript(script);

    fieldHintToScript(script);

    if (frozen_frames.size())
        freezeFramesToScript(script);

    rgbConversionToScript(script);

    setOutputToScript(script);

    return script;
}