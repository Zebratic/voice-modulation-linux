#include "gui/MainWindow.h"
#include "audio/AudioEngine.h"
#include "profile/ProfileManager.h"
#include "gui/PipelineWidget.h"
#include "gui/EffectSettingsPanel.h"
#include "gui/VuMeter.h"
#include "gui/SystemTray.h"
#include "effects/EffectRegistry.h"
#include "audio/ClipRecorder.h"
#include <QToolBar>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QApplication>
#include <QCloseEvent>

MainWindow::MainWindow(AudioEngine& engine, ProfileManager& profileManager, QWidget* parent)
    : QMainWindow(parent), m_engine(engine), m_profileManager(profileManager) {
    setWindowTitle("VML - Voice Modulation for Linux");
    setMinimumSize(800, 550);
    setupUI();
    setupToolbar();
    connectSignals();
    refreshProfiles();
    refreshInputDevices();
    refreshOutputDevices();

    m_meterTimer.setInterval(30);
    connect(&m_meterTimer, &QTimer::timeout, this, [this]() {
        m_inputMeter->setLevel(m_engine.inputLevel());
        m_outputMeter->setLevel(m_engine.outputLevel());
    });
    m_meterTimer.start();
}

void MainWindow::setupUI() {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    m_tabWidget->addTab(createEffectsTab(), "Effects");
    m_tabWidget->addTab(createConfigurationTab(), "Configuration");
    m_tabWidget->addTab(createSettingsTab(), "Settings");
}

QWidget* MainWindow::createEffectsTab() {
    auto* page = new QWidget();
    auto* mainLayout = new QVBoxLayout(page);

    // VU Meters at top
    auto* meterLayout = new QHBoxLayout();
    m_inputMeter = new VuMeter(page);
    m_inputMeter->setLabel("IN");
    m_outputMeter = new VuMeter(page);
    m_outputMeter->setLabel("OUT");
    meterLayout->addWidget(m_inputMeter);
    meterLayout->addWidget(m_outputMeter);
    mainLayout->addLayout(meterLayout);

    // Horizontal split: effect list | vertical pipeline | effect settings
    auto* splitter = new QSplitter(Qt::Horizontal, page);

    // Left: available effects list
    auto* effectListWidget = new QWidget(splitter);
    auto* effectListLayout = new QVBoxLayout(effectListWidget);
    effectListLayout->setContentsMargins(4, 4, 4, 4);
    effectListLayout->addWidget(new QLabel("Available Effects"));
    m_effectList = new QListWidget(effectListWidget);
    for (auto& id : EffectRegistry::instance().availableEffects()) {
        auto effect = EffectRegistry::instance().create(id);
        if (effect) {
            auto* item = new QListWidgetItem(QString::fromStdString(effect->name()));
            item->setData(Qt::UserRole, QString::fromStdString(id));
            m_effectList->addItem(item);
        }
    }
    effectListLayout->addWidget(m_effectList);

    auto* addBtn = new QPushButton("Add to Pipeline", effectListWidget);
    connect(addBtn, &QPushButton::clicked, this, &MainWindow::addSelectedEffect);
    effectListLayout->addWidget(addBtn);

    auto* removeBtn = new QPushButton("Remove Selected", effectListWidget);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::removeSelectedEffect);
    effectListLayout->addWidget(removeBtn);

    splitter->addWidget(effectListWidget);

    // Center: vertical pipeline (input at top, output at bottom)
    m_pipelineWidget = new PipelineWidget(splitter);
    m_pipelineWidget->setPipeline(&m_engine.pipeline());
    m_pipelineWidget->setModulationManager(&m_engine.modulationManager());
    splitter->addWidget(m_pipelineWidget);

    // Right: effect settings panel
    m_settingsPanel = new EffectSettingsPanel(splitter);
    m_settingsPanel->setModulationManager(&m_engine.modulationManager());
    m_pipelineWidget->setSettingsPanel(m_settingsPanel);
    splitter->addWidget(m_settingsPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);

    mainLayout->addWidget(splitter, 1);
    return page;
}

