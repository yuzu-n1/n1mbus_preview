#include "script_engine.hpp"
#include "plugin_api.hpp"
#include <string>
#include <vector>
#include <cstdio>
#include <windows.h>
#include <cmath>
#include <cfloat>

#include "minecraft_mappings.hpp"
#include "jni_manager.hpp"
#include "mc.hpp"
#include "mc_register.hpp"

// JNI context for Lua C API – set before calling script OnUpdate
thread_local void* g_luaJniEnv    = nullptr;
thread_local void* g_luaMcObj     = nullptr;
thread_local void* g_luaPlayerObj = nullptr;

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

// ── Lua C API: mc.* helpers ───────────────────────────────────

static JNIEnv* L_env(lua_State* L) {
    return (JNIEnv*)g_luaJniEnv;
}
static jobject L_mc(lua_State* L) {
    return (jobject)g_luaMcObj;
}
static jobject L_player(lua_State* L) {
    return (jobject)g_luaPlayerObj;
}

static int push_entity_table(lua_State* L, JNIEnv* env, jobject entity, int listIndex = -1) {
    if (!entity) return 0;
    lua_newtable(L);

    jclass entityClass = env->GetObjectClass(entity);

    // name
    jmethodID getName = MC::methodID(env, entityClass, "Entity.getName");
    if (getName) {
        jstring nameStr = (jstring)env->CallObjectMethod(entity, getName);
        if (nameStr) {
            const char* nameC = env->GetStringUTFChars(nameStr, nullptr);
            lua_pushstring(L, nameC);
            lua_setfield(L, -2, "name");
            env->ReleaseStringUTFChars(nameStr, nameC);
            env->DeleteLocalRef(nameStr);
        }
    }

    // posX/Y/Z fields
    jfieldID posXf = MC::fieldID(env, entityClass, "Entity.posX", "D");
    jfieldID posYf = MC::fieldID(env, entityClass, "Entity.posY", "D");
    jfieldID posZf = MC::fieldID(env, entityClass, "Entity.posZ", "D");
    if (posXf && posYf && posZf) {
        lua_pushnumber(L, env->GetDoubleField(entity, posXf));
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, env->GetDoubleField(entity, posYf));
        lua_setfield(L, -2, "y");
        lua_pushnumber(L, env->GetDoubleField(entity, posZf));
        lua_setfield(L, -2, "z");
    }

    // rotationYaw/Pitch fields
    jfieldID yawF = MC::fieldID(env, entityClass, "Entity.rotationYaw", "F");
    jfieldID pitchF = MC::fieldID(env, entityClass, "Entity.rotationPitch", "F");
    if (yawF && pitchF) {
        lua_pushnumber(L, env->GetFloatField(entity, yawF));
        lua_setfield(L, -2, "yaw");
        lua_pushnumber(L, env->GetFloatField(entity, pitchF));
        lua_setfield(L, -2, "pitch");
    }

    // health
    jmethodID getHealth = MC::methodID(env, entityClass, "EntityLivingBase.getHealth");
    if (getHealth) {
        lua_pushnumber(L, env->CallFloatMethod(entity, getHealth));
        lua_setfield(L, -2, "hp");
    } else {
        lua_pushnumber(L, 20.0f);
        lua_setfield(L, -2, "hp");
    }

    // sequential index (not entity ID)
    if (listIndex >= 0) {
        lua_pushinteger(L, listIndex);
        lua_setfield(L, -2, "index");
    }

    env->DeleteLocalRef(entityClass);
    return 1;
}

static int l_mc_get_player(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject player = L_player(L);
    if (!player) return 0;
    return push_entity_table(L, env, player);
}

