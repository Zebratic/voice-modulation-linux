#pragma once
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <functional>
#include <string>
#include <vector>
#include <atomic>

struct AudioDevice {
    uint32_t id;
    std::string name;
    std::string description;
};

class PipeWireDevice {
public:
    using AudioCallback = std::function<void(float* input, float* output, int numFrames)>;

    PipeWireDevice();
    ~PipeWireDevice();

    bool start(AudioCallback callback);
    void stop();
    bool running() const { return m_running.load(); }

    static std::vector<AudioDevice> enumerateInputDevices();
    static std::vector<AudioDevice> enumerateOutputDevices();

    void setInputDeviceId(uint32_t nodeId);
    uint32_t inputDeviceId() const { return m_targetInputId; }
    void reconnectCapture();

    void setMonitorOutputId(uint32_t nodeId);
    uint32_t monitorOutputId() const { return m_targetMonitorId; }
    void reconnectMonitor();

    void setMonitorEnabled(bool enabled);
    bool monitorEnabled() const { return m_monitorEnabled.load(); }

    void setPlaybackActive(bool active) { m_playbackActive.store(active, std::memory_order_relaxed); }
    bool playbackActive() const { return m_playbackActive.load(std::memory_order_relaxed); }

    void setVirtualMicName(const std::string& name);
    std::string virtualMicName() const { return m_virtualMicName; }
    void reconnectSource();

    float sampleRate() const { return 48000.f; }
    int blockSize() const { return 256; }

private:
    static void onCaptureProcess(void* userdata);
    static void onSourceProcess(void* userdata);
    static void onMonitorProcess(void* userdata);
    static std::vector<AudioDevice> enumerateDevices(const std::string& mediaClass);

    void threadFunc();
    void buildFormatParams(uint8_t* buf, size_t bufSize, const spa_pod** param);

    // Invoke a function on the PipeWire loop thread (thread-safe)
    void invokeOnLoop(std::function<void()> fn);
    static int loopInvokeCallback(struct spa_loop* loop, bool async, uint32_t seq,
                                  const void* data, size_t size, void* user_data);

    pw_main_loop* m_loop = nullptr;
    pw_context* m_context = nullptr;
    pw_core* m_core = nullptr;

    pw_stream* m_captureStream = nullptr;
    pw_stream* m_sourceStream = nullptr;
    pw_stream* m_monitorStream = nullptr;

    AudioCallback m_callback;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_monitorEnabled{false};
    std::atomic<bool> m_playbackActive{false};
    uint32_t m_targetInputId = PW_ID_ANY;
    uint32_t m_targetMonitorId = PW_ID_ANY;
    std::string m_virtualMicName = "VML Virtual Microphone";

    static constexpr int MAX_BLOCK = 1024;
    float m_processBuffer[MAX_BLOCK] = {};
    int m_processedFrames = 0;

    pw_stream_events m_captureEvents{};
    pw_stream_events m_sourceEvents{};
    pw_stream_events m_monitorEvents{};
    spa_hook m_captureHook{};
    spa_hook m_sourceHook{};
    spa_hook m_monitorHook{};
};
