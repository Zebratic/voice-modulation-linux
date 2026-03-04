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

// Custom widget for keyframe curve editing
class KeyframeCurveWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyframeCurveWidget(QWidget* parent = nullptr);

    void setKeyframes(const std::vector<Keyframe>& keyframes);
    std::vector<Keyframe> keyframes() const { return m_keyframes; }

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

    std::vector<Keyframe> m_keyframes;
    int m_dragIndex = -1;
};
