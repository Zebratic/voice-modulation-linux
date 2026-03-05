#include "modulation/ModulationManager.h"

using json = nlohmann::json;

int ModulationManager::addModulator(const ModulatorConfig& config, std::atomic<float>* target) {
    std::lock_guard lock(m_mutex);
    // Replace existing modulator for same effect+param
    for (size_t i = 0; i < m_entries.size(); ++i) {
        auto& cfg = m_entries[i].modulator->config();
        if (cfg.effectId == config.effectId && cfg.paramId == config.paramId) {
            m_entries[i].modulator = std::make_unique<Modulator>(config, target);
            return static_cast<int>(i);
        }
    }
    m_entries.push_back({std::make_unique<Modulator>(config, target)});
    return static_cast<int>(m_entries.size()) - 1;
}

void ModulationManager::removeModulator(int index) {
    std::lock_guard lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_entries.size()))
        m_entries.erase(m_entries.begin() + index);
}

void ModulationManager::removeModulatorsForEffect(const std::string& effectId) {
    std::lock_guard lock(m_mutex);
    m_entries.erase(
        std::remove_if(m_entries.begin(), m_entries.end(),
            [&](const Entry& e) { return e.modulator->config().effectId == effectId; }),
        m_entries.end());
}

void ModulationManager::tick(float sampleRate, int numFrames) {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries)
        e.modulator->tick(sampleRate, numFrames);
}

std::vector<ModulatorConfig> ModulationManager::snapshot() const {
    std::lock_guard lock(m_mutex);
    std::vector<ModulatorConfig> result;
    result.reserve(m_entries.size());
    for (auto& e : m_entries)
        result.push_back(e.modulator->config());
    return result;
}

ModulatorConfig* ModulationManager::findConfig(const std::string& effectId, const std::string& paramId) {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        if (cfg.effectId == effectId && cfg.paramId == paramId)
            return const_cast<ModulatorConfig*>(&e.modulator->config());
    }
    return nullptr;
}

const ModulatorConfig* ModulationManager::findConfig(const std::string& effectId, const std::string& paramId) const {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        if (cfg.effectId == effectId && cfg.paramId == paramId)
            return &cfg;
    }
    return nullptr;
}

bool ModulationManager::hasActiveModulator(const std::string& effectId, const std::string& paramId) const {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        if (cfg.effectId == effectId && cfg.paramId == paramId && cfg.active)
            return true;
    }
    return false;
}

float ModulationManager::currentValue(const std::string& effectId, const std::string& paramId) const {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        if (cfg.effectId == effectId && cfg.paramId == paramId)
            return e.modulator->currentValue();
    }
    return 0.f;
}

float ModulationManager::currentPhase(const std::string& effectId, const std::string& paramId) const {
    std::lock_guard lock(m_mutex);
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        if (cfg.effectId == effectId && cfg.paramId == paramId)
            return e.modulator->currentPhase();
    }
    return 0.f;
}

void ModulationManager::clear() {
    std::lock_guard lock(m_mutex);
    m_entries.clear();
}

int ModulationManager::count() const {
    std::lock_guard lock(m_mutex);
    return static_cast<int>(m_entries.size());
}

static std::string curveToString(ModCurve c) {
    switch (c) {
    case ModCurve::Linear: return "linear";
    case ModCurve::EaseInOut: return "ease_in_out";
    case ModCurve::Rubberband: return "rubberband";
    case ModCurve::Keyframe: return "keyframe";
    }
    return "linear";
}

static ModCurve stringToCurve(const std::string& s) {
    if (s == "ease_in_out") return ModCurve::EaseInOut;
    if (s == "rubberband") return ModCurve::Rubberband;
    if (s == "keyframe") return ModCurve::Keyframe;
    return ModCurve::Linear;
}

json ModulationManager::toJson() const {
    std::lock_guard lock(m_mutex);
    json arr = json::array();
    for (auto& e : m_entries) {
        auto& cfg = e.modulator->config();
        json mj;
        mj["effect_id"] = cfg.effectId;
        mj["param_id"] = cfg.paramId;
        mj["start_value"] = cfg.startValue;
        mj["end_value"] = cfg.endValue;
        mj["duration"] = cfg.durationSeconds;
        mj["curve"] = curveToString(cfg.curve);
        mj["active"] = cfg.active;
        if (cfg.curve == ModCurve::Keyframe && !cfg.keyframes.empty()) {
            json kfs = json::array();
            for (auto& kf : cfg.keyframes)
                kfs.push_back({{"pos", kf.position}, {"val", kf.value}});
            mj["keyframes"] = kfs;
        }
        arr.push_back(mj);
    }
    return arr;
}

void ModulationManager::fromJson(const json& j,
    std::function<std::atomic<float>*(const std::string&, const std::string&)> resolver) {
    std::lock_guard lock(m_mutex);
    m_entries.clear();
    if (!j.is_array()) return;

    for (auto& mj : j) {
        ModulatorConfig cfg;
        cfg.effectId = mj.value("effect_id", "");
        cfg.paramId = mj.value("param_id", "");
        cfg.startValue = mj.value("start_value", 0.f);
        cfg.endValue = mj.value("end_value", 1.f);
        cfg.durationSeconds = mj.value("duration", 2.f);
        cfg.curve = stringToCurve(mj.value("curve", "linear"));
        cfg.active = mj.value("active", true);

        if (mj.contains("keyframes") && mj["keyframes"].is_array()) {
            for (auto& kfj : mj["keyframes"])
                cfg.keyframes.push_back({kfj.value("pos", 0.f), kfj.value("val", 0.f)});
        }

        auto* target = resolver(cfg.effectId, cfg.paramId);
        if (target)
            m_entries.push_back({std::make_unique<Modulator>(cfg, target)});
    }
}
