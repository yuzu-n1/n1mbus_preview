#pragma once
#include "modules/Module.hpp"
#include "plugin_api.hpp"
#include <string>
#include <vector>

// Adapter: PluginModule → Module
class PluginModuleAdapter : public Module {
    PluginModule* m_pm;
public:
    explicit PluginModuleAdapter(PluginModule* pm)
        : Module(pm->GetName(), pm->GetKeybind()), m_pm(pm) {}
    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        m_pm->OnUpdate();
    }
    void onEnable()  override { m_pm->OnEnable(); }
    void onDisable() override { m_pm->OnDisable(); }
};

class N1mbusPlugin;

// Scans plugins/ directory, loads all valid plugin DLLs.
std::vector<N1mbusPlugin*> LoadPlugins(const std::string& directory);

// Unload all plugins.
void UnloadPlugins();

// Reload all plugins (unload + load).
std::vector<N1mbusPlugin*> PluginLoader_ReloadAll(const std::string& directory);
