#include "gui/KeyframeTimelineWidget.h"
#include "modulation/ModulationManager.h"
#include "audio/EffectPipeline.h"
#include "effects/EffectBase.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLabel>
#include <QGroupBox>
#include <algorithm>
#include <cmath>

const QColor KeyframeTimelineWidget::s_laneColors[] = {
    QColor(100, 200, 255),  // cyan
    QColor(255, 150, 80),   // orange
    QColor(120, 255, 120),  // green
    QColor(255, 100, 150),  // pink
    QColor(200, 150, 255),  // purple
    QColor(255, 255, 100),  // yellow
    QColor(100, 255, 220),  // teal
    QColor(255, 130, 130),  // red
};

// ---------- TimelineLane ----------

TimelineLane::TimelineLane(const QString& label, const QColor& color, QWidget* parent)
    : QWidget(parent), m_label(label), m_color(color) {
    setMinimumHeight(60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void TimelineLane::setConfig(const ModulatorConfig& config) {
    m_config = config;
    update();
}

void TimelineLane::setPlayheadPosition(float pos) {
    m_playhead = pos;
    update();
}

QPointF TimelineLane::keyframeToWidget(const Keyframe& kf) const {
    float margin = 8.f;
    float labelW = 0.f; // label drawn separately
    float w = width() - 2 * margin - labelW;
    float h = height() - 2 * margin;
    return {margin + kf.position * w, margin + (1.f - kf.value) * h};
}

Keyframe TimelineLane::widgetToKeyframe(const QPointF& p) const {
    float margin = 8.f;
    float w = width() - 2 * margin;
    float h = height() - 2 * margin;
    float pos = std::clamp(static_cast<float>((p.x() - margin) / w), 0.f, 1.f);
    float val = std::clamp(1.f - static_cast<float>((p.y() - margin) / h), 0.f, 1.f);
    return {pos, val};
}

int TimelineLane::hitTest(const QPointF& pos) const {
    for (size_t i = 0; i < m_config.keyframes.size(); ++i) {
        QPointF kp = keyframeToWidget(m_config.keyframes[i]);
        float dx = static_cast<float>(pos.x() - kp.x());
        float dy = static_cast<float>(pos.y() - kp.y());
        if (dx * dx + dy * dy < 64.f)
            return static_cast<int>(i);
    }
    return -1;
}

// Evaluate curve value at normalized time t (0-1), matching Modulator::evaluateCurve
static float evaluateCurveForDisplay(const ModulatorConfig& cfg, float t) {
    t = std::clamp(t, 0.f, 1.f);
    switch (cfg.curve) {
    case ModCurve::Linear:
        return t;
    case ModCurve::EaseInOut:
        return t * t * (3.f - 2.f * t);
    case ModCurve::Rubberband:
        return 1.f - std::pow(2.f, -8.f * t);
    case ModCurve::Keyframe: {
        const auto& kfs = cfg.keyframes;
        if (kfs.empty()) return t;
        if (kfs.size() == 1) return kfs[0].value;
        if (t <= kfs.front().position) return kfs.front().value;
        if (t >= kfs.back().position) return kfs.back().value;
        for (size_t i = 0; i + 1 < kfs.size(); ++i) {
            if (t >= kfs[i].position && t <= kfs[i + 1].position) {
                float segLen = kfs[i + 1].position - kfs[i].position;
                if (segLen <= 0.f) return kfs[i].value;
                float segT = (t - kfs[i].position) / segLen;
                return kfs[i].value + (kfs[i + 1].value - kfs[i].value) * segT;
            }
        }
        return kfs.back().value;
    }
    }
    return t;
}

void TimelineLane::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    float margin = 8.f;
    float w = width() - 2 * margin;
    float h = height() - 2 * margin;

    // Background
    p.fillRect(rect(), QColor(25, 25, 35));

    // Subtle grid lines
    p.setPen(QPen(QColor(50, 50, 60), 1));
    for (int i = 0; i <= 10; ++i) {
        float x = margin + w * i / 10.f;
        p.drawLine(QPointF(x, margin), QPointF(x, margin + h));
    }
    // Horizontal center line
    p.drawLine(QPointF(margin, margin + h / 2), QPointF(margin + w, margin + h / 2));

    // Border
    p.setPen(QPen(QColor(60, 60, 80), 1));
    p.drawRect(QRectF(margin - 1, margin - 1, w + 2, h + 2));

    // Draw curve by sampling
    p.setPen(QPen(m_color, 2));
    int steps = std::max(static_cast<int>(w), 60);
    QPointF prev;
    for (int i = 0; i <= steps; ++i) {
        float t = static_cast<float>(i) / steps;
        float val = evaluateCurveForDisplay(m_config, t);
        QPointF pt(margin + t * w, margin + (1.f - val) * h);
        if (i > 0)
            p.drawLine(prev, pt);
        prev = pt;
    }

    // Draw keyframe points (only for keyframe curves)
    if (m_config.curve == ModCurve::Keyframe) {
        const auto& kfs = m_config.keyframes;
        for (size_t i = 0; i < kfs.size(); ++i) {
            QPointF pt = keyframeToWidget(kfs[i]);
            p.setPen(QPen(Qt::white, 1.5));
            p.setBrush(static_cast<int>(i) == m_dragIndex ? QColor(255, 200, 50) : m_color);
            p.drawEllipse(pt, 5, 5);
        }
    }

    // Playhead
    if (m_playhead >= 0.f && m_playhead <= 1.f) {
        float px = margin + m_playhead * w;
        p.setPen(QPen(QColor(255, 255, 255, 180), 1, Qt::DashLine));
        p.drawLine(QPointF(px, margin), QPointF(px, margin + h));
    }

    // Label
    p.setPen(QColor(200, 200, 200));
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);
    p.drawText(QRectF(margin + 4, margin + 2, w, 14), Qt::AlignLeft, m_label);
}

