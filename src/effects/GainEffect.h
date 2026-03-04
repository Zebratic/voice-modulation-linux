#pragma once
#include "effects/EffectBase.h"
#include <cmath>

class GainEffect : public EffectBase {
public:
    GainEffect() {
        m_params.emplace_back("gain_db", "Gain (dB)", -24.f, 24.f, 0.f);
    }

    std::string id() const override { return "gain"; }
    std::string name() const override { return "Gain"; }

    void process(float* data, int numFrames) override {
        float db = m_params[0].value.load(std::memory_order_relaxed);
        float gain = std::pow(10.f, db / 20.f);
        for (int i = 0; i < numFrames; ++i)
            data[i] *= gain;
    }
};
