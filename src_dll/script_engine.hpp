#pragma once
#include <string>
#include <vector>

class PluginModule;

// Set/clear JNI context for Lua C API on the current thread.
// Called from the render thread before/after script tick.
void SetLuaJniContext(void* env, void* mcObj, void* playerObj);
void ClearLuaJniContext();

// Lua script engine – loads and runs .lua scripts as modules.
class ScriptEngine {
public:
    ScriptEngine();
    ~ScriptEngine();

    bool Initialize();
    void Shutdown();

    // Load all .lua scripts from directory, returns created modules.
    std::vector<PluginModule*> LoadScripts(const std::string& directory);

    // Reload scripts (same Lua state, old refs stay valid)
    std::vector<PluginModule*> ReloadScripts(const std::string& directory);

    void* GetLuaState() const { return m_state; }

private:
    void* m_state = nullptr; // lua_State*
    bool m_initialized = false;
};
