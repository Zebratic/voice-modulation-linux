#pragma once
#include "effects/EffectBase.h"
#include <cmath>
#include <algorithm>

class PhaserEffect : public EffectBase {
public:
    PhaserEffect() {
        m_params.emplace_back("stages", "Stages", 2.f, 12.f, 6.f);
        m_params.emplace_back("rate", "Rate (Hz)", 0.05f, 5.f, 0.3f);
        m_params.emplace_back("depth", "Depth", 0.f, 1.f, 0.7f);
        m_params.emplace_back("feedback", "Feedback", -0.95f, 0.95f, 0.5f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "phaser"; }
    std::string name() const override { return "Phaser"; }

    void reset() override {
        for (int i = 0; i < 12; ++i) m_allpassState[i] = 0.f;
        m_phase = 0.f;
        m_feedbackSample = 0.f;
    }

    void process(float* data, int numFrames) override {
        int stages = static_cast<int>(m_params[0].value.load(std::memory_order_relaxed));
        float rate = m_params[1].value.load(std::memory_order_relaxed);
        float depth = m_params[2].value.load(std::memory_order_relaxed);
        float feedback = m_params[3].value.load(std::memory_order_relaxed);
        float mix = m_params[4].value.load(std::memory_order_relaxed);

        stages = std::clamp(stages, 2, 12);
        float phaseInc = rate / m_sampleRate;

        for (int i = 0; i < numFrames; ++i) {
            float dry = data[i];
            float lfo = std::sin(2.f * static_cast<float>(M_PI) * m_phase);

            // Sweep frequency between 200-5000 Hz
            float minFreq = 200.f;
            float maxFreq = 5000.f;
            float sweepFreq = minFreq + (maxFreq - minFreq) * (0.5f + 0.5f * lfo * depth);

            // Allpass coefficient
            float tanVal = std::tan(static_cast<float>(M_PI) * sweepFreq / m_sampleRate);
            float coeff = (1.f - tanVal) / (1.f + tanVal);

            // Input with feedback
            float x = dry + m_feedbackSample * feedback;

            // Cascade of first-order allpass filters
            for (int s = 0; s < stages; ++s) {
                float y = -coeff * x + m_allpassState[s];
                m_allpassState[s] = x + coeff * y;
                x = y;
            }

            m_feedbackSample = x;
            data[i] = dry * (1.f - mix) + x * mix;

            m_phase += phaseInc;
            if (m_phase >= 1.f) m_phase -= 1.f;
        }
    }

private:
    float m_allpassState[12] = {};
    float m_phase = 0.f;
    float m_feedbackSample = 0.f;
};
