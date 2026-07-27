#include "../../src_dll/plugin_api.hpp"

class TestModule : public PluginModule {
public:
    const char* GetName() const override { return "TestMod"; }
    const char* GetDescription() const override { return "Plugin test module"; }
    const char* GetCategory() const override { return "Plugins"; }
    int  GetKeybind() const override { return 0x54; } // T key
    void OnUpdate() override {
        // called every frame while enabled
    }
};

class TestPlugin : public N1mbusPlugin {
    TestModule m_mod;
public:
    const char* GetName() const override { return "TestPlugin"; }
    const char* GetAuthor() const override { return "n1mbus"; }
    void GetModules(std::vector<PluginModule*>& modules) override {
        modules = { &m_mod };
    }
};

extern "C" __declspec(dllexport) N1mbusPlugin* CreatePlugin() {
    return new TestPlugin();
}
extern "C" __declspec(dllexport) int GetPluginApiVersion() {
    return N1MBUS_PLUGIN_API_VERSION;
}
