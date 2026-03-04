#include "gui/VuMeter.h"
#include <QPainter>
#include <QLinearGradient>
#include <algorithm>

VuMeter::VuMeter(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(20);
    m_decayTimer.setInterval(30);
    connect(&m_decayTimer, &QTimer::timeout, this, [this]() {
        m_peak = std::max(0.f, m_peak - 0.02f);
        update();
    });
    m_decayTimer.start();
}

void VuMeter::setLevel(float level) {
    m_level = level;
    if (level > m_peak) m_peak = level;
    update();
}

void VuMeter::setLabel(const QString& label) {
    m_label = label;
}

void VuMeter::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(0, 0, -1, -1);

    // Background
    p.fillRect(r, QColor(30, 30, 30));

    // Level bar
    float clampedLevel = std::clamp(m_level, 0.f, 1.5f);
    int barWidth = static_cast<int>(r.width() * std::min(clampedLevel, 1.f));

    QLinearGradient grad(0, 0, r.width(), 0);
    grad.setColorAt(0.0, QColor(0, 200, 0));
    grad.setColorAt(0.7, QColor(200, 200, 0));
    grad.setColorAt(1.0, QColor(200, 0, 0));

    p.fillRect(r.x(), r.y(), barWidth, r.height(), grad);

    // Peak indicator
    float clampedPeak = std::clamp(m_peak, 0.f, 1.f);
    int peakX = static_cast<int>(r.width() * clampedPeak);
    p.setPen(QPen(Qt::white, 2));
    p.drawLine(peakX, r.y(), peakX, r.bottom());

    // Label
    if (!m_label.isEmpty()) {
        p.setPen(Qt::white);
        p.drawText(r.adjusted(4, 0, 0, 0), Qt::AlignVCenter, m_label);
    }

    // Border
    p.setPen(QColor(60, 60, 60));
    p.drawRect(r);
}
