#include "app/Application.h"
#include "app/VerboseLogger.h"
#include "audio/AudioEngine.h"
#include "profile/ProfileManager.h"
#include "gui/MainWindow.h"
#include <QCommandLineParser>
#include <QMessageBox>
#include <QSettings>
#include <QPalette>
#include <QStyle>
#include <iostream>

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {
    setApplicationName("VML");
    setApplicationDisplayName("Voice Modulation for Linux");
    setApplicationVersion("1.0.0");
    setQuitOnLastWindowClosed(false); // Keep running in tray

    parseArguments();
}

void Application::parseArguments() {
    QCommandLineParser parser;
    parser.setApplicationDescription("Voice Modulation for Linux");
    parser.addHelpOption();
    parser.addVersionOption();

    // Note: -v is taken by Qt for --version, so we use --verbose as long form
    QCommandLineOption verboseOption(
        QStringList() << "verbose",
        "Enable verbose debug output to stderr"
    );
    parser.addOption(verboseOption);

    parser.process(*this);

    g_verboseEnabled = parser.isSet(verboseOption);

    // Use std::cerr for startup messages (before event loop)
    if (g_verboseEnabled) {
        std::cerr << "[VML 0ms] Starting Voice Modulation for Linux v" << applicationVersion().toStdString() << std::endl;
        std::cerr << "[VML 0ms] Qt version: " << QT_VERSION_STR << std::endl;
        std::cerr << "[VML 0ms] Verbose mode: enabled" << std::endl;
        std::cerr.flush();
    }
}

Application::~Application() = default;

int Application::run() {
    VML_INFO("Application::run() started");
    std::cerr.flush();

    // Apply saved theme
    VML_DEBUG("Loading settings...");
    std::cerr.flush();
    QSettings settings("vml", "vml");
    bool dark = settings.value("darkTheme", true).toBool();
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
        setPalette(pal);
    }
    VML_DEBUG(QString("Theme applied, dark mode: %1").arg(dark ? "true" : "false"));
    std::cerr.flush();

    VML_DEBUG("Creating ProfileManager...");
    std::cerr.flush();
    m_profileManager = std::make_unique<ProfileManager>();
    VML_DEBUG("ProfileManager created");
    std::cerr.flush();

    // Install any missing built-in profiles
    VML_DEBUG("Installing default profiles...");
    std::cerr.flush();
    m_profileManager->installDefaultProfiles();
    VML_DEBUG("Default profiles installed");
    std::cerr.flush();

    // Log available input devices
    VML_DEBUG("Enumerating input devices...");
    std::cerr.flush();
    auto inputDevices = AudioEngine::enumerateInputDevices();
    VML_INFO(QString("Found %1 input devices").arg(inputDevices.size()));
    for (const auto& dev : inputDevices) {
        VML_DEBUG(QString("  Input device: %1 - %2").arg(dev.id).arg(QString::fromStdString(dev.description)));
    }
    std::cerr.flush();

    // Log available output devices
    VML_DEBUG("Enumerating output devices...");
    std::cerr.flush();
    auto outputDevices = AudioEngine::enumerateOutputDevices();
    VML_INFO(QString("Found %1 output devices").arg(outputDevices.size()));
    for (const auto& dev : outputDevices) {
        VML_DEBUG(QString("  Output device: %1 - %2").arg(dev.id).arg(QString::fromStdString(dev.description)));
    }
    std::cerr.flush();

    VML_DEBUG("Creating AudioEngine...");
    std::cerr.flush();
    m_engine = std::make_unique<AudioEngine>();
    VML_DEBUG("AudioEngine created");
    std::cerr.flush();

    VML_DEBUG("Starting AudioEngine...");
    std::cerr.flush();
    if (!m_engine->start()) {
        QMessageBox::critical(nullptr, "VML Error",
            "Failed to start audio engine.\n"
            "Make sure PipeWire is running.");
        return 1;
    }
    VML_DEBUG("AudioEngine started successfully");
    std::cerr.flush();

    VML_DEBUG("Creating MainWindow...");
    std::cerr.flush();
    m_mainWindow = std::make_unique<MainWindow>(*m_engine, *m_profileManager);
    VML_DEBUG("MainWindow created, showing...");
    std::cerr.flush();
    m_mainWindow->show();
    VML_INFO("Application ready, entering event loop");
    std::cerr.flush();

    return exec();
}
