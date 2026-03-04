#include "gui/ModulatorEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>

// ---------- ModulatorEditorDialog ----------

ModulatorEditorDialog::ModulatorEditorDialog(const QString& paramName, float paramMin, float paramMax,
    const ModulatorConfig* existing, QWidget* parent)
    : QDialog(parent), m_paramMin(paramMin), m_paramMax(paramMax) {
    setWindowTitle("Modulator: " + paramName);
    setMinimumWidth(350);

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();

    m_activeCheck = new QCheckBox("Active", this);
    m_activeCheck->setChecked(existing ? existing->active : true);
    form->addRow(m_activeCheck);

    m_durationSpin = new QDoubleSpinBox(this);
    m_durationSpin->setRange(0.1, 60.0);
    m_durationSpin->setSingleStep(0.1);
    m_durationSpin->setSuffix(" s");
    m_durationSpin->setValue(existing ? existing->durationSeconds : 2.0);
    form->addRow("Duration:", m_durationSpin);

    m_startSpin = new QDoubleSpinBox(this);
    m_startSpin->setRange(paramMin, paramMax);
    m_startSpin->setSingleStep((paramMax - paramMin) / 100.0);
    m_startSpin->setDecimals(3);
    m_startSpin->setValue(existing ? existing->startValue : paramMin);
    form->addRow("Start Value:", m_startSpin);

    m_endSpin = new QDoubleSpinBox(this);
    m_endSpin->setRange(paramMin, paramMax);
    m_endSpin->setSingleStep((paramMax - paramMin) / 100.0);
    m_endSpin->setDecimals(3);
    m_endSpin->setValue(existing ? existing->endValue : paramMax);
    form->addRow("End Value:", m_endSpin);

    m_curveCombo = new QComboBox(this);
    m_curveCombo->addItem("Linear", static_cast<int>(ModCurve::Linear));
    m_curveCombo->addItem("Ease In/Out", static_cast<int>(ModCurve::EaseInOut));
    m_curveCombo->addItem("Rubberband", static_cast<int>(ModCurve::Rubberband));
    m_curveCombo->addItem("Keyframe", static_cast<int>(ModCurve::Keyframe));
    if (existing)
        m_curveCombo->setCurrentIndex(static_cast<int>(existing->curve));
    form->addRow("Curve:", m_curveCombo);

    layout->addLayout(form);

    // Curve preview / keyframe editor (always visible)
    m_keyframeWidget = new KeyframeCurveWidget(this);
    if (existing && existing->curve == ModCurve::Keyframe)
        m_keyframeWidget->setKeyframes(existing->keyframes);
    else
        m_keyframeWidget->setKeyframes({{0.f, 0.f}, {0.5f, 1.f}, {1.f, 0.f}});
    ModCurve initialCurve = existing ? existing->curve : ModCurve::Linear;
    m_keyframeWidget->setCurveMode(initialCurve);
    layout->addWidget(m_keyframeWidget);

    connect(m_curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ModulatorEditorDialog::onCurveChanged);

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    if (existing) {
        auto* removeBtn = new QPushButton("Remove", this);
        removeBtn->setStyleSheet("color: #ff6666;");
        connect(removeBtn, &QPushButton::clicked, this, [this]() {
            m_removeRequested = true;
            accept();
        });
        btnLayout->addWidget(removeBtn);
    }
    btnLayout->addStretch();
    auto* cancelBtn = new QPushButton("Cancel", this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    auto* okBtn = new QPushButton(existing ? "Update" : "Create", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    layout->addLayout(btnLayout);
}

ModulatorConfig ModulatorEditorDialog::result() const {
    ModulatorConfig cfg;
    cfg.startValue = static_cast<float>(m_startSpin->value());
    cfg.endValue = static_cast<float>(m_endSpin->value());
    cfg.durationSeconds = static_cast<float>(m_durationSpin->value());
    cfg.curve = static_cast<ModCurve>(m_curveCombo->currentData().toInt());
    cfg.active = m_activeCheck->isChecked();
    if (cfg.curve == ModCurve::Keyframe)
        cfg.keyframes = m_keyframeWidget->keyframes();
    return cfg;
}

void ModulatorEditorDialog::onCurveChanged(int) {
    m_keyframeWidget->setCurveMode(static_cast<ModCurve>(m_curveCombo->currentData().toInt()));
    m_keyframeWidget->update();
}

// ---------- KeyframeCurveWidget ----------

KeyframeCurveWidget::KeyframeCurveWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(300, 150);
    setMouseTracking(true);
}

void KeyframeCurveWidget::setCurveMode(ModCurve mode) {
    m_curveMode = mode;
    update();
}

float KeyframeCurveWidget::evaluateCurve(float t) const {
    t = std::clamp(t, 0.f, 1.f);
    switch (m_curveMode) {
    case ModCurve::Linear: return t;
    case ModCurve::EaseInOut: return t * t * (3.f - 2.f * t);
    case ModCurve::Rubberband: return 1.f - std::pow(2.f, -8.f * t);
    default: return t;
    }
}

void KeyframeCurveWidget::drawCurvePreview(QPainter& p, float margin, float w, float h) {
    // Draw the curve shape with ping-pong (0->1->0) to show full cycle
    p.setPen(QPen(QColor(100, 200, 255), 2));
    constexpr int steps = 100;
    QPointF prev;
    for (int i = 0; i <= steps; ++i) {
        float phase = static_cast<float>(i) / steps * 2.f; // 0 to 2 (full ping-pong)
        float t = phase <= 1.f ? phase : 2.f - phase;
        float val = evaluateCurve(t);
        QPointF pt(margin + (static_cast<float>(i) / steps) * w,
                   margin + (1.f - val) * h);
        if (i > 0) p.drawLine(prev, pt);
        prev = pt;
    }
}

void KeyframeCurveWidget::setKeyframes(const std::vector<Keyframe>& keyframes) {
    m_keyframes = keyframes;
    std::sort(m_keyframes.begin(), m_keyframes.end(),
        [](const Keyframe& a, const Keyframe& b) { return a.position < b.position; });
    update();
}

QPointF KeyframeCurveWidget::keyframeToWidget(const Keyframe& kf) const {
    float margin = 10.f;
    float w = width() - 2 * margin;
    float h = height() - 2 * margin;
    return {margin + kf.position * w, margin + (1.f - kf.value) * h};
}

Keyframe KeyframeCurveWidget::widgetToKeyframe(const QPointF& p) const {
    float margin = 10.f;
    float w = width() - 2 * margin;
    float h = height() - 2 * margin;
    float pos = std::clamp(static_cast<float>((p.x() - margin) / w), 0.f, 1.f);
    float val = std::clamp(1.f - static_cast<float>((p.y() - margin) / h), 0.f, 1.f);
    return {pos, val};
}

int KeyframeCurveWidget::hitTest(const QPointF& pos) const {
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        QPointF kp = keyframeToWidget(m_keyframes[i]);
        float dx = static_cast<float>(pos.x() - kp.x());
        float dy = static_cast<float>(pos.y() - kp.y());
        if (dx * dx + dy * dy < 64.f) // 8px radius
            return static_cast<int>(i);
    }
    return -1;
}

void KeyframeCurveWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background
    p.fillRect(rect(), QColor(30, 30, 40));

    // Grid
    p.setPen(QPen(QColor(60, 60, 70), 1));
    float margin = 10.f;
    float w = width() - 2 * margin;
    float h = height() - 2 * margin;
    for (int i = 0; i <= 4; ++i) {
        float x = margin + w * i / 4.f;
        float y = margin + h * i / 4.f;
        p.drawLine(QPointF(x, margin), QPointF(x, margin + h));
        p.drawLine(QPointF(margin, y), QPointF(margin + w, y));
    }

    // Non-keyframe modes: draw read-only curve preview
    if (m_curveMode != ModCurve::Keyframe) {
        drawCurvePreview(p, margin, w, h);
        return;
    }

    // Keyframe mode: interactive curve with draggable points
    if (m_keyframes.empty()) return;

    // Curve
    p.setPen(QPen(QColor(100, 200, 255), 2));
    for (size_t i = 0; i + 1 < m_keyframes.size(); ++i) {
        QPointF a = keyframeToWidget(m_keyframes[i]);
        QPointF b = keyframeToWidget(m_keyframes[i + 1]);
        p.drawLine(a, b);
    }

    // Points
    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        QPointF pt = keyframeToWidget(m_keyframes[i]);
        p.setPen(Qt::NoPen);
        p.setBrush(static_cast<int>(i) == m_dragIndex ? QColor(255, 200, 50) : QColor(255, 100, 100));
        p.drawEllipse(pt, 5, 5);
    }
}

void KeyframeCurveWidget::mousePressEvent(QMouseEvent* event) {
    if (m_curveMode != ModCurve::Keyframe) return;
    m_dragIndex = hitTest(event->position());
    update();
}

void KeyframeCurveWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_curveMode != ModCurve::Keyframe || m_dragIndex < 0) return;
    Keyframe kf = widgetToKeyframe(event->position());
    // Don't allow dragging past neighbors
    if (m_dragIndex > 0)
        kf.position = std::max(kf.position, m_keyframes[m_dragIndex - 1].position + 0.01f);
    if (m_dragIndex < static_cast<int>(m_keyframes.size()) - 1)
        kf.position = std::min(kf.position, m_keyframes[m_dragIndex + 1].position - 0.01f);
    m_keyframes[m_dragIndex] = kf;
    update();
}

void KeyframeCurveWidget::mouseReleaseEvent(QMouseEvent*) {
    m_dragIndex = -1;
    update();
}

void KeyframeCurveWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (m_curveMode != ModCurve::Keyframe) return;
    int hit = hitTest(event->position());
    if (hit >= 0) {
        // Remove point (keep at least 2)
        if (m_keyframes.size() > 2) {
            m_keyframes.erase(m_keyframes.begin() + hit);
            update();
        }
    } else {
        // Add point
        Keyframe kf = widgetToKeyframe(event->position());
        m_keyframes.push_back(kf);
        std::sort(m_keyframes.begin(), m_keyframes.end(),
            [](const Keyframe& a, const Keyframe& b) { return a.position < b.position; });
        update();
    }
}
