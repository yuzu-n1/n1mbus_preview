#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"

class BridgeAssist : public Module {
public:
    float pitchCheck = 65.0f;
    bool onlyOnShift = false; // Add option to only work if player is already holding shift? Or maybe just simple safe walk
    bool blocksOnly = true;

    BridgeAssist() : Module("BridgeAssist") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj) return;

        static bool wasSneaking = false;

        jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
        if (!entityClass) { _ex(env); return; }

        jfieldID pitchF = env->GetFieldID(entityClass, Mappings::Entity_rotationPitch_Name, Mappings::Entity_rotationPitch_Sig);
        jfieldID onGroundF = env->GetFieldID(entityClass, Mappings::Entity_onGround_Name, Mappings::Entity_onGround_Sig);
        _ex(env);

        if (!pitchF || !onGroundF) { env->DeleteLocalRef(entityClass); return; }

        float pitch = env->GetFloatField(playerObj, pitchF);
        bool onGround = env->GetBooleanField(playerObj, onGroundF);

        // Get game settings to control sneak key
        jclass mcClass = env->GetObjectClass(mcObj);
        jfieldID gsF = env->GetFieldID(mcClass, Mappings::Minecraft_gameSettings_Name, Mappings::Minecraft_gameSettings_Sig);
        if (!gsF) { _cleanup(env, entityClass, mcClass, nullptr, nullptr, nullptr); return; }
        
        jobject gsObj = env->GetObjectField(mcObj, gsF);
        if (!gsObj) { _cleanup(env, entityClass, mcClass, nullptr, nullptr, nullptr); return; }

        jclass gsClass = env->GetObjectClass(gsObj);
        jfieldID sneakKeyF = env->GetFieldID(gsClass, Mappings::GameSettings_keyBindSneak_Name, Mappings::GameSettings_keyBindSneak_Sig);
        if (!sneakKeyF) { _cleanup(env, entityClass, mcClass, gsObj, gsClass, nullptr); return; }

        jobject sneakKeyObj = env->GetObjectField(gsObj, sneakKeyF);
        if (!sneakKeyObj) { _cleanup(env, entityClass, mcClass, gsObj, gsClass, nullptr); return; }

        jclass keyBindClass = JniManager::FindClassWithLoader(env, Mappings::KeyBinding_Class);
        if (!keyBindClass) { _cleanup(env, entityClass, mcClass, gsObj, gsClass, sneakKeyObj); return; }

        jfieldID pressedF = env->GetFieldID(keyBindClass, Mappings::KeyBinding_pressed_Name, Mappings::KeyBinding_pressed_Sig);
        if (!pressedF) { _cleanup(env, entityClass, mcClass, gsObj, gsClass, sneakKeyObj); env->DeleteLocalRef(keyBindClass); return; }

        bool sPressed = (GetAsyncKeyState('S') & 0x8000) != 0;
        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;

        // Bridge logic: looking down, walking backwards, on ground (don't get stuck in air)
        if (onGround && pitch > pitchCheck && sPressed && !spacePressed) {
            env->SetBooleanField(sneakKeyObj, pressedF, JNI_TRUE);
            wasSneaking = true;
        } else {
            if (wasSneaking) {
                // Only un-sneak if the user isn't actually holding shift physically
                if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) {
                    env->SetBooleanField(sneakKeyObj, pressedF, JNI_FALSE);
                }
                wasSneaking = false;
            }
        }

        _cleanup(env, entityClass, mcClass, gsObj, gsClass, sneakKeyObj);
        env->DeleteLocalRef(keyBindClass);
        _ex(env);
    }

private:
    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }
    static void _cleanup(JNIEnv* e, jclass c1, jclass c2, jobject o1, jclass c3, jobject o2) {
        if (c1) e->DeleteLocalRef(c1);
        if (c2) e->DeleteLocalRef(c2);
        if (o1) e->DeleteLocalRef(o1);
        if (c3) e->DeleteLocalRef(c3);
        if (o2) e->DeleteLocalRef(o2);
    }
};
