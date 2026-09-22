#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../mapping_resolver.hpp"
#include "../jni_manager.hpp"
#include <windows.h>
#include <chrono>
#include <random>
#include <algorithm>

extern HWND g_hWnd;
extern bool g_ShowMenu;
// Physical mouse state tracked by WndProcHook (not affected by our own SendInput calls)
extern bool g_PhysLMBDown;
extern bool g_PhysRMBDown;

// AutoClicker fires OS-level clicks via SendInput.
// Physical button state is read from g_PhysLMBDown/RMBDown (set by WndProcHook's
// WM_LBUTTONDOWN/UP) rather than GetAsyncKeyState, so that our own SendInput(LEFTUP)
// events do not falsely terminate the "button held" condition and break repeated clicks.
//
// Left-click fires only when the crosshair is over any LivingEntity (player or mob).
// Right-click fires unconditionally while RMB is held.
class AutoClicker : public Module {
public:
    int  mode         = 0;    // 0=Left, 1=Right, 2=Both
    float  leftMinCps   = 10.0f;
    float  leftMaxCps   = 14.0f;
    float  rightMinCps  = 10.0f;
    float  rightMaxCps  = 14.0f;
    bool entityOnly   = true; // left-click always requires crosshair on entity

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
            if (screenObj) {
                env->DeleteLocalRef(screenObj);
                env->DeleteLocalRef(mcClass);
                return;
            }
        }
        env->DeleteLocalRef(mcClass);

        long long now = m_clock();

        // Use physical button state (not GetAsyncKeyState) so our SendInput(UP)
        // events don't break the held-state detection next frame.
        bool leftDown  = g_PhysLMBDown;
        bool rightDown = g_PhysRMBDown;

        // Left click: fires only when crosshair is over any entity (player or mob)
        if ((mode == 0 || mode == 2) && leftDown) {
            if (_crosshairOverEntity(env, mcObj)) {
                if (m_leftState._allowed(now, (int)leftMinCps, (int)leftMaxCps)) {
                    _sendClick(true);
                }
            }
        }

        // Right click: OS-level, fires whenever RMB is held
        if ((mode == 1 || mode == 2) && rightDown) {
            if (m_rightState._allowed(now, (int)rightMinCps, (int)rightMaxCps)) {
                _sendClick(false);
            }
        }
    }

private:
    struct ClickState {
        long long lastTime = 0;
        int nextCps = 12;
        bool _allowed(long long now, int minCps, int maxCps) {
            if (lastTime == 0) lastTime = now;
            if (nextCps <= 0) nextCps = (minCps + maxCps) / 2;
            int cMin = minCps, cMax = maxCps;
            if (cMin > cMax) std::swap(cMin, cMax);
            if (cMin == cMax) nextCps = cMin;
            if ((now - lastTime) < (1000 / nextCps)) return false;
            lastTime = now;
            if (cMin < cMax) nextCps = cMin + (rand() % (cMax - cMin + 1));
            return true;
        }
    };
    ClickState m_leftState, m_rightState;

    static long long m_clock() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // Send an OS-level mouse click (DOWN + UP) via SendInput.
    // Appears on Lunar-style CPS counters and bypasses JNI-click detection ACs.
    // kAutoClickSentinel is stamped in dwExtraInfo so WndProcHook can ignore these
    // injected WM_LBUTTON* messages when updating g_PhysLMBDown.
    static void _sendClick(bool left) {
        constexpr ULONG_PTR kAutoClickSentinel = 0xA7C0C112UL;
        INPUT inputs[2] = {};
        inputs[0].type            = INPUT_MOUSE;
        inputs[0].mi.dwFlags      = left ? MOUSEEVENTF_LEFTDOWN  : MOUSEEVENTF_RIGHTDOWN;
        inputs[0].mi.dwExtraInfo  = kAutoClickSentinel;
        inputs[1].type            = INPUT_MOUSE;
        inputs[1].mi.dwFlags      = left ? MOUSEEVENTF_LEFTUP    : MOUSEEVENTF_RIGHTUP;
        inputs[1].mi.dwExtraInfo  = kAutoClickSentinel;
        SendInput(2, inputs, sizeof(INPUT));
    }

    // Returns true when objectMouseOver.entityHit is non-null (any living entity:
    // player, zombie, creeper, etc.).  Self is excluded via the MOP field itself
    // (Minecraft never sets entityHit to the local player).
    bool _crosshairOverEntity(JNIEnv* env, jobject mcObj) {
        jclass mcClass = env->GetObjectClass(mcObj);
        if (!mcClass) return false;

        jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mcClass);
        if (!mopF) return false;

        jobject mopObj = env->GetObjectField(mcObj, mopF);
        if (!mopObj) return false;

        jclass mopClass = env->GetObjectClass(mopObj);
        jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mopClass);

        bool hit = false;
        if (entityHitF) {
            jobject hitObj = env->GetObjectField(mopObj, entityHitF);
            if (hitObj) {
                hit = true;
                env->DeleteLocalRef(hitObj);
            }
        }
        env->DeleteLocalRef(mopObj);
        return hit;
    }
};