#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <windows.h>
#include <chrono>

class SprintReset : public Module {
public:
    SprintReset() : Module("SprintReset") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj) return;

        static bool isAction = false;
        static long long startTime = 0;

        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        if (isAction) {
            // Release S after a short delay (e.g. 50ms)
            if (now - startTime > 50) {
                keybd_event(0x53, 0, KEYEVENTF_KEYUP, 0); // Release 'S'
                isAction = false;
            }
            return;
        }

        // Check if attacking (LBUTTON pressed)
        if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0) return;

        // Check if we hit an entity
        jclass mcClass = env->GetObjectClass(mcObj);
        jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
        if (!mopF) { env->DeleteLocalRef(mcClass); return; }

        jobject mopObj = env->GetObjectField(mcObj, mopF);
        if (!mopObj) { env->DeleteLocalRef(mcClass); return; }

        jclass mopClass = env->GetObjectClass(mopObj);
        jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
        if (!entityHitF) { env->DeleteLocalRef(mopClass); env->DeleteLocalRef(mopObj); env->DeleteLocalRef(mcClass); return; }

        jobject entityHitObj = env->GetObjectField(mopObj, entityHitF);
        if (entityHitObj) {
            // We are looking at an entity and clicking!
            // Do an S-Tap to reset sprint
            keybd_event(0x53, 0, 0, 0); // Press 'S'
            isAction = true;
            startTime = now;
            env->DeleteLocalRef(entityHitObj);
        }

        env->DeleteLocalRef(mopClass);
        env->DeleteLocalRef(mopObj);
        env->DeleteLocalRef(mcClass);
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
};
