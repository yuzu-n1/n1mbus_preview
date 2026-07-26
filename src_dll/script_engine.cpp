#include "script_engine.hpp"
#include "plugin_api.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <windows.h>

extern "C" {
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

// ── ScriptModule: bridges a Lua table to PluginModule ──
class ScriptModule : public PluginModule {
public:
    ScriptModule(lua_State* L, int tableRef, const char* name)
        : m_L(L), m_tableRef(tableRef), m_name(name) {}
    ~ScriptModule() override {
        if (m_L && m_tableRef != LUA_REFNIL)
            luaL_unref(m_L, LUA_REGISTRYINDEX, m_tableRef);
    }

    const char* GetName()        const override { return m_name.c_str(); }
    const char* GetDescription()  const override { return m_desc.c_str(); }
    const char* GetCategory()     const override { return m_category.c_str(); }
    int         GetKeybind()      const override { return m_keybind; }

    void setDescription(const char* d) { m_desc = d ? d : ""; }
    void setCategory(const char* c)    { m_category = c ? c : "Scripts"; }
    void setKeybind(int k)             { m_keybind = k; }

    void OnEnable()  override { callFunc("onEnable");  }
    void OnDisable() override { callFunc("onDisable"); }
    void OnUpdate()  override { callFunc("onUpdate");  }

private:
    lua_State* m_L;
    int m_tableRef;
    std::string m_name, m_desc, m_category = "Scripts";
    int m_keybind = 0;

    void callFunc(const char* name) {
        if (!m_L) return;
        lua_rawgeti(m_L, LUA_REGISTRYINDEX, m_tableRef); // push table
        lua_getfield(m_L, -1, name);                     // push func
        if (lua_isfunction(m_L, -1)) {
            lua_pushvalue(m_L, -2);                      // push self (table) as arg
            if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(m_L, -1);
                if (err) OutputDebugStringA(err);
                lua_pop(m_L, 1);
            }
        } else {
            lua_pop(m_L, 1); // pop nil
        }
        lua_pop(m_L, 1); // pop table
    }
};

// ── ScriptEngine ──

ScriptEngine::ScriptEngine() {}
ScriptEngine::~ScriptEngine() { Shutdown(); }

bool ScriptEngine::Initialize() {
    if (m_initialized) return true;

    m_state = luaL_newstate();
    if (!m_state) return false;

    lua_State* L = (lua_State*)m_state;
    luaL_openlibs(L);

    // Register the `module` global table
    lua_newtable(L);
    lua_setglobal(L, "module");

    m_initialized = true;
    return true;
}

void ScriptEngine::Shutdown() {
    if (m_state) {
        lua_close((lua_State*)m_state);
        m_state = nullptr;
    }
    m_initialized = false;
}

std::vector<PluginModule*> ScriptEngine::LoadScripts(const std::string& directory) {
    std::vector<PluginModule*> modules;
    if (!m_initialized) return modules;

    lua_State* L = (lua_State*)m_state;

    std::string searchPath = directory + "\\*.lua";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return modules;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        std::string fullPath = directory + "\\" + ffd.cFileName;
        if (luaL_loadfile(L, fullPath.c_str()) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) OutputDebugStringA(err);
            lua_pop(L, 1);
            continue;
        }

        // Call the chunk – expect it to return a table
        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            const char* err = lua_tostring(L, -1);
            if (err) OutputDebugStringA(err);
            lua_pop(L, 1);
            continue;
        }

        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        // Read name (required)
        lua_getfield(L, -1, "name");
        const char* name = lua_tostring(L, -1);
        if (!name) { lua_pop(L, 2); continue; }

        // Create script module
        int ref = luaL_ref(L, LUA_REGISTRYINDEX); // pop & ref the table
        auto* mod = new ScriptModule(L, ref, name);

        // Optional fields
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        lua_getfield(L, -1, "description");
        if (lua_isstring(L, -1)) mod->setDescription(lua_tostring(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "category");
        if (lua_isstring(L, -1)) mod->setCategory(lua_tostring(L, -1));
        lua_pop(L, 1);

        lua_getfield(L, -1, "keybind");
        if (lua_isinteger(L, -1)) mod->setKeybind((int)lua_tointeger(L, -1));
        lua_pop(L, 1);

        lua_pop(L, 1); // pop table copy
        modules.push_back(mod);

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    return modules;
}
