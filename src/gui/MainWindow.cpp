#include "gui/MainWindow.h"
#include "audio/AudioEngine.h"
#include "profile/ProfileManager.h"
#include "gui/PipelineWidget.h"
#include "gui/EffectSettingsPanel.h"
#include "gui/WaveformWidget.h"
#include "gui/SpectrumWidget.h"
#include "gui/KeyframeTimelineWidget.h"
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
#include <QFileDialog>
#include <QApplication>
#include <QCloseEvent>
#include <QShortcut>
#include <QSettings>
#include <QDir>
#include <algorithm>
#include <filesystem>
#include <cctype>
#include <QStyleFactory>
#include <QFrame>
#include <QFont>

MainWindow::MainWindow(AudioEngine& engine, ProfileManager& profileManager, QWidget* parent)
    : QMainWindow(parent), m_engine(engine), m_profileManager(profileManager) {
    setWindowTitle("VML - Voice Modulation for Linux");
    setMinimumSize(900, 600);
    setupUI();
    setupToolbar();
    connectSignals();
    refreshInputDevices();
    refreshOutputDevices();

    m_meterTimer.setInterval(30);
    connect(&m_meterTimer, &QTimer::timeout, this, [this]() {
        float buf[512];
        auto& inRb = m_engine.inputWaveform();
        size_t avail = inRb.availableRead();
        while (avail > 0) {
            size_t n = inRb.read(buf, std::min(avail, size_t(512)));
            m_inputWaveform->pushSamples(buf, static_cast<int>(n));
            avail -= n;
        }
        m_inputWaveform->setLevel(m_engine.inputLevel());

        auto& outRb = m_engine.outputWaveform();
        avail = outRb.availableRead();
        while (avail > 0) {
            size_t n = outRb.read(buf, std::min(avail, size_t(512)));
            m_outputWaveform->pushSamples(buf, static_cast<int>(n));
            m_spectrumWidget->pushSamples(buf, static_cast<int>(n));
            avail -= n;
        }
        m_outputWaveform->setLevel(m_engine.outputLevel());
    });
    m_meterTimer.start();
}

void MainWindow::setupUI() {
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);

    m_tabWidget->addTab(createVoicesTab(), "Voices");
    m_tabWidget->addTab(createEditVoiceTab(), "Edit Voice");
    m_tabWidget->addTab(createConfigurationTab(), "Configuration");
    m_tabWidget->addTab(createSettingsTab(), "Settings");

    // Start on Voices tab
    m_tabWidget->setCurrentIndex(0);
}

// ---------------------------------------------------------------------------
// Voices Tab — grid of voice cards
// ---------------------------------------------------------------------------

QWidget* MainWindow::createVoicesTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);

    // Top row: New Voice, Import
    auto* topRow = new QHBoxLayout();
    auto* newBtn = new QPushButton("+ New Voice", page);
    newBtn->setFixedHeight(32);
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::createNewVoice);
    topRow->addWidget(newBtn);

    auto* importBtn = new QPushButton("Import Voice", page);
    importBtn->setFixedHeight(32);
    connect(importBtn, &QPushButton::clicked, this, &MainWindow::importVoice);
    topRow->addWidget(importBtn);

    topRow->addStretch();
    layout->addLayout(topRow);

    // Scrollable grid of voice cards
    m_voiceScrollArea = new QScrollArea(page);
    m_voiceScrollArea->setWidgetResizable(true);
    m_voiceScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_voiceScrollArea->setFrameShape(QFrame::NoFrame);

    m_voiceGridWidget = new QWidget();
    m_voiceGridLayout = new QGridLayout(m_voiceGridWidget);
    m_voiceGridLayout->setSpacing(10);
    m_voiceGridLayout->setContentsMargins(4, 4, 4, 4);

    m_voiceScrollArea->setWidget(m_voiceGridWidget);
    layout->addWidget(m_voiceScrollArea, 1);

    rebuildVoiceGrid();
    return page;
}

