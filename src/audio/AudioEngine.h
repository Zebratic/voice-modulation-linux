#pragma once
#include "audio/PipeWireDevice.h"
#include "audio/EffectPipeline.h"
#include "audio/ClipRecorder.h"
#include "audio/RingBuffer.h"
#include "modulation/ModulationManager.h"
#include <atomic>
#include <memory>

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool start();
    void stop();
    bool running() const;

    EffectPipeline& pipeline() { return m_pipeline; }
    ModulationManager& modulationManager() { return m_modulationMgr; }
    ClipRecorder& clipRecorder() { return m_clipRecorder; }

    void setEnabled(bool enabled);
    bool enabled() const { return m_enabled.load(std::memory_order_relaxed); }

    void setMonitorEnabled(bool enabled);
    bool monitorEnabled() const;

    // Input device selection
    static std::vector<AudioDevice> enumerateInputDevices() { return PipeWireDevice::enumerateInputDevices(); }
    void setInputDevice(uint32_t nodeId);
    uint32_t inputDeviceId() const { return m_device.inputDeviceId(); }

    // Monitor output device selection
    static std::vector<AudioDevice> enumerateOutputDevices() { return PipeWireDevice::enumerateOutputDevices(); }
    void setMonitorOutputDevice(uint32_t nodeId);
    uint32_t monitorOutputId() const { return m_device.monitorOutputId(); }

    // Virtual mic name
    void setVirtualMicName(const std::string& name);
    std::string virtualMicName() const { return m_device.virtualMicName(); }

    float inputLevel() const { return m_inputLevel.load(std::memory_order_relaxed); }
    float outputLevel() const { return m_pipeline.lastPeakLevel(); }

    // Waveform ring buffers for GUI visualization (read from GUI thread)
    RingBuffer& inputWaveform() { return m_inputWaveform; }
    RingBuffer& outputWaveform() { return m_outputWaveform; }

private:
    void audioCallback(float* input, float* output, int numFrames);

    PipeWireDevice m_device;
    EffectPipeline m_pipeline;
    ModulationManager m_modulationMgr;
    ClipRecorder m_clipRecorder;
    RingBuffer m_inputWaveform{4096};
    RingBuffer m_outputWaveform{4096};
    std::atomic<bool> m_enabled{true};
    std::atomic<float> m_inputLevel{0.f};
};