void TimelineLane::mousePressEvent(QMouseEvent* event) {
    if (m_config.curve != ModCurve::Keyframe) return;
    if (event->button() == Qt::LeftButton)
        m_dragIndex = hitTest(event->position());
    update();
}

void TimelineLane::mouseMoveEvent(QMouseEvent* event) {
    if (m_config.curve != ModCurve::Keyframe || m_dragIndex < 0) return;
    Keyframe kf = widgetToKeyframe(event->position());
    // Constrain between neighbors
    if (m_dragIndex > 0)
        kf.position = std::max(kf.position, m_config.keyframes[m_dragIndex - 1].position + 0.005f);
    if (m_dragIndex < static_cast<int>(m_config.keyframes.size()) - 1)
        kf.position = std::min(kf.position, m_config.keyframes[m_dragIndex + 1].position - 0.005f);
    m_config.keyframes[m_dragIndex] = kf;
    update();
}

void TimelineLane::mouseReleaseEvent(QMouseEvent*) {
    if (m_dragIndex >= 0) {
        m_dragIndex = -1;
        emit keyframesChanged(m_config.effectId, m_config.paramId, m_config.keyframes);
        update();
    }
}

void TimelineLane::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_config.curve != ModCurve::Keyframe) return;
    int hit = hitTest(event->position());
    if (hit >= 0) {
        if (m_config.keyframes.size() > 2) {
            m_config.keyframes.erase(m_config.keyframes.begin() + hit);
            emit keyframesChanged(m_config.effectId, m_config.paramId, m_config.keyframes);
            update();
        }
    } else {
        Keyframe kf = widgetToKeyframe(event->position());
        m_config.keyframes.push_back(kf);
        std::sort(m_config.keyframes.begin(), m_config.keyframes.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.position < b.position; });
        emit keyframesChanged(m_config.effectId, m_config.paramId, m_config.keyframes);
        update();
    }
}

// ---------- KeyframeTimelineWidget ----------

KeyframeTimelineWidget::KeyframeTimelineWidget(QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    auto* headerLabel = new QLabel("Keyframe Timeline", this);
    headerLabel->setStyleSheet("font-weight: bold; font-size: 12px; color: #aaa; padding: 2px;");
    mainLayout->addWidget(headerLabel);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");

    auto* container = new QWidget(m_scrollArea);
    m_lanesLayout = new QVBoxLayout(container);
    m_lanesLayout->setContentsMargins(0, 0, 0, 0);
    m_lanesLayout->setSpacing(2);
    m_scrollArea->setWidget(container);

    mainLayout->addWidget(m_scrollArea);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(80);

    m_playheadTimer.setInterval(33); // ~30 fps
    connect(&m_playheadTimer, &QTimer::timeout, this, &KeyframeTimelineWidget::updatePlayhead);
}

void KeyframeTimelineWidget::rebuild() {
    // Clear existing lanes
    for (auto* lane : m_lanes) {
        m_lanesLayout->removeWidget(lane);
        delete lane;
    }
    m_lanes.clear();

    if (!m_modulationMgr || !m_pipeline) {
        m_playheadTimer.stop();
        return;
    }

    auto configs = m_modulationMgr->snapshot();
    int colorIdx = 0;

    for (const auto& cfg : configs) {
        if (!cfg.active) continue;

        // Find the effect/param names for the label
        QString label;
        int count = m_pipeline->effectCount();
        for (int i = 0; i < count; ++i) {
            auto* effect = m_pipeline->effectAt(i);
            if (effect && effect->id() == cfg.effectId) {
                for (auto& param : effect->params()) {
                    if (param.id == cfg.paramId) {
                        label = QString::fromStdString(effect->name()) + " > " +
                                QString::fromStdString(param.name);
                        break;
                    }
                }
                break;
            }
        }
        if (label.isEmpty())
            label = QString::fromStdString(cfg.effectId + "." + cfg.paramId);

        QColor color = s_laneColors[colorIdx % 8];
        colorIdx++;

        auto* lane = new TimelineLane(label, color, m_scrollArea->widget());
        lane->setConfig(cfg);
        m_lanesLayout->addWidget(lane);
        m_lanes.push_back(lane);

        connect(lane, &TimelineLane::keyframesChanged,
                this, &KeyframeTimelineWidget::onLaneKeyframesChanged);
    }

    if (!m_lanes.empty())
        m_playheadTimer.start();
    else
        m_playheadTimer.stop();
}

void KeyframeTimelineWidget::updatePlayhead() {
    if (!m_modulationMgr) return;

    for (auto* lane : m_lanes) {
        const auto& cfg = lane->config();
        float phase = m_modulationMgr->currentPhase(cfg.effectId, cfg.paramId);
        lane->setPlayheadPosition(std::clamp(phase, 0.f, 1.f));
    }
}

void KeyframeTimelineWidget::onLaneKeyframesChanged(
    const std::string& effectId, const std::string& paramId,
    const std::vector<Keyframe>& keyframes) {
    if (!m_modulationMgr) return;

    // Update the modulator config with new keyframes
    auto* cfg = m_modulationMgr->findConfig(effectId, paramId);
    if (cfg) {
        cfg->keyframes = keyframes;
    }
}
