#include "plugin_loader.hpp"
#include "plugin_api.hpp"
#include <windows.h>
#include <vector>
#include <string>

struct PluginEntry {
    HMODULE dll = nullptr;
    N1mbusPlugin* instance = nullptr;
};

static std::vector<PluginEntry> g_LoadedPlugins;

using CreatePluginFn = N1mbusPlugin* (*)();
using GetApiVersionFn = int (*)();

std::vector<N1mbusPlugin*> LoadPlugins(const std::string& directory) {
    std::vector<N1mbusPlugin*> plugins;

    std::string searchPath = directory + "\\*.dll";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return plugins;

    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        std::string fullPath = directory + "\\" + ffd.cFileName;

        HMODULE hMod = LoadLibraryA(fullPath.c_str());
        if (!hMod) continue;

        // Check API version
        auto getVer = (GetApiVersionFn)GetProcAddress(hMod, "GetPluginApiVersion");
        if (!getVer || getVer() != N1MBUS_PLUGIN_API_VERSION) {
            FreeLibrary(hMod);
            continue;
        }

        // Create plugin instance
        auto createFn = (CreatePluginFn)GetProcAddress(hMod, "CreatePlugin");
        if (!createFn) { FreeLibrary(hMod); continue; }

        N1mbusPlugin* plugin = createFn();
        if (!plugin) { FreeLibrary(hMod); continue; }

        plugin->OnLoad();
        g_LoadedPlugins.push_back({ hMod, plugin });
        plugins.push_back(plugin);

    } while (FindNextFileA(hFind, &ffd));

    FindClose(hFind);
    return plugins;
}

void UnloadPlugins() {
    for (auto& entry : g_LoadedPlugins) {
        if (entry.instance) {
            entry.instance->OnUnload();
            delete entry.instance;
        }
        if (entry.dll) FreeLibrary(entry.dll);
    }
    g_LoadedPlugins.clear();
}

std::vector<N1mbusPlugin*> PluginLoader_ReloadAll(const std::string& directory) {
    UnloadPlugins();
    return LoadPlugins(directory);
}
