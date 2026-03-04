#pragma once
#include <QWidget>
#include <vector>
#include <cmath>

class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);

    void pushSamples(const float* data, int count);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return {400, 100}; }
    QSize minimumSizeHint() const override { return {200, 60}; }

private:
    void computeFFT();

    static constexpr int FFT_SIZE = 1024;
    static constexpr int NUM_BINS = FFT_SIZE / 2;

    std::vector<float> m_timeDomain;  // circular buffer
    int m_writePos = 0;
    float m_magnitudes[NUM_BINS] = {};    // smoothed magnitude in dB
    float m_window[FFT_SIZE] = {};        // Hann window
    bool m_needsUpdate = false;
};
