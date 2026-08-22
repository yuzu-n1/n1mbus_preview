#pragma once
#include <string>
#include <vector>
#include <cstdint>

#define N1MBUS_PLUGIN_API_VERSION 1

// ── Theme colors (RGBA 0.0–1.0) ──
struct N1mbusThemeColors {
    float accentR = 0.3f, accentG = 0.6f, accentB = 1.0f, accentA = 1.0f;
    float accentSecondaryR = 0.2f, accentSecondaryG = 0.4f, accentSecondaryB = 0.8f, accentSecondaryA = 1.0f;
    float bgR = 0.04f, bgG = 0.09f, bgB = 0.12f, bgA = 1.0f;
    float textR = 0.8f, textG = 0.85f, textB = 0.9f, textA = 1.0f;
    float moduleActiveR = 0.2f, moduleActiveG = 0.7f, moduleActiveB = 0.3f, moduleActiveA = 1.0f;
    float moduleInactiveR = 0.5f, moduleInactiveG = 0.5f, moduleInactiveB = 0.5f, moduleInactiveA = 1.0f;
};

// ── Plugin module base ──
// Subclass this to create a new module from a plugin.
class PluginModule {
public:
    virtual ~PluginModule() = default;
    virtual const char* GetName() const = 0;
    virtual const char* GetDescription() const { return ""; }
    virtual const char* GetCategory() const { return "Plugins"; }
    virtual int  GetKeybind() const { return 0; } // VK_ code, 0 = none
    virtual void OnEnable()  {}
    virtual void OnDisable() {}
    virtual void OnUpdate()  {} // called every render frame while enabled
};

// ── Plugin base class ──
class N1mbusPlugin {
public:
    virtual ~N1mbusPlugin() = default;
    virtual const char* GetName()    const { return "Unnamed Plugin"; }
    virtual const char* GetAuthor()  const { return ""; }
    virtual const char* GetVersion() const { return "1.0"; }
    virtual void OnLoad()   {} // called right after plugin is loaded
    virtual void OnUnload() {} // called before plugin is unloaded
    // Fill modules with your PluginModule instances
    virtual void GetModules(std::vector<PluginModule*>& modules) { modules.clear(); }
    // Return true to override theme colors
    virtual bool GetTheme(N1mbusThemeColors& colors) { return false; }
};

// ── DLL entry point ──
// Every plugin DLL must export these two functions:
//   extern "C" __declspec(dllexport) N1mbusPlugin* CreatePlugin();
//   extern "C" __declspec(dllexport) int           GetPluginApiVersion();