static int l_mc_get_entities(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject mcObj = L_mc(L);
    if (!mcObj) return 0;

    jclass mcClass = env->GetObjectClass(mcObj);
    jfieldID worldF = MC::fieldID(env, mcClass, "Minecraft.theWorld",
        ("L" + std::string(MC_Register::WORLD_CLI) + ";").c_str());
    env->DeleteLocalRef(mcClass);
    if (!worldF) return 0;

    jobject world = env->GetObjectField(mcObj, worldF);
    if (!world) return 0;

    jclass worldClass = env->GetObjectClass(world);
    jfieldID entitiesF = MC::fieldID(env, worldClass, "World.loadedEntityList", "Ljava/util/List;");
    env->DeleteLocalRef(worldClass);
    if (!entitiesF) { env->DeleteLocalRef(world); return 0; }

    jobject entities = env->GetObjectField(world, entitiesF);
    env->DeleteLocalRef(world);
    if (!entities) return 0;

    jclass listClass = env->GetObjectClass(entities);
    jmethodID listSize = MC::methodID(env, listClass, "List.size");
    jmethodID listGet  = MC::methodID(env, listClass, "List.get");
    env->DeleteLocalRef(listClass);

    lua_newtable(L);
    if (listSize && listGet) {
        jint size = env->CallIntMethod(entities, listSize);
        for (jint i = 0; i < size; ++i) {
            jobject e = env->CallObjectMethod(entities, listGet, i);
            if (e) {
                lua_pushinteger(L, i);
                push_entity_table(L, env, e, (int)i);
                lua_settable(L, -3);
                env->DeleteLocalRef(e);
            }
        }
    }
    env->DeleteLocalRef(entities);
    return 1;
}

static int l_mc_attack_by_index(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject mcObj = L_mc(L);
    jobject player = L_player(L);
    if (!mcObj || !player) return 0;

    int targetIndex = (int)lua_tointeger(L, 1);

    jclass mcClass = env->GetObjectClass(mcObj);

    // get world
    jfieldID worldF = MC::fieldID(env, mcClass, "Minecraft.theWorld",
        ("L" + std::string(MC_Register::WORLD_CLI) + ";").c_str());
    if (!worldF) { env->DeleteLocalRef(mcClass); return 0; }
    jobject world = env->GetObjectField(mcObj, worldF);

    // get player controller
    jfieldID ctrlF = MC::fieldID(env, mcClass, "Minecraft.playerController",
        ("L" + std::string("net/minecraft/client/multiplayer/PlayerControllerMP") + ";").c_str());
    env->DeleteLocalRef(mcClass);
    if (!ctrlF) { if (world) env->DeleteLocalRef(world); return 0; }
    jobject ctrl = env->GetObjectField(mcObj, ctrlF);

    if (!world || !ctrl) {
        if (world) env->DeleteLocalRef(world);
        if (ctrl) env->DeleteLocalRef(ctrl);
        return 0;
    }

    jclass worldClass = env->GetObjectClass(world);
    jfieldID entitiesF = MC::fieldID(env, worldClass, "World.loadedEntityList", "Ljava/util/List;");
    env->DeleteLocalRef(worldClass);
    if (!entitiesF) { env->DeleteLocalRef(world); env->DeleteLocalRef(ctrl); return 0; }

    jobject entities = env->GetObjectField(world, entitiesF);
    env->DeleteLocalRef(world);
    if (!entities) { env->DeleteLocalRef(ctrl); return 0; }

    jclass listClass = env->GetObjectClass(entities);
    jmethodID listSize = MC::methodID(env, listClass, "List.size");
    jmethodID listGet  = MC::methodID(env, listClass, "List.get");
    env->DeleteLocalRef(listClass);

    if (listSize && listGet) {
        jint size = env->CallIntMethod(entities, listSize);
        for (jint i = 0; i < size; ++i) {
            if (i == targetIndex) {
                jobject target = env->CallObjectMethod(entities, listGet, i);
                if (target) {
                    jclass ctrlClass = env->GetObjectClass(ctrl);
                    jmethodID attack = MC::methodID(env, ctrlClass, "PlayerControllerMP.attackEntity");
                    env->DeleteLocalRef(ctrlClass);
                    if (attack) {
                        env->CallVoidMethod(ctrl, attack, player, target);
                    }
                    env->DeleteLocalRef(target);
                }
                break;
            }
        }
    }
    env->DeleteLocalRef(entities);
    env->DeleteLocalRef(ctrl);
    return 0;
}

