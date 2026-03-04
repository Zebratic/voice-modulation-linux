#pragma once
#include "effects/EffectBase.h"
#include <vector>

class EchoEffect : public EffectBase {
public:
    EchoEffect() {
        m_params.emplace_back("delay_ms", "Delay (ms)", 10.f, 1000.f, 250.f);
        m_params.emplace_back("feedback", "Feedback", 0.f, 0.95f, 0.4f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.3f);
    }

    std::string id() const override { return "echo"; }
    std::string name() const override { return "Echo"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    std::vector<float> m_buffer;
    int m_writePos = 0;
    int m_maxDelaySamples = 0;
};
