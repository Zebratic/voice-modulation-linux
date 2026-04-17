#pragma once
#include <QDebug>
#include <QString>
#include <QElapsedTimer>
#include <iostream>

// Global verbose flag - set by argument parsing
extern bool g_verboseEnabled;

// Get elapsed time in milliseconds since app start
inline qint64 vmlElapsedMs() {
    static QElapsedTimer timer;
    static bool started = false;
    if (!started) {
        timer.start();
        started = true;
    }
    return timer.elapsed();
}

// Helper function to log a message
inline void vmlLog(const char* level, const QString& msg) {
    if (g_verboseEnabled) {
        std::cerr << "[VML " << vmlElapsedMs() << "ms] " << level << " " << msg.toStdString() << std::endl;
        std::cerr.flush();
    }
}

// Simple macro for single string messages
#define VML_LOG(LEVEL, MSG) \
    do { if (g_verboseEnabled) { vmlLog(LEVEL, MSG); } } while(0)

#define VML_DEBUG(MSG) VML_LOG("DEBUG", MSG)
#define VML_INFO(MSG) VML_LOG("INFO", MSG)
#define VML_WARNING(MSG) VML_LOG("WARNING", MSG)