void MainWindow::rebuildVoiceGrid() {
    // Clear existing cards
    QLayoutItem* item;
    while ((item = m_voiceGridLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto profiles = m_profileManager.listProfiles();
    int cols = 3;
    int row = 0, col = 0;

    for (auto& profile : profiles) {
        bool isActive = (profile.filename == m_activeVoiceFilename);

        // Card frame
        auto* card = new QFrame(m_voiceGridWidget);
        card->setFrameShape(QFrame::StyledPanel);
        card->setMinimumSize(220, 120);
        card->setMaximumHeight(140);
        card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        if (isActive) {
            card->setStyleSheet(
                "QFrame { background-color: #1a3a2a; border: 2px solid #2d7d2d; border-radius: 8px; }"
                "QFrame:hover { background-color: #1f4530; }");
        } else {
            card->setStyleSheet(
                "QFrame { background-color: #252535; border: 1px solid #444; border-radius: 8px; }"
                "QFrame:hover { background-color: #2a2a40; border-color: #666; }");
        }

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(6);

        // Top row: name + tag
        auto* headerRow = new QHBoxLayout();
        auto* nameLabel = new QLabel(QString::fromStdString(profile.name), card);
        QFont nameFont = nameLabel->font();
        nameFont.setPointSize(12);
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        headerRow->addWidget(nameLabel);

        headerRow->addStretch();

        // Tag
        auto* tag = new QLabel(profile.builtin ? "Built-in" : "Custom", card);
        QFont tagFont = tag->font();
        tagFont.setPixelSize(10);
        tag->setFont(tagFont);
        if (profile.builtin) {
            tag->setStyleSheet("QLabel { color: #7da8c9; background-color: #1a2a3a; border-radius: 4px; padding: 2px 6px; }");
        } else {
            tag->setStyleSheet("QLabel { color: #c9a87d; background-color: #3a2a1a; border-radius: 4px; padding: 2px 6px; }");
        }
        headerRow->addWidget(tag);

        if (isActive) {
            auto* activeLabel = new QLabel("ACTIVE", card);
            QFont af = activeLabel->font();
            af.setPixelSize(10);
            af.setBold(true);
            activeLabel->setFont(af);
            activeLabel->setStyleSheet("QLabel { color: #2d7d2d; padding: 2px 6px; }");
            headerRow->addWidget(activeLabel);
        }

        cardLayout->addLayout(headerRow);

        // Effect count summary
        int effectCount = 0;
        if (profile.data.contains("effects") && profile.data["effects"].is_array())
            effectCount = static_cast<int>(profile.data["effects"].size());
        auto* summaryLabel = new QLabel(QString("%1 effect%2").arg(effectCount).arg(effectCount != 1 ? "s" : ""), card);
        summaryLabel->setStyleSheet("QLabel { color: #888; }");
        QFont sf = summaryLabel->font();
        sf.setPixelSize(11);
        summaryLabel->setFont(sf);
        cardLayout->addWidget(summaryLabel);

        cardLayout->addStretch();

        // Button row
        auto* btnRow = new QHBoxLayout();

        auto* activateBtn = new QPushButton(isActive ? "Active" : "Activate", card);
        activateBtn->setFixedHeight(26);
        activateBtn->setEnabled(!isActive);
        if (isActive) {
            activateBtn->setStyleSheet("QPushButton { background-color: #2d7d2d; color: white; border-radius: 4px; }");
        }
        std::string fn = profile.filename;
        connect(activateBtn, &QPushButton::clicked, this, [this, fn]() {
            activateVoice(fn);
        });
        btnRow->addWidget(activateBtn);

        auto* editBtn = new QPushButton("Edit", card);
        editBtn->setFixedHeight(26);
        connect(editBtn, &QPushButton::clicked, this, [this, fn]() {
            editVoice(fn);
        });
        btnRow->addWidget(editBtn);

        auto* exportBtn = new QPushButton("Export", card);
        exportBtn->setFixedHeight(26);
        connect(exportBtn, &QPushButton::clicked, this, [this, fn]() {
            exportVoice(fn);
        });
        btnRow->addWidget(exportBtn);

        if (!profile.builtin) {
            auto* deleteBtn = new QPushButton("Delete", card);
            deleteBtn->setFixedHeight(26);
            deleteBtn->setStyleSheet("QPushButton { color: #c44; }");
            connect(deleteBtn, &QPushButton::clicked, this, [this, fn]() {
                deleteVoice(fn);
            });
            btnRow->addWidget(deleteBtn);
        }

        cardLayout->addLayout(btnRow);

        m_voiceGridLayout->addWidget(card, row, col);
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }

    // Fill remaining columns with spacers
    if (col > 0) {
        for (int c = col; c < cols; c++)
            m_voiceGridLayout->addWidget(new QWidget(), row, c);
    }

    // Update tray profiles
    QStringList names;
    for (auto& p : profiles)
        names << QString::fromStdString(p.name);
    if (m_tray) m_tray->setProfiles(names);
}

void MainWindow::activateVoice(const std::string& filename) {
    m_activeVoiceFilename = filename;
    applyProfile(filename);
    rebuildVoiceGrid();
}

void MainWindow::editVoice(const std::string& filename) {
    m_editingVoiceFilename = filename;
    applyProfile(filename);
    m_activeVoiceFilename = filename;

    try {
        auto profile = m_profileManager.loadProfile(filename);
        m_editingVoiceName = profile.name;
        m_editingLabel->setText(QString("Editing: %1").arg(QString::fromStdString(profile.name)));
    } catch (...) {
        m_editingVoiceName = filename;
        m_editingLabel->setText(QString("Editing: %1").arg(QString::fromStdString(filename)));
    }

    m_tabWidget->setCurrentIndex(1); // Switch to Edit Voice tab
}

void MainWindow::deleteVoice(const std::string& filename) {
    try {
        auto profile = m_profileManager.loadProfile(filename);
        if (profile.builtin) {
            QMessageBox::information(this, "Cannot Delete",
                "Built-in voices cannot be deleted.");
            return;
        }
    } catch (...) {}

    auto reply = QMessageBox::question(this, "Delete Voice",
        "Are you sure you want to delete this voice?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    if (filename == m_activeVoiceFilename) {
        m_engine.modulationManager().clear();
        m_engine.pipeline().clear();
        m_pipelineWidget->rebuild();
        m_settingsPanel->clearEffect();
        m_activeVoiceFilename.clear();
    }

    m_profileManager.deleteProfile(filename);
    rebuildVoiceGrid();
}

void MainWindow::exportVoice(const std::string& filename) {
    try {
        auto profile = m_profileManager.loadProfile(filename);
        QString defaultName = QString::fromStdString(profile.name) + ".json";
        QString path = QFileDialog::getSaveFileName(this, "Export Voice",
            QDir::homePath() + "/" + defaultName, "VML Voice (*.json)");
        if (path.isEmpty()) return;
        m_profileManager.exportProfile(filename, path.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Export Error", e.what());
    }
}

void MainWindow::importVoice() {
    QString path = QFileDialog::getOpenFileName(this, "Import Voice",
        QDir::homePath(), "VML Voice (*.json)");
    if (path.isEmpty()) return;

    try {
        m_profileManager.importProfile(path.toStdString());
        rebuildVoiceGrid();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Import Error",
            QString("Failed to import voice: %1").arg(e.what()));
    }
}

void MainWindow::createNewVoice() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Voice", "Voice name:",
        QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    auto filename = name.toLower().replace(' ', '-').toStdString() + ".json";
    auto json = ProfileManager::pipelineToJson(name.toStdString(), {});
    json["version"] = 2;
    json["modulators"] = nlohmann::json::array();
    m_profileManager.saveProfile({name.toStdString(), filename, json, false});

    editVoice(filename);
    rebuildVoiceGrid();
}

void MainWindow::saveEditedVoice() {
    if (m_editingVoiceFilename.empty()) return;

    // Check if this is a built-in voice
    try {
        auto profile = m_profileManager.loadProfile(m_editingVoiceFilename);
        if (profile.builtin) {
            // Prompt user for a new name — can't overwrite built-in
            bool ok = false;
            QString newName = QInputDialog::getText(this, "Save as Copy",
                "Built-in voices cannot be overwritten.\n"
                "A copy will be created. Enter a name for the new voice:",
                QLineEdit::Normal,
                QString::fromStdString(m_editingVoiceName) + " (Custom)", &ok);
            if (!ok || newName.trimmed().isEmpty()) return;

            std::string name = newName.trimmed().toStdString();
            // Generate a new filename
            std::string base = name;
            std::transform(base.begin(), base.end(), base.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            std::replace(base.begin(), base.end(), ' ', '-');
            std::string newFilename = base + ".json";
            int counter = 1;
            auto dir = m_profileManager.profileDir();
            while (std::filesystem::exists(dir / newFilename))
                newFilename = base + "-" + std::to_string(counter++) + ".json";

            savePipelineToProfile(name, newFilename);
            m_editingVoiceFilename = newFilename;
            m_editingVoiceName = name;
            m_editingLabel->setText(QString("Editing: %1").arg(newName.trimmed()));
            return;
        }
    } catch (...) {}

    savePipelineToProfile(m_editingVoiceName, m_editingVoiceFilename);
}

void MainWindow::backToVoices() {
    saveEditedVoice();
    rebuildVoiceGrid();
    m_tabWidget->setCurrentIndex(0);
}

// ---------------------------------------------------------------------------
// Edit Voice Tab — the effects editor
// ---------------------------------------------------------------------------

QWidget* MainWindow::createEditVoiceTab() {
    auto* page = new QWidget();
    auto* mainLayout = new QVBoxLayout(page);

    // Top bar: back button + editing label + save button
    auto* topBar = new QHBoxLayout();
    auto* backBtn = new QPushButton("<< Back to Voices", page);
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::backToVoices);
    topBar->addWidget(backBtn);

    m_editingLabel = new QLabel("Editing: (none)", page);
    QFont f = m_editingLabel->font();
    f.setPointSize(12);
    f.setBold(true);
    m_editingLabel->setFont(f);
    topBar->addWidget(m_editingLabel);
    topBar->addStretch();

    auto* saveBtn = new QPushButton("Save", page);
    connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveEditedVoice);
    topBar->addWidget(saveBtn);

    mainLayout->addLayout(topBar);

    // Waveform indicators
    auto* meterLayout = new QHBoxLayout();
    m_inputWaveform = new WaveformWidget(page);
    m_inputWaveform->setLabel("IN");
    m_outputWaveform = new WaveformWidget(page);
    m_outputWaveform->setLabel("OUT");
    meterLayout->addWidget(m_inputWaveform);
    meterLayout->addWidget(m_outputWaveform);
    mainLayout->addLayout(meterLayout);

    // Spectrum analyzer
    m_spectrumWidget = new SpectrumWidget(page);
    mainLayout->addWidget(m_spectrumWidget);

    // Horizontal split: effect list | pipeline | settings
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

    splitter->addWidget(effectListWidget);

    // Center: pipeline
    m_pipelineWidget = new PipelineWidget(splitter);
    m_pipelineWidget->setPipeline(&m_engine.pipeline());
    m_pipelineWidget->setModulationManager(&m_engine.modulationManager());
    splitter->addWidget(m_pipelineWidget);

    // Right: effect settings
    m_settingsPanel = new EffectSettingsPanel(splitter);
    m_settingsPanel->setModulationManager(&m_engine.modulationManager());
    m_pipelineWidget->setSettingsPanel(m_settingsPanel);
    splitter->addWidget(m_settingsPanel);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 2);

    // Vertical splitter: top (horizontal splitter) | bottom (keyframe timeline)
    auto* vSplitter = new QSplitter(Qt::Vertical, page);
    vSplitter->addWidget(splitter);

    m_keyframeTimeline = new KeyframeTimelineWidget(vSplitter);
    m_keyframeTimeline->setModulationManager(&m_engine.modulationManager());
    m_keyframeTimeline->setPipeline(&m_engine.pipeline());
    vSplitter->addWidget(m_keyframeTimeline);

    vSplitter->setStretchFactor(0, 3);
    vSplitter->setStretchFactor(1, 1);

    mainLayout->addWidget(vSplitter, 1);

    return page;
}

