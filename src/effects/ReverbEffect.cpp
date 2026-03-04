#include "effects/ReverbEffect.h"
#include "effects/EffectRegistry.h"
#include <cmath>
#include <algorithm>

// Comb filter delay lengths (in samples at 44100Hz, scaled for actual rate)
static constexpr int COMB_LENGTHS[] = {1116, 1188, 1277, 1356};
static constexpr int ALLPASS_LENGTHS[] = {556, 441};
static constexpr float FIXED_GAIN = 0.015f;

void ReverbEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);
    float scale = sampleRate / 44100.f;

    for (int i = 0; i < 4; ++i) {
        int len = static_cast<int>(COMB_LENGTHS[i] * scale);
        m_combs[i].buffer.resize(len, 0.f);
        m_combs[i].pos = 0;
        m_combs[i].filterState = 0.f;
    }

    for (int i = 0; i < 2; ++i) {
        int len = static_cast<int>(ALLPASS_LENGTHS[i] * scale);
        m_allpasses[i].buffer.resize(len, 0.f);
        m_allpasses[i].pos = 0;
    }
}

void ReverbEffect::process(float* data, int numFrames) {
    float roomSize = m_params[0].value.load(std::memory_order_relaxed);
    float damping = m_params[1].value.load(std::memory_order_relaxed);
    float mix = m_params[2].value.load(std::memory_order_relaxed);

    float feedback = roomSize * 0.28f + 0.7f;
    float damp1 = damping * 0.4f;
    float damp2 = 1.f - damp1;

    for (int i = 0; i < numFrames; ++i) {
        float input = data[i] * FIXED_GAIN;
        float wet = 0.f;

        // Parallel comb filters
        for (int c = 0; c < 4; ++c) {
            auto& comb = m_combs[c];
            float output = comb.buffer[comb.pos];
            comb.filterState = output * damp2 + comb.filterState * damp1;
            comb.buffer[comb.pos] = input + comb.filterState * feedback;
            comb.pos = (comb.pos + 1) % static_cast<int>(comb.buffer.size());
            wet += output;
        }

        // Series allpass filters
        for (int a = 0; a < 2; ++a) {
            auto& ap = m_allpasses[a];
            float bufOut = ap.buffer[ap.pos];
            ap.buffer[ap.pos] = wet + bufOut * 0.5f;
            wet = bufOut - wet;
            ap.pos = (ap.pos + 1) % static_cast<int>(ap.buffer.size());
        }

        data[i] = data[i] * (1.f - mix) + wet * mix;
    }
}

void ReverbEffect::reset() {
    for (auto& c : m_combs) {
        std::fill(c.buffer.begin(), c.buffer.end(), 0.f);
        c.pos = 0;
        c.filterState = 0.f;
    }
    for (auto& a : m_allpasses) {
        std::fill(a.buffer.begin(), a.buffer.end(), 0.f);
        a.pos = 0;
    }
}

VML_REGISTER_EFFECT(ReverbEffect)
