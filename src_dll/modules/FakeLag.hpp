#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <windows.h>

class FakeLag : public Module {
public:
    int duration = 150; // ms

    FakeLag() : Module("FakeLag") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env) return;
        
        jclass agentClass = JniManager::FindClassWithLoader(env, "n1mbus/N1mbusAgent");
        if (agentClass) {
            jmethodID setFakeLag = env->GetStaticMethodID(agentClass, "setFakeLag", "(ZI)V");
            if (setFakeLag) {
                env->CallStaticVoidMethod(agentClass, setFakeLag, JNI_TRUE, duration);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(agentClass);
        }
    }

    void onDisable() override {
        JNIEnv* env = JniManager::GetEnv();
        if (env) {
            jclass agentClass = JniManager::FindClassWithLoader(env, "n1mbus/N1mbusAgent");
            if (agentClass) {
                jmethodID setFakeLag = env->GetStaticMethodID(agentClass, "setFakeLag", "(ZI)V");
                if (setFakeLag) {
                    env->CallStaticVoidMethod(agentClass, setFakeLag, JNI_FALSE, duration);
                }
                if (env->ExceptionCheck()) env->ExceptionClear();
                env->DeleteLocalRef(agentClass);
            }
        }
    }
};