// ---------------------------------------------------------------------------
// Configuration Tab
// ---------------------------------------------------------------------------

QWidget* MainWindow::createConfigurationTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

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

// ---------------------------------------------------------------------------
// Settings Tab
// ---------------------------------------------------------------------------

QWidget* MainWindow::createSettingsTab() {
    auto* page = new QWidget();
    auto* layout = new QVBoxLayout(page);

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

    // Theme toggle
    auto* themeGroup = new QGroupBox("Appearance", page);
    auto* themeLayout = new QVBoxLayout(themeGroup);
    auto* darkThemeCheck = new QCheckBox("Dark Theme", themeGroup);
    QSettings settings("vml", "vml");
    darkThemeCheck->setChecked(settings.value("darkTheme", true).toBool());
    connect(darkThemeCheck, &QCheckBox::toggled, this, [this](bool dark) {
        applyTheme(dark);
        QSettings s("vml", "vml");
        s.setValue("darkTheme", dark);
    });
    themeLayout->addWidget(darkThemeCheck);
    layout->addWidget(themeGroup);

    layout->addStretch();
    return page;
}

// ---------------------------------------------------------------------------
// Toolbar
// ---------------------------------------------------------------------------

void MainWindow::setupToolbar() {
    setContextMenuPolicy(Qt::NoContextMenu);
    auto* toolbar = addToolBar("Main");
    toolbar->setMovable(false);

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

    m_recordBtn = new QPushButton("Record Clip");
    connect(m_recordBtn, &QPushButton::clicked, this, &MainWindow::onRecordClicked);
    toolbar->addWidget(m_recordBtn);

    m_playbackBtn = new QPushButton("Playback Recording");
    m_playbackBtn->setCheckable(true);
    m_playbackBtn->setEnabled(false);
    m_playbackBtn->setStyleSheet("QPushButton:checked { background-color: #7d2d7d; }");
    connect(m_playbackBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (checked) {
            m_engine.clipRecorder().startPlayback();
            m_engine.setPlaybackActive(true);
        } else {
            m_engine.clipRecorder().stopPlayback();
            m_engine.setPlaybackActive(false);
        }
    });
    toolbar->addWidget(m_playbackBtn);

    m_recordTimer.setInterval(100);
    connect(&m_recordTimer, &QTimer::timeout, this, &MainWindow::updateRecordingState);
}

