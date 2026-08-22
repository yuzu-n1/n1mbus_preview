#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <windows.h>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  Fly  –  Four modes:
//    0 Vanilla  – sets allowFlying/isFlying capabilities flag (Minecraft's own
//                 built-in flight, exactly like creative mode)
//    1 Glide    – allows flight but applies a gentle downward drift while no
//                 vertical key is held (parachute feel)
//    2 Freeze   – full hover; vertical motion only while SPACE/SHIFT held,
//                 horizontal motion zeroed when no WASD pressed
//    3 Smooth   – vanilla caps + smooth acceleration achieved by scaling
//                 motionX/Z toward the desired vector each tick
// ─────────────────────────────────────────────────────────────────────────────
class Fly : public Module {
public:
    int   mode  = 0;      // 0=Vanilla 1=Glide 2=Freeze 3=Smooth
    float speed = 1.0f;   // Speed multiplier (1.0 = default creative speed)

    Fly() : Module("Fly") {}

    void onDisable() override {
        // Forcibly reset flight state on next tick via wasFlying flag
        m_resetNeeded = true;
    }

    void onUpdate(JNIEnv* env, jobject /*mcObj*/, jobject playerObj, jclass playerClass) override {
        if (!env || !playerObj || !playerClass) return;

        // ── capabilities ─────────────────────────────────────────────────────
        jfieldID capField = env->GetFieldID(playerClass,
            Mappings::EntityPlayer_capabilities_Name,
            Mappings::EntityPlayer_capabilities_Sig);
        if (!capField) { _clearEx(env); return; }

        jobject capObj = env->GetObjectField(playerObj, capField);
        if (!capObj)   { _clearEx(env); return; }

        jclass capClass = env->GetObjectClass(capObj);

        jfieldID isFlyingField    = env->GetFieldID(capClass, Mappings::PlayerCapabilities_isFlying_Name,    Mappings::PlayerCapabilities_isFlying_Sig);
        jfieldID allowFlyingField = env->GetFieldID(capClass, Mappings::PlayerCapabilities_allowFlying_Name, Mappings::PlayerCapabilities_allowFlying_Sig);
        jfieldID flySpeedField    = env->GetFieldID(capClass, Mappings::PlayerCapabilities_flySpeed_Name,    Mappings::PlayerCapabilities_flySpeed_Sig);

        if (!m_resetNeeded) {
            // Enable flight & set speed
            if (allowFlyingField) env->SetBooleanField(capObj, allowFlyingField, JNI_TRUE);
            if (isFlyingField)    env->SetBooleanField(capObj, isFlyingField,    JNI_TRUE);
            if (flySpeedField)    env->SetFloatField  (capObj, flySpeedField,    speed * 0.05f);
        } else {
            // Reset back to survival
            if (allowFlyingField) env->SetBooleanField(capObj, allowFlyingField, JNI_FALSE);
            if (isFlyingField)    env->SetBooleanField(capObj, isFlyingField,    JNI_FALSE);
            if (flySpeedField)    env->SetFloatField  (capObj, flySpeedField,    0.05f);
            m_resetNeeded = false;
            env->DeleteLocalRef(capClass);
            env->DeleteLocalRef(capObj);
            return;
        }

        env->DeleteLocalRef(capClass);
        env->DeleteLocalRef(capObj);

        // ── mode-specific motion ──────────────────────────────────────────────
        if (mode == 1 || mode == 2 || mode == 3) {
            jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
            if (!entityClass) { _clearEx(env); return; }

            jfieldID mXf = env->GetFieldID(entityClass, Mappings::Entity_motionX_Name, Mappings::Entity_motionX_Sig);
            jfieldID mYf = env->GetFieldID(entityClass, Mappings::Entity_motionY_Name, Mappings::Entity_motionY_Sig);
            jfieldID mZf = env->GetFieldID(entityClass, Mappings::Entity_motionZ_Name, Mappings::Entity_motionZ_Sig);
            _clearEx(env);

            bool space = (GetAsyncKeyState(VK_SPACE)  & 0x8000) != 0;
            bool shift = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
            bool wasd  = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('A') & 0x8000) ||
                         (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState('D') & 0x8000);

            if (mode == 1 && mYf) {
                // Glide: gentle drift downward when coasting
                if (!space && !shift)
                    env->SetDoubleField(playerObj, mYf, -0.04);
            } else if (mode == 2) {
                // Freeze: zero everything unless key held
                if (mYf) {
                    double vert = (double)(speed * 0.25);
                    if      (space) env->SetDoubleField(playerObj, mYf,  vert);
                    else if (shift) env->SetDoubleField(playerObj, mYf, -vert);
                    else            env->SetDoubleField(playerObj, mYf,  0.0);
                }
                if (!wasd) {
                    if (mXf) env->SetDoubleField(playerObj, mXf, 0.0);
                    if (mZf) env->SetDoubleField(playerObj, mZf, 0.0);
                }
            } else if (mode == 3 && mXf && mYf && mZf) {
                // Smooth: lerp horizontal motion toward zero when no key input
                if (!wasd) {
                    double mx = env->GetDoubleField(playerObj, mXf);
                    double mz = env->GetDoubleField(playerObj, mZf);
                    env->SetDoubleField(playerObj, mXf, mx * 0.7);
                    env->SetDoubleField(playerObj, mZf, mz * 0.7);
                }
            }

            env->DeleteLocalRef(entityClass);
        }
        _clearEx(env);
    }

private:
    bool m_resetNeeded = false;
    static void _clearEx(JNIEnv* env) { if (env->ExceptionCheck()) env->ExceptionClear(); }
};
