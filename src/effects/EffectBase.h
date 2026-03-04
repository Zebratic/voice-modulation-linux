#pragma once
#include <atomic>
#include <string>
#include <vector>

struct EffectParam {
    std::string id;
    std::string name;
    float min;
    float max;
    float defaultValue;
    std::atomic<float> value;

    EffectParam(std::string id_, std::string name_, float min_, float max_, float def)
        : id(std::move(id_)), name(std::move(name_)), min(min_), max(max_),
          defaultValue(def), value(def) {}

    EffectParam(const EffectParam& o)
        : id(o.id), name(o.name), min(o.min), max(o.max),
          defaultValue(o.defaultValue), value(o.value.load()) {}
};

class EffectBase {
public:
    virtual ~EffectBase() = default;

    virtual std::string id() const = 0;
    virtual std::string name() const = 0;

    // Called once when added to pipeline
    virtual void prepare(float sampleRate, int blockSize) {
        m_sampleRate = sampleRate;
        m_blockSize = blockSize;
    }

    // Process audio in-place. Must be RT-safe: no allocations, no locks.
    virtual void process(float* data, int numFrames) = 0;

    // Reset internal state (e.g. delay buffers)
    virtual void reset() {}

    virtual std::vector<EffectParam>& params() { return m_params; }

    void setEnabled(bool e) { m_enabled.store(e, std::memory_order_relaxed); }
    bool enabled() const { return m_enabled.load(std::memory_order_relaxed); }

protected:
    float m_sampleRate = 48000.f;
    int m_blockSize = 256;
    std::vector<EffectParam> m_params;
    std::atomic<bool> m_enabled{true};
};
