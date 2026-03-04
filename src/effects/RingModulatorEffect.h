#pragma once
#include "effects/EffectBase.h"
#include <cmath>

class RingModulatorEffect : public EffectBase {
public:
    RingModulatorEffect() {
        m_params.emplace_back("frequency", "Frequency (Hz)", 20.f, 2000.f, 440.f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "ring_modulator"; }
    std::string name() const override { return "Ring Modulator"; }

    void reset() override { m_phase = 0.0; }

    void process(float* data, int numFrames) override {
        float freq = m_params[0].value.load(std::memory_order_relaxed);
        float mix = m_params[1].value.load(std::memory_order_relaxed);
        double phaseInc = static_cast<double>(freq) / m_sampleRate;

        for (int i = 0; i < numFrames; ++i) {
            float wet = data[i] * static_cast<float>(std::sin(2.0 * M_PI * m_phase));
            data[i] = data[i] * (1.f - mix) + wet * mix;
            m_phase += phaseInc;
            if (m_phase >= 1.0) m_phase -= 1.0;
        }
    }

private:
    double m_phase = 0.0;
};
