#pragma once
#include "effects/EffectBase.h"
#include <vector>

// Schroeder reverb: 4 parallel comb filters + 2 series allpass filters
class ReverbEffect : public EffectBase {
public:
    ReverbEffect() {
        m_params.emplace_back("room_size", "Room Size", 0.f, 1.f, 0.5f);
        m_params.emplace_back("damping", "Damping", 0.f, 1.f, 0.5f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 0.3f);
    }

    std::string id() const override { return "reverb"; }
    std::string name() const override { return "Reverb"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    struct CombFilter {
        std::vector<float> buffer;
        int pos = 0;
        float filterState = 0.f;
    };

    struct AllpassFilter {
        std::vector<float> buffer;
        int pos = 0;
    };

    CombFilter m_combs[4];
    AllpassFilter m_allpasses[2];
};
