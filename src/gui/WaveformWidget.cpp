#include "gui/WaveformWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    m_waveform.resize(m_displaySamples, 0.f);
    setMinimumHeight(40);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void WaveformWidget::setLabel(const QString& label) {
    m_label = label;
}

void WaveformWidget::pushSamples(const float* data, int count) {
    for (int i = 0; i < count; ++i) {
        m_waveform[m_writePos] = data[i];
        m_writePos = (m_writePos + 1) % m_displaySamples;
    }
    update();
}

void WaveformWidget::setLevel(float level) {
    m_level = level;
    if (level > m_peak) m_peak = level;
    // Decay peak
    m_peak = std::max(0.f, m_peak - 0.005f);
}

QColor WaveformWidget::levelColor(float level) const {
    float t = std::clamp(level, 0.f, 1.f);
    // Green at 0, yellow at 0.6, red at 1.0
    int r, g;
    if (t < 0.6f) {
        float s = t / 0.6f;
        r = static_cast<int>(s * 200);
        g = 180;
    } else {
        float s = (t - 0.6f) / 0.4f;
        r = 200;
        g = static_cast<int>(180 * (1.f - s));
    }
    return QColor(r, g, 0);
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    float midY = h * 0.5f;

    // Dark background
    p.fillRect(rect(), QColor(18, 18, 28));

    // Volume-colored background glow (fills from bottom proportional to level)
    float lvl = std::clamp(m_level, 0.f, 1.f);
    if (lvl > 0.01f) {
        QColor col = levelColor(lvl);
        col.setAlpha(static_cast<int>(40 + 50 * lvl));
        int glowH = static_cast<int>(h * lvl);
        QLinearGradient grad(0, h - glowH, 0, h);
        QColor top = col;
        top.setAlpha(0);
        grad.setColorAt(0.0, top);
        grad.setColorAt(1.0, col);
        p.fillRect(0, h - glowH, w, glowH, grad);
    }

    // Center line
    p.setPen(QPen(QColor(50, 50, 65), 1));
    p.drawLine(0, static_cast<int>(midY), w, static_cast<int>(midY));

    // Draw waveform
    if (m_displaySamples < 2) return;

    int effectiveSamples = std::max(2, static_cast<int>(m_displaySamples / m_zoom));

    QColor waveColor = levelColor(std::clamp(m_peak, 0.f, 1.f));
    waveColor.setAlpha(220);
    p.setPen(Qt::NoPen);

    // We read from the most recent effectiveSamples
    float xStep = static_cast<float>(w) / static_cast<float>(effectiveSamples);
    int startOffset = m_displaySamples - effectiveSamples;

    // Draw filled waveform (mirrored around center)
    QPainterPath pathTop;
    QPainterPath pathBot;
    bool topStarted = false;
    bool botStarted = false;

    for (int i = 0; i < effectiveSamples; ++i) {
        int idx = (m_writePos + startOffset + i) % m_displaySamples;
        float sample = std::clamp(m_waveform[idx], -1.f, 1.f);
        float x = i * xStep;
        float yOff = std::abs(sample) * (midY - 2.f);

        if (!topStarted) {
            pathTop.moveTo(x, midY - yOff);
            pathBot.moveTo(x, midY + yOff);
            topStarted = botStarted = true;
        } else {
            pathTop.lineTo(x, midY - yOff);
            pathBot.lineTo(x, midY + yOff);
        }
    }

    // Close the paths back to center to fill
    pathTop.lineTo(w, midY);
    pathTop.lineTo(0, midY);
    pathTop.closeSubpath();

    pathBot.lineTo(w, midY);
    pathBot.lineTo(0, midY);
    pathBot.closeSubpath();

    // Gradient fill for waveform: brighter toward center, fading out
    QLinearGradient waveGrad(0, 0, 0, h);
    QColor bright = waveColor;
    bright.setAlpha(200);
    QColor dim = waveColor;
    dim.setAlpha(60);
    waveGrad.setColorAt(0.0, dim);
    waveGrad.setColorAt(0.45, bright);
    waveGrad.setColorAt(0.55, bright);
    waveGrad.setColorAt(1.0, dim);

    p.setBrush(waveGrad);
    p.drawPath(pathTop);
    p.drawPath(pathBot);

    // Waveform edge lines
    QPen edgePen(waveColor, 1.2);
    p.setPen(edgePen);
    p.setBrush(Qt::NoBrush);

    QPainterPath edgeTop, edgeBot;
    for (int i = 0; i < effectiveSamples; ++i) {
        int idx = (m_writePos + startOffset + i) % m_displaySamples;
        float sample = std::clamp(m_waveform[idx], -1.f, 1.f);
        float x = i * xStep;
        float yOff = std::abs(sample) * (midY - 2.f);

        if (i == 0) {
            edgeTop.moveTo(x, midY - yOff);
            edgeBot.moveTo(x, midY + yOff);
        } else {
            edgeTop.lineTo(x, midY - yOff);
            edgeBot.lineTo(x, midY + yOff);
        }
    }
    p.drawPath(edgeTop);
    p.drawPath(edgeBot);

    // Label
    if (!m_label.isEmpty()) {
        p.setPen(QColor(180, 180, 180, 180));
        QFont f = font();
        f.setPixelSize(11);
        f.setBold(true);
        p.setFont(f);
        p.drawText(rect().adjusted(6, 4, 0, 0), Qt::AlignTop | Qt::AlignLeft, m_label);
    }

    // Border
    p.setPen(QColor(40, 40, 55));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void WaveformWidget::wheelEvent(QWheelEvent* event) {
    if (event->angleDelta().y() > 0)
        m_zoom *= 1.2f;
    else
        m_zoom *= 0.83f;
    m_zoom = std::clamp(m_zoom, 0.25f, 8.f);
    update();
    event->accept();
}
