#pragma once
#include <atomic>
#include <vector>
#include <cstring>

class ClipRecorder {
public:
    enum class State { Idle, Recording, PlaybackLoop };

    explicit ClipRecorder(float sampleRate = 48000.f, float maxSeconds = 10.f)
        : m_sampleRate(sampleRate),
          m_clipLength(static_cast<int>(sampleRate * maxSeconds)),
          m_buffer(m_clipLength, 0.f) {}

    // Start recording. Call from GUI thread.
    void startRecording() {
        m_writePos.store(0, std::memory_order_relaxed);
        m_readPos = 0;
        m_recordedLength.store(0, std::memory_order_relaxed);
        std::fill(m_buffer.begin(), m_buffer.end(), 0.f);
        m_state.store(State::Recording, std::memory_order_release);
    }

    // Start looped playback. Call from GUI thread.
    void startPlayback() {
        m_readPos = 0;
        m_state.store(State::PlaybackLoop, std::memory_order_release);
    }

    // Stop recording early. Keeps whatever was recorded. Call from GUI thread.
    void stopRecording() {
        if (m_state.load(std::memory_order_acquire) != State::Recording) return;
        int wp = m_writePos.load(std::memory_order_relaxed);
        m_recordedLength.store(wp, std::memory_order_release);
        m_state.store(State::Idle, std::memory_order_release);
    }

    // Stop playback but keep the clip. Call from GUI thread.
    void stopPlayback() {
        if (m_state.load(std::memory_order_acquire) == State::PlaybackLoop)
            m_state.store(State::Idle, std::memory_order_release);
    }

    // Stop playback and clear. Call from GUI thread.
    void stopAndClear() {
        m_state.store(State::Idle, std::memory_order_release);
        m_writePos.store(0, std::memory_order_relaxed);
        m_readPos = 0;
        m_recordedLength.store(0, std::memory_order_release);
    }

    // Called from audio callback during Recording state.
    // Copies input into buffer. Returns true while recording, false when done.
    bool recordBlock(const float* input, int numFrames) {
        if (m_state.load(std::memory_order_acquire) != State::Recording) return false;

        int wp = m_writePos.load(std::memory_order_relaxed);
        int remaining = m_clipLength - wp;
        int toCopy = std::min(numFrames, remaining);
        if (toCopy > 0) {
            std::memcpy(m_buffer.data() + wp, input, toCopy * sizeof(float));
            wp += toCopy;
            m_writePos.store(wp, std::memory_order_relaxed);
        }

        if (wp >= m_clipLength) {
            m_recordedLength.store(m_clipLength, std::memory_order_release);
            m_state.store(State::Idle, std::memory_order_release);
            return false; // done
        }
        return true; // still recording
    }

    // Called from audio callback during PlaybackLoop state.
    // Fills output with looped clip data.
    void playBlock(float* output, int numFrames) {
        int recLen = m_recordedLength.load(std::memory_order_acquire);
        if (m_state.load(std::memory_order_acquire) != State::PlaybackLoop || recLen == 0)
            return;

        for (int i = 0; i < numFrames; ++i) {
            output[i] = m_buffer[m_readPos];
            m_readPos = (m_readPos + 1) % recLen;
        }
    }

    State state() const { return m_state.load(std::memory_order_acquire); }
    bool hasClip() const { return m_recordedLength.load(std::memory_order_acquire) > 0; }
    float recordProgress() const {
        if (m_state.load(std::memory_order_acquire) != State::Recording) return 0.f;
        return static_cast<float>(m_writePos.load(std::memory_order_relaxed)) /
               static_cast<float>(m_clipLength);
    }
    float recordedSeconds() const {
        return static_cast<float>(m_writePos.load(std::memory_order_relaxed)) / m_sampleRate;
    }
    float maxSeconds() const {
        return static_cast<float>(m_clipLength) / m_sampleRate;
    }

private:
    float m_sampleRate;
    int m_clipLength;
    std::vector<float> m_buffer;
    std::atomic<State> m_state{State::Idle};
    std::atomic<int> m_writePos{0};
    int m_readPos = 0;
    std::atomic<int> m_recordedLength{0};
};
