#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimer>
#include <vector>
#include <string>
#include "modulation/Modulator.h"

class ModulationManager;
class EffectPipeline;

// A single lane in the timeline showing one parameter's keyframes
class TimelineLane : public QWidget {
    Q_OBJECT
public:
    TimelineLane(const QString& label, const QColor& color, QWidget* parent = nullptr);

    void setConfig(const ModulatorConfig& config);
    const ModulatorConfig& config() const { return m_config; }
    void setPlayheadPosition(float normalizedPos);

    QString paramLabel() const { return m_label; }

signals:
    void keyframesChanged(const std::string& effectId, const std::string& paramId,
                          const std::vector<Keyframe>& keyframes);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    QSize sizeHint() const override { return {600, 60}; }

private:
    QPointF keyframeToWidget(const Keyframe& kf) const;
    Keyframe widgetToKeyframe(const QPointF& p) const;
    int hitTest(const QPointF& pos) const;

    QString m_label;
    QColor m_color;
    ModulatorConfig m_config;
    int m_dragIndex = -1;
    float m_playhead = 0.f;
};

// The full timeline widget showing all keyframed parameters
class KeyframeTimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit KeyframeTimelineWidget(QWidget* parent = nullptr);

    void setModulationManager(ModulationManager* mgr) { m_modulationMgr = mgr; }
    void setPipeline(EffectPipeline* pipeline) { m_pipeline = pipeline; }
    void rebuild();

private:
    void updatePlayhead();
    void onLaneKeyframesChanged(const std::string& effectId, const std::string& paramId,
                                const std::vector<Keyframe>& keyframes);

    ModulationManager* m_modulationMgr = nullptr;
    EffectPipeline* m_pipeline = nullptr;
    QVBoxLayout* m_lanesLayout;
    QScrollArea* m_scrollArea;
    std::vector<TimelineLane*> m_lanes;
    QTimer m_playheadTimer;

    static const QColor s_laneColors[];
};
