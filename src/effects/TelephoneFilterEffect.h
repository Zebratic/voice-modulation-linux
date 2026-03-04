#pragma once
#include "effects/EffectBase.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

class TelephoneFilterEffect : public EffectBase {
public:
    TelephoneFilterEffect() {
        m_params.emplace_back("bandwidth", "Bandwidth", 0.f, 1.f, 0.5f);
        m_params.emplace_back("noise", "Noise", 0.f, 1.f, 0.1f);
        m_params.emplace_back("distortion", "Distortion", 0.f, 1.f, 0.3f);
    }

    std::string id() const override { return "telephone"; }
    std::string name() const override { return "Telephone Filter"; }

    void reset() override {
        m_hpX1 = m_hpX2 = m_hpY1 = m_hpY2 = 0.f;
        m_lpX1 = m_lpX2 = m_lpY1 = m_lpY2 = 0.f;
        m_rng = 12345u;
    }

    void process(float* data, int numFrames) override {
        float bw = m_params[0].value.load(std::memory_order_relaxed);
        float noiseAmt = m_params[1].value.load(std::memory_order_relaxed);
        float distAmt = m_params[2].value.load(std::memory_order_relaxed);

        // HPF cutoff: 100-500 Hz mapped from bandwidth (higher bw = lower cutoff)
        float hpFreq = 500.f - bw * 400.f;
        // LPF cutoff: 2000-5000 Hz mapped from bandwidth
        float lpFreq = 2000.f + bw * 3000.f;

        // Compute biquad coefficients for HPF
        float hpB0, hpB1, hpB2, hpA1, hpA2;
        computeHPF(hpFreq, hpB0, hpB1, hpB2, hpA1, hpA2);

        // Compute biquad coefficients for LPF
        float lpB0, lpB1, lpB2, lpA1, lpA2;
        computeLPF(lpFreq, lpB0, lpB1, lpB2, lpA1, lpA2);

        for (int i = 0; i < numFrames; ++i) {
            float x = data[i];

            // HPF biquad
            float hpOut = hpB0 * x + hpB1 * m_hpX1 + hpB2 * m_hpX2
                        - hpA1 * m_hpY1 - hpA2 * m_hpY2;
            m_hpX2 = m_hpX1; m_hpX1 = x;
            m_hpY2 = m_hpY1; m_hpY1 = hpOut;

            // LPF biquad
            float lpOut = lpB0 * hpOut + lpB1 * m_lpX1 + lpB2 * m_lpX2
                        - lpA1 * m_lpY1 - lpA2 * m_lpY2;
            m_lpX2 = m_lpX1; m_lpX1 = hpOut;
            m_lpY2 = m_lpY1; m_lpY1 = lpOut;

            // Add noise
            float noise = xorshift() * noiseAmt * 0.02f;
            float wet = lpOut + noise;

            // Soft clip
            wet = std::tanh(wet * (1.f + distAmt * 10.f));

            data[i] = wet;
        }
    }

private:
    float m_hpX1 = 0.f, m_hpX2 = 0.f, m_hpY1 = 0.f, m_hpY2 = 0.f;
    float m_lpX1 = 0.f, m_lpX2 = 0.f, m_lpY1 = 0.f, m_lpY2 = 0.f;
    uint32_t m_rng = 12345u;

    float xorshift() {
        m_rng ^= m_rng << 13;
        m_rng ^= m_rng >> 17;
        m_rng ^= m_rng << 5;
        return static_cast<float>(static_cast<int32_t>(m_rng)) / 2147483648.f;
    }

    void computeHPF(float freq, float& b0, float& b1, float& b2, float& a1, float& a2) {
        float w0 = 2.f * static_cast<float>(M_PI) * freq / m_sampleRate;
        float cosw0 = std::cos(w0);
        float alpha = std::sin(w0) / (2.f * 0.707f);
        float a0 = 1.f + alpha;
        b0 = ((1.f + cosw0) / 2.f) / a0;
        b1 = -(1.f + cosw0) / a0;
        b2 = b0;
        a1 = (-2.f * cosw0) / a0;
        a2 = (1.f - alpha) / a0;
    }

    void computeLPF(float freq, float& b0, float& b1, float& b2, float& a1, float& a2) {
        float w0 = 2.f * static_cast<float>(M_PI) * freq / m_sampleRate;
        float cosw0 = std::cos(w0);
        float alpha = std::sin(w0) / (2.f * 0.707f);
        float a0 = 1.f + alpha;
        b0 = ((1.f - cosw0) / 2.f) / a0;
        b1 = (1.f - cosw0) / a0;
        b2 = b0;
        a1 = (-2.f * cosw0) / a0;
        a2 = (1.f - alpha) / a0;
    }
};
