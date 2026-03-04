#pragma once
#include "modulation/Modulator.h"
#include <memory>
#include <mutex>
#include <vector>
#include <nlohmann/json.hpp>

class ModulationManager {
public:
    ModulationManager() = default;

    // Add a modulator. Returns index.
    int addModulator(const ModulatorConfig& config, std::atomic<float>* target);

    // Remove modulator by index
    void removeModulator(int index);

    // Remove all modulators targeting a given effect
    void removeModulatorsForEffect(const std::string& effectId);

    // Called from audio thread — advances all modulators
    void tick(float sampleRate, int numFrames);

    // Thread-safe snapshot of current configs
    std::vector<ModulatorConfig> snapshot() const;

    // Get modulator config for a specific effect param (returns nullptr if none)
    ModulatorConfig* findConfig(const std::string& effectId, const std::string& paramId);
    const ModulatorConfig* findConfig(const std::string& effectId, const std::string& paramId) const;

    // Check if a modulator exists and is active for this param
    bool hasActiveModulator(const std::string& effectId, const std::string& paramId) const;

    // Get current modulated value for display
    float currentValue(const std::string& effectId, const std::string& paramId) const;

    void clear();

    // Serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j,
        std::function<std::atomic<float>*(const std::string& effectId, const std::string& paramId)> resolver);

    int count() const;

private:
    struct Entry {
        std::unique_ptr<Modulator> modulator;
    };
    std::vector<Entry> m_entries;
    mutable std::mutex m_mutex;
};
