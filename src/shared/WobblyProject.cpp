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


WobblyProject::WobblyProject(bool _is_wobbly)
    : is_wobbly(_is_wobbly)
{

}


void WobblyProject::writeProject(const std::string &path) {
    QFile file(QString::fromStdString(path));

    if (!file.open(QIODevice::WriteOnly))
        throw WobblyException("Couldn't open project file. Error message: " + file.errorString());

    project_path = path;

    QJsonObject json_project;

    json_project.insert("wibbly wobbly version", 42); // XXX use real version


    json_project.insert("input file", QString::fromStdString(input_file));


    QJsonArray json_fps;
    json_fps.append((qint64)fps_num);
    json_fps.append((qint64)fps_den);
    json_project.insert("input frame rate", json_fps);


    QJsonArray json_resolution;
    json_resolution.append(width);
    json_resolution.append(height);
    json_project.insert("input resolution", json_resolution);


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

    for (auto it = combed_frames.cbegin(); it != combed_frames.cend(); it++)
        json_combed_frames.append(*it);

    for (size_t i = 0; i < decimated_frames.size(); i++)
        for (auto it = decimated_frames[i].cbegin(); it != decimated_frames[i].cend(); it++)
            json_decimated_frames.append((int)i * 5 + *it);

    for (size_t i = 0; i < decimate_metrics.size(); i++)
        json_decimate_metrics.append(decimate_metrics[i]);

    json_project.insert("mics", json_mics);
    json_project.insert("matches", json_matches);
    json_project.insert("combed frames", json_combed_frames);
    json_project.insert("decimated frames", json_decimated_frames);
    json_project.insert("decimate metrics", json_decimate_metrics);


    QJsonArray json_sections;

    for (auto it = sections.cbegin(); it != sections.cend(); it++) {
        QJsonObject json_section;
        json_section.insert("start", it->second.start);
        QJsonArray json_presets;
        for (size_t i = 0; i < it->second.presets.size(); i++)
            json_presets.append(QString::fromStdString(it->second.presets[i]));
        json_section.insert("presets", json_presets);
        json_section.insert("fps_num", (qint64)it->second.fps_num);
        json_section.insert("fps_den", (qint64)it->second.fps_den);
        json_section.insert("num_frames", it->second.num_frames);

        json_sections.append(json_section);
    }

    json_project.insert("sections", json_sections);


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


        QJsonArray json_custom_lists;

        for (size_t i = 0; i < custom_lists.size(); i++) {
            QJsonObject json_custom_list;
            json_custom_list.insert("name", QString::fromStdString(custom_lists[i].name));
            json_custom_list.insert("preset", QString::fromStdString(custom_lists[i].preset));
            json_custom_list.insert("position", custom_lists[i].position);
            QJsonArray json_frames;
            for (auto it = custom_lists[i].frames.cbegin(); it != custom_lists[i].frames.cend(); it++) {
                QJsonArray json_pair;
                json_pair.append(it->second.first);
                json_pair.append(it->second.last);
                json_frames.append(json_pair);
            }
            json_custom_list.insert("frames", json_frames);

            json_custom_lists.append(json_custom_list);
        }

        json_project.insert("custom lists", json_custom_lists);


        if (resize.enabled) {
            QJsonObject json_resize;
            json_resize.insert("width", resize.width);
            json_resize.insert("height", resize.height);
            json_project.insert("resize", json_resize);
        }

        if (crop.enabled) {
            QJsonObject json_crop;
            json_crop.insert("left", crop.left);
            json_crop.insert("top", crop.top);
            json_crop.insert("right", crop.right);
            json_crop.insert("bottom", crop.bottom);
            json_project.insert("crop", json_crop);
        }
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


    fps_num = (int64_t)json_project["input frame rate"].toArray()[0].toDouble();
    fps_den = (int64_t)json_project["input frame rate"].toArray()[1].toDouble();


    width = (int)json_project["input resolution"].toArray()[0].toDouble();
    height = (int)json_project["input resolution"].toArray()[1].toDouble();


    num_frames[PostSource] = 0;

    QJsonArray json_trims = json_project["trim"].toArray();
    for (int i = 0; i < json_trims.size(); i++) {
        QJsonArray json_trim = json_trims[i].toArray();
        FrameRange range;
        range.first = (int)json_trim[0].toDouble();
        range.last = (int)json_trim[1].toDouble();
        trims.insert(std::make_pair(range.first, range));
        num_frames[PostSource] += range.last - range.first + 1;
    }

    num_frames[PostFieldMatch] = num_frames[PostSource];

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
    mics.resize(num_frames[PostSource], { 0 });
    for (int i = 0; i < json_mics.size(); i++) {
        QJsonArray json_mic = json_mics[i].toArray();
        for (int j = 0; j < 5; j++)
            mics[i][j] = (int16_t)json_mic[j].toDouble();
    }


    json_matches = json_project["matches"].toArray();
    matches.resize(num_frames[PostSource], 'c');
    for (int i = 0; i < std::min(json_matches.size(), (int)matches.size()); i++)
        matches[i] = json_matches[i].toString().toStdString()[0];


    json_original_matches = json_project["original matches"].toArray();
    original_matches.resize(num_frames[PostSource], 'c');
    for (int i = 0; i < std::min(json_original_matches.size(), (int)original_matches.size()); i++)
        original_matches[i] = json_original_matches[i].toString().toStdString()[0];

    if (json_matches.size() == 0 && json_original_matches.size() != 0) {
        memcpy(matches.data(), original_matches.data(), matches.size());
    }


    json_combed_frames = json_project["combed frames"].toArray();
    for (int i = 0; i < json_combed_frames.size(); i++)
        addCombedFrame((int)json_combed_frames[i].toDouble());


    decimated_frames.resize((num_frames[PostSource] - 1) / 5 + 1);
    json_decimated_frames = json_project["decimated frames"].toArray();
    for (int i = 0; i < json_decimated_frames.size(); i++)
        addDecimatedFrame((int)json_decimated_frames[i].toDouble());

    // num_frames[PostDecimate] is correct at this point.

    json_decimate_metrics = json_project["decimate metrics"].toArray();
    decimate_metrics.resize(num_frames[PostSource], 0);
    for (int i = 0; i < std::min(json_decimate_metrics.size(), (int)decimate_metrics.size()); i++)
        decimate_metrics[i] = (int)json_decimate_metrics[i].toDouble();


    QJsonArray json_presets, json_frozen_frames;

    json_presets = json_project["presets"].toArray();
    for (int i = 0; i < json_presets.size(); i++) {
        QJsonObject json_preset = json_presets[i].toObject();
        addPreset(json_preset["name"].toString().toStdString(), json_preset["contents"].toString().toStdString());
    }


    json_frozen_frames = json_project["frozen frames"].toArray();
    for (int i = 0; i < json_frozen_frames.size(); i++) {
        QJsonArray json_ff = json_frozen_frames[i].toArray();
        addFreezeFrame((int)json_ff[0].toDouble(), (int)json_ff[1].toDouble(), (int)json_ff[2].toDouble());
    }


    QJsonArray json_sections, json_custom_lists;

    json_sections = json_project["sections"].toArray();

    for (int j = 0; j < json_sections.size(); j++) {
        QJsonObject json_section = json_sections[j].toObject();
        int section_start = (int)json_section["start"].toDouble();
        int section_fps_num = (int)json_section["fps_num"].toDouble();
        int section_fps_den = (int)json_section["fps_den"].toDouble();
        int section_num_frames = (int)json_section["num_frames"].toDouble();
        Section section(section_start, section_fps_num, section_fps_den, section_num_frames);
        json_presets = json_section["presets"].toArray();
        section.presets.resize(json_presets.size());
        for (int k = 0; k < json_presets.size(); k++)
            section.presets[k] = json_presets[k].toString().toStdString();

        addSection(section);
    }

    if (json_sections.size() == 0) {
        addSection(0);
    }

    json_custom_lists = json_project["custom lists"].toArray();

    custom_lists.reserve(json_custom_lists.size());

    for (int i = 0; i < json_custom_lists.size(); i++) {
        QJsonObject json_list = json_custom_lists[i].toObject();

        CustomList list(json_list["name"].toString().toStdString(),
                json_list["preset"].toString().toStdString(),
                (int)json_list["position"].toDouble());

        QJsonArray json_frames = json_list["frames"].toArray();
        for (int j = 0; j < json_frames.size(); j++) {
            QJsonArray json_range = json_frames[j].toArray();
            list.addFrameRange((int)json_range[0].toDouble(), (int)json_range[1].toDouble());
        }

        addCustomList(list);
    }


    QJsonObject json_resize, json_crop;

    json_resize = json_project["resize"].toObject();
    resize.enabled = !json_resize.isEmpty();
    resize.width = (int)json_resize["width"].toDouble(width);
    resize.height = (int)json_resize["height"].toDouble(height);

    json_crop = json_project["crop"].toObject();
    crop.enabled = !json_crop.isEmpty();
    crop.left = (int)json_crop["left"].toDouble();
    crop.top = (int)json_crop["top"].toDouble();
    crop.right = (int)json_crop["right"].toDouble();
    crop.bottom = (int)json_crop["bottom"].toDouble();
}

