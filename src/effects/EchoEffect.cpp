#include "effects/EchoEffect.h"
#include "effects/EffectRegistry.h"
#include <algorithm>
#include <cmath>

void EchoEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);
    m_maxDelaySamples = static_cast<int>(sampleRate); // 1 second max
    m_buffer.resize(m_maxDelaySamples, 0.f);
    m_writePos = 0;
}

void EchoEffect::process(float* data, int numFrames) {
    float delayMs = m_params[0].value.load(std::memory_order_relaxed);
    float feedback = m_params[1].value.load(std::memory_order_relaxed);
    float mix = m_params[2].value.load(std::memory_order_relaxed);

    int delaySamples = std::clamp(static_cast<int>(m_sampleRate * delayMs * 0.001f), 1, m_maxDelaySamples - 1);

    for (int i = 0; i < numFrames; ++i) {
        int readPos = m_writePos - delaySamples;
        if (readPos < 0) readPos += m_maxDelaySamples;

        float delayed = m_buffer[readPos];
        float input = data[i];
        m_buffer[m_writePos] = input + delayed * feedback;
        data[i] = input * (1.f - mix) + delayed * mix;

        m_writePos = (m_writePos + 1) % m_maxDelaySamples;
    }
}

void EchoEffect::reset() {
    std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
    m_writePos = 0;
}

VML_REGISTER_EFFECT(EchoEffect)
