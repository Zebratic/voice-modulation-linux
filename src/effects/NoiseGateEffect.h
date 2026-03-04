#pragma once
#include "effects/EffectBase.h"

class NoiseGateEffect : public EffectBase {
public:
    NoiseGateEffect() {
        m_params.emplace_back("threshold_db", "Threshold (dB)", -80.f, 0.f, -40.f);
        m_params.emplace_back("attack_ms", "Attack (ms)", 0.1f, 50.f, 1.f);
        m_params.emplace_back("release_ms", "Release (ms)", 1.f, 500.f, 100.f);
    }

    std::string id() const override { return "noise_gate"; }
    std::string name() const override { return "Noise Gate"; }

    void prepare(float sampleRate, int blockSize) override {
        EffectBase::prepare(sampleRate, blockSize);
        m_envelope = 0.f;
        m_gain = 0.f;
    }

    void process(float* data, int numFrames) override;
    void reset() override { m_envelope = 0.f; m_gain = 0.f; }

private:
    float m_envelope = 0.f;
    float m_gain = 0.f;
};
