#include "effects/NoiseGateEffect.h"
#include "effects/EffectRegistry.h"
#include <cmath>

void NoiseGateEffect::process(float* data, int numFrames) {
    float threshDb = m_params[0].value.load(std::memory_order_relaxed);
    float attackMs = m_params[1].value.load(std::memory_order_relaxed);
    float releaseMs = m_params[2].value.load(std::memory_order_relaxed);

    float threshold = std::pow(10.f, threshDb / 20.f);
    float attackCoeff = std::exp(-1.f / (m_sampleRate * attackMs * 0.001f));
    float releaseCoeff = std::exp(-1.f / (m_sampleRate * releaseMs * 0.001f));

    for (int i = 0; i < numFrames; ++i) {
        float absVal = std::abs(data[i]);

        // Envelope follower
        if (absVal > m_envelope)
            m_envelope = attackCoeff * m_envelope + (1.f - attackCoeff) * absVal;
        else
            m_envelope = releaseCoeff * m_envelope + (1.f - releaseCoeff) * absVal;

        // Gate
        float target = (m_envelope > threshold) ? 1.f : 0.f;
        float coeff = (target > m_gain) ? (1.f - attackCoeff) : (1.f - releaseCoeff);
        m_gain += coeff * (target - m_gain);

        data[i] *= m_gain;
    }
}

VML_REGISTER_EFFECT(NoiseGateEffect)
