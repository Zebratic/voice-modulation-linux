#pragma once
#include <QWidget>
#include <QWheelEvent>
#include <vector>

class WaveformWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void setLabel(const QString& label);

    // Push a block of new samples (called from GUI timer, copies from ring buffer)
    void pushSamples(const float* data, int count);

    // Set current peak level (0.0 - 1.0+) for background color
    void setLevel(float level);

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QSize sizeHint() const override { return {400, 80}; }
    QSize minimumSizeHint() const override { return {100, 40}; }

private:
    QColor levelColor(float level) const;

    std::vector<float> m_waveform;   // circular buffer of display samples
    int m_writePos = 0;
    int m_displaySamples = 512;      // how many samples to show
    float m_level = 0.f;
    float m_peak = 0.f;
    float m_zoom = 1.f;
    QString m_label;
};
