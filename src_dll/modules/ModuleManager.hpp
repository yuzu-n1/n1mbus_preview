#pragma once
#include "Module.hpp"
#include <vector>
#include <memory>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  ModuleManager  – owns all Module instances, dispatches per-frame ticks
// ─────────────────────────────────────────────────────────────────────────────
class ModuleManager {
public:
    static ModuleManager& get() {
        static ModuleManager instance;
        return instance;
    }

    // Register a module (call once at startup)
    template<typename T, typename... Args>
    T* add(Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        m_modules.push_back(std::move(ptr));
        return raw;
    }

    // Dispatch onUpdate to all enabled modules
    void tickAll(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) {
        for (auto& m : m_modules)
            if (m->isEnabled())
                m->onUpdate(env, mcObj, playerObj, playerClass);
    }

    // Retrieve by name (case-sensitive)
    Module* find(const std::string& name) {
        for (auto& m : m_modules)
            if (m->getName() == name) return m.get();
        return nullptr;
    }

    const std::vector<std::unique_ptr<Module>>& all() const { return m_modules; }

private:
    ModuleManager() = default;
    std::vector<std::unique_ptr<Module>> m_modules;
};
