#include "audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <cstring>

AudioEngine::AudioEngine()
    : m_clipRecorder(m_device.sampleRate(), 3.f) {
    m_pipeline.prepare(m_device.sampleRate(), m_device.blockSize());
    m_pipeline.setModulationManager(&m_modulationMgr);
}

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start() {
    return m_device.start([this](float* input, float* output, int n) {
        audioCallback(input, output, n);
    });
}

void AudioEngine::stop() {
    m_device.stop();
}

bool AudioEngine::running() const {
    return m_device.running();
}

void AudioEngine::setEnabled(bool enabled) {
    m_enabled.store(enabled, std::memory_order_relaxed);
}

void AudioEngine::setMonitorEnabled(bool enabled) {
    m_device.setMonitorEnabled(enabled);
}

bool AudioEngine::monitorEnabled() const {
    return m_device.monitorEnabled();
}

void AudioEngine::setInputDevice(uint32_t nodeId) {
    m_device.setInputDeviceId(nodeId);
    if (m_device.running())
        m_device.reconnectCapture();
}

void AudioEngine::setMonitorOutputDevice(uint32_t nodeId) {
    m_device.setMonitorOutputId(nodeId);
    if (m_device.running())
        m_device.reconnectMonitor();
}

void AudioEngine::setVirtualMicName(const std::string& name) {
    m_device.setVirtualMicName(name);
    if (m_device.running())
        m_device.reconnectSource();
}

void AudioEngine::audioCallback(float* input, float* output, int numFrames) {
    float peak = 0.f;
    for (int i = 0; i < numFrames; ++i)
        peak = std::max(peak, std::abs(input[i]));
    m_inputLevel.store(peak, std::memory_order_relaxed);

    // Record mic input if recording
    if (m_clipRecorder.state() == ClipRecorder::State::Recording)
        m_clipRecorder.recordBlock(input, numFrames);

    // If playing back clip, replace output with looped clip data
    if (m_clipRecorder.state() == ClipRecorder::State::PlaybackLoop)
        m_clipRecorder.playBlock(output, numFrames);

    if (m_enabled.load(std::memory_order_relaxed)) {
        m_pipeline.process(output, numFrames);
    }
}
