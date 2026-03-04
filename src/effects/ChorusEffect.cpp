#include "effects/ChorusEffect.h"
#include "effects/EffectRegistry.h"

void ChorusEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);
    // 50ms max delay buffer
    m_bufferSize = static_cast<int>(sampleRate * 0.05f) + 1;
    m_buffer.resize(m_bufferSize, 0.f);
    m_writePos = 0;
    for (int i = 0; i < 6; ++i) m_phases[i] = static_cast<float>(i) / 6.f;
}

void ChorusEffect::process(float* data, int numFrames) {
    int voices = static_cast<int>(m_params[0].value.load(std::memory_order_relaxed));
    float rate = m_params[1].value.load(std::memory_order_relaxed);
    float depthMs = m_params[2].value.load(std::memory_order_relaxed);
    float mix = m_params[3].value.load(std::memory_order_relaxed);

    voices = std::clamp(voices, 2, 6);
    float depthSamples = depthMs * 0.001f * m_sampleRate;
    float phaseInc = rate / m_sampleRate;

    for (int i = 0; i < numFrames; ++i) {
        float dry = data[i];
        m_buffer[m_writePos] = dry;

        float wet = 0.f;
        for (int v = 0; v < voices; ++v) {
            float lfo = std::sin(2.f * static_cast<float>(M_PI) * m_phases[v]);
            float delaySamples = depthSamples * (0.5f + 0.5f * lfo);

            // Linear interpolation read
            float readPos = static_cast<float>(m_writePos) - delaySamples;
            if (readPos < 0.f) readPos += m_bufferSize;
            int idx0 = static_cast<int>(readPos);
            int idx1 = (idx0 + 1) % m_bufferSize;
            float frac = readPos - idx0;
            idx0 = idx0 % m_bufferSize;

            wet += m_buffer[idx0] * (1.f - frac) + m_buffer[idx1] * frac;
        }
        wet /= static_cast<float>(voices);

        data[i] = dry * (1.f - mix) + wet * mix;

        // Advance LFO phases
        for (int v = 0; v < voices; ++v) {
            m_phases[v] += phaseInc;
            if (m_phases[v] >= 1.f) m_phases[v] -= 1.f;
            // Initialize phase offsets on first sample
        }

        m_writePos = (m_writePos + 1) % m_bufferSize;
    }
}

void ChorusEffect::reset() {
    std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    m_writePos = 0;
    for (int i = 0; i < 6; ++i) {
        m_phases[i] = static_cast<float>(i) / 6.f; // Reset with phase offsets
    }
}

VML_REGISTER_EFFECT(ChorusEffect)
