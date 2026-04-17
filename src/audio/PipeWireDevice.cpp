#include "audio/PipeWireDevice.h"
#include "app/VerboseLogger.h"
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <cstring>
#include <thread>
#include <array>
#include <sstream>
#include <mutex>
#include <condition_variable>

PipeWireDevice::PipeWireDevice() {
    pw_init(nullptr, nullptr);
}

PipeWireDevice::~PipeWireDevice() {
    stop();
    pw_deinit();
}

void PipeWireDevice::onCaptureProcess(void* userdata) {
    auto* self = static_cast<PipeWireDevice*>(userdata);
    pw_buffer* buf = pw_stream_dequeue_buffer(self->m_captureStream);
    if (!buf) return;

    spa_data& d = buf->buffer->datas[0];
    auto* input = static_cast<float*>(d.data);
    int numFrames = d.chunk->size / sizeof(float);
    if (numFrames > MAX_BLOCK) numFrames = MAX_BLOCK;

    if (input && self->m_callback) {
        std::memcpy(self->m_processBuffer, input, numFrames * sizeof(float));
        self->m_callback(self->m_processBuffer, self->m_processBuffer, numFrames);
        self->m_processedFrames = numFrames;
    }

    pw_stream_queue_buffer(self->m_captureStream, buf);

    if (self->m_sourceStream)
        pw_stream_trigger_process(self->m_sourceStream);
    if (self->m_monitorStream &&
        (self->m_monitorEnabled.load(std::memory_order_relaxed) ||
         self->m_playbackActive.load(std::memory_order_relaxed)))
        pw_stream_trigger_process(self->m_monitorStream);
}

void PipeWireDevice::onSourceProcess(void* userdata) {
    auto* self = static_cast<PipeWireDevice*>(userdata);
    pw_buffer* buf = pw_stream_dequeue_buffer(self->m_sourceStream);
    if (!buf) return;

    spa_data& d = buf->buffer->datas[0];
    auto* output = static_cast<float*>(d.data);
    int numFrames = self->m_processedFrames;

    if (output && numFrames > 0) {
        std::memcpy(output, self->m_processBuffer, numFrames * sizeof(float));
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = numFrames * sizeof(float);
    }

    pw_stream_queue_buffer(self->m_sourceStream, buf);
}

void PipeWireDevice::onMonitorProcess(void* userdata) {
    auto* self = static_cast<PipeWireDevice*>(userdata);
    pw_buffer* buf = pw_stream_dequeue_buffer(self->m_monitorStream);
    if (!buf) return;

    spa_data& d = buf->buffer->datas[0];
    auto* output = static_cast<float*>(d.data);
    int numFrames = self->m_processedFrames;

    if (output && numFrames > 0 &&
        (self->m_monitorEnabled.load(std::memory_order_relaxed) ||
         self->m_playbackActive.load(std::memory_order_relaxed))) {
        std::memcpy(output, self->m_processBuffer, numFrames * sizeof(float));
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = numFrames * sizeof(float);
    } else if (output) {
        std::memset(output, 0, d.maxsize);
        d.chunk->offset = 0;
        d.chunk->stride = sizeof(float);
        d.chunk->size = d.maxsize;
    }

    pw_stream_queue_buffer(self->m_monitorStream, buf);
}

void PipeWireDevice::buildFormatParams(uint8_t* buf, size_t bufSize, const spa_pod** param) {
    spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, static_cast<uint32_t>(bufSize));
    spa_audio_info_raw rawInfo = SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_F32,
        .rate = 48000,
        .channels = 1
    );
    *param = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &rawInfo);
}

// --- Thread-safe invoke on PipeWire loop ---

int PipeWireDevice::loopInvokeCallback(struct spa_loop*, bool, uint32_t,
                                        const void* data, size_t, void*) {
    auto* fn = static_cast<const std::function<void()>*>(data);
    (*fn)();
    return 0;
}