void WobblyProject::addFreezeFrame(int first, int last, int replacement) {
    if (first > last)
        std::swap(first, last);

    if (first < 0 || first >= num_frames[PostSource] ||
        last < 0 || last >= num_frames[PostSource] ||
        replacement < 0 || replacement >= num_frames[PostSource])
        throw WobblyException("Can't add FreezeFrame (" + std::to_string(first) + "," + std::to_string(last) + "," + std::to_string(replacement) + "): values out of range.");

    const FreezeFrame *overlap = findFreezeFrame(first);
    if (!overlap)
        overlap = findFreezeFrame(last);
    if (!overlap) {
        auto it = frozen_frames.upper_bound(first);
        if (it != frozen_frames.cend() && it->second.first < last)
            overlap = &it->second;
    }

    if (overlap)
        throw WobblyException("Can't add FreezeFrame (" + std::to_string(first) + "," + std::to_string(last) + "," + std::to_string(replacement) + "): overlaps (" + std::to_string(overlap->first) + "," + std::to_string(overlap->last) + "," + std::to_string(overlap->replacement) + ").");

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

const FreezeFrame *WobblyProject::findFreezeFrame(int frame) {
    if (!frozen_frames.size())
        return nullptr;

    auto it = frozen_frames.upper_bound(frame);

    it--;

    if (it->second.first <= frame && frame <= it->second.last)
        return &it->second;

    return nullptr;
}


void WobblyProject::addPreset(const std::string &preset_name) {
    std::string contents =
            "# The preset is a Python function. It takes a single parameter, called 'clip'.\n"
            "# Filter that and assign the result to the same variable.\n"
            "# The VapourSynth core object is called 'c'.\n";
    addPreset(preset_name, contents);
}


bool WobblyProject::isNameSafeForPython(const std::string &name) {
    for (size_t i = 0; i < name.size(); i++) {
        const char &c = name[i];

        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (i && c >= '0' && c <= '9') || c == '_'))
            return false;
    }

    return true;
}

