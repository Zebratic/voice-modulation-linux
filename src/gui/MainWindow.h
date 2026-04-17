#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QTimer>
#include <QScrollArea>
#include <QGridLayout>
#include <QStackedWidget>
#include <QLabel>

class AudioEngine;
class ProfileManager;
class PipelineWidget;
class EffectSettingsPanel;
class WaveformWidget;
class SpectrumWidget;
class KeyframeTimelineWidget;
class SystemTray;
class FolderSidebar;
class VoiceDetailsPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AudioEngine& engine, ProfileManager& profileManager, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void setupUI();
    void setupToolbar();
    void connectSignals();

    QWidget* createVoicesTab();
    QWidget* createEditVoiceTab();
    QWidget* createConfigurationTab();
    QWidget* createSettingsTab();

    void rebuildVoiceGrid();
    void activateVoice(const std::string& filename);
    void editVoice(const std::string& filename);
    void deleteVoice(const std::string& filename);
    void exportVoice(const std::string& filename);
    void renameVoice(const std::string& filename);
    void exportCurrentFolder();
    void importVoice();
    void createNewVoice();
    void saveEditedVoice();
    void backToVoices();

    void refreshInputDevices();
    void refreshOutputDevices();
    void addSelectedEffect();
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void onVirtualMicNameChanged();

    void applyProfile(const std::string& filename);
    void savePipelineToProfile(const std::string& name, const std::string& filename);

    // Settings persistence
    void loadSettings();
    void saveSettings();

    // Clip recorder
    void onRecordClicked();
    void updateRecordingState();
    void applyTheme(bool dark);

    // Modulation param resolver for profile loading
    std::atomic<float>* resolveParam(const std::string& effectId, const std::string& paramId);

    AudioEngine& m_engine;
    ProfileManager& m_profileManager;

    QTabWidget* m_tabWidget;

    // Voices tab
    FolderSidebar* m_folderSidebar = nullptr;
    VoiceDetailsPanel* m_voiceDetailsPanel = nullptr;
    std::string m_activeVoiceFilename;

    // Edit Voice tab
    std::string m_editingVoiceFilename;
    std::string m_editingVoiceName;
    PipelineWidget* m_pipelineWidget;
    EffectSettingsPanel* m_settingsPanel;
    QListWidget* m_effectList;
    WaveformWidget* m_inputWaveform;
    WaveformWidget* m_outputWaveform;
    SpectrumWidget* m_spectrumWidget;
    KeyframeTimelineWidget* m_keyframeTimeline;
    QLabel* m_editingLabel;

    // Configuration tab
    QComboBox* m_inputDeviceCombo;
    QComboBox* m_outputDeviceCombo;

    // Settings tab
    QLineEdit* m_virtualMicNameEdit;
    QCheckBox* m_muteMicIfDisabledCheck;

    // Toolbar
    QPushButton* m_enableBtn;
    QPushButton* m_monitorBtn;
    QPushButton* m_recordBtn;
    QPushButton* m_playbackBtn;
    WaveformWidget* m_toolbarInputWaveform;
    WaveformWidget* m_toolbarOutputWaveform;
    QTimer m_recordTimer;

    SystemTray* m_tray = nullptr;
    QTimer m_meterTimer;
};
