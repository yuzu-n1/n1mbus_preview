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
    int minCps = 10;
    int maxCps = 14;
    bool entityOnly = true;  // Only click when looking at an entity

    AutoClicker() : Module("AutoClicker") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!g_hWnd || GetForegroundWindow() != g_hWnd) return;
        if (!env || !mcObj) return;

        // Prevent clicking while DLL UI is open
        if (g_ShowMenu) return;

        // Prevent clicking while Minecraft menu (inventory, chat, etc.) is open
        jclass mcClass = env->GetObjectClass(mcObj);
        if (!mcClass) return;
        
        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (currentScreenF) {
            jobject currentScreenObj = env->GetObjectField(mcObj, currentScreenF);
            if (currentScreenObj) {
                env->DeleteLocalRef(currentScreenObj);
                env->DeleteLocalRef(mcClass);
                return; // Minecraft menu is open
            }
        }

        // Must be holding left click
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        // Entity-only: check objectMouseOver for entity hit
        if (entityOnly && env && mcObj) {
            jclass mcClass = env->GetObjectClass(mcObj);
            if (!mcClass) return;
            jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (!mopF) { env->DeleteLocalRef(mcClass); return; }

            jobject mopObj = env->GetObjectField(mcObj, mopF);
            if (!mopObj) { env->DeleteLocalRef(mcClass); return; } // No target at all

            jclass mopClass = env->GetObjectClass(mopObj);
            jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
            if (env->ExceptionCheck()) env->ExceptionClear();

            bool lookingAtEntity = false;
            if (entityHitF) {
                jobject entityHitObj = env->GetObjectField(mopObj, entityHitF);
                lookingAtEntity = (entityHitObj != nullptr);
                if (entityHitObj) env->DeleteLocalRef(entityHitObj);
            }

            env->DeleteLocalRef(mopClass);
            env->DeleteLocalRef(mopObj);
            env->DeleteLocalRef(mcClass);
            if (env->ExceptionCheck()) env->ExceptionClear();

            if (!lookingAtEntity) return; // Looking at block or air -> skip
        }

        // CPS timing
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (m_lastClickTime == 0) m_lastClickTime = now;
        if (m_nextCps <= 0) m_nextCps = 10;

        if ((now - m_lastClickTime) < (1000 / m_nextCps)) return;

        // Send the click
        POINT pos;
        GetCursorPos(&pos);
        ScreenToClient(g_hWnd, &pos);
        PostMessage(g_hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pos.x, pos.y));
        PostMessage(g_hWnd, WM_LBUTTONUP, 0, MAKELPARAM(pos.x, pos.y));

        m_lastClickTime = now;

        // Randomize next interval
        int cMin = minCps, cMax = maxCps;
        if (cMin > cMax) std::swap(cMin, cMax);
        if (cMin == cMax) { m_nextCps = cMin; return; }
        m_nextCps = cMin + (rand() % (cMax - cMin + 1));
    }

private:
    long long m_lastClickTime = 0;
    int m_nextCps = 12;
};