QWidget* MainWindow::createConfigurationTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    // Input device group
    auto* inputGroup = new QGroupBox("Input Microphone", page);
    auto* inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->addWidget(new QLabel("Select which physical microphone VML captures from:"));
    auto* inputRow = new QHBoxLayout();
    m_inputDeviceCombo = new QComboBox(inputGroup);
    m_inputDeviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    inputRow->addWidget(m_inputDeviceCombo, 1);
    auto* refreshInputBtn = new QPushButton("Refresh", inputGroup);
    connect(refreshInputBtn, &QPushButton::clicked, this, &MainWindow::refreshInputDevices);
    inputRow->addWidget(refreshInputBtn);
    inputLayout->addLayout(inputRow);
    layout->addWidget(inputGroup);

    // Monitor output group
    auto* outputGroup = new QGroupBox("Monitor Output", page);
    auto* outputLayout = new QVBoxLayout(outputGroup);
    outputLayout->addWidget(new QLabel("Select which speakers/headphones to use for monitor playback:"));
    auto* outputRow = new QHBoxLayout();
    m_outputDeviceCombo = new QComboBox(outputGroup);
    m_outputDeviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    outputRow->addWidget(m_outputDeviceCombo, 1);
    auto* refreshOutputBtn = new QPushButton("Refresh", outputGroup);
    connect(refreshOutputBtn, &QPushButton::clicked, this, &MainWindow::refreshOutputDevices);
    outputRow->addWidget(refreshOutputBtn);
    outputLayout->addLayout(outputRow);
    outputLayout->addWidget(new QLabel(
        "Enable the \"Monitor\" button in the toolbar to hear your processed voice through these speakers."));
    layout->addWidget(outputGroup);

    layout->addStretch();
    return page;
}

QWidget* MainWindow::createSettingsTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

    // Virtual mic name group
    auto* nameGroup = new QGroupBox("Virtual Microphone", page);
    auto* nameLayout = new QVBoxLayout(nameGroup);
    nameLayout->addWidget(new QLabel(
        "This is the device name that appears in other applications (Discord, OBS, etc.):"));
    auto* nameRow = new QHBoxLayout();
    m_virtualMicNameEdit = new QLineEdit(nameGroup);
    m_virtualMicNameEdit->setText(QString::fromStdString(m_engine.virtualMicName()));
    m_virtualMicNameEdit->setPlaceholderText("VML Virtual Microphone");
    nameRow->addWidget(m_virtualMicNameEdit, 1);
    auto* applyNameBtn = new QPushButton("Apply", nameGroup);
    connect(applyNameBtn, &QPushButton::clicked, this, &MainWindow::onVirtualMicNameChanged);
    nameRow->addWidget(applyNameBtn);
    nameLayout->addLayout(nameRow);
    nameLayout->addWidget(new QLabel(
        "After changing, you may need to re-select the input device in your application."));
    layout->addWidget(nameGroup);

    layout->addStretch();
    return page;
}

void MainWindow::setupToolbar() {
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    toolbar->addWidget(new QLabel(" Profile: "));
    m_profileCombo = new QComboBox();
    m_profileCombo->setMinimumWidth(150);
    toolbar->addWidget(m_profileCombo);

    auto* saveBtn = new QPushButton("Save");
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveCurrentProfile);
    toolbar->addWidget(saveBtn);

    auto* deleteBtn = new QPushButton("Delete");
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        int idx = m_profileCombo->currentIndex();
        if (idx <= 0) return; // "(None)" selected
        auto filename = m_profileCombo->itemData(idx).toString().toStdString();
        auto name = m_profileCombo->currentText();
        auto reply = QMessageBox::question(this, "Delete Profile",
            QString("Delete profile \"%1\"?").arg(name),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            m_engine.modulationManager().clear();
            m_engine.pipeline().clear();
            m_pipelineWidget->rebuild();
            m_settingsPanel->clearEffect();
            m_profileManager.deleteProfile(filename);
            refreshProfiles();
        }
    });
    toolbar->addWidget(deleteBtn);

    toolbar->addSeparator();

    m_enableBtn = new QPushButton("Enabled");
    m_enableBtn->setCheckable(true);
    m_enableBtn->setChecked(true);
    m_enableBtn->setStyleSheet("QPushButton:checked { background-color: #2d7d2d; }");
    toolbar->addWidget(m_enableBtn);

    m_monitorBtn = new QPushButton("Monitor");
    m_monitorBtn->setCheckable(true);
    m_monitorBtn->setChecked(false);
    m_monitorBtn->setStyleSheet("QPushButton:checked { background-color: #7d7d2d; }");
    toolbar->addWidget(m_monitorBtn);

    toolbar->addSeparator();

    // Clip recorder controls
    m_recordBtn = new QPushButton("Record 3s Clip");
    connect(m_recordBtn, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    toolbar->addWidget(m_recordBtn);

    m_loopPlaybackCheck = new QCheckBox("Loop Playback");
    m_loopPlaybackCheck->setVisible(false);
    connect(m_loopPlaybackCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            m_engine.clipRecorder().startPlayback();
            m_recordBtn->setEnabled(false);
        } else {
            m_engine.clipRecorder().stopAndClear();
            m_recordBtn->setEnabled(true);
            m_loopPlaybackCheck->setVisible(false);
        }
    });
    toolbar->addWidget(m_loopPlaybackCheck);

    // Timer for recording progress
    m_recordTimer.setInterval(100);
    connect(&m_recordTimer, &QTimer::timeout, this, &MainWindow::updateRecordingState);
}

