#pragma once
#include "effects/EffectBase.h"
#include <cmath>
#include <algorithm>

class DistortionEffect : public EffectBase {
public:
    DistortionEffect() {
        m_params.emplace_back("drive", "Drive", 1.f, 50.f, 5.f);
        m_params.emplace_back("tone", "Tone", 0.f, 1.f, 0.5f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "distortion"; }
    std::string name() const override { return "Distortion"; }

    void reset() override { m_lpState = 0.f; }

    void process(float* data, int numFrames) override {
        float drive = m_params[0].value.load(std::memory_order_relaxed);
        float tone = m_params[1].value.load(std::memory_order_relaxed);
        float mix = m_params[2].value.load(std::memory_order_relaxed);

        // Tone maps to cutoff: 200 Hz at 0, 20000 Hz at 1 (log scale)
        float cutoff = 200.f * std::pow(100.f, tone);
        cutoff = std::min(cutoff, m_sampleRate * 0.49f);
        float rc = 1.f / (2.f * static_cast<float>(M_PI) * cutoff);
        float dt = 1.f / m_sampleRate;
        float alpha = dt / (rc + dt);

        for (int i = 0; i < numFrames; ++i) {
            float dry = data[i];
            float wet = std::tanh(dry * drive);
            // One-pole lowpass
            m_lpState += alpha * (wet - m_lpState);
            // Blend filtered/unfiltered by tone, then wet/dry
            float shaped = m_lpState * tone + wet * (1.f - tone);
            data[i] = dry * (1.f - mix) + shaped * mix;
        }
    }

private:
    float m_lpState = 0.f;
};
