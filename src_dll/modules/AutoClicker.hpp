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

class AutoClicker : public Module {
public:
    int  mode         = 0;    // 0=Left, 1=Right, 2=Both
    float  leftMinCps   = 10.0f;
    float  leftMaxCps   = 14.0f;
    float  rightMinCps  = 10.0f;
    float  rightMaxCps  = 14.0f;
    bool entityOnly   = true;

    AutoClicker() : Module("AutoClicker") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!g_hWnd || GetForegroundWindow() != g_hWnd) return;
        if (!env || !mcObj) return;
        if (g_ShowMenu) return;

        jclass mcClass = env->GetObjectClass(mcObj);
        if (!mcClass) return;

        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (currentScreenF) {
            jobject screenObj = env->GetObjectField(mcObj, currentScreenF);
            if (screenObj) { env->DeleteLocalRef(screenObj); env->DeleteLocalRef(mcClass); return; }
        }

        bool leftDown  = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

        // Entity-only check (applies to left-click attacks)
        if ((mode == 0 || mode == 2) && leftDown && entityOnly) {
            jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (mopF) {
                jobject mopObj = env->GetObjectField(mcObj, mopF);
                if (mopObj) {
                    jclass mopClass = env->GetObjectClass(mopObj);
                    jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
                    if (env->ExceptionCheck()) env->ExceptionClear();
                    bool hasEntity = false;
                    if (entityHitF) {
                        jobject hitObj = env->GetObjectField(mopObj, entityHitF);
                        hasEntity = (hitObj != nullptr);
                        if (hitObj) env->DeleteLocalRef(hitObj);
                    }
                    env->DeleteLocalRef(mopClass);
                    env->DeleteLocalRef(mopObj);
                    if (!hasEntity) { env->DeleteLocalRef(mcClass); return; }
                } else {
                    env->DeleteLocalRef(mcClass); return;
                }
            } else {
                env->DeleteLocalRef(mcClass); return;
            }
        }

        env->DeleteLocalRef(mcClass);

        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        POINT pos;
        GetCursorPos(&pos);
        ScreenToClient(g_hWnd, &pos);

        // Left click
        if ((mode == 0 || mode == 2) && leftDown) {
            _click(now, pos, m_leftState, WM_LBUTTONDOWN, WM_LBUTTONUP, MK_LBUTTON, (int)leftMinCps, (int)leftMaxCps);
        }

        // Right click
        if ((mode == 1 || mode == 2) && rightDown) {
            _click(now, pos, m_rightState, WM_RBUTTONDOWN, WM_RBUTTONUP, MK_RBUTTON, (int)rightMinCps, (int)rightMaxCps);
        }
    }

private:
    struct ClickState {
        long long lastTime = 0;
        int nextCps = 12;
    };
    ClickState m_leftState, m_rightState;

    static void _click(long long now, POINT pos, ClickState& st,
                       UINT downMsg, UINT upMsg, WPARAM wParam,
                       int minCps, int maxCps) {
        if (st.lastTime == 0) st.lastTime = now;
        if (st.nextCps <= 0) st.nextCps = (minCps + maxCps) / 2;

        int cMin = minCps, cMax = maxCps;
        if (cMin > cMax) std::swap(cMin, cMax);
        if (cMin == cMax) st.nextCps = cMin;

        if ((now - st.lastTime) < (1000 / st.nextCps)) return;

        PostMessage(g_hWnd, downMsg, wParam, MAKELPARAM(pos.x, pos.y));
        PostMessage(g_hWnd, upMsg, 0, MAKELPARAM(pos.x, pos.y));

        st.lastTime = now;
        if (cMin < cMax)
            st.nextCps = cMin + (rand() % (cMax - cMin + 1));
    }
};
