#include "effects/FlangerEffect.h"
#include "effects/EffectRegistry.h"

void FlangerEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);
    // 15ms max delay
    m_bufferSize = static_cast<int>(sampleRate * 0.015f) + 1;
    m_buffer.resize(m_bufferSize, 0.f);
    m_writePos = 0;
    m_phase = 0.f;
    m_feedbackSample = 0.f;
}

void FlangerEffect::process(float* data, int numFrames) {
    float rate = m_params[0].value.load(std::memory_order_relaxed);
    float depthMs = m_params[1].value.load(std::memory_order_relaxed);
    float feedback = m_params[2].value.load(std::memory_order_relaxed);
    float mix = m_params[3].value.load(std::memory_order_relaxed);

    float depthSamples = depthMs * 0.001f * m_sampleRate;
    float phaseInc = rate / m_sampleRate;

    for (int i = 0; i < numFrames; ++i) {
        float dry = data[i];
        m_buffer[m_writePos] = dry + m_feedbackSample * feedback;

        float lfo = std::sin(2.f * static_cast<float>(M_PI) * m_phase);
        float delaySamples = depthSamples * (0.5f + 0.5f * lfo);
        delaySamples = std::max(delaySamples, 0.5f);

        float readPos = static_cast<float>(m_writePos) - delaySamples;
        if (readPos < 0.f) readPos += m_bufferSize;
        int idx0 = static_cast<int>(readPos) % m_bufferSize;
        int idx1 = (idx0 + 1) % m_bufferSize;
        float frac = readPos - std::floor(readPos);

        float delayed = m_buffer[idx0] * (1.f - frac) + m_buffer[idx1] * frac;
        m_feedbackSample = delayed;

        data[i] = dry * (1.f - mix) + delayed * mix;

        m_phase += phaseInc;
        if (m_phase >= 1.f) m_phase -= 1.f;
        m_writePos = (m_writePos + 1) % m_bufferSize;
    }
}

void FlangerEffect::reset() {
    std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    m_writePos = 0;
    m_phase = 0.f;
    m_feedbackSample = 0.f;
}

VML_REGISTER_EFFECT(FlangerEffect)
