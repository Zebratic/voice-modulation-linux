#include "effects/EffectRegistry.h"
#include "effects/EffectBase.h"
#include "effects/NoiseSuppressionEffect.h"

EffectRegistry& EffectRegistry::instance() {
    static EffectRegistry reg;
    return reg;
}

void EffectRegistry::registerEffect(const std::string& id, Factory factory) {
    m_entries.push_back({id, std::move(factory)});
}

std::unique_ptr<EffectBase> EffectRegistry::create(const std::string& id) const {
    for (auto& e : m_entries)
        if (e.id == id) return e.factory();
    return nullptr;
}

std::vector<std::string> EffectRegistry::availableEffects() const {
    std::vector<std::string> ids;
    ids.reserve(m_entries.size());
    for (auto& e : m_entries) ids.push_back(e.id);
    return ids;
}
