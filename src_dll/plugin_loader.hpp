#pragma once
#include <string>
#include <vector>

class N1mbusPlugin;

// Scans plugins/ directory, loads all valid plugin DLLs.
// Returns loaded plugin instances.
std::vector<N1mbusPlugin*> LoadPlugins(const std::string& directory);

// Unload all plugins (calls OnUnload, frees DLLs).
void UnloadPlugins();