static int l_mc_swing(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject player = L_player(L);
    if (!player) return 0;

    jclass playerClass = env->GetObjectClass(player);
    jmethodID swing = MC::methodID(env, playerClass, "EntityLivingBase.swingItem");
    env->DeleteLocalRef(playerClass);
    if (swing) {
        env->CallVoidMethod(player, swing);
    }
    return 0;
}

static int l_mc_look_at(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject player = L_player(L);
    if (!player) return 0;

    double tx = lua_tonumber(L, 1);
    double ty = lua_tonumber(L, 2);
    double tz = lua_tonumber(L, 3);

    jclass entityClass = env->GetObjectClass(player);

    jfieldID posXf = MC::fieldID(env, entityClass, "Entity.posX", "D");
    jfieldID posYf = MC::fieldID(env, entityClass, "Entity.posY", "D");
    jfieldID posZf = MC::fieldID(env, entityClass, "Entity.posZ", "D");
    if (!posXf || !posYf || !posZf) { env->DeleteLocalRef(entityClass); return 0; }

    double px = env->GetDoubleField(player, posXf);
    double py = env->GetDoubleField(player, posYf);
    double pz = env->GetDoubleField(player, posZf);

    double dx = tx - px;
    double dy = ty - (py + 1.62);
    double dz = tz - pz;

    double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 0.001) { env->DeleteLocalRef(entityClass); return 0; }

    float yaw   = (float)(-atan2(dx, dz) * 180.0 / 3.141592653589793);
    float pitch = (float)(-asin(dy / dist) * 180.0 / 3.141592653589793);

    jfieldID yawF = MC::fieldID(env, entityClass, "Entity.rotationYaw", "F");
    jfieldID pitchF = MC::fieldID(env, entityClass, "Entity.rotationPitch", "F");
    jfieldID prevYawF = MC::fieldID(env, entityClass, "Entity.prevRotationYaw", "F");
    jfieldID prevPitchF = MC::fieldID(env, entityClass, "Entity.prevRotationPitch", "F");
    jfieldID renderYawF = MC::fieldID(env, entityClass, "Entity.renderYawOffset", "F");

    if (yawF)    env->SetFloatField(player, yawF, yaw);
    if (pitchF)  env->SetFloatField(player, pitchF, pitch);
    if (prevYawF)   env->SetFloatField(player, prevYawF, yaw);
    if (prevPitchF) env->SetFloatField(player, prevPitchF, pitch);
    if (renderYawF) env->SetFloatField(player, renderYawF, yaw);

    env->DeleteLocalRef(entityClass);
    return 0;
}

