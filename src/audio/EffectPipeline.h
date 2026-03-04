#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include "effects/EffectBase.h"

class ModulationManager;

class EffectPipeline {
public:
    EffectPipeline() = default;

    void prepare(float sampleRate, int blockSize);

    // RT-safe: processes through the current chain in-place
    void process(float* data, int numFrames);

    // Non-RT: modify chain (called from GUI thread)
    void addEffect(std::unique_ptr<EffectBase> effect);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    void clear();

    int effectCount() const;
    EffectBase* effectAt(int index) const;

    // Peak level of last processed block (for VU meter)
    float lastPeakLevel() const { return m_peakLevel.load(std::memory_order_relaxed); }

    void setModulationManager(ModulationManager* mgr) { m_modulationMgr = mgr; }

private:
    // Double-buffer: GUI builds new chain, then swaps pointer atomically
    using Chain = std::vector<std::unique_ptr<EffectBase>>;
    Chain m_chain;
    mutable std::mutex m_mutex; // only held by non-RT operations
    float m_sampleRate = 48000.f;
    int m_blockSize = 256;
    std::atomic<float> m_peakLevel{0.f};
    ModulationManager* m_modulationMgr = nullptr;
};
