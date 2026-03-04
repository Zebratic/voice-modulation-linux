#include "profile/ProfileManager.h"
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
using json = nlohmann::json;

ProfileManager::ProfileManager() {
    const char* home = std::getenv("HOME");
    m_profileDir = fs::path(home ? home : "/tmp") / ".config" / "vml" / "profiles";
    fs::create_directories(m_profileDir);
}

std::vector<Profile> ProfileManager::listProfiles() const {
    std::vector<Profile> profiles;
    if (!fs::exists(m_profileDir)) return profiles;

    for (auto& entry : fs::directory_iterator(m_profileDir)) {
        if (entry.path().extension() == ".json") {
            try {
                profiles.push_back(loadProfile(entry.path().filename().string()));
            } catch (...) {}
        }
    }
    std::sort(profiles.begin(), profiles.end(),
        [](const Profile& a, const Profile& b) { return a.name < b.name; });
    return profiles;
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

void ProfileManager::installDefaultProfiles() const {
    // Deep Voice
    auto deep = pipelineToJson("Deep Voice", {
        {"pitch_shift", {{"semitones", -4.f}}},
        {"gain", {{"gain_db", 3.f}}}
    });
    saveProfile({"Deep Voice", "deep-voice.json", deep});

    // Robot
    auto robot = pipelineToJson("Robot", {
        {"pitch_shift", {{"semitones", -2.f}}},
        {"echo", {{"delay_ms", 30.f}, {"feedback", 0.5f}, {"mix", 0.4f}}},
        {"reverb", {{"room_size", 0.3f}, {"damping", 0.8f}, {"mix", 0.2f}}}
    });
    saveProfile({"Robot", "robot.json", robot});

    // Radio
    auto radio = pipelineToJson("Radio", {
        {"noise_gate", {{"threshold_db", -35.f}}},
        {"gain", {{"gain_db", 6.f}}},
        {"reverb", {{"room_size", 0.2f}, {"damping", 0.9f}, {"mix", 0.1f}}}
    });
    saveProfile({"Radio", "radio.json", radio});
}