void WobblyProject::addPreset(const std::string &preset_name, const std::string &preset_contents) {
    if (!isNameSafeForPython(preset_name))
        throw WobblyException("Can't add preset '" + preset_name + "': name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    Preset preset;
    preset.name = preset_name;
    preset.contents = preset_contents;
    presets.insert(std::make_pair(preset_name, preset));
}

void WobblyProject::renamePreset(const std::string &old_name, const std::string &new_name) {
    if (!presets.count(old_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': no such preset.");

    if (!isNameSafeForPython(new_name))
        throw WobblyException("Can't rename preset '" + old_name + "' to '" + new_name + "': new name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    Preset preset;
    preset.name = new_name;
    preset.contents = presets.at(old_name).contents;

    presets.erase(old_name);
    presets.insert(std::make_pair(new_name, preset));

    for (auto it = sections.begin(); it != sections.end(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == old_name)
                it->second.presets[j] = new_name;

    for (auto it = custom_lists.begin(); it != custom_lists.end(); it++)
        if (it->preset == old_name)
            it->preset = new_name;
}

void WobblyProject::deletePreset(const std::string &preset_name) {
    if (presets.erase(preset_name) == 0)
        throw WobblyException("Can't delete preset '" + preset_name + "': no such preset.");

    for (auto it = sections.begin(); it != sections.end(); it++)
        for (size_t j = 0; j < it->second.presets.size(); j++)
            if (it->second.presets[j] == preset_name)
                it->second.presets.erase(it->second.presets.cbegin() + j);

    for (auto it = custom_lists.begin(); it != custom_lists.end(); it++)
        if (it->preset == preset_name)
            it->preset.clear();
}

const std::string &WobblyProject::getPresetContents(const std::string &preset_name) {
    if (!presets.count(preset_name))
        throw WobblyException("Can't retrieve the contents of preset '" + preset_name + "': no such preset.");

    const Preset &preset = presets.at(preset_name);
    return preset.contents;
}

void WobblyProject::setPresetContents(const std::string &preset_name, const std::string &preset_contents) {
    if (!presets.count(preset_name))
        throw WobblyException("Can't modify the contents of preset '" + preset_name + "': no such preset.");

    Preset &preset = presets.at(preset_name);
    preset.contents = preset_contents;
}

void WobblyProject::assignPresetToSection(const std::string &preset_name, int section_start) {
    // The user may want to assign the same preset twice.
    sections.at(section_start).presets.push_back(preset_name);
}


void WobblyProject::setMatch(int frame, char match) {
    matches[frame] = match;
}


void WobblyProject::addSection(int section_start) {
    Section section(section_start);
    addSection(section);
}

void WobblyProject::addSection(const Section &section) {
    if (section.start < 0 || section.start >= num_frames[PostSource])
        throw WobblyException("Can't add section starting at " + std::to_string(section.start) + ": value out of range.");

    sections.insert(std::make_pair(section.start, section));
}

void WobblyProject::deleteSection(int section_start) {
    // Never delete the very first section.
    if (section_start > 0)
        sections.erase(section_start);
}

const Section *WobblyProject::findSection(int frame) {
    auto it = sections.upper_bound(frame);
    it--;
    return &it->second;
}

const Section *WobblyProject::findNextSection(int frame) {
    auto it = sections.upper_bound(frame);

    if (it != sections.cend())
        return &it->second;

    return nullptr;
}

int WobblyProject::getSectionEnd(int frame) {
    const Section *next_section = findNextSection(frame);
    if (next_section)
        return next_section->start;
    else
        return num_frames[PostSource];
}

void WobblyProject::setSectionMatchesFromPattern(int section_start, const std::string &pattern) {
    int section_end = getSectionEnd(section_start);

    for (int i = 0; i < section_end - section_start; i++) {
        if ((section_start + i == 0 && (pattern[i % 5] == 'p' || pattern[i % 5] == 'b')) ||
            (section_start + i == num_frames[PostSource] - 1 && (pattern[i % 5] == 'n' || pattern[i % 5] == 'u')))
            // Skip the first and last frame if their new matches are incompatible.
            continue;

        // Yatta does it like this.
        matches[section_start + i] = pattern[i % 5];
    }
}

void WobblyProject::setSectionDecimationFromPattern(int section_start, const std::string &pattern) {
    int section_end = getSectionEnd(section_start);

    for (int i = 0; i < section_end - section_start; i++) {
        // Yatta does it like this.
        if (pattern[i % 5] == 'd')
            addDecimatedFrame(section_start + i);
        else
            deleteDecimatedFrame(section_start + i);
    }
}


void WobblyProject::resetRangeMatches(int start, int end) {
    if (start > end)
        std::swap(start, end);

    if (start < 0 || end >= num_frames[PostSource])
        throw WobblyException("Can't reset the matches for range [" + std::to_string(start) + "," + std::to_string(end) + "]: values out of range.");

    memcpy(matches.data() + start, original_matches.data() + start, end - start + 1);
}


void WobblyProject::resetSectionMatches(int section_start) {
    int section_end = getSectionEnd(section_start);

    resetRangeMatches(section_start, section_end - 1);
}


void WobblyProject::addCustomList(const std::string &list_name) {
    CustomList list(list_name);
    addCustomList(list);
}

void WobblyProject::addCustomList(const CustomList &list) {
    if (list.position < 0 || list.position >= 3)
        throw WobblyException("Can't add custom list '" + list.name + "' with position " + std::to_string(list.position) + ": position out of range.");

    if (!isNameSafeForPython(list.name))
        throw WobblyException("Can't add custom list '" + list.name + "': name is invalid. Use only letters, numbers, and the underscore character. The first character cannot be a number.");

    if (list.preset.size() && presets.count(list.preset) == 0)
        throw WobblyException("Can't add custom list '" + list.name + "' with preset '" + list.preset + "': no such preset.");

    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == list.name)
            throw WobblyException("Can't add custom list '" + list.name + "': a list with this name already exists.");

    custom_lists.push_back(list);
}

void WobblyProject::deleteCustomList(const std::string &list_name) {
    for (size_t i = 0; i < custom_lists.size(); i++)
        if (custom_lists[i].name == list_name) {
            deleteCustomList(i);
            return;
        }

    throw WobblyException("Can't delete custom list with name '" + list_name + "': no such list.");
}

void WobblyProject::deleteCustomList(int list_index) {
    if (list_index < 0 || list_index >= (int)custom_lists.size())
        throw WobblyException("Can't delete custom list with index " + std::to_string(list_index) + ": index out of range.");

    custom_lists.erase(custom_lists.cbegin() + list_index);
}


void WobblyProject::addDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " for decimation: value out of range.");

    auto result = decimated_frames[frame / 5].insert(frame % 5);

    if (result.second)
        num_frames[PostDecimate]--;
}


void WobblyProject::deleteDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't delete decimated frame " + std::to_string(frame) + ": value out of range.");

    size_t result = decimated_frames[frame / 5].erase(frame % 5);

    if (result)
        num_frames[PostDecimate]++;
}


bool WobblyProject::isDecimatedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't check if frame " + std::to_string(frame) + " is decimated: value out of range.");

    return (bool)decimated_frames[frame / 5].count(frame % 5);
}


