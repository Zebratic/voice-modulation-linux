#include "effects/PitchShiftEffect.h"
#include "effects/EffectRegistry.h"
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void PitchShiftEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);

    // Grain size ~20ms — good balance between latency and quality for voice
    m_grainSize = static_cast<int>(sampleRate * 0.020f);
    // Circular buffer: 4x grain size
    m_inBufSize = m_grainSize * 4;
    m_inBuf.resize(m_inBufSize, 0.f);
    m_writePos = 0;

    // Hann window
    m_window.resize(m_grainSize);
    for (int i = 0; i < m_grainSize; ++i)
        m_window[i] = 0.5f * (1.f - std::cos(2.0 * M_PI * i / (m_grainSize - 1)));

    // Initialize two read heads, offset by half a grain for smooth crossfade
    m_readPos0 = 0.0;
    m_readPos1 = m_grainSize / 2;
    m_grainCounter0 = 0;
    m_grainCounter1 = m_grainSize / 2;
}

void PitchShiftEffect::process(float* data, int numFrames) {
    float semitones = m_params[0].value.load(std::memory_order_relaxed);
    if (std::abs(semitones) < 0.01f) return;

    double shiftRatio = std::pow(2.0, semitones / 12.0);

    // Write incoming audio into circular buffer
    for (int i = 0; i < numFrames; ++i) {
        m_inBuf[m_writePos] = data[i];
        m_writePos = (m_writePos + 1) % m_inBufSize;
    }

    // Read back with two overlapping grains
    for (int i = 0; i < numFrames; ++i) {
        // Grain 0: read with interpolation
        int idx0a = static_cast<int>(m_readPos0) % m_inBufSize;
        if (idx0a < 0) idx0a += m_inBufSize;
        int idx0b = (idx0a + 1) % m_inBufSize;
        double frac0 = m_readPos0 - std::floor(m_readPos0);
        float sample0 = m_inBuf[idx0a] * (1.f - static_cast<float>(frac0))
                       + m_inBuf[idx0b] * static_cast<float>(frac0);
        float win0 = m_window[m_grainCounter0];

        // Grain 1: read with interpolation
        int idx1a = static_cast<int>(m_readPos1) % m_inBufSize;
        if (idx1a < 0) idx1a += m_inBufSize;
        int idx1b = (idx1a + 1) % m_inBufSize;
        double frac1 = m_readPos1 - std::floor(m_readPos1);
        float sample1 = m_inBuf[idx1a] * (1.f - static_cast<float>(frac1))
                       + m_inBuf[idx1b] * static_cast<float>(frac1);
        float win1 = m_window[m_grainCounter1];

        // Mix the two grains
        data[i] = sample0 * win0 + sample1 * win1;

        // Advance read positions at shifted rate
        m_readPos0 += shiftRatio;
        m_readPos1 += shiftRatio;

        // Keep read positions within buffer bounds
        if (m_readPos0 >= m_inBufSize) m_readPos0 -= m_inBufSize;
        if (m_readPos1 >= m_inBufSize) m_readPos1 -= m_inBufSize;

        // Advance grain counters; when a grain finishes, reset its read position
        // to align with the current write position (to stay near fresh data)
        m_grainCounter0++;
        if (m_grainCounter0 >= m_grainSize) {
            m_grainCounter0 = 0;
            // Reset to write position minus a small lookback
            m_readPos0 = (m_writePos - numFrames + i - m_grainSize + m_inBufSize) % m_inBufSize;
        }

        m_grainCounter1++;
        if (m_grainCounter1 >= m_grainSize) {
            m_grainCounter1 = 0;
            m_readPos1 = (m_writePos - numFrames + i - m_grainSize + m_inBufSize) % m_inBufSize;
        }
    }
}

void PitchShiftEffect::reset() {
    std::fill(m_inBuf.begin(), m_inBuf.end(), 0.f);
    m_writePos = 0;
    m_readPos0 = 0.0;
    m_readPos1 = m_grainSize / 2;
    m_grainCounter0 = 0;
    m_grainCounter1 = m_grainSize / 2;
}

VML_REGISTER_EFFECT(PitchShiftEffect)
