#include "gui/SystemTray.h"
#include <QApplication>
#include <QIcon>

SystemTray::SystemTray(QObject* parent) : QObject(parent) {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme("audio-input-microphone"));
    m_trayIcon->setToolTip("VML - Voice Modulation for Linux");

    m_menu = new QMenu();
    m_profileGroup = new QActionGroup(this);
    m_profileGroup->setExclusive(true);

    buildMenu();

    m_trayIcon->setContextMenu(m_menu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit showWindowRequested();
    });

    m_trayIcon->show();
}

void SystemTray::buildMenu() {
    m_menu->clear();

    // Profile submenu
    if (!m_profileNames.isEmpty()) {
        auto* profileMenu = m_menu->addMenu("Profiles");
        for (int i = 0; i < m_profileNames.size(); ++i) {
            auto* action = profileMenu->addAction(m_profileNames[i]);
            action->setCheckable(true);
            action->setChecked(i == m_currentProfile);
            m_profileGroup->addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() {
                m_currentProfile = i;
                emit profileSelected(i);
            });
        }
        m_menu->addSeparator();
    }

    // Enable toggle
    m_enableAction = m_menu->addAction("Enabled");
    m_enableAction->setCheckable(true);
    m_enableAction->setChecked(true);
    connect(m_enableAction, &QAction::toggled, this, &SystemTray::enableToggled);

    // Monitor toggle
    m_monitorAction = m_menu->addAction("Monitor Audio");
    m_monitorAction->setCheckable(true);
    m_monitorAction->setChecked(false);
    connect(m_monitorAction, &QAction::toggled, this, &SystemTray::monitorToggled);

    m_menu->addSeparator();

    // Show window
    auto* showAction = m_menu->addAction("Open Window");
    connect(showAction, &QAction::triggered, this, &SystemTray::showWindowRequested);

    // Quit
    auto* quitAction = m_menu->addAction("Quit");
    connect(quitAction, &QAction::triggered, this, &SystemTray::quitRequested);
}

void SystemTray::setProfiles(const QStringList& names, int current) {
    m_profileNames = names;
    m_currentProfile = current;
    buildMenu();
}

void SystemTray::setEnabled(bool enabled) {
    m_enableAction->setChecked(enabled);
}

void SystemTray::setMonitorEnabled(bool enabled) {
    m_monitorAction->setChecked(enabled);
}