void WobblyProject::clearDecimatedFramesFromCycle(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't clear decimated frames from cycle containing frame " + std::to_string(frame) + ": value out of range.");

    int cycle = frame / 5;

    size_t new_frames = decimated_frames[cycle].size();

    decimated_frames[cycle].clear();

    num_frames[PostDecimate] += new_frames;
}


void WobblyProject::addCombedFrame(int frame) {
    if (frame < 0 || frame >= num_frames[PostSource])
        throw WobblyException("Can't mark frame " + std::to_string(frame) + " as combed: value out of range.");

    combed_frames.insert(frame);
}


void WobblyProject::deleteCombedFrame(int frame) {
    combed_frames.erase(frame);
}


bool WobblyProject::isCombedFrame(int frame) {
    return (bool)combed_frames.count(frame);
}


void WobblyProject::setResize(int new_width, int new_height) {
    if (new_width <= 0 || new_height <= 0)
        throw WobblyException("Can't resize to " + std::to_string(new_width) + "x" + std::to_string(new_height) + ": dimensions must be positive.");

    resize.width = new_width;
    resize.height = new_height;
}


void WobblyProject::setResizeEnabled(bool enabled) {
    resize.enabled = enabled;
}


bool WobblyProject::isResizeEnabled() {
    return resize.enabled;
}


