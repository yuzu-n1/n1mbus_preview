#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <algorithm>
#include <cmath>
#include <windows.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Speed  –  Horizontal movement acceleration
//
//  Modes:
//    0  Ground  – strafe boost applied only while on the ground (low motionY),
//                 giving a "ground speed" feel without flying.
//    1  Boost   – combines setSprinting(true) with a motion boost, similar to
//                 the classic "speed" hack used by legacy clients.
//    2  BHop    – Auto-jumps while moving and applies strafe speed in the air.
// ─────────────────────────────────────────────────────────────────────────────
class Speed : public Module {
public:
    int   mode       = 0;    // 0=Ground  1=Boost  2=BHop
    float multiplier = 1.3f; // Velocity scale factor

    Speed() : Module("Speed") {}

    void onUpdate(JNIEnv* env, jobject /*mcObj*/, jobject playerObj, jclass playerClass) override {
        if (!env || !playerObj || !playerClass) return;

        bool w = (GetAsyncKeyState('W') & 0x8000) != 0;
        bool a = (GetAsyncKeyState('A') & 0x8000) != 0;
        bool s = (GetAsyncKeyState('S') & 0x8000) != 0;
        bool d = (GetAsyncKeyState('D') & 0x8000) != 0;
        bool moving = w || a || s || d;

        jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
        if (!entityClass) { _ex(env); return; }

        jfieldID mXf = env->GetFieldID(entityClass, Mappings::Entity_motionX_Name, Mappings::Entity_motionX_Sig);
        jfieldID mYf = env->GetFieldID(entityClass, Mappings::Entity_motionY_Name, Mappings::Entity_motionY_Sig);
        jfieldID mZf = env->GetFieldID(entityClass, Mappings::Entity_motionZ_Name, Mappings::Entity_motionZ_Sig);
        jfieldID onGroundF = env->GetFieldID(entityClass, Mappings::Entity_onGround_Name, Mappings::Entity_onGround_Sig);
        jfieldID yawF = env->GetFieldID(entityClass, Mappings::Entity_rotationYaw_Name, Mappings::Entity_rotationYaw_Sig);
        _ex(env);

        if (!mXf || !mZf || !mYf || !onGroundF || !yawF) { env->DeleteLocalRef(entityClass); return; }

        bool onGround = env->GetBooleanField(playerObj, onGroundF) == JNI_TRUE;
        float yaw = env->GetFloatField(playerObj, yawF);

        // BHop mode: Jump if on ground and moving
        if (mode == 2 && onGround && moving) {
            env->SetDoubleField(playerObj, mYf, 0.42); // Vanilla jump velocity
            onGround = false; // airborne for speed calc
        }

        if (mode == 0 && !onGround) {
            // Ground mode only boosts while grounded
        } else {
            if (!moving) {
                // Stop instantly or let vanilla friction take over
                // env->SetDoubleField(playerObj, mXf, 0.0);
                // env->SetDoubleField(playerObj, mZf, 0.0);
            } else {
                double speedFactor = (mode == 1) ? 1.3 : 1.0;
                double mult = multiplier * speedFactor;
                double maxSpd = 0.28 * mult; // 0.28 is approx vanilla sprint speed

                double moveForward = 0.0;
                double moveStrafe = 0.0;
                if (w) moveForward += 1.0;
                if (s) moveForward -= 1.0;
                if (a) moveStrafe += 1.0; // Left
                if (d) moveStrafe -= 1.0; // Right

                if (moveForward != 0.0) {
                    if (moveStrafe > 0.0) yaw += (moveForward > 0.0) ? -45 : 45;
                    else if (moveStrafe < 0.0) yaw += (moveForward > 0.0) ? 45 : -45;
                    moveStrafe = 0.0;
                    if (moveForward > 0) moveForward = 1.0;
                    else if (moveForward < 0) moveForward = -1.0;
                }

                double rad = yaw * (3.14159265358979323846 / 180.0);
                double sinYaw = std::sin(rad);
                double cosYaw = std::cos(rad);

                double nextMx = (moveForward * -sinYaw + moveStrafe * cosYaw) * maxSpd;
                double nextMz = (moveForward * cosYaw - moveStrafe * -sinYaw) * maxSpd;

                env->SetDoubleField(playerObj, mXf, nextMx);
                env->SetDoubleField(playerObj, mZf, nextMz);
            }
        }

        if (mode == 1 && moving && w && !s) {
            jmethodID setSprinting = env->GetMethodID(playerClass,
                Mappings::Entity_setSprinting_Name,
                Mappings::Entity_setSprinting_Sig);
            if (setSprinting) env->CallVoidMethod(playerObj, setSprinting, JNI_TRUE);
        }

        env->DeleteLocalRef(entityClass);
        _ex(env);
    }

private:
    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }
};
