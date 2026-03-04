#pragma once
#include "effects/EffectBase.h"
#include <cmath>
#include <algorithm>

class FormantShifterEffect : public EffectBase {
public:
    FormantShifterEffect() {
        m_params.emplace_back("shift", "Shift (semitones)", -12.f, 12.f, 0.f);
        m_params.emplace_back("mix", "Mix", 0.f, 1.f, 1.f);
    }

    std::string id() const override { return "formant_shifter"; }
    std::string name() const override { return "Formant Shifter"; }

    void reset() override {
        for (int i = 0; i < 5; ++i) {
            m_x1[i] = m_x2[i] = m_y1[i] = m_y2[i] = 0.f;
        }
        m_cachedShift = -999.f;
    }

    void process(float* data, int numFrames) override {
        float shift = m_params[0].value.load(std::memory_order_relaxed);
        float mix = m_params[1].value.load(std::memory_order_relaxed);

        // Recalculate coefficients if shift changed
        if (std::abs(shift - m_cachedShift) > 0.01f) {
            m_cachedShift = shift;
            float ratio = std::pow(2.f, shift / 12.f);
            static const float formants[5] = {500.f, 1500.f, 2500.f, 3500.f, 4500.f};
            for (int i = 0; i < 5; ++i) {
                float freq = formants[i] * ratio;
                freq = std::clamp(freq, 50.f, m_sampleRate * 0.45f);
                computeBPF(freq, 5.f, m_b0[i], m_b1[i], m_b2[i], m_a1[i], m_a2[i]);
            }
        }

        for (int i = 0; i < numFrames; ++i) {
            float dry = data[i];
            float wet = 0.f;

            for (int f = 0; f < 5; ++f) {
                float out = m_b0[f] * dry + m_b1[f] * m_x1[f] + m_b2[f] * m_x2[f]
                          - m_a1[f] * m_y1[f] - m_a2[f] * m_y2[f];
                m_x2[f] = m_x1[f]; m_x1[f] = dry;
                m_y2[f] = m_y1[f]; m_y1[f] = out;
                wet += out;
            }

            data[i] = dry * (1.f - mix) + wet * mix;
        }
    }

private:
    float m_x1[5] = {}, m_x2[5] = {}, m_y1[5] = {}, m_y2[5] = {};
    float m_b0[5] = {}, m_b1[5] = {}, m_b2[5] = {};
    float m_a1[5] = {}, m_a2[5] = {};
    float m_cachedShift = -999.f;

    void computeBPF(float freq, float Q, float& b0, float& b1, float& b2, float& a1, float& a2) {
        float w0 = 2.f * static_cast<float>(M_PI) * freq / m_sampleRate;
        float alpha = std::sin(w0) / (2.f * Q);
        float a0 = 1.f + alpha;
        b0 = alpha / a0;
        b1 = 0.f;
        b2 = -alpha / a0;
        a1 = (-2.f * std::cos(w0)) / a0;
        a2 = (1.f - alpha) / a0;
    }
};