void WobblyProject::setCrop(int left, int top, int right, int bottom) {
    if (left < 0 || top < 0 || right < 0 || bottom < 0)
        throw WobblyException("Can't crop (" + std::to_string(left) + "," + std::to_string(top) + "," + std::to_string(right) + "," + std::to_string(bottom) + "): negative values.");

    crop.left = left;
    crop.top = top;
    crop.right = right;
    crop.bottom = bottom;
}


void WobblyProject::setCropEnabled(bool enabled) {
    crop.enabled = enabled;
}


bool WobblyProject::isCropEnabled() {
    return crop.enabled;
}


std::string WobblyProject::frameToTime(int frame) {
    int milliseconds = (int)((frame * fps_den * 1000 / fps_num) % 1000);
    int seconds_total = (int)(frame * fps_den / fps_num);
    int seconds = seconds_total % 60;
    int minutes = (seconds_total / 60) % 60;
    int hours = seconds_total / 3600;

    char time[16];
#ifdef _MSC_VER
    _snprintf
#else
    snprintf
#endif
            (time, sizeof(time), "%02d:%02d:%02d.%03d", hours, minutes, seconds, milliseconds);
    time[15] = '\0';

    return std::string(time);
}


int WobblyProject::frameNumberAfterDecimation(int frame) {
    if (frame < 0)
        return 0;

    if (frame >= num_frames[PostSource])
        return num_frames[PostDecimate] - 1;

    int cycle_number = frame / 5;

    int position_in_cycle = frame % 5;

    int out_frame = cycle_number * 5;

    for (int i = 0; i < cycle_number; i++)
        out_frame -= decimated_frames[i].size();

    for (int8_t i = 0; i < position_in_cycle; i++)
        if (!decimated_frames[cycle_number].count(i))
            out_frame++;

    return out_frame;
}


