#pragma once
#include <QMainWindow>
#include <QComboBox>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QTimer>

class AudioEngine;
class ProfileManager;
class PipelineWidget;
class EffectSettingsPanel;
class WaveformWidget;
class SpectrumWidget;
class SystemTray;

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

    QWidget* createEffectsTab();
    QWidget* createConfigurationTab();
    QWidget* createSettingsTab();

    void refreshProfiles();
    void refreshInputDevices();
    void refreshOutputDevices();
    void loadProfile(int index);
    void saveCurrentProfile();
    void addSelectedEffect();
    void removeSelectedEffect();
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);
    void onVirtualMicNameChanged();

    void applyProfile(const std::string& filename);
    void savePipelineToProfile(const std::string& name, const std::string& filename);

    // Clip recorder
    void onRecordClicked();
    void updateRecordingState();
    void applyTheme(bool dark);

    // Modulation param resolver for profile loading
    std::atomic<float>* resolveParam(const std::string& effectId, const std::string& paramId);

    AudioEngine& m_engine;
    ProfileManager& m_profileManager;

    QTabWidget* m_tabWidget;

    // Effects tab
    PipelineWidget* m_pipelineWidget;
    EffectSettingsPanel* m_settingsPanel;
    QListWidget* m_effectList;
    WaveformWidget* m_inputWaveform;
    WaveformWidget* m_outputWaveform;
    SpectrumWidget* m_spectrumWidget;

    // Configuration tab
    QComboBox* m_inputDeviceCombo;
    QComboBox* m_outputDeviceCombo;

    // Settings tab
    QLineEdit* m_virtualMicNameEdit;

    // Toolbar
    QComboBox* m_profileCombo;
    QPushButton* m_enableBtn;
    QPushButton* m_monitorBtn;
    QPushButton* m_recordBtn;
    QCheckBox* m_loopPlaybackCheck;
    QTimer m_recordTimer;

    SystemTray* m_tray;
    QTimer m_meterTimer;
};
