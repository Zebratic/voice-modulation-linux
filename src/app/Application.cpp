#include "app/Application.h"
#include "audio/AudioEngine.h"
#include "profile/ProfileManager.h"
#include "gui/MainWindow.h"
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
}

Application::~Application() = default;

int Application::run() {
    // Apply saved theme
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

    m_profileManager = std::make_unique<ProfileManager>();

    // Install any missing built-in profiles
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
