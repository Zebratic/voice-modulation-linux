#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

class EffectBase;

class EffectRegistry {
public:
    using Factory = std::function<std::unique_ptr<EffectBase>()>;

    static EffectRegistry& instance();

    void registerEffect(const std::string& id, Factory factory);
    std::unique_ptr<EffectBase> create(const std::string& id) const;
    std::vector<std::string> availableEffects() const;

private:
    EffectRegistry() = default;
    struct Entry { std::string id; Factory factory; };
    std::vector<Entry> m_entries;
};

// Place VML_REGISTER_EFFECT(ClassName) in the .cpp file of each effect.
#define VML_REGISTER_EFFECT(Class) \
    static struct Class##_Registrar { \
        Class##_Registrar() { \
            EffectRegistry::instance().registerEffect( \
                Class().id(), []() -> std::unique_ptr<EffectBase> { return std::make_unique<Class>(); }); \
        } \
    } s_##Class##_registrar;
