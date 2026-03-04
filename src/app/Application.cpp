#include "app/Application.h"
#include "audio/AudioEngine.h"
#include "profile/ProfileManager.h"
#include "gui/MainWindow.h"
#include <QMessageBox>
#include <iostream>

Application::Application(int& argc, char** argv) : QApplication(argc, argv) {
    setApplicationName("VML");
    setApplicationDisplayName("Voice Modulation for Linux");
    setApplicationVersion("1.0.0");
    setQuitOnLastWindowClosed(false); // Keep running in tray
}

Application::~Application() = default;

int Application::run() {
    m_profileManager = std::make_unique<ProfileManager>();

    // Install default profiles if none exist
    if (m_profileManager->listProfiles().empty())
        m_profileManager->installDefaultProfiles();

    m_engine = std::make_unique<AudioEngine>();

    if (!m_engine->start()) {
        QMessageBox::critical(nullptr, "VML Error",
            "Failed to start audio engine.\n"
            "Make sure PipeWire is running.");
        return 1;
    }

    m_mainWindow = std::make_unique<MainWindow>(*m_engine, *m_profileManager);
    m_mainWindow->show();

    return exec();
}
