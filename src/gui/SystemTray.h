#pragma once
#include <QSystemTrayIcon>
#include <QMenu>
#include <QActionGroup>
#include <functional>

class SystemTray : public QObject {
    Q_OBJECT
public:
    explicit SystemTray(QObject* parent = nullptr);

    void setProfiles(const QStringList& names, int current = -1);
    void setEnabled(bool enabled);
    void setMonitorEnabled(bool enabled);

    std::function<void(int)> onProfileSelected;
    std::function<void(bool)> onEnableToggled;
    std::function<void(bool)> onMonitorToggled;
    std::function<void()> onShowWindow;
    std::function<void()> onQuit;

signals:
    void profileSelected(int index);
    void enableToggled(bool enabled);
    void monitorToggled(bool enabled);
    void showWindowRequested();
    void quitRequested();

private:
    void buildMenu();

    QSystemTrayIcon* m_trayIcon;
    QMenu* m_menu;
    QActionGroup* m_profileGroup;
    QAction* m_enableAction;
    QAction* m_monitorAction;
    QStringList m_profileNames;
    int m_currentProfile = -1;
};