static int l_mc_find_closest(lua_State* L) {
    JNIEnv* env = L_env(L);
    jobject mcObj = L_mc(L);
    jobject player = L_player(L);
    if (!mcObj || !player) return 0;

    int filterRef = LUA_REFNIL;
    if (lua_isfunction(L, 1)) {
        lua_pushvalue(L, 1);
        filterRef = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    jclass mcClass = env->GetObjectClass(mcObj);
    jfieldID worldF = MC::fieldID(env, mcClass, "Minecraft.theWorld",
        ("L" + std::string(MC_Register::WORLD_CLI) + ";").c_str());
    env->DeleteLocalRef(mcClass);
    if (!worldF) { if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef); return 0; }

    jobject world = env->GetObjectField(mcObj, worldF);
    if (!world) { if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef); return 0; }

    jclass worldClass = env->GetObjectClass(world);
    jfieldID entitiesF = MC::fieldID(env, worldClass, "World.loadedEntityList", "Ljava/util/List;");
    env->DeleteLocalRef(worldClass);
    if (!entitiesF) { env->DeleteLocalRef(world); if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef); return 0; }

    jobject entities = env->GetObjectField(world, entitiesF);
    env->DeleteLocalRef(world);
    if (!entities) { if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef); return 0; }

    jclass listClass = env->GetObjectClass(entities);
    jmethodID listSize = MC::methodID(env, listClass, "List.size");
    jmethodID listGet  = MC::methodID(env, listClass, "List.get");
    env->DeleteLocalRef(listClass);

    jclass playerClass = env->GetObjectClass(player);
    jfieldID pposXf = MC::fieldID(env, playerClass, "Entity.posX", "D");
    jfieldID pposYf = MC::fieldID(env, playerClass, "Entity.posY", "D");
    jfieldID pposZf = MC::fieldID(env, playerClass, "Entity.posZ", "D");
    env->DeleteLocalRef(playerClass);
    if (!pposXf || !pposYf || !pposZf) {
        env->DeleteLocalRef(entities);
        if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef);
        return 0;
    }

    double px = env->GetDoubleField(player, pposXf);
    double py = env->GetDoubleField(player, pposYf);
    double pz = env->GetDoubleField(player, pposZf);

    double bestDist = DBL_MAX;
    jobject bestEntity = nullptr;

    if (listSize && listGet) {
        jint size = env->CallIntMethod(entities, listSize);
        for (jint i = 0; i < size; ++i) {
            jobject e = env->CallObjectMethod(entities, listGet, i);
            if (!e || e == player) { if (e) env->DeleteLocalRef(e); continue; }

            // filter
            bool skip = false;
            if (filterRef != LUA_REFNIL) {
                lua_rawgeti(L, LUA_REGISTRYINDEX, filterRef);
                push_entity_table(L, env, e, (int)i);
                if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
                    lua_pop(L, 1);
                    skip = true;
                } else if (!lua_toboolean(L, -1)) {
                    skip = true;
                }
                lua_pop(L, 1);
            }
            if (skip) { env->DeleteLocalRef(e); continue; }

            jclass eClass = env->GetObjectClass(e);
            jfieldID exf = MC::fieldID(env, eClass, "Entity.posX", "D");
            jfieldID eyf = MC::fieldID(env, eClass, "Entity.posY", "D");
            jfieldID ezf = MC::fieldID(env, eClass, "Entity.posZ", "D");
            env->DeleteLocalRef(eClass);

            if (exf && eyf && ezf) {
                double ex = env->GetDoubleField(e, exf);
                double ey = env->GetDoubleField(e, eyf);
                double ez = env->GetDoubleField(e, ezf);
                double d = (ex-px)*(ex-px) + (ey-py)*(ey-py) + (ez-pz)*(ez-pz);
                if (d < bestDist) {
                    bestDist = d;
                    if (bestEntity) env->DeleteLocalRef(bestEntity);
                    bestEntity = env->NewLocalRef(e);
                }
            }
            env->DeleteLocalRef(e);
        }
    }
    env->DeleteLocalRef(entities);

    if (filterRef != LUA_REFNIL) luaL_unref(L, LUA_REGISTRYINDEX, filterRef);

    if (bestEntity) {
        int ret = push_entity_table(L, env, bestEntity);
        env->DeleteLocalRef(bestEntity);
        return ret;
    }
    return 0;
}

// ── Lua C API: input.* ────────────────────────────────────────

static int l_input_is_key_down(lua_State* L) {
    int vk = (int)lua_tointeger(L, 1);
    lua_pushboolean(L, (GetAsyncKeyState(vk) & 0x8000) != 0);
    return 1;
}

// ── ScriptEngine ──

ScriptEngine::ScriptEngine() {}
ScriptEngine::~ScriptEngine() { Shutdown(); }