void PipeWireDevice::invokeOnLoop(std::function<void()> fn) {
    VML_DEBUG("invokeOnLoop: starting");
    if (!m_loop || !m_running.load()) {
        VML_DEBUG("invokeOnLoop: early return (no loop or not running)");
        return;
    }

    // Wait for the loop thread to be active before invoking
    // This prevents deadlocks when the loop thread hasn't started yet
    {
        std::unique_lock<std::mutex> lock(m_loopMutex);
        VML_DEBUG("invokeOnLoop: waiting for loop to be active");
        bool ready = m_loopCv.wait_for(lock, std::chrono::seconds(5), [this] { return m_loopActive.load(); });
        if (!ready) {
            VML_WARNING("invokeOnLoop: timeout waiting for loop to be active!");
            return;
        }
        VML_DEBUG("invokeOnLoop: loop is active");
    }

    pw_loop* loop = pw_main_loop_get_loop(m_loop);
    VML_DEBUG("invokeOnLoop: calling pw_loop_invoke");
    // pw_loop_invoke runs fn synchronously on the loop thread and blocks until done
    pw_loop_invoke(loop, loopInvokeCallback, 0, &fn, sizeof(fn), true, nullptr);
    VML_DEBUG("invokeOnLoop: pw_loop_invoke returned");
}

// --- Device enumeration ---

std::vector<AudioDevice> PipeWireDevice::enumerateDevices(const std::string& mediaClass) {
    std::vector<AudioDevice> devices;

    FILE* pipe = popen("pw-cli list-objects Node 2>/dev/null", "r");
    if (!pipe) return devices;

    std::string output;
    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe))
        output += buf.data();
    pclose(pipe);

    std::istringstream stream(output);
    std::string line;
    uint32_t currentId = 0;
    std::string currentName, currentDesc, currentClass;
    bool inNode = false;

    auto flushNode = [&]() {
        if (inNode && currentId != 0 && currentClass == mediaClass &&
            currentName.find("vml-") == std::string::npos) {
            AudioDevice dev;
            dev.id = currentId;
            dev.name = currentName;
            dev.description = currentDesc.empty() ? currentName : currentDesc;
            devices.push_back(dev);
        }
        currentId = 0;
        currentName.clear();
        currentDesc.clear();
        currentClass.clear();
    };

    while (std::getline(stream, line)) {
        if (line.find("id ") != std::string::npos && line.find("type") != std::string::npos) {
            flushNode();
            inNode = true;
            auto pos = line.find("id ");
            if (pos != std::string::npos)
                currentId = std::strtoul(line.c_str() + pos + 3, nullptr, 10);
        }
        auto eqPos = line.find(" = ");
        if (eqPos == std::string::npos) continue;
        std::string key = line.substr(0, eqPos);
        std::string val = line.substr(eqPos + 3);
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(0, 1);
        while (!val.empty() && val.back() == '\n') val.pop_back();
        if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
            val = val.substr(1, val.size() - 2);

        if (key == "node.name") currentName = val;
        else if (key == "node.description") currentDesc = val;
        else if (key == "media.class") currentClass = val;
    }
    flushNode();
    return devices;
}

std::vector<AudioDevice> PipeWireDevice::enumerateInputDevices() {
    auto sources = enumerateDevices("Audio/Source");
    auto duplex = enumerateDevices("Audio/Duplex");
    auto virtSrc = enumerateDevices("Audio/Source/Virtual");
    sources.insert(sources.end(), duplex.begin(), duplex.end());
    sources.insert(sources.end(), virtSrc.begin(), virtSrc.end());
    return sources;
}

std::vector<AudioDevice> PipeWireDevice::enumerateOutputDevices() {
    return enumerateDevices("Audio/Sink");
}

// --- Device selection (setters just store, reconnect dispatches to loop) ---

void PipeWireDevice::setInputDeviceId(uint32_t nodeId) {
    m_targetInputId = nodeId;
}

void PipeWireDevice::setMonitorOutputId(uint32_t nodeId) {
    m_targetMonitorId = nodeId;
}

void PipeWireDevice::setVirtualMicName(const std::string& name) {
    m_virtualMicName = name;
}

void PipeWireDevice::setMonitorEnabled(bool enabled) {
    m_monitorEnabled.store(enabled, std::memory_order_relaxed);
}

// --- Reconnect methods: all dispatched to PipeWire loop thread ---