void MainWindow::onRecordClicked() {
    auto& rec = m_engine.clipRecorder();

    // If currently recording, stop
    if (rec.state() == ClipRecorder::State::Recording) {
        rec.stopRecording();
        m_recordTimer.stop();
        m_recordBtn->setText("Record Clip");
        m_recordBtn->setStyleSheet("");
        m_playbackBtn->setEnabled(rec.hasClip());
        return;
    }

    // Start recording
    if (m_playbackBtn->isChecked())
        m_playbackBtn->setChecked(false);
    m_playbackBtn->setEnabled(false);
    rec.startRecording();
    m_recordBtn->setText("Stop (0.0s)");
    m_recordBtn->setStyleSheet("background-color: #cc3333; color: white; font-weight: bold;");
    m_recordTimer.start();
}

void MainWindow::updateRecordingState() {
    auto& rec = m_engine.clipRecorder();
    if (rec.state() == ClipRecorder::State::Recording) {
        float seconds = rec.recordedSeconds();
        m_recordBtn->setText(QString("Stop (%1s)").arg(seconds, 0, 'f', 1));
    } else {
        // Auto-stopped at max length
        m_recordTimer.stop();
        m_recordBtn->setText("Record Clip");
        m_recordBtn->setStyleSheet("");
        m_playbackBtn->setEnabled(rec.hasClip());
    }
}

