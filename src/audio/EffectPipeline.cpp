#include "audio/EffectPipeline.h"
#include "modulation/ModulationManager.h"
#include <algorithm>
#include <cmath>

void EffectPipeline::prepare(float sampleRate, int blockSize) {
    std::lock_guard lock(m_mutex);
    m_sampleRate = sampleRate;
    m_blockSize = blockSize;
    for (auto& e : m_chain)
        e->prepare(sampleRate, blockSize);
}

void EffectPipeline::process(float* data, int numFrames) {
    // Process through chain — effects read enabled() atomically
    std::lock_guard lock(m_mutex);

    // Tick modulators before processing effects
    if (m_modulationMgr)
        m_modulationMgr->tick(m_sampleRate, numFrames);

    for (auto& e : m_chain) {
        if (e->enabled())
            e->process(data, numFrames);
    }

    // Calculate peak level
    float peak = 0.f;
    for (int i = 0; i < numFrames; ++i)
        peak = std::max(peak, std::abs(data[i]));
    m_peakLevel.store(peak, std::memory_order_relaxed);
}

void EffectPipeline::addEffect(std::unique_ptr<EffectBase> effect) {
    std::lock_guard lock(m_mutex);
    effect->prepare(m_sampleRate, m_blockSize);
    m_chain.push_back(std::move(effect));
}

void EffectPipeline::removeEffect(int index) {
    std::lock_guard lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_chain.size()))
        m_chain.erase(m_chain.begin() + index);
}

void EffectPipeline::moveEffect(int from, int to) {
    std::lock_guard lock(m_mutex);
    int size = static_cast<int>(m_chain.size());
    if (from < 0 || from >= size || to < 0 || to >= size || from == to) return;

    auto tmp = std::move(m_chain[from]);
    m_chain.erase(m_chain.begin() + from);
    m_chain.insert(m_chain.begin() + to, std::move(tmp));
}

void EffectPipeline::clear() {
    std::lock_guard lock(m_mutex);
    m_chain.clear();
}

int EffectPipeline::effectCount() const {
    std::lock_guard lock(m_mutex);
    return static_cast<int>(m_chain.size());
}

EffectBase* EffectPipeline::effectAt(int index) const {
    std::lock_guard lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_chain.size()))
        return m_chain[index].get();
    return nullptr;
}