void PipeWireDevice::reconnectCapture() {
    VML_DEBUG(QString("reconnectCapture: starting, targetInputId=%1").arg(m_targetInputId));
    if (!m_running.load() || !m_captureStream) {
        VML_DEBUG("reconnectCapture: early return (not running or no stream)");
        return;
    }
    VML_DEBUG("reconnectCapture: invoking on loop");
    invokeOnLoop([this]() {
        VML_DEBUG("reconnectCapture: on loop thread");

        uint8_t paramBuf[1024];
        const spa_pod* param;
        buildFormatParams(paramBuf, sizeof(paramBuf), &param);

        // Note: Don't call pw_stream_disconnect first - it blocks while audio is flowing.
        // Just call pw_stream_connect again with the new device; PipeWire handles it.
        VML_DEBUG("reconnectCapture: calling pw_stream_connect");
        int result = pw_stream_connect(m_captureStream,
            PW_DIRECTION_INPUT, m_targetInputId,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
            &param, 1);
        VML_DEBUG(QString("reconnectCapture: pw_stream_connect returned, result=%1").arg(result));
    });
    VML_DEBUG("reconnectCapture: done");
}

void PipeWireDevice::reconnectMonitor() {
    VML_DEBUG(QString("reconnectMonitor: starting, targetMonitorId=%1").arg(m_targetMonitorId));
    if (!m_running.load() || !m_monitorStream) {
        VML_DEBUG("reconnectMonitor: early return");
        return;
    }
    VML_DEBUG("reconnectMonitor: invoking on loop");
    invokeOnLoop([this]() {
        VML_DEBUG("reconnectMonitor: on loop thread");

        uint8_t paramBuf[1024];
        const spa_pod* param;
        buildFormatParams(paramBuf, sizeof(paramBuf), &param);

        // Don't call pw_stream_disconnect first - it blocks. Just reconnect.
        int result = pw_stream_connect(m_monitorStream,
            PW_DIRECTION_OUTPUT, m_targetMonitorId,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_TRIGGER),
            &param, 1);
        VML_DEBUG(QString("reconnectMonitor: pw_stream_connect returned, result=%1").arg(result));
    });
    VML_DEBUG("reconnectMonitor: done");
}

void PipeWireDevice::reconnectSource() {
    VML_DEBUG(QString("reconnectSource: starting, name=%1").arg(QString::fromStdString(m_virtualMicName)));
    if (!m_running.load() || !m_sourceStream) {
        VML_DEBUG("reconnectSource: early return");
        return;
    }
    VML_DEBUG("reconnectSource: invoking on loop");
    invokeOnLoop([this]() {
        VML_DEBUG("reconnectSource: on loop thread");

        uint8_t paramBuf[1024];
        const spa_pod* param;
        buildFormatParams(paramBuf, sizeof(paramBuf), &param);

        // Don't destroy and recreate - just reconnect. pw_stream_destroy blocks.
        // This only changes the virtual mic name; the stream itself stays connected.
        // Note: We can't change the stream properties without recreating, so
        // for now this just logs. Full virtual mic name changes need a different approach.
        VML_DEBUG("reconnectSource: stream reconnected (name update not implemented)");
    });
}

// --- Start / Stop ---

