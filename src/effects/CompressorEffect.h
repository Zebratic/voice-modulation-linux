#pragma once
#include "effects/EffectBase.h"
#include <cmath>
#include <algorithm>

class CompressorEffect : public EffectBase {
public:
    CompressorEffect() {
        m_params.emplace_back("threshold", "Threshold (dB)", -60.f, 0.f, -20.f);
        m_params.emplace_back("ratio", "Ratio", 1.f, 20.f, 4.f);
        m_params.emplace_back("attack_ms", "Attack (ms)", 0.1f, 100.f, 10.f);
        m_params.emplace_back("release_ms", "Release (ms)", 10.f, 1000.f, 100.f);
        m_params.emplace_back("makeup_db", "Makeup (dB)", 0.f, 40.f, 0.f);
    }

    std::string id() const override { return "compressor"; }
    std::string name() const override { return "Compressor"; }

    void reset() override { m_envDb = -96.f; }

    void process(float* data, int numFrames) override {
        float threshold = m_params[0].value.load(std::memory_order_relaxed);
        float ratio = m_params[1].value.load(std::memory_order_relaxed);
        float attackMs = m_params[2].value.load(std::memory_order_relaxed);
        float releaseMs = m_params[3].value.load(std::memory_order_relaxed);
        float makeupDb = m_params[4].value.load(std::memory_order_relaxed);

        float attackCoeff = std::exp(-1.f / (attackMs * 0.001f * m_sampleRate));
        float releaseCoeff = std::exp(-1.f / (releaseMs * 0.001f * m_sampleRate));
        float makeupGain = std::pow(10.f, makeupDb / 20.f);

        for (int i = 0; i < numFrames; ++i) {
            float inputAbs = std::abs(data[i]);
            float inputDb = (inputAbs > 1e-6f) ? 20.f * std::log10(inputAbs) : -96.f;

            // Envelope follower
            float coeff = (inputDb > m_envDb) ? attackCoeff : releaseCoeff;
            m_envDb = coeff * m_envDb + (1.f - coeff) * inputDb;

            // Gain reduction
            float gainDb = 0.f;
            if (m_envDb > threshold) {
                gainDb = (threshold - m_envDb) * (1.f - 1.f / ratio);
            }

            float gain = std::pow(10.f, gainDb / 20.f) * makeupGain;
            data[i] *= gain;
        }
    }

private:
    float m_envDb = -96.f;
};