bool ScriptEngine::Initialize() {
    if (m_initialized) return true;

    m_state = luaL_newstate();
    if (!m_state) return false;

    lua_State* L = (lua_State*)m_state;
    luaL_openlibs(L);

    // Register the `module` and `key` global tables
    lua_newtable(L);
    lua_setglobal(L, "module");

    // Key name → VK code mapping
    lua_newtable(L);
    struct KeyEntry { const char* name; int vk; };
    KeyEntry keys[] = {
        {"backspace",    0x08}, {"tab",       0x09}, {"enter",     0x0D},
        {"shift",        0x10}, {"ctrl",      0x11}, {"alt",       0x12},
        {"pause",        0x13}, {"caps",      0x14}, {"escape",    0x1B},
        {"space",        0x20}, {"page_up",   0x21}, {"page_down", 0x22},
        {"end",          0x23}, {"home",      0x24},
        {"left",         0x25}, {"up",        0x26}, {"right",     0x27}, {"down", 0x28},
        {"print",        0x2A}, {"prtsc",     0x2C}, {"insert",    0x2D}, {"del",  0x2E},
        {"0", 0x30},{"1", 0x31},{"2", 0x32},{"3", 0x33},{"4", 0x34},
        {"5", 0x35},{"6", 0x36},{"7", 0x37},{"8", 0x38},{"9", 0x39},
        {"a", 0x41},{"b", 0x42},{"c", 0x43},{"d", 0x44},{"e", 0x45},
        {"f", 0x46},{"g", 0x47},{"h", 0x48},{"i", 0x49},{"j", 0x4A},
        {"k", 0x4B},{"l", 0x4C},{"m", 0x4D},{"n", 0x4E},{"o", 0x4F},
        {"p", 0x50},{"q", 0x51},{"r", 0x52},{"s", 0x53},{"t", 0x54},
        {"u", 0x55},{"v", 0x56},{"w", 0x57},{"x", 0x58},{"y", 0x59},{"z", 0x5A},
        {"numpad_0", 0x60},{"numpad_1", 0x61},{"numpad_2", 0x62},{"numpad_3", 0x63},
        {"numpad_4", 0x64},{"numpad_5", 0x65},{"numpad_6", 0x66},{"numpad_7", 0x67},
        {"numpad_8", 0x68},{"numpad_9", 0x69},
        {"multiply", 0x6A},{"add", 0x6B},{"subtract", 0x6D},{"divide", 0x6F},
        {"f1",0x70},{"f2",0x71},{"f3",0x72},{"f4",0x73},{"f5",0x74},
        {"f6",0x75},{"f7",0x76},{"f8",0x77},{"f9",0x78},{"f10",0x79},
        {"f11",0x7A},{"f12",0x7B},{"f13",0x7C},{"f14",0x7D},{"f15",0x7E},
        {"scroll", 0x91}, {"numlock", 0x90},
        {"left_shift", 0xA0},{"right_shift", 0xA1},
        {"left_ctrl",  0xA2},{"right_ctrl",  0xA3},
        {"left_alt",   0xA4},{"right_alt",   0xA5},
        {"semicolon", 0xBA},{"equal", 0xBB},{"comma", 0xBC},{"minus", 0xBD},
        {"period", 0xBE},{"slash", 0xBF},{"tilde", 0xC0},
        {"lbracket", 0xDB},{"backslash", 0xDC},{"rbracket", 0xDD},{"quote", 0xDE},
    };
    for (auto& k : keys) {
        lua_pushstring(L, k.name);
        lua_pushinteger(L, k.vk);
        lua_settable(L, -3);
    }
    lua_setglobal(L, "key");

    // Register mc.* table
    lua_newtable(L);
    lua_pushcfunction(L, l_mc_get_player);    lua_setfield(L, -2, "get_player");
    lua_pushcfunction(L, l_mc_get_entities);  lua_setfield(L, -2, "get_entities");
    lua_pushcfunction(L, l_mc_attack_by_index); lua_setfield(L, -2, "attack_by_index");
    lua_pushcfunction(L, l_mc_swing);         lua_setfield(L, -2, "swing");
    lua_pushcfunction(L, l_mc_look_at);       lua_setfield(L, -2, "look_at");
    lua_pushcfunction(L, l_mc_find_closest);  lua_setfield(L, -2, "find_closest");
    lua_setglobal(L, "mc");

    // Register input.* table
    lua_newtable(L);
    lua_pushcfunction(L, l_input_is_key_down); lua_setfield(L, -2, "is_key_down");
    lua_setglobal(L, "input");

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
        if (lua_isinteger(L, -1)) {
            mod->setKeybind((int)lua_tointeger(L, -1));
        } else if (lua_isstring(L, -1)) {
            // Look up in the key table
            const char* kname = lua_tostring(L, -1);
            lua_getglobal(L, "key");
            lua_getfield(L, -1, kname);
            if (lua_isinteger(L, -1)) mod->setKeybind((int)lua_tointeger(L, -1));
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
        lua_pop(L, 1);

        lua_pop(L, 1); // pop table copy
        modules.push_back(mod);

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    return modules;
}

std::vector<PluginModule*> ScriptEngine::ReloadScripts(const std::string& directory) {
    return LoadScripts(directory);
}