void WobblyProject::guessSectionPatternsFromMatches(int section_start, int use_third_n_match, int drop_duplicate) {
    int section_end = getSectionEnd(section_start);

    // Count the "nc" pairs in each position.
    int positions[5] = { 0 };
    int total = 0;

    for (int i = section_start; i < std::min(section_end, num_frames[PostSource] - 1); i++) {
        if (original_matches[i] == 'n' && original_matches[i + 1] == 'c') {
            positions[i % 5]++;
            total++;
        }
    }

    // Find the two positions with the most "nc" pairs.
    int best = 0;
    int next_best = 0;
    int tmp = -1;

    for (int i = 0; i < 5; i++)
        if (positions[i] > tmp) {
            tmp = positions[i];
            best = i;
        }

    tmp = -1;

    for (int i = 0; i < 5; i++) {
        if (i == best)
            continue;

        if (positions[i] > tmp) {
            tmp = positions[i];
            next_best = i;
        }
    }

    float best_percent = 0.0f;
    float next_best_percent = 0.0f;

    if (total > 0) {
        best_percent = positions[best] * 100 / (float)total;
        next_best_percent = positions[next_best] * 100 / (float)total;
    }

    // Totally arbitrary thresholds.
    if (best_percent > 40.0f && best_percent - next_best_percent > 10.0f) {
        // Take care of decimation first.

        // If the first duplicate is the last frame in the cycle, we have to drop the same duplicate in the entire section.
        if (drop_duplicate == DropUglierDuplicatePerCycle && best == 4)
            drop_duplicate = DropUglierDuplicatePerSection;

        int drop = -1;

        if (drop_duplicate == DropUglierDuplicatePerSection) {
            // Find the uglier duplicate.
            int drop_n = 0;
            int drop_c = 0;

            for (int i = section_start; i < std::min(section_end, num_frames[PostSource] - 1); i++) {
                if (i % 5 == best) {
                    int16_t mic_n = mics[i][2];
                    int16_t mic_c = mics[i + 1][1];
                    if (mic_n > mic_c)
                        drop_n++;
                    else
                        drop_c++;
                }
            }

            if (drop_n > drop_c)
                drop = best;
            else
                drop = (best + 1) % 5;
        } else if (drop_duplicate == DropFirstDuplicate) {
            drop = best;
        } else if (drop_duplicate == DropSecondDuplicate) {
            drop = (best + 1) % 5;
        }

        int first_cycle = section_start / 5;
        int last_cycle = (section_end - 1) / 5;
        for (int i = first_cycle; i < last_cycle + 1; i++) {
            if (drop_duplicate == DropUglierDuplicatePerCycle) {
                if (i == first_cycle) {
                    if (section_start % 5 > best + 1)
                        continue;
                    else if (section_start % 5 > best)
                        drop = best + 1;
                } else if (i == last_cycle) {
                    if ((section_end - 1) % 5 < best)
                        continue;
                    else if ((section_end - 1) % 5 < best + 1)
                        drop = best;
                }

                if (drop == -1) {
                    int16_t mic_n = mics[i * 5 + best][2];
                    int16_t mic_c = mics[i * 5 + best + 1][1];
                    if (mic_n > mic_c)
                        drop = best;
                    else
                        drop = (best + 1) % 5;
                }
            }

            // At this point we know what frame to drop in this cycle.

            if (i == first_cycle) {
                // See if the cycle has a decimated frame from the previous section.

                /*
                bool conflicting_patterns = false;

                for (int j = i * 5; j < section_start; j++)
                    if (isDecimatedFrame(j)) {
                        conflicting_patterns = true;
                        break;
                    }

                if (conflicting_patterns) {
                    // If 18 fps cycles are not wanted, try to decimate from the side with more motion.
                }
                */

                // Clear decimated frames in the cycle, but only from this section.
                for (int j = section_start; j < (i + 1) * 5; j++)
                    if (isDecimatedFrame(j))
                        deleteDecimatedFrame(j);
            } else if (i == last_cycle) {
                // See if the cycle has a decimated frame from the next section.

                // Clear decimated frames in the cycle, but only from this section.
                for (int j = i * 5; j < section_end; j++)
                    if (isDecimatedFrame(j))
                        deleteDecimatedFrame(j);
            } else {
                clearDecimatedFramesFromCycle(i * 5);
            }

            addDecimatedFrame(i * 5 + drop);
        }


        // Now the matches.
        std::string patterns[5] = { "ncccn", "nnccc", "cnncc", "ccnnc", "cccnn" };
        if (use_third_n_match == UseThirdNMatchAlways)
            for (int i = 0; i < 5; i++)
                patterns[i][(i + 3) % 5] = 'n';

        const std::string &pattern = patterns[best];

        for (int i = section_start; i < section_end; i++) {
            if (use_third_n_match == UseThirdNMatchIfPrettier && pattern[i % 5] == 'c' && pattern[(i + 1) % 5] == 'n') {
                int16_t mic_n = mics[i][2];
                int16_t mic_c = mics[i][1];
                if (mic_n < mic_c)
                    matches[i] = 'n';
                else
                    matches[i] = 'c';
            } else {
                matches[i] = pattern[i % 5];
            }
        }

        // If the last frame of the section has much higher mic with c/n matches than with p match, use the p match.
        char match_index = matchCharToIndex(matches[section_end - 1]);
        int16_t mic_cn = mics[section_end - 1][match_index];
        int16_t mic_p = mics[section_end - 1][0];
        if (mic_cn > mic_p * 2)
            matches[section_end - 1] = 'p';
    }
}


