#include "profile/ProfileManager.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <random>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string generateUuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char chars[] = "0123456789abcdef";
    std::string result;
    for (int i = 0; i < 16; ++i) {
        result += chars[dis(gen)];
    }
    return result;
}

std::string ProfileManager::generateFolderId() {
    return "f_" + generateUuid();
}

ProfileManager::ProfileManager() {
    const char* home = std::getenv("HOME");
    m_profileDir = fs::path(home ? home : "/tmp") / ".config" / "vml" / "profiles";
    fs::create_directories(m_profileDir);
}

std::vector<Profile> ProfileManager::listProfiles() const {
    std::vector<Profile> profiles;
    if (!fs::exists(m_profileDir)) return profiles;

    for (const auto& entry : fs::directory_iterator(m_profileDir)) {
        if (entry.path().extension() == ".json") {
            try {
                profiles.push_back(loadProfile(entry.path().filename().string()));
            } catch (const std::exception& e) {
                std::cerr << "Warning: failed to load profile '" << entry.path().filename().string()
                          << "': " << e.what() << '\n';
            }
        }
    }
    // Sort: built-in first, then alphabetical within each group
    std::sort(profiles.begin(), profiles.end(),
        [](const Profile& a, const Profile& b) {
            if (a.builtin != b.builtin) return a.builtin > b.builtin;
            return a.name < b.name;
        });
    return profiles;
}

