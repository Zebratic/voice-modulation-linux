#include "effects/NoiseSuppressionEffect.h"
#include "effects/EffectRegistry.h"
#include <cmath>
#include <cstring>

NoiseSuppressionEffect::NoiseSuppressionEffect() {
    m_params.emplace_back("sensitivity", "Sensitivity", 0.f, 100.f, 50.f);
    m_params.emplace_back("floor_db", "Floor (dB)", -80.f, -20.f, -40.f);
    m_params.emplace_back("smoothing", "Smoothing", 0.f, 100.f, 50.f);
}

void NoiseSuppressionEffect::prepare(float sampleRate, int blockSize) {
    EffectBase::prepare(sampleRate, blockSize);

    // Initialize Hann window
    for (int i = 0; i < FFT_SIZE; ++i) {
        m_windowBuffer[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (FFT_SIZE - 1)));
    }

    // Reset buffers
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    std::memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    std::memset(m_noiseFloorAccum, 0, sizeof(m_noiseFloorAccum));
    std::memset(m_noiseProfile, 0, sizeof(m_noiseProfile));
    m_noiseFrameCount = 0;
    m_noiseProfileValid = false;
    m_bufferPos = 0;
}

void NoiseSuppressionEffect::process(float* data, int numFrames) {
    float sensitivity = m_params[0].value.load(std::memory_order_relaxed) / 100.f;
    float floorDb = m_params[1].value.load(std::memory_order_relaxed);
    float smoothing = m_params[2].value.load(std::memory_order_relaxed) / 100.f;

    m_sensitivity = sensitivity;
    m_floorDb = floorDb;
    m_smoothing = smoothing;

    // Process sample-by-sample into input buffer
    for (int i = 0; i < numFrames; ++i) {
        m_inputBuffer[m_bufferPos++] = data[i];

        // When buffer is full, process one frame
        if (m_bufferPos >= FFT_SIZE) {
            // Check if this frame is silence (for noise profiling)
            bool silence = isSilence(m_inputBuffer, FFT_SIZE);

            // Apply window
            for (int j = 0; j < FFT_SIZE; ++j) {
                m_fftReal[j] = m_inputBuffer[j] * m_windowBuffer[j];
            }
            std::memset(m_fftImag, 0, sizeof(m_fftImag));

            // Forward FFT
            fft(m_fftReal, m_fftImag, FFT_SIZE, false);

            // Compute magnitude and phase
            computeMagnitudePhase();

            // Update noise profile during silence
            if (silence && !m_noiseProfileValid) {
                updateNoiseProfile();
            }

            // Apply spectral subtraction if we have a valid noise profile
            if (m_noiseProfileValid) {
                applySpectralSubtraction();

                // Inverse FFT
                fft(m_fftReal, m_fftImag, FFT_SIZE, true);

                // Overlap-add to output
                overlapAdd(m_fftReal, m_outputBuffer, 0);
            }

            // Shift input buffer by hop size
            std::memmove(m_inputBuffer, m_inputBuffer + HOP_SIZE,
                         (FFT_SIZE - HOP_SIZE) * sizeof(float));
            m_bufferPos = FFT_SIZE - HOP_SIZE;
        }
    }

    // Copy output to input data (with appropriate gain)
    // Use first numFrames samples from output buffer
    float gain = 1.0f;  // Compensate for window overlap
    for (int i = 0; i < numFrames; ++i) {
        data[i] = m_outputBuffer[i] * gain;
    }

    // Shift output buffer
    std::memmove(m_outputBuffer, m_outputBuffer + numFrames,
                 (FFT_SIZE - numFrames) * sizeof(float));
    std::memset(m_outputBuffer + (FFT_SIZE - numFrames), 0,
                numFrames * sizeof(float));
}

void NoiseSuppressionEffect::reset() {
    std::memset(m_inputBuffer, 0, sizeof(m_inputBuffer));
    std::memset(m_outputBuffer, 0, sizeof(m_outputBuffer));
    std::memset(m_noiseFloorAccum, 0, sizeof(m_noiseFloorAccum));
    std::memset(m_noiseProfile, 0, sizeof(m_noiseProfile));
    m_noiseFrameCount = 0;
    m_noiseProfileValid = false;
    m_bufferPos = 0;
}

