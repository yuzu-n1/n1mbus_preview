#include <windows.h>
#include <process.h>
#include "hook.hpp"

unsigned __stdcall MainThread(void* pReserved) {
    HMODULE hModule = static_cast<HMODULE>(pReserved);
    N1mbusHook::Initialize(hModule);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        _beginthreadex(nullptr, 0, MainThread, hModule, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
