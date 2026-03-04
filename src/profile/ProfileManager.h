#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <filesystem>

struct Profile {
    std::string name;
    std::string filename;
    nlohmann::json data;
};

class ProfileManager {
public:
    ProfileManager();

    std::vector<Profile> listProfiles() const;
    Profile loadProfile(const std::string& filename) const;
    void saveProfile(const Profile& profile) const;
    void deleteProfile(const std::string& filename) const;

    // Convert pipeline state to/from JSON
    static nlohmann::json pipelineToJson(const std::string& name,
        const std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>>& effects);
    static std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>>
        jsonToPipeline(const nlohmann::json& data);

    std::filesystem::path profileDir() const { return m_profileDir; }

    void installDefaultProfiles() const;

private:
    std::filesystem::path m_profileDir;
};