// Simple Radix-2 Cooley-Tukey FFT
void NoiseSuppressionEffect::fft(float* real, float* imag, int n, bool inverse) {
    // Bit-reversal permutation
    int j = 0;
    for (int i = 0; i < n - 1; ++i) {
        if (i < j) {
            std::swap(real[i], real[j]);
            std::swap(imag[i], imag[j]);
        }
        int k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    // Cooley-Tukey iterative FFT
    for (int len = 2; len <= n; len *= 2) {
        float angle = (inverse ? 2.0f : -2.0f) * M_PI / len;
        float wReal = 1.0f;
        float wImag = 0.0f;
        float wStepReal = std::cos(angle);
        float wStepImag = std::sin(angle);

        for (int i = 0; i < len / 2; ++i) {
            for (int k = i; k < n; k += len) {
                int kPlusHalf = k + len / 2;
                float tReal = wReal * real[kPlusHalf] - wImag * imag[kPlusHalf];
                float tImag = wReal * imag[kPlusHalf] + wImag * real[kPlusHalf];
                real[kPlusHalf] = real[k] - tReal;
                imag[kPlusHalf] = imag[k] - tImag;
                real[k] += tReal;
                imag[k] += tImag;
            }
            float newWReal = wReal * wStepReal - wImag * wStepImag;
            wImag = wReal * wStepImag + wImag * wStepReal;
            wReal = newWReal;
        }
    }

    // Scale for inverse transform
    if (inverse) {
        float scale = 1.0f / n;
        for (int i = 0; i < n; ++i) {
            real[i] *= scale;
            imag[i] *= scale;
        }
    }
}

void NoiseSuppressionEffect::computeMagnitudePhase() {
    for (int i = 0; i < NUM_BINS; ++i) {
        float re = m_fftReal[i];
        float im = m_fftImag[i];
        m_magSpectrum[i] = std::sqrt(re * re + im * im) + 1e-10f;
        m_phaseSpectrum[i] = std::atan2(im, re);
    }
}

void NoiseSuppressionEffect::applySpectralSubtraction() {
    float floor = std::pow(10.0f, m_floorDb / 20.0f);

    for (int i = 0; i < NUM_BINS; ++i) {
        // Subtract noise magnitude
        float mag = m_magSpectrum[i] - m_noiseProfile[i] * m_sensitivity;

        // Apply spectral floor
        if (mag < floor) {
            mag = floor;
        }

        // Apply smoothing (spectral smoothing using neighbors)
        if (m_smoothing > 0 && i > 0 && i < NUM_BINS - 1) {
            float smoothFactor = m_smoothing * 0.25f;
            mag = (1.0f - smoothFactor) * mag +
                  smoothFactor * 0.5f * (m_magSpectrum[i-1] + m_magSpectrum[i+1]);
        }

        // Apply power spectral subtraction (more aggressive at high sensitivity)
        if (m_sensitivity > 0.7f) {
            float powerMag = mag * mag;
            float noisePower = m_noiseProfile[i] * m_noiseProfile[i];
            if (powerMag > noisePower) {
                mag = std::sqrt(powerMag - noisePower * m_sensitivity);
            } else {
                mag = floor;
            }
        }

        // Reconstruct with new magnitude, preserve phase
        mag = std::max(mag, floor);
        m_fftReal[i] = mag * std::cos(m_phaseSpectrum[i]);
        m_fftImag[i] = mag * std::sin(m_phaseSpectrum[i]);
    }
}

void NoiseSuppressionEffect::inverseFft(float* real, float* imag) {
    fft(real, imag, FFT_SIZE, true);
}

void NoiseSuppressionEffect::overlapAdd(const float* frame, float* output, int outputOffset) {
    // In-place addition for overlap
    for (int i = 0; i < FFT_SIZE; ++i) {
        if (outputOffset + i < FFT_SIZE) {
            output[outputOffset + i] += frame[i];
        }
    }
}

bool NoiseSuppressionEffect::isSilence(const float* data, int numFrames) const {
    float sum = 0.0f;
    for (int i = 0; i < numFrames; ++i) {
        sum += data[i] * data[i];
    }
    float rms = std::sqrt(sum / numFrames);
    return rms < m_speechThreshold;
}

void NoiseSuppressionEffect::updateNoiseProfile() {
    for (int i = 0; i < NUM_BINS; ++i) {
        m_noiseFloorAccum[i] += m_magSpectrum[i];
    }
    m_noiseFrameCount++;

    // After accumulating enough frames, compute average
    if (m_noiseFrameCount >= 10) {  // ~200ms of silence
        for (int i = 0; i < NUM_BINS; ++i) {
            m_noiseProfile[i] = m_noiseFloorAccum[i] / m_noiseFrameCount;
        }
        m_noiseProfileValid = true;
    }
}

VML_REGISTER_EFFECT(NoiseSuppressionEffect)
