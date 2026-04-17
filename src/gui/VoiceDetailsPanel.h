#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <string>
#include <map>
#include <nlohmann/json.hpp>

class ProfileManager;

class VoiceDetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit VoiceDetailsPanel(ProfileManager& profileManager, QWidget* parent = nullptr);
    ~VoiceDetailsPanel() override = default;

    void setVoice(const std::string& filename);
    void clear();

signals:
    void activateRequested(const std::string& filename);
    void editRequested(const std::string& filename);
    void exportRequested(const std::string& filename);
    void deleteRequested(const std::string& filename);
    void renameRequested(const std::string& filename);

private:
    void setupUI();
    void populateEffects(const nlohmann::json& profileData);

    ProfileManager& m_profileManager;
    std::string m_currentFilename;
    bool m_isBuiltin = false;

    QLabel* m_nameLabel = nullptr;
    QPushButton* m_renameBtn = nullptr;
    QPushButton* m_activateBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QWidget* m_effectsContainer = nullptr;
    QVBoxLayout* m_effectsLayout = nullptr;
};