void WobblyProject::guessProjectPatternsFromMatches(int minimum_length, int use_third_n_match, int drop_duplicate) {
    for (auto it = sections.cbegin(); it != sections.cend(); it++) {
        int length = getSectionEnd(it->second.start) - it->second.start;

        if (length < minimum_length)
            // XXX Record the sections skipped due to their length.
            continue;

        // XXX Record the sections where the matches didn't reveal a pattern.
        guessSectionPatternsFromMatches(it->second.start, use_third_n_match, drop_duplicate);
    }
}


void WobblyProject::sectionsToScript(std::string &script) {
    // XXX Make a temporary copy of the sections map and merge sections with identical presets, to generate as few trims as possible.
    std::string splice = "src = c.std.Splice(mismatch=True, clips=[";
    for (auto it = sections.cbegin(); it != sections.cend(); it++) {
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
        if (it_next != sections.cend())
            script += std::to_string(it_next->second.start);
        script += "]\n";

        splice += section_name + ",";
    }
    splice +=
            "])\n"
            "\n";

    script += splice;
}

void WobblyProject::customListsToScript(std::string &script, PositionInFilterChain position) {
    for (size_t i = 0; i < custom_lists.size(); i++) {
        // Ignore lists that are in a different position in the filter chain.
        if (custom_lists[i].position != position)
            continue;

        // Ignore lists with no frame ranges.
        if (!custom_lists[i].frames.size())
            continue;

        // Complain if the custom list doesn't have a preset assigned.
        if (!custom_lists[i].preset.size())
            throw WobblyException("Custom list '" + custom_lists[i].name + "' has no preset assigned.");

        std::string list_name = "cl_";
        list_name += custom_lists[i].name;

        script += list_name + " = " + custom_lists[i].preset + "(src)\n";

        std::string splice = "src = c.std.Splice(mismatch=True, clips=[";

        auto it = custom_lists[i].frames.cbegin();
        auto it_prev = it;

        if (it->second.first > 0) {
            splice += "src[0:";
            splice += std::to_string(it->second.first) + "],";
        }
        splice += list_name + "[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";

        it++;
        for ( ; it != custom_lists[i].frames.cend(); it++, it_prev++) {
            if (it->second.first - it_prev->second.last > 1) {
                splice += "src[";
                splice += std::to_string(it_prev->second.last + 1) + ":" + std::to_string(it->second.first) + "],";
            }

            splice += list_name + "[" + std::to_string(it->second.first) + ":" + std::to_string(it->second.last + 1) + "],";
        }

        // it_prev is cend()-1 at the end of the loop.

        if (it_prev->second.last < num_frames[PostSource] - 1) {
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
    for (auto it = presets.cbegin(); it != presets.cend(); it++) {
        script += "def " + it->second.name + "(clip):\n";
        for (size_t start = 0, end = 0; end != std::string::npos; ) {
            end = it->second.name.find('\n', start);
            script += "    " + it->second.name.substr(start, end) + "\n";
            start = end + 1;
        }
        script += "    return clip\n";
        script += "\n\n";
    }
}

void WobblyProject::sourceToScript(std::string &script) {
    script +=
            "try:\n"
            "    src = vs.get_output(index=1)\n"
            "except KeyError:\n"
            "    src = c.d2v.Source(input=r'" + input_file + "')\n"
            "    src.set_output(index=1)\n"
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
        for (auto it = decimated_frames[i].cbegin(); it != decimated_frames[i].cend(); it++)
            script += std::to_string(i * 5 + *it) + ",";

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

void WobblyProject::showCropToScript(std::string &script) {
    script += "src = c.std.AddBorders(clip=src, left=";
    script += std::to_string(crop.left) + ", top=";
    script += std::to_string(crop.top) + ", right=";
    script += std::to_string(crop.right) + ", bottom=";
    script += std::to_string(crop.bottom) + ", color=[128, 230, 180])\n\n";
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

std::string WobblyProject::generateFinalScript(bool for_preview) {
    // XXX Insert comments before and after each part.
    std::string script;

    headerToScript(script);

    presetsToScript(script);

    sourceToScript(script);

    trimToScript(script);

    customListsToScript(script, PostSource);

    fieldHintToScript(script);

    // XXX Put them and FreezeFrames in the same order as Yatta does.
    sectionsToScript(script);

    customListsToScript(script, PostFieldMatch);

    if (frozen_frames.size())
        freezeFramesToScript(script);

    bool decimation_needed = false;
    for (size_t i = 0; i < decimated_frames.size(); i++)
        if (decimated_frames[i].size()) {
            decimation_needed = true;
            break;
        }
    if (decimation_needed)
        decimatedFramesToScript(script);

    // XXX DeleteFrames doesn't change the frame rate or the frame durations. This must be done separately.

    customListsToScript(script, PostDecimate);

    if (crop.enabled)
        cropToScript(script);

    if (resize.enabled)
        resizeToScript(script);

    // Maybe this doesn't belong here after all.
    if (for_preview)
        rgbConversionToScript(script);

    setOutputToScript(script);

    return script;
}

std::string WobblyProject::generateMainDisplayScript(bool show_crop) {
    // I guess use text.Text to print matches, frame number, metrics, etc. Or just QLabels.

    std::string script;

    headerToScript(script);

    sourceToScript(script);

    trimToScript(script);

    fieldHintToScript(script);

    if (frozen_frames.size())
        freezeFramesToScript(script);

    if (show_crop && crop.enabled) {
        cropToScript(script);
        showCropToScript(script);
    }

    rgbConversionToScript(script);

    setOutputToScript(script);

    return script;
}