void MainWindow::onRecordClicked() {
    m_engine.clipRecorder().startRecording();
    m_recordBtn->setEnabled(false);
    m_recordBtn->setText("Recording...");
    m_recordTimer.start();
}

void MainWindow::updateRecordingState() {
    auto& rec = m_engine.clipRecorder();
    if (rec.state() == ClipRecorder::State::Recording) {
        float progress = rec.recordProgress();
        m_recordBtn->setText(QString("Recording %1%").arg(static_cast<int>(progress * 100)));
    } else {
        // Recording finished
        m_recordTimer.stop();
        m_recordBtn->setText("Record 3s Clip");
        m_recordBtn->setEnabled(true);
        if (rec.hasClip()) {
            m_loopPlaybackCheck->setVisible(true);
        }
    }
}

void MainWindow::connectSignals() {
    connect(m_enableBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_engine.setEnabled(checked);
    });

    connect(m_monitorBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_engine.setMonitorEnabled(checked);
    });

    connect(m_profileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::loadProfile);

    connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onInputDeviceChanged);

    connect(m_outputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onOutputDeviceChanged);

    // System tray
    m_tray = new SystemTray(this);
    connect(m_tray, &SystemTray::enableToggled, this, [this](bool e) {
        m_engine.setEnabled(e);
        m_enableBtn->setChecked(e);
    });
    connect(m_tray, &SystemTray::monitorToggled, this, [this](bool e) {
        m_engine.setMonitorEnabled(e);
        m_monitorBtn->setChecked(e);
    });
    connect(m_tray, &SystemTray::showWindowRequested, this, [this]() {
        show();
        raise();
        activateWindow();
    });
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);
    connect(m_tray, &SystemTray::profileSelected, this, [this](int idx) {
        m_profileCombo->setCurrentIndex(idx);
    });
}

void MainWindow::refreshProfiles() {
    m_profileCombo->blockSignals(true);
    m_profileCombo->clear();
    m_profileCombo->addItem("(None)");

    auto profiles = m_profileManager.listProfiles();
    QStringList names;
    for (auto& p : profiles) {
        m_profileCombo->addItem(QString::fromStdString(p.name),
            QString::fromStdString(p.filename));
        names << QString::fromStdString(p.name);
    }
    m_tray->setProfiles(names);
    m_profileCombo->blockSignals(false);
}

void MainWindow::refreshInputDevices() {
    m_inputDeviceCombo->blockSignals(true);
    m_inputDeviceCombo->clear();
    m_inputDeviceCombo->addItem("Default (auto)", QVariant(static_cast<uint>(PW_ID_ANY)));

    auto devices = AudioEngine::enumerateInputDevices();
    uint32_t currentId = m_engine.inputDeviceId();
    int selectIdx = 0;

    for (size_t i = 0; i < devices.size(); ++i) {
        auto& dev = devices[i];
        m_inputDeviceCombo->addItem(
            QString::fromStdString(dev.description),
            QVariant(static_cast<uint>(dev.id)));
        if (dev.id == currentId)
            selectIdx = static_cast<int>(i) + 1;
    }

    m_inputDeviceCombo->setCurrentIndex(selectIdx);
    m_inputDeviceCombo->blockSignals(false);
}

void MainWindow::refreshOutputDevices() {
    m_outputDeviceCombo->blockSignals(true);
    m_outputDeviceCombo->clear();
    m_outputDeviceCombo->addItem("Default (auto)", QVariant(static_cast<uint>(PW_ID_ANY)));

    auto devices = AudioEngine::enumerateOutputDevices();
    uint32_t currentId = m_engine.monitorOutputId();
    int selectIdx = 0;

    for (size_t i = 0; i < devices.size(); ++i) {
        auto& dev = devices[i];
        m_outputDeviceCombo->addItem(
            QString::fromStdString(dev.description),
            QVariant(static_cast<uint>(dev.id)));
        if (dev.id == currentId)
            selectIdx = static_cast<int>(i) + 1;
    }

    m_outputDeviceCombo->setCurrentIndex(selectIdx);
    m_outputDeviceCombo->blockSignals(false);
}

