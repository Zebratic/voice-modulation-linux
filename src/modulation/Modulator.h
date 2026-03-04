#pragma once
#include <atomic>
#include <string>
#include <vector>

enum class ModCurve {
    Linear,
    EaseInOut,
    Rubberband,
    Keyframe
};

struct Keyframe {
    float position; // 0.0 - 1.0 (normalized time)
    float value;    // 0.0 - 1.0 (normalized value)
};

struct ModulatorConfig {
    std::string effectId;
    std::string paramId;
    float startValue = 0.f;
    float endValue = 1.f;
    float durationSeconds = 2.f;
    ModCurve curve = ModCurve::Linear;
    std::vector<Keyframe> keyframes; // used when curve == Keyframe
    bool active = true;
};

class Modulator {
public:
    explicit Modulator(const ModulatorConfig& config, std::atomic<float>* target);

    void tick(float sampleRate, int numFrames);
    void reset() { m_phase = 0.f; }

    const ModulatorConfig& config() const { return m_config; }
    void setConfig(const ModulatorConfig& config) { m_config = config; }

    std::atomic<float>* target() const { return m_target; }
    void setTarget(std::atomic<float>* t) { m_target = t; }

    float currentValue() const { return m_currentValue; }

private:
    float evaluateCurve(float t) const;

    ModulatorConfig m_config;
    std::atomic<float>* m_target;
    float m_phase = 0.f;
    float m_currentValue = 0.f;
};
