#pragma once
#include "effects/EffectBase.h"
#include <vector>
#include <cmath>
#include <algorithm>

class FlangerEffect : public EffectBase {
public:
    FlangerEffect() {
        m_params.emplace_back("rate", "Rate (Hz)", 0.05f, 5.f, 0.25f);
        m_params.emplace_back("depth_ms", "Depth (ms)", 0.5f, 10.f, 3.f);
        m_params.emplace_back("feedback", "Feedback", -0.95f, 0.95f, 0.5f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "flanger"; }
    std::string name() const override { return "Flanger"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    std::vector<float> m_buffer;
    int m_writePos = 0;
    int m_bufferSize = 0;
    float m_phase = 0.f;
    float m_feedbackSample = 0.f;
};
