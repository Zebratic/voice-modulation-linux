#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <map>
#include <filesystem>

struct Profile {
    std::string name;
    std::string filename;
    nlohmann::json data;
    bool builtin = false;
    std::string folderId;  // empty = root level
};

struct Folder {
    std::string id;
    std::string name;
    std::string parentId;  // empty = root level
    int order = 0;
};

// Voice folder structure for voice profile organization
struct VoiceFolder {
    std::string id;           // UUID or "builtin" sentinel
    std::string name;         // Display name
    std::string parentId;     // empty for root-level folders
    bool expanded = true;
    int order = 0;
};

struct FolderStructure {
    int version = 1;
    std::vector<VoiceFolder> folders;
    std::map<std::string, std::string> voiceAssignments; // filename -> folderId
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

    // Install built-in profiles (only writes files that don't exist yet)
    void installDefaultProfiles() const;

    // Export a profile to an external path
    void exportProfile(const std::string& filename, const std::filesystem::path& destPath) const;

    // Import a profile from an external path. Returns the new filename.
    std::string importProfile(const std::filesystem::path& srcPath) const;

    // Folder management
    std::vector<Folder> listFolders() const;
    void saveFolder(const Folder& folder);
    void deleteFolder(const std::string& folderId);
    void moveProfileToFolder(const std::string& filename, const std::string& folderId);
    void exportFolder(const std::string& folderId, const std::filesystem::path& destPath) const;
    std::string importFolder(const std::filesystem::path& srcPath, const std::string& parentFolderId);

    // Update profile's folder
    void setProfileFolder(const std::string& filename, const std::string& folderId);

    // Generate a unique folder ID
    static std::string generateFolderId();

    // Voice folder structure management (voice organization layer)
    void saveFolderStructure(const FolderStructure& structure) const;
    FolderStructure loadFolderStructure() const;
    void moveVoiceToFolder(const std::string& voiceFilename, const std::string& folderId);
    std::vector<std::string> getVoicesInFolder(const std::string& folderId) const;
    int getFolderDepth(const std::string& folderId) const;

private:
    void installIfMissing(const Profile& profile) const;
    std::filesystem::path m_profileDir;
};
