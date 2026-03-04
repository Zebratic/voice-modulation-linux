#include "gui/SpectrumWidget.h"
#include <QPainter>
#include <QLinearGradient>
#include <cmath>
#include <algorithm>
#include <complex>

SpectrumWidget::SpectrumWidget(QWidget* parent) : QWidget(parent) {
    m_timeDomain.resize(FFT_SIZE, 0.f);
    // Precompute Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        m_window[i] = 0.5f * (1.f - std::cos(2.f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
    setMinimumHeight(60);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SpectrumWidget::pushSamples(const float* data, int count) {
    for (int i = 0; i < count; ++i) {
        m_timeDomain[m_writePos] = data[i];
        m_writePos = (m_writePos + 1) % FFT_SIZE;
    }
    m_needsUpdate = true;
    computeFFT();
    update();
}

// Radix-2 in-place FFT
static void fft(std::complex<float>* x, int n) {
    // Bit-reversal permutation
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(x[i], x[j]);
    }
    // Butterfly
    for (int len = 2; len <= n; len <<= 1) {
        float angle = -2.f * static_cast<float>(M_PI) / len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (int i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (int j = 0; j < len / 2; ++j) {
                auto u = x[i + j];
                auto v = x[i + j + len / 2] * w;
                x[i + j] = u + v;
                x[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void SpectrumWidget::computeFFT() {
    std::complex<float> buf[FFT_SIZE];

    // Copy with Hann window, reading from oldest to newest
    for (int i = 0; i < FFT_SIZE; ++i) {
        int idx = (m_writePos + i) % FFT_SIZE;
        buf[i] = std::complex<float>(m_timeDomain[idx] * m_window[i], 0.f);
    }

    fft(buf, FFT_SIZE);

    // Magnitude to dB with exponential smoothing
    for (int i = 0; i < NUM_BINS; ++i) {
        float mag = std::abs(buf[i]) / FFT_SIZE;
        float db = (mag > 1e-7f) ? 20.f * std::log10(mag) : -140.f;
        db = std::clamp(db, -90.f, 0.f);
        // Normalize to 0-1 range (-90dB = 0, 0dB = 1)
        float norm = (db + 90.f) / 90.f;
        m_magnitudes[i] = m_magnitudes[i] * 0.7f + norm * 0.3f;
    }
}

void SpectrumWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    int w = width();
    int h = height();

    // Dark background
    p.fillRect(rect(), QColor(18, 18, 28));

    // Draw spectrum as bars with log-frequency X axis
    // Map bins to pixels using log scale
    // Bin i corresponds to frequency i * sampleRate / FFT_SIZE
    // We'll assume 48000 Hz sample rate for display purposes
    float minFreq = 40.f;
    float maxFreq = 20000.f;
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);

    // Gradient for bars
    QLinearGradient grad(0, h, 0, 0);
    grad.setColorAt(0.0, QColor(30, 80, 150));
    grad.setColorAt(0.5, QColor(50, 180, 100));
    grad.setColorAt(0.9, QColor(220, 200, 50));
    grad.setColorAt(1.0, QColor(220, 50, 50));
    p.setBrush(grad);
    p.setPen(Qt::NoPen);

    float sampleRate = 48000.f;
    for (int px = 0; px < w; ++px) {
        // Map pixel to frequency (log scale)
        float logFreq = logMin + (logMax - logMin) * px / w;
        float freq = std::pow(10.f, logFreq);
        int bin = static_cast<int>(freq * FFT_SIZE / sampleRate);
        bin = std::clamp(bin, 0, NUM_BINS - 1);

        float mag = m_magnitudes[bin];
        int barH = static_cast<int>(mag * (h - 4));
        if (barH > 0) {
            p.drawRect(px, h - barH, 1, barH);
        }
    }

    // Grid lines at common frequencies
    p.setPen(QPen(QColor(60, 60, 80), 1));
    float gridFreqs[] = {100, 200, 500, 1000, 2000, 5000, 10000};
    for (float gf : gridFreqs) {
        float logF = std::log10(gf);
        int gx = static_cast<int>((logF - logMin) / (logMax - logMin) * w);
        if (gx > 0 && gx < w) {
            p.drawLine(gx, 0, gx, h);
        }
    }

    // Frequency labels
    p.setPen(QColor(120, 120, 140));
    QFont f = font();
    f.setPixelSize(9);
    p.setFont(f);
    const char* labels[] = {"100", "1k", "10k"};
    float labelFreqs[] = {100, 1000, 10000};
    for (int i = 0; i < 3; ++i) {
        float logF = std::log10(labelFreqs[i]);
        int lx = static_cast<int>((logF - logMin) / (logMax - logMin) * w);
        p.drawText(lx + 2, h - 3, labels[i]);
    }

    // Border
    p.setPen(QColor(40, 40, 55));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}