// ---------------------------------------------------------------------------
// Signals & Shortcuts
// ---------------------------------------------------------------------------

void MainWindow::connectSignals() {
    connect(m_enableBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_engine.setEnabled(checked);
    });

    connect(m_monitorBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_engine.setMonitorEnabled(checked);
    });

    connect(m_inputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onInputDeviceChanged);

    connect(m_outputDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::onOutputDeviceChanged);

    // Keyboard shortcuts
    auto* scEnable = new QShortcut(QKeySequence("Ctrl+E"), this);
    connect(scEnable, &QShortcut::activated, this, [this]() { m_enableBtn->toggle(); });
    auto* scMonitor = new QShortcut(QKeySequence("Ctrl+M"), this);
    connect(scMonitor, &QShortcut::activated, this, [this]() { m_monitorBtn->toggle(); });

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
        show(); raise(); activateWindow();
    });
    connect(m_tray, &SystemTray::quitRequested, qApp, &QApplication::quit);
    connect(m_tray, &SystemTray::profileSelected, this, [this](int idx) {
        auto profiles = m_profileManager.listProfiles();
        if (idx >= 0 && idx < static_cast<int>(profiles.size()))
            activateVoice(profiles[idx].filename);
    });

    // Rebuild keyframe timeline when modulation changes
    connect(m_settingsPanel, &EffectSettingsPanel::modulationChanged,
            this, [this]() { m_keyframeTimeline->rebuild(); });

    // Rebuild voice grid after tray selection
    rebuildVoiceGrid();
}