bool PipeWireDevice::start(AudioCallback callback) {
    VML_DEBUG("PipeWireDevice::start: creating main loop");
    m_callback = std::move(callback);

    m_loop = pw_main_loop_new(nullptr);
    if (!m_loop) return false;
    VML_DEBUG("PipeWireDevice::start: loop created");

    VML_DEBUG("PipeWireDevice::start: creating context");
    m_context = pw_context_new(pw_main_loop_get_loop(m_loop), nullptr, 0);
    if (!m_context) return false;
    VML_DEBUG("PipeWireDevice::start: context created");

    VML_DEBUG("PipeWireDevice::start: connecting to core");
    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) return false;
    VML_DEBUG("PipeWireDevice::start: core connected");

    uint8_t paramBuf[1024];
    const spa_pod* param;
    buildFormatParams(paramBuf, sizeof(paramBuf), &param);

    // Capture stream (from mic)
    VML_DEBUG("PipeWireDevice::start: creating capture stream");
    m_captureEvents = {};
    m_captureEvents.version = PW_VERSION_STREAM_EVENTS;
    m_captureEvents.process = onCaptureProcess;

    auto* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Capture",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, "vml-capture",
        PW_KEY_NODE_DESCRIPTION, "VML Capture",
        nullptr);

    m_captureStream = pw_stream_new(m_core, "VML Capture", props);
    spa_zero(m_captureHook);
    pw_stream_add_listener(m_captureStream, &m_captureHook, &m_captureEvents, this);
    VML_DEBUG(QString("PipeWireDevice::start: connecting capture stream to input %1").arg(m_targetInputId));
    pw_stream_connect(m_captureStream,
        PW_DIRECTION_INPUT, m_targetInputId,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        &param, 1);
    VML_DEBUG("PipeWireDevice::start: capture stream connected");

    // Virtual source stream (output as virtual mic)
    VML_DEBUG("PipeWireDevice::start: creating source stream");
    m_sourceEvents = {};
    m_sourceEvents.version = PW_VERSION_STREAM_EVENTS;
    m_sourceEvents.process = onSourceProcess;

    auto* srcProps = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_CLASS, "Audio/Source",
        PW_KEY_NODE_NAME, "vml-virtual-mic",
        PW_KEY_NODE_DESCRIPTION, m_virtualMicName.c_str(),
        nullptr);

    m_sourceStream = pw_stream_new(m_core, m_virtualMicName.c_str(), srcProps);
    spa_zero(m_sourceHook);
    pw_stream_add_listener(m_sourceStream, &m_sourceHook, &m_sourceEvents, this);
    VML_DEBUG("PipeWireDevice::start: connecting source stream");
    pw_stream_connect(m_sourceStream,
        PW_DIRECTION_OUTPUT, PW_ID_ANY,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_TRIGGER),
        &param, 1);
    VML_DEBUG("PipeWireDevice::start: source stream connected");

    // Monitor playback stream (to speakers)
    VML_DEBUG("PipeWireDevice::start: creating monitor stream");
    m_monitorEvents = {};
    m_monitorEvents.version = PW_VERSION_STREAM_EVENTS;
    m_monitorEvents.process = onMonitorProcess;

    auto* monProps = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, "vml-monitor",
        PW_KEY_NODE_DESCRIPTION, "VML Monitor",
        nullptr);

    m_monitorStream = pw_stream_new(m_core, "VML Monitor", monProps);
    spa_zero(m_monitorHook);
    pw_stream_add_listener(m_monitorStream, &m_monitorHook, &m_monitorEvents, this);
    VML_DEBUG(QString("PipeWireDevice::start: connecting monitor stream to output %1").arg(m_targetMonitorId));
    pw_stream_connect(m_monitorStream,
        PW_DIRECTION_OUTPUT, m_targetMonitorId,
        static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS | PW_STREAM_FLAG_TRIGGER),
        &param, 1);
    VML_DEBUG("PipeWireDevice::start: monitor stream connected");

    m_running.store(true);
    VML_DEBUG("PipeWireDevice::start: starting loop thread");
    std::thread([this]() { threadFunc(); }).detach();

    VML_DEBUG("PipeWireDevice::start: done");
    return true;
}

void PipeWireDevice::stop() {
    if (!m_running.load()) return;
    m_running.store(false);

    if (m_loop)
        pw_main_loop_quit(m_loop);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (m_captureStream) { pw_stream_destroy(m_captureStream); m_captureStream = nullptr; }
    if (m_sourceStream) { pw_stream_destroy(m_sourceStream); m_sourceStream = nullptr; }
    if (m_monitorStream) { pw_stream_destroy(m_monitorStream); m_monitorStream = nullptr; }
    if (m_core) { pw_core_disconnect(m_core); m_core = nullptr; }
    if (m_context) { pw_context_destroy(m_context); m_context = nullptr; }
    if (m_loop) { pw_main_loop_destroy(m_loop); m_loop = nullptr; }
}

void PipeWireDevice::threadFunc() {
    VML_DEBUG("threadFunc: loop thread starting");
    // Signal that the loop is now running
    {
        std::lock_guard<std::mutex> lock(m_loopMutex);
        m_loopActive = true;
    }
    m_loopCv.notify_all();
    VML_DEBUG("threadFunc: loop is active, entering pw_main_loop_run");
    pw_main_loop_run(m_loop);
    VML_DEBUG("threadFunc: loop exited");
}