std::vector<Folder> ProfileManager::listFolders() const {
    fs::path foldersFile = m_profileDir / "folders.json";
    std::vector<Folder> folders;
    if (!fs::exists(foldersFile)) return folders;

    try {
        std::ifstream f(foldersFile);
        json data = json::parse(f);
        if (data.is_array()) {
            for (auto& item : data) {
                Folder folder;
                folder.id = item.value("id", "");
                folder.name = item.value("name", "Untitled");
                folder.parentId = item.value("parentId", "");
                folder.order = item.value("order", 0);
                folders.push_back(folder);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: failed to load folders: " << e.what() << '\n';
    }
    return folders;
}

void ProfileManager::saveFolder(const Folder& folder) {
    fs::path foldersFile = m_profileDir / "folders.json";
    json data;
    if (fs::exists(foldersFile)) {
        try {
            std::ifstream f(foldersFile);
            data = json::parse(f);
        } catch (const std::exception& e) {
            std::cerr << "Warning: failed to parse folders.json: " << e.what() << '\n';
            data = json::array();
        }
    } else {
        data = json::array();
    }

    bool found = false;
    for (auto& item : data) {
        if (item.value("id", "") == folder.id) {
            item["name"] = folder.name;
            item["parentId"] = folder.parentId;
            item["order"] = folder.order;
            found = true;
            break;
        }
    }
    if (!found) {
        json item;
        item["id"] = folder.id;
        item["name"] = folder.name;
        item["parentId"] = folder.parentId;
        item["order"] = folder.order;
        data.push_back(item);
    }

    std::ofstream f(foldersFile);
    f << data.dump(2);
}

void ProfileManager::deleteFolder(const std::string& folderId) {
    // Move all profiles in this folder to root
    auto profiles = listProfiles();
    for (auto& p : profiles) {
        if (p.folderId == folderId) {
            setProfileFolder(p.filename, "");
        }
    }

    // Move child folders to root
    auto folders = listFolders();
    for (auto& f : folders) {
        if (f.parentId == folderId) {
            Folder updated = f;
            updated.parentId = "";
            saveFolder(updated);
        }
    }

    // Remove the folder from folders.json
    fs::path foldersFile = m_profileDir / "folders.json";
    if (fs::exists(foldersFile)) {
        try {
            std::ifstream f(foldersFile);
            json data = json::parse(f);
            json newData = json::array();
            for (auto& item : data) {
                if (item.value("id", "") != folderId) {
                    newData.push_back(item);
                }
            }
            std::ofstream out(foldersFile);
            out << newData.dump(2);
        } catch (const std::exception& e) {
            std::cerr << "Warning: failed to write folders.json: " << e.what() << '\n';
        }
    }
}

void ProfileManager::setProfileFolder(const std::string& filename, const std::string& folderId) {
    auto profile = loadProfile(filename);
    profile.data["folderId"] = folderId;
    saveProfile(profile);
}

void ProfileManager::moveProfileToFolder(const std::string& filename, const std::string& folderId) {
    setProfileFolder(filename, folderId);
}

void ProfileManager::exportFolder(const std::string& folderId, const fs::path& destPath) const {
    fs::create_directories(destPath);

    // Export the folder manifest
    auto folders = listFolders();
    std::string folderName = "Exported Folder";
    for (const auto& f : folders) {
        if (f.id == folderId) {
            folderName = f.name;
            break;
        }
    }

    json manifest;
    manifest["type"] = "vml-folder";
    manifest["name"] = folderName;
    manifest["version"] = 1;
    manifest["profiles"] = json::array();

    auto profiles = listProfiles();
    for (const auto& p : profiles) {
        if (p.folderId == folderId) {
            json pf = p.data;
            pf.erase("builtin");
            pf.erase("folderId");
            manifest["profiles"].push_back(pf);
        }
    }

    // Save manifest
    std::ofstream mf(destPath / "manifest.json");
    mf << manifest.dump(2);

    // Save each profile
    for (const auto& p : profiles) {
        if (p.folderId == folderId) {
            json pf = p.data;
            pf.erase("builtin");
            pf.erase("folderId");
            std::string pname = p.name;
            std::transform(pname.begin(), pname.end(), pname.begin(), [](unsigned char c) { return std::tolower(c); });
            std::replace(pname.begin(), pname.end(), ' ', '-');
            std::ofstream pfFile(destPath / (pname + ".json"));
            pfFile << pf.dump(2);
        }
    }
}

std::string ProfileManager::importFolder(const fs::path& srcPath, const std::string& parentFolderId) {
    fs::path manifestPath = srcPath / "manifest.json";
    if (!fs::exists(manifestPath)) {
        throw std::runtime_error("No manifest.json found in folder");
    }

    std::ifstream mf(manifestPath);
    json manifest = json::parse(mf);

    std::string folderName = manifest.value("name", "Imported Folder");
    std::string folderId = generateFolderId();

    Folder folder;
    folder.id = folderId;
    folder.name = folderName;
    folder.parentId = parentFolderId;
    saveFolder(folder);

    if (manifest.contains("profiles")) {
        for (auto& pf : manifest["profiles"]) {
            std::string name = pf.value("name", "Imported Voice");
            pf.erase("builtin");
            pf["folderId"] = folderId;

            std::string baseFilename = name;
            std::transform(baseFilename.begin(), baseFilename.end(), baseFilename.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::replace(baseFilename.begin(), baseFilename.end(), ' ', '-');
            std::string filename = baseFilename + ".json";
            int counter = 1;
            while (fs::exists(m_profileDir / filename)) {
                filename = baseFilename + "-" + std::to_string(counter++) + ".json";
            }

            pf["name"] = name;
            saveProfile({name, filename, pf, false});
        }
    }

    return folderId;
}

Profile ProfileManager::loadProfile(const std::string& filename) const {
    fs::path path = m_profileDir / filename;
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open profile: " + path.string());

    Profile p;
    p.filename = filename;
    p.data = json::parse(f);
    p.name = p.data.value("name", filename);
    p.builtin = p.data.value("builtin", false);
    p.folderId = p.data.value("folderId", "");
    return p;
}

void ProfileManager::saveProfile(const Profile& profile) const {
    fs::path path = m_profileDir / profile.filename;
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot save profile: " + path.string());
    f << profile.data.dump(2);
}

void ProfileManager::deleteProfile(const std::string& filename) const {
    fs::remove(m_profileDir / filename);
}

void ProfileManager::exportProfile(const std::string& filename, const fs::path& destPath) const {
    auto profile = loadProfile(filename);
    // Strip builtin flag for export
    profile.data.erase("builtin");
    std::ofstream f(destPath);
    if (!f.is_open())
        throw std::runtime_error("Cannot write to: " + destPath.string());
    f << profile.data.dump(2);
}

std::string ProfileManager::importProfile(const fs::path& srcPath) const {
    std::ifstream f(srcPath);
    if (!f.is_open())
        throw std::runtime_error("Cannot read: " + srcPath.string());

    json data = json::parse(f);
    std::string name = data.value("name", srcPath.stem().string());
    data.erase("builtin"); // Never import as built-in

    // Generate a unique filename
    std::string baseFilename = name;
    std::transform(baseFilename.begin(), baseFilename.end(), baseFilename.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    std::replace(baseFilename.begin(), baseFilename.end(), ' ', '-');
    std::string filename = baseFilename + ".json";
    int counter = 1;
    while (fs::exists(m_profileDir / filename)) {
        filename = baseFilename + "-" + std::to_string(counter++) + ".json";
    }

    data["name"] = name;
    saveProfile({name, filename, data, false});
    return filename;
}

void ProfileManager::installIfMissing(const Profile& profile) const {
    fs::path path = m_profileDir / profile.filename;
    if (!fs::exists(path))
        saveProfile(profile);
}

json ProfileManager::pipelineToJson(const std::string& name,
    const std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>>& effects) {
    json j;
    j["name"] = name;
    j["version"] = 1;
    j["effects"] = json::array();

    for (auto& [effectId, params] : effects) {
        json ej;
        ej["id"] = effectId;
        ej["enabled"] = true;
        ej["params"] = json::object();
        for (auto& [pid, val] : params)
            ej["params"][pid] = val;
        j["effects"].push_back(ej);
    }
    return j;
}

std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>>
ProfileManager::jsonToPipeline(const json& data) {
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>> result;

    if (!data.contains("effects")) return result;

    for (auto& ej : data["effects"]) {
        std::string id = ej.value("id", "");
        std::vector<std::pair<std::string, float>> params;
        if (ej.contains("params") && ej["params"].is_object()) {
            for (auto& [key, val] : ej["params"].items()) {
                if (val.is_number())
                    params.emplace_back(key, val.get<float>());
            }
        }
        result.emplace_back(id, params);
    }
    return result;
}

// Helper to create a built-in profile JSON with the builtin flag
static json makeBuiltin(json j) {
    j["builtin"] = true;
    return j;
}

void ProfileManager::installDefaultProfiles() const {
    // Deep Voice
    installIfMissing({"Deep Voice", "deep-voice.json", makeBuiltin(pipelineToJson("Deep Voice", {
        {"pitch_shift", {{"semitones", -4.f}}},
        {"gain", {{"gain_db", 3.f}}}
    })), true});

    // Robot
    installIfMissing({"Robot", "robot.json", makeBuiltin(pipelineToJson("Robot", {
        {"pitch_shift", {{"semitones", -2.f}}},
        {"echo", {{"delay_ms", 30.f}, {"feedback", 0.5f}, {"mix", 0.4f}}},
        {"reverb", {{"room_size", 0.3f}, {"damping", 0.8f}, {"mix", 0.2f}}}
    })), true});

    // Radio
    installIfMissing({"Radio", "radio.json", makeBuiltin(pipelineToJson("Radio", {
        {"noise_gate", {{"threshold_db", -35.f}}},
        {"gain", {{"gain_db", 6.f}}},
        {"reverb", {{"room_size", 0.2f}, {"damping", 0.9f}, {"mix", 0.1f}}}
    })), true});

    // Chipmunk
    installIfMissing({"Chipmunk", "chipmunk.json", makeBuiltin(pipelineToJson("Chipmunk", {
        {"pitch_shift", {{"semitones", 8.f}}},
        {"chorus", {{"voices", 3.f}, {"rate", 3.5f}, {"depth_ms", 4.f}, {"mix", 0.35f}}},
        {"compressor", {{"threshold", -15.f}, {"ratio", 6.f}, {"attack_ms", 5.f}, {"release_ms", 50.f}, {"makeup_db", 6.f}}},
        {"gain", {{"gain_db", 2.f}}}
    })), true});

    // Demon
    {
        auto j = makeBuiltin(pipelineToJson("Demon", {
            {"pitch_shift", {{"semitones", -7.f}}},
            {"distortion", {{"drive", 12.f}, {"tone", 0.3f}, {"mix", 0.6f}}},
            {"ring_modulator", {{"frequency", 55.f}, {"mix", 0.15f}}},
            {"reverb", {{"room_size", 0.85f}, {"damping", 0.3f}, {"mix", 0.4f}}},
            {"compressor", {{"threshold", -18.f}, {"ratio", 8.f}, {"attack_ms", 5.f}, {"release_ms", 80.f}, {"makeup_db", 8.f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "ring_modulator"}, {"param_id", "frequency"}, {"start_value", 40.f}, {"end_value", 80.f},
             {"duration", 4.0f}, {"curve", "ease_in_out"}, {"active", true}}
        });
        installIfMissing({"Demon", "demon.json", j, true});
    }

    // Walkie-Talkie
    installIfMissing({"Walkie-Talkie", "walkie-talkie.json", makeBuiltin(pipelineToJson("Walkie-Talkie", {
        {"noise_gate", {{"threshold_db", -30.f}, {"attack_ms", 1.f}, {"release_ms", 50.f}}},
        {"compressor", {{"threshold", -20.f}, {"ratio", 10.f}, {"attack_ms", 2.f}, {"release_ms", 40.f}, {"makeup_db", 10.f}}},
        {"telephone", {{"bandwidth", 0.2f}, {"noise", 0.25f}, {"distortion", 0.4f}}},
        {"gain", {{"gain_db", -2.f}}}
    })), true});

    // Ghost
    {
        auto j = makeBuiltin(pipelineToJson("Ghost", {
            {"pitch_shift", {{"semitones", 3.f}}},
            {"phaser", {{"stages", 8.f}, {"rate", 0.15f}, {"depth", 0.9f}, {"feedback", 0.75f}, {"mix", 0.6f}}},
            {"chorus", {{"voices", 4.f}, {"rate", 0.3f}, {"depth_ms", 12.f}, {"mix", 0.4f}}},
            {"reverb", {{"room_size", 0.95f}, {"damping", 0.2f}, {"mix", 0.65f}}},
            {"gain", {{"gain_db", -3.f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "pitch_shift"}, {"param_id", "semitones"}, {"start_value", 2.f}, {"end_value", 5.f},
             {"duration", 6.0f}, {"curve", "keyframe"}, {"active", true},
             {"keyframes", json::array({
                 {{"pos", 0.0f}, {"val", 0.3f}}, {{"pos", 0.2f}, {"val", 0.8f}},
                 {{"pos", 0.4f}, {"val", 0.1f}}, {{"pos", 0.6f}, {"val", 0.9f}},
                 {{"pos", 0.8f}, {"val", 0.4f}}, {{"pos", 1.0f}, {"val", 0.3f}}
             })}}
        });
        installIfMissing({"Ghost", "ghost.json", j, true});
    }

    // Underwater
    {
        auto j = makeBuiltin(pipelineToJson("Underwater", {
            {"telephone", {{"bandwidth", 0.1f}, {"noise", 0.0f}, {"distortion", 0.0f}}},
            {"flanger", {{"rate", 0.2f}, {"depth_ms", 6.f}, {"feedback", 0.7f}, {"mix", 0.7f}}},
            {"chorus", {{"voices", 4.f}, {"rate", 0.4f}, {"depth_ms", 15.f}, {"mix", 0.5f}}},
            {"reverb", {{"room_size", 0.7f}, {"damping", 0.6f}, {"mix", 0.35f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "flanger"}, {"param_id", "depth_ms"}, {"start_value", 2.f}, {"end_value", 9.f},
             {"duration", 5.0f}, {"curve", "ease_in_out"}, {"active", true}}
        });
        installIfMissing({"Underwater", "underwater.json", j, true});
    }

    // Megaphone
    installIfMissing({"Megaphone", "megaphone.json", makeBuiltin(pipelineToJson("Megaphone", {
        {"compressor", {{"threshold", -25.f}, {"ratio", 20.f}, {"attack_ms", 0.5f}, {"release_ms", 30.f}, {"makeup_db", 15.f}}},
        {"distortion", {{"drive", 8.f}, {"tone", 0.7f}, {"mix", 0.5f}}},
        {"telephone", {{"bandwidth", 0.35f}, {"noise", 0.05f}, {"distortion", 0.5f}}},
        {"gain", {{"gain_db", -4.f}}}
    })), true});

    // Alien
    {
        auto j = makeBuiltin(pipelineToJson("Alien", {
            {"formant_shifter", {{"shift", 7.f}, {"mix", 0.8f}}},
            {"ring_modulator", {{"frequency", 180.f}, {"mix", 0.3f}}},
            {"phaser", {{"stages", 10.f}, {"rate", 0.8f}, {"depth", 0.8f}, {"feedback", 0.6f}, {"mix", 0.5f}}},
            {"echo", {{"delay_ms", 80.f}, {"feedback", 0.3f}, {"mix", 0.25f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "ring_modulator"}, {"param_id", "frequency"}, {"start_value", 80.f}, {"end_value", 400.f},
             {"duration", 3.0f}, {"curve", "keyframe"}, {"active", true},
             {"keyframes", json::array({
                 {{"pos", 0.0f}, {"val", 0.2f}}, {{"pos", 0.15f}, {"val", 0.9f}},
                 {{"pos", 0.3f}, {"val", 0.1f}}, {{"pos", 0.5f}, {"val", 0.7f}},
                 {{"pos", 0.7f}, {"val", 0.3f}}, {{"pos", 0.85f}, {"val", 1.0f}},
                 {{"pos", 1.0f}, {"val", 0.2f}}
             })}}
        });
        installIfMissing({"Alien", "alien.json", j, true});
    }

    // Cave Troll
    {
        auto j = makeBuiltin(pipelineToJson("Cave Troll", {
            {"pitch_shift", {{"semitones", -10.f}}},
            {"bitcrush", {{"bit_depth", 8.f}, {"sample_rate_reduction", 3.f}, {"mix", 0.3f}}},
            {"distortion", {{"drive", 4.f}, {"tone", 0.2f}, {"mix", 0.35f}}},
            {"reverb", {{"room_size", 0.95f}, {"damping", 0.15f}, {"mix", 0.55f}}},
            {"compressor", {{"threshold", -15.f}, {"ratio", 6.f}, {"attack_ms", 10.f}, {"release_ms", 150.f}, {"makeup_db", 10.f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "reverb"}, {"param_id", "mix"}, {"start_value", 0.35f}, {"end_value", 0.75f},
             {"duration", 3.5f}, {"curve", "rubberband"}, {"active", true}}
        });
        installIfMissing({"Cave Troll", "cave-troll.json", j, true});
    }

    // Psychedelic
    {
        auto j = makeBuiltin(pipelineToJson("Psychedelic", {
            {"pitch_shift", {{"semitones", 1.f}}},
            {"chorus", {{"voices", 5.f}, {"rate", 1.2f}, {"depth_ms", 14.f}, {"mix", 0.5f}}},
            {"flanger", {{"rate", 0.4f}, {"depth_ms", 7.f}, {"feedback", 0.8f}, {"mix", 0.45f}}},
            {"phaser", {{"stages", 12.f}, {"rate", 0.5f}, {"depth", 1.0f}, {"feedback", 0.7f}, {"mix", 0.5f}}},
            {"echo", {{"delay_ms", 300.f}, {"feedback", 0.45f}, {"mix", 0.3f}}},
            {"reverb", {{"room_size", 0.6f}, {"damping", 0.4f}, {"mix", 0.3f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "pitch_shift"}, {"param_id", "semitones"}, {"start_value", -2.f}, {"end_value", 4.f},
             {"duration", 8.0f}, {"curve", "keyframe"}, {"active", true},
             {"keyframes", json::array({
                 {{"pos", 0.0f}, {"val", 0.5f}}, {{"pos", 0.25f}, {"val", 0.8f}},
                 {{"pos", 0.5f}, {"val", 0.2f}}, {{"pos", 0.75f}, {"val", 0.9f}},
                 {{"pos", 1.0f}, {"val", 0.5f}}
             })}},
            {{"effect_id", "chorus"}, {"param_id", "rate"}, {"start_value", 0.3f}, {"end_value", 4.0f},
             {"duration", 6.0f}, {"curve", "ease_in_out"}, {"active", true}}
        });
        installIfMissing({"Psychedelic", "psychedelic.json", j, true});
    }

    // Broken Android
    {
        auto j = makeBuiltin(pipelineToJson("Broken Android", {
            {"pitch_shift", {{"semitones", -1.f}}},
            {"bitcrush", {{"bit_depth", 6.f}, {"sample_rate_reduction", 8.f}, {"mix", 0.5f}}},
            {"ring_modulator", {{"frequency", 300.f}, {"mix", 0.2f}}},
            {"echo", {{"delay_ms", 50.f}, {"feedback", 0.6f}, {"mix", 0.35f}}},
            {"telephone", {{"bandwidth", 0.3f}, {"noise", 0.15f}, {"distortion", 0.2f}}},
            {"compressor", {{"threshold", -12.f}, {"ratio", 15.f}, {"attack_ms", 1.f}, {"release_ms", 30.f}, {"makeup_db", 5.f}}}
        }));
        j["version"] = 2;
        j["modulators"] = json::array({
            {{"effect_id", "bitcrush"}, {"param_id", "bit_depth"}, {"start_value", 3.f}, {"end_value", 12.f},
             {"duration", 2.0f}, {"curve", "keyframe"}, {"active", true},
             {"keyframes", json::array({
                 {{"pos", 0.0f}, {"val", 0.2f}}, {{"pos", 0.1f}, {"val", 0.9f}},
                 {{"pos", 0.2f}, {"val", 0.1f}}, {{"pos", 0.35f}, {"val", 0.7f}},
                 {{"pos", 0.5f}, {"val", 0.05f}}, {{"pos", 0.6f}, {"val", 0.95f}},
                 {{"pos", 0.75f}, {"val", 0.3f}}, {{"pos", 0.9f}, {"val", 0.8f}},
                 {{"pos", 1.0f}, {"val", 0.2f}}
             })}},
            {{"effect_id", "ring_modulator"}, {"param_id", "frequency"}, {"start_value", 100.f}, {"end_value", 800.f},
             {"duration", 1.5f}, {"curve", "rubberband"}, {"active", true}}
        });
        installIfMissing({"Broken Android", "broken-android.json", j, true});
    }
}

// =============================================================================
// Voice folder structure — persistence layer
// =============================================================================

static fs::path folderStructurePath(const fs::path& profileDir) {
    return profileDir / "folder-structure.json";
}

void ProfileManager::saveFolderStructure(const FolderStructure& structure) const {
    fs::path path = folderStructurePath(m_profileDir);
    std::ofstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot save folder structure: " + path.string());

    json j;
    j["version"] = structure.version;
    j["folders"] = json::array();
    for (const auto& folder : structure.folders) {
        json fj;
        fj["id"] = folder.id;
        fj["name"] = folder.name;
        fj["parentId"] = folder.parentId;
        fj["expanded"] = folder.expanded;
        fj["order"] = folder.order;
        j["folders"].push_back(std::move(fj));
    }
    j["voiceAssignments"] = json::object();
    for (const auto& [filename, folderId] : structure.voiceAssignments) {
        j["voiceAssignments"][filename] = folderId;
    }
    f << j.dump(2);
}

FolderStructure ProfileManager::loadFolderStructure() const {
    fs::path path = folderStructurePath(m_profileDir);

    if (!fs::exists(path)) {
        // First run: create default structure with Built-in folder
        FolderStructure structure;
        structure.version = 1;

        VoiceFolder builtin;
        builtin.id = "builtin";
        builtin.name = "Built-in";
        builtin.parentId = "";
        builtin.expanded = true;
        builtin.order = 0;
        structure.folders.push_back(builtin);

        // Assign all existing builtin voices to Built-in folder
        for (const auto& profile : listProfiles()) {
            if (profile.builtin) {
                structure.voiceAssignments[profile.filename] = "builtin";
            }
            // User voices stay at root (no entry in voiceAssignments)
        }

        saveFolderStructure(structure);
        return structure;
    }

    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open folder structure: " + path.string());

    json j = json::parse(f);

    FolderStructure structure;
    structure.version = j.value("version", 1);

    if (j.contains("folders") && j["folders"].is_array()) {
        for (const auto& fj : j["folders"]) {
            VoiceFolder folder;
            folder.id = fj.value("id", "");
            folder.name = fj.value("name", "Untitled");
            folder.parentId = fj.value("parentId", "");
            folder.expanded = fj.value("expanded", true);
            folder.order = fj.value("order", 0);
            structure.folders.push_back(folder);
        }
    }

    if (j.contains("voiceAssignments") && j["voiceAssignments"].is_object()) {
        for (const auto& [filename, folderId] : j["voiceAssignments"].items()) {
            structure.voiceAssignments[filename] = folderId.get<std::string>();
        }
    }

    return structure;
}

void ProfileManager::moveVoiceToFolder(const std::string& voiceFilename,
                                        const std::string& folderId) {
    if (folderId == "builtin")
        throw std::runtime_error("Cannot move voices out of the builtin folder");

    FolderStructure structure = loadFolderStructure();

    if (folderId.empty()) {
        // Move to root: remove assignment
        structure.voiceAssignments.erase(voiceFilename);
    } else {
        structure.voiceAssignments[voiceFilename] = folderId;
    }

    saveFolderStructure(structure);
}

std::vector<std::string> ProfileManager::getVoicesInFolder(
        const std::string& folderId) const {
    FolderStructure structure = loadFolderStructure();
    std::vector<std::string> result;

    for (const auto& [filename, fid] : structure.voiceAssignments) {
        if (fid == folderId) {
            result.push_back(filename);
        }
    }

    return result;
}

int ProfileManager::getFolderDepth(const std::string& folderId) const {
    FolderStructure structure = loadFolderStructure();

    if (folderId == "builtin")
        return 1;

    // Build a map from id -> parentId for quick lookup
    std::map<std::string, std::string> parentMap;
    for (const auto& f : structure.folders) {
        parentMap[f.id] = f.parentId;
    }

    int depth = 1;
    std::string current = folderId;
    while (!current.empty()) {
        auto it = parentMap.find(current);
        if (it == parentMap.end())
            break;
        ++depth;
        current = it->second;
    }

    return depth;
}
