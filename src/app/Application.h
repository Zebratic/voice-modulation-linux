#pragma once
#include <QApplication>
#include <memory>

class AudioEngine;
class ProfileManager;
class MainWindow;

class Application : public QApplication {
    Q_OBJECT
public:
    Application(int& argc, char** argv);
    ~Application();

    int run();

private:
    void parseArguments();

    std::unique_ptr<AudioEngine> m_engine;
    std::unique_ptr<ProfileManager> m_profileManager;
    std::unique_ptr<MainWindow> m_mainWindow;
};