// ---------------------------------------------------------------------------
// Profile loading/saving
// ---------------------------------------------------------------------------

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

        if (profile.data.contains("modulators") && profile.data["modulators"].is_array()) {
            m_engine.modulationManager().fromJson(profile.data["modulators"],
                [this](const std::string& eid, const std::string& pid) {
                    return resolveParam(eid, pid);
                });
        }

        m_pipelineWidget->rebuild();
        m_settingsPanel->clearEffect();
        m_keyframeTimeline->rebuild();
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Error", QString("Failed to load profile: %1").arg(e.what()));
    }
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
    json["version"] = 2;
    json["modulators"] = m_engine.modulationManager().toJson();

    // Preserve builtin flag if editing a built-in profile
    try {
        auto existing = m_profileManager.loadProfile(filename);
        if (existing.builtin)
            json["builtin"] = true;
    } catch (...) {}

    m_profileManager.saveProfile({name, filename, json});
}

void MainWindow::addSelectedEffect() {
    auto* item = m_effectList->currentItem();
    if (!item) return;
    auto effectId = item->data(Qt::UserRole).toString().toStdString();
    m_pipelineWidget->addEffect(effectId);
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

void MainWindow::applyTheme(bool dark) {
    if (dark) {
        QPalette pal;
        pal.setColor(QPalette::Window, QColor(0x2b, 0x2b, 0x2b));
        pal.setColor(QPalette::WindowText, Qt::white);
        pal.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
        pal.setColor(QPalette::AlternateBase, QColor(0x2b, 0x2b, 0x2b));
        pal.setColor(QPalette::ToolTipBase, Qt::white);
        pal.setColor(QPalette::ToolTipText, Qt::white);
        pal.setColor(QPalette::Text, Qt::white);
        pal.setColor(QPalette::Button, QColor(0x35, 0x35, 0x35));
        pal.setColor(QPalette::ButtonText, Qt::white);
        pal.setColor(QPalette::BrightText, Qt::red);
        pal.setColor(QPalette::Link, QColor(0x2d, 0x7d, 0x9a));
        pal.setColor(QPalette::Highlight, QColor(0x2d, 0x7d, 0x9a));
        pal.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(pal);
    } else {
        qApp->setPalette(qApp->style()->standardPalette());
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    hide();
    event->ignore();
}
