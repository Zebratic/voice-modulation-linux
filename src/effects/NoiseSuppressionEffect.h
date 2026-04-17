#pragma once
#include "effects/EffectBase.h"

class NoiseSuppressionEffect : public EffectBase {
public:
    NoiseSuppressionEffect();

    std::string id() const override { return "noise_suppression"; }
    std::string name() const override { return "Noise Suppression"; }

    void prepare(float sampleRate, int blockSize) override;
    void process(float* data, int numFrames) override;
    void reset() override;

private:
    // FFT configuration
    static constexpr int FFT_SIZE = 2048;
    static constexpr int HOP_SIZE = FFT_SIZE / 2;  // 50% overlap
    static constexpr int NUM_BINS = FFT_SIZE / 2 + 1;

    // Process overlap-add buffers
    float m_inputBuffer[FFT_SIZE] = {};
    float m_outputBuffer[FFT_SIZE] = {};
    float m_windowBuffer[FFT_SIZE] = {};
    int m_bufferPos = 0;

    // FFT scratch buffers (pre-allocated for RT safety)
    float m_fftReal[FFT_SIZE] = {};
    float m_fftImag[FFT_SIZE] = {};
    float m_magSpectrum[NUM_BINS] = {};
    float m_phaseSpectrum[NUM_BINS] = {};
    float m_noiseProfile[NUM_BINS] = {};

    // Noise tracking
    float m_noiseFloorAccum[NUM_BINS] = {};
    int m_noiseFrameCount = 0;
    bool m_noiseProfileValid = false;
    float m_speechThreshold = 0.01f;  // Below this = potential noise

    // Parameters
    float m_sensitivity = 0.5f;   // 0-1, how aggressive
    float m_floorDb = -40.0f;     // Noise floor in dB
    float m_smoothing = 0.5f;     // 0-1, spectral smoothing

    // Simple Radix-2 FFT implementation
    void fft(float* real, float* imag, int n, bool inverse);
    void computeMagnitudePhase();
    void applySpectralSubtraction();
    void inverseFft(float* real, float* imag);
    void overlapAdd(const float* frame, float* output, int outputOffset);
    bool isSilence(const float* data, int numFrames) const;
    void updateNoiseProfile();
};
