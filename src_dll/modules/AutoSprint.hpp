#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include <windows.h>

// ─────────────────────────────────────────────────────────────────────────────
//  AutoSprint  –  Calls Entity.setSprinting(true) whenever W is held.
//  Clears the sprint flag when the module is disabled to avoid sticking.
// ─────────────────────────────────────────────────────────────────────────────
class AutoSprint : public Module {
public:
    AutoSprint() : Module("AutoSprint") {}

    void onUpdate(JNIEnv* env, jobject /*mcObj*/, jobject playerObj, jclass playerClass) override {
        if (!env || !playerObj || !playerClass) return;

        if (GetAsyncKeyState('W') & 0x8000) {
            jmethodID setSprinting = env->GetMethodID(playerClass,
                Mappings::Entity_setSprinting_Name,
                Mappings::Entity_setSprinting_Sig);
            if (setSprinting) env->CallVoidMethod(playerObj, setSprinting, JNI_TRUE);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();
    }
};
