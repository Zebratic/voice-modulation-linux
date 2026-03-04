#include "modulation/Modulator.h"
#include <cmath>
#include <algorithm>

Modulator::Modulator(const ModulatorConfig& config, std::atomic<float>* target)
    : m_config(config), m_target(target) {}

void Modulator::tick(float sampleRate, int numFrames) {
    if (!m_config.active || !m_target || m_config.durationSeconds <= 0.f)
        return;

    float phaseInc = static_cast<float>(numFrames) / (sampleRate * m_config.durationSeconds);
    m_phase += phaseInc;

    // Ping-pong: reflect at boundaries for continuous triangle wave
    while (m_phase > 2.f) m_phase -= 2.f;
    float t = m_phase <= 1.f ? m_phase : 2.f - m_phase;

    float curved = evaluateCurve(t);
    m_currentValue = m_config.startValue + (m_config.endValue - m_config.startValue) * curved;
    m_target->store(m_currentValue, std::memory_order_relaxed);
}

float Modulator::evaluateCurve(float t) const {
    t = std::clamp(t, 0.f, 1.f);

    switch (m_config.curve) {
    case ModCurve::Linear:
        return t;

    case ModCurve::EaseInOut:
        return t * t * (3.f - 2.f * t); // smoothstep

    case ModCurve::Rubberband:
        return 1.f - std::pow(2.f, -8.f * t);

    case ModCurve::Keyframe: {
        if (m_config.keyframes.empty()) return t;
        if (m_config.keyframes.size() == 1) return m_config.keyframes[0].value;

        // Find surrounding keyframes and interpolate
        const auto& kfs = m_config.keyframes;
        if (t <= kfs.front().position) return kfs.front().value;
        if (t >= kfs.back().position) return kfs.back().value;

        for (size_t i = 0; i + 1 < kfs.size(); ++i) {
            if (t >= kfs[i].position && t <= kfs[i + 1].position) {
                float segLen = kfs[i + 1].position - kfs[i].position;
                if (segLen <= 0.f) return kfs[i].value;
                float segT = (t - kfs[i].position) / segLen;
                return kfs[i].value + (kfs[i + 1].value - kfs[i].value) * segT;
            }
        }
        return kfs.back().value;
    }
    }
    return t;
}
