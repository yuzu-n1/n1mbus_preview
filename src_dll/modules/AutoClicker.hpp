#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <windows.h>
#include <chrono>
#include <random>
#include <algorithm>

extern HWND g_hWnd;
extern bool g_ShowMenu;

// Clicks are performed through the game's own click handler:
//   Minecraft.clickMouse() / rightClickMouse().
// LWJGL2 reads input via raw input, so PostMessage (WM_LBUTTONDOWN) is never
// seen by the game, and SendInput clicks get throttled/dropped by the game's
// internal leftClickCounter. Calling the vanilla click methods directly is the
// only reliable way to get controlled-CPS clicking (entity attacks, block
// breaking, swings). A SendInput fallback is used only if the mappings are
// unavailable.
class AutoClicker : public Module {
public:
    int  mode         = 0;    // 0=Left, 1=Right, 2=Both
    float  leftMinCps   = 10.0f;
    float  leftMaxCps   = 14.0f;
    float  rightMinCps  = 10.0f;
    float  rightMaxCps  = 14.0f;
    bool entityOnly   = false;

    AutoClicker() : Module("AutoClicker") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!g_hWnd || GetForegroundWindow() != g_hWnd) return;
        if (!env || !mcObj) return;
        if (g_ShowMenu) return;

        jclass mcClass = env->GetObjectClass(mcObj);
        if (!mcClass) return;

        // Skip while any Minecraft screen (inventory, chat, ...) is open
        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (currentScreenF) {
            jobject screenObj = env->GetObjectField(mcObj, currentScreenF);
            if (screenObj) { env->DeleteLocalRef(screenObj); env->DeleteLocalRef(mcClass); return; }
        }

        bool leftDown  = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        // Entity-only check (applies to left-click attacks). When enabled the
        // auto-click is skipped unless the crosshair is over an entity.
        if ((mode == 0 || mode == 2) && leftDown && entityOnly) {
            jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
            if (env->ExceptionCheck()) env->ExceptionClear();
            bool hasEntity = false;
            if (mopF) {
                jobject mopObj = env->GetObjectField(mcObj, mopF);
                if (mopObj) {
                    jclass mopClass = env->GetObjectClass(mopObj);
                    jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    if (entityHitF) {
                        jobject hitObj = env->GetObjectField(mopObj, entityHitF);
                        hasEntity = (hitObj != nullptr);
                        if (hitObj) env->DeleteLocalRef(hitObj);
                    }
                    env->DeleteLocalRef(mopClass);
                    env->DeleteLocalRef(mopObj);
                }
            }
            if (!hasEntity) { env->DeleteLocalRef(mcClass); return; }
        }

        jmethodID clickMeth  = env->GetMethodID(mcClass, Mappings::Minecraft_clickMouse_Name,     Mappings::Minecraft_clickMouse_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        jmethodID rightMeth  = env->GetMethodID(mcClass, Mappings::Minecraft_rightClickMouse_Name, Mappings::Minecraft_rightClickMouse_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();

        env->DeleteLocalRef(mcClass);

        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Left click
        if ((mode == 0 || mode == 2) && leftDown) {
            if (_clickAllowed(now, m_leftState, (int)leftMinCps, (int)leftMaxCps)) {
                if (clickMeth) {
                    env->CallVoidMethod(mcObj, clickMeth);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                } else {
                    _sendInputClick(MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP);
                }
            }
        }

        // Right click
        if ((mode == 1 || mode == 2) && rightDown) {
            if (_clickAllowed(now, m_rightState, (int)rightMinCps, (int)rightMaxCps)) {
                if (rightMeth) {
                    env->CallVoidMethod(mcObj, rightMeth);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                } else {
                    _sendInputClick(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
                }
            }
        }
    }

private:
    struct ClickState {
        long long lastTime = 0;
        int nextCps = 12;
    };
    ClickState m_leftState, m_rightState;

    static bool _clickAllowed(long long now, ClickState& st, int minCps, int maxCps) {
        if (st.lastTime == 0) st.lastTime = now;
        if (st.nextCps <= 0) st.nextCps = (minCps + maxCps) / 2;

        int cMin = minCps, cMax = maxCps;
        if (cMin > cMax) std::swap(cMin, cMax);
        if (cMin == cMax) st.nextCps = cMin;

        if ((now - st.lastTime) < (1000 / st.nextCps)) return false;

        st.lastTime = now;
        if (cMin < cMax)
            st.nextCps = cMin + (rand() % (cMax - cMin + 1));
        return true;
    }

    static void _sendInputClick(DWORD downFlag, DWORD upFlag) {
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE;
        in[0].mi.dwFlags = downFlag;
        in[1].type = INPUT_MOUSE;
        in[1].mi.dwFlags = upFlag;
        SendInput(2, in, sizeof(INPUT));
    }
};