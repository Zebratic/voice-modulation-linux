#pragma once
#include "effects/EffectBase.h"
#include <vector>
#include <cmath>
#include <algorithm>

class ChorusEffect : public EffectBase {
public:
    ChorusEffect() {
        m_params.emplace_back("voices", "Voices", 2.f, 6.f, 3.f);
        m_params.emplace_back("rate", "Rate (Hz)", 0.1f, 5.f, 0.5f);
        m_params.emplace_back("depth_ms", "Depth (ms)", 1.f, 20.f, 7.f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.5f);
    }

    std::string id() const override { return "chorus"; }
    std::string name() const override { return "Chorus"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    std::vector<float> m_buffer;
    int m_writePos = 0;
    int m_bufferSize = 0;
    float m_phases[6] = {};
};
