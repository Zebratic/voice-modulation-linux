#pragma once
#include <QDialog>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include "modulation/Modulator.h"

class KeyframeCurveWidget;

class ModulatorEditorDialog : public QDialog {
    Q_OBJECT
public:
    ModulatorEditorDialog(const QString& paramName, float paramMin, float paramMax,
        const ModulatorConfig* existing = nullptr, QWidget* parent = nullptr);

    ModulatorConfig result() const;
    bool removeRequested() const { return m_removeRequested; }

private:
    void onCurveChanged(int index);

    QDoubleSpinBox* m_durationSpin;
    QDoubleSpinBox* m_startSpin;
    QDoubleSpinBox* m_endSpin;
    QComboBox* m_curveCombo;
    QCheckBox* m_activeCheck;
    KeyframeCurveWidget* m_keyframeWidget;
    bool m_removeRequested = false;
    float m_paramMin;
    float m_paramMax;
};

// Custom widget for keyframe curve editing and curve preview
class KeyframeCurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyframeCurveWidget(QWidget* parent = nullptr);

    void setKeyframes(const std::vector<Keyframe>& keyframes);
    std::vector<Keyframe> keyframes() const { return m_keyframes; }

    // Set curve mode — for non-Keyframe modes, renders a read-only preview
    void setCurveMode(ModCurve mode);
    ModCurve curveMode() const { return m_curveMode; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return {300, 150}; }

private:
    QPointF keyframeToWidget(const Keyframe& kf) const;
    Keyframe widgetToKeyframe(const QPointF& p) const;
    int hitTest(const QPointF& pos) const;
    float evaluateCurve(float t) const;
    void drawCurvePreview(QPainter& p, float margin, float w, float h);

    std::vector<Keyframe> m_keyframes;
    int m_dragIndex = -1;
    ModCurve m_curveMode = ModCurve::Keyframe;
};