void MainWindow::loadProfile(int index) {
    if (index <= 0) return;
    auto filename = m_profileCombo->itemData(index).toString().toStdString();
    applyProfile(filename);
}

std::atomic<float>* MainWindow::resolveParam(const std::string& effectId, const std::string& paramId) {
    int count = m_engine.pipeline().effectCount();
    for (int i = 0; i < count; ++i) {
        auto* effect = m_engine.pipeline().effectAt(i);
        if (!effect || effect->id() != effectId) continue;
        for (auto& p : effect->params()) {
            if (p.id == paramId)
                return &p.value;
        }
    }
    return nullptr;
}

void MainWindow::applyProfile(const std::string& filename) {
    try {
        auto profile = m_profileManager.loadProfile(filename);
        auto effects = ProfileManager::jsonToPipeline(profile.data);

        m_engine.modulationManager().clear();
        m_engine.pipeline().clear();
        for (auto& [effectId, params] : effects) {
            auto effect = EffectRegistry::instance().create(effectId);
            if (!effect) continue;
            for (auto& [pid, val] : params) {
                for (auto& p : effect->params()) {
                    if (p.id == pid)
                        p.value.store(val, std::memory_order_relaxed);
                }
            }
            m_engine.pipeline().addEffect(std::move(effect));
        }

        // Load modulators if present (version 2+)
        if (profile.data.contains("modulators") && profile.data["modulators"].is_array()) {
            m_engine.modulationManager().fromJson(profile.data["modulators"],
                [this](const std::string& eid, const std::string& pid) {
                    return resolveParam(eid, pid);
                });
        }

        m_pipelineWidget->rebuild();
        m_settingsPanel->clearEffect();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error", QString("Failed to load profile: %1").arg(e.what()));
    }
}

void MainWindow::saveCurrentProfile() {
    bool ok;
    QString name = QInputDialog::getText(this, "Save Profile", "Profile name:",
        QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    auto filename = name.toLower().replace(' ', '-').toStdString() + ".json";
    savePipelineToProfile(name.toStdString(), filename);
    refreshProfiles();
}

void MainWindow::savePipelineToProfile(const std::string& name, const std::string& filename) {
    std::vector<std::pair<std::string, std::vector<std::pair<std::string, float>>>> effects;

    int count = m_engine.pipeline().effectCount();
    for (int i = 0; i < count; ++i) {
        auto* effect = m_engine.pipeline().effectAt(i);
        if (!effect) continue;
        std::vector<std::pair<std::string, float>> params;
        for (auto& p : effect->params())
            params.emplace_back(p.id, p.value.load(std::memory_order_relaxed));
        effects.emplace_back(effect->id(), params);
    }

    auto json = ProfileManager::pipelineToJson(name, effects);

    // Add modulators (version 2)
    json["version"] = 2;
    json["modulators"] = m_engine.modulationManager().toJson();

    m_profileManager.saveProfile({name, filename, json});
}

void MainWindow::addSelectedEffect() {
    auto* item = m_effectList->currentItem();
    if (!item) return;
    auto effectId = item->data(Qt::UserRole).toString().toStdString();
    m_pipelineWidget->addEffect(effectId);
}

void MainWindow::removeSelectedEffect() {
    m_pipelineWidget->removeSelectedEffect();
}

void MainWindow::onInputDeviceChanged(int index) {
    if (index < 0) return;
    uint32_t nodeId = m_inputDeviceCombo->itemData(index).toUInt();
    m_engine.setInputDevice(nodeId);
}

void MainWindow::onOutputDeviceChanged(int index) {
    if (index < 0) return;
    uint32_t nodeId = m_outputDeviceCombo->itemData(index).toUInt();
    m_engine.setMonitorOutputDevice(nodeId);
}

void MainWindow::onVirtualMicNameChanged() {
    QString name = m_virtualMicNameEdit->text().trimmed();
    if (name.isEmpty()) {
        name = "VML Virtual Microphone";
        m_virtualMicNameEdit->setText(name);
    }
    m_engine.setVirtualMicName(name.toStdString());
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}
