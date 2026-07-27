#pragma once
#include <string>
#include <vector>

class PluginModule;

// JNI context for Lua C API – set before calling script OnUpdate
extern thread_local void* g_luaJniEnv;
extern thread_local void* g_luaMcObj;
extern thread_local void* g_luaPlayerObj;

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
