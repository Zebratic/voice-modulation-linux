#pragma once
#include "effects/EffectBase.h"
#include <vector>

// Granular pitch shifter using overlap-add with windowed grains.
// Reads from a circular input buffer at a different rate to shift pitch,
// crossfading grains to avoid discontinuities.
class PitchShiftEffect : public EffectBase {
public:
    PitchShiftEffect() {
        m_params.emplace_back("semitones", "Semitones", -12.f, 12.f, 0.f);
    }

    std::string id() const override { return "pitch_shift"; }
    std::string name() const override { return "Pitch Shift"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    std::vector<float> m_inBuf;    // circular input buffer
    std::vector<float> m_window;   // Hann window
    int m_inBufSize = 0;
    int m_writePos = 0;
    int m_grainSize = 0;

    // Two read heads for crossfading
    double m_readPos0 = 0.0;
    double m_readPos1 = 0.0;
    int m_grainCounter0 = 0;
    int m_grainCounter1 = 0;
};
