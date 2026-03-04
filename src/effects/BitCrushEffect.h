#pragma once
#include "effects/EffectBase.h"
#include <cmath>

class BitCrushEffect : public EffectBase {
public:
    BitCrushEffect() {
        m_params.emplace_back("bit_depth", "Bit Depth", 1.f, 16.f, 16.f);
        m_params.emplace_back("sample_rate_reduction", "Sample Rate Reduction", 1.f, 100.f, 1.f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "bitcrush"; }
    std::string name() const override { return "BitCrush"; }

    void reset() override { m_holdCounter = 0.f; m_holdSample = 0.f; }

    void process(float* data, int numFrames) override {
        float bitDepth = m_params[0].value.load(std::memory_order_relaxed);
        float rateReduction = m_params[1].value.load(std::memory_order_relaxed);
        float mix = m_params[2].value.load(std::memory_order_relaxed);

        float levels = std::pow(2.f, std::floor(bitDepth));

        for (int i = 0; i < numFrames; ++i) {
            float dry = data[i];

            // Sample-and-hold for rate reduction
            m_holdCounter += 1.f;
            if (m_holdCounter >= rateReduction) {
                m_holdCounter -= rateReduction;
                // Quantize: map [-1,1] to [0,1], quantize, map back
                float normalized = (dry + 1.f) * 0.5f;
                m_holdSample = std::floor(normalized * levels) / levels * 2.f - 1.f;
            }

            data[i] = dry + (m_holdSample - dry) * mix;
        }
    }

private:
    float m_holdCounter = 0.f;
    float m_holdSample = 0.f;
};
