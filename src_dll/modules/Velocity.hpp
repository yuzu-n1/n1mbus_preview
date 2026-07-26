#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <Windows.h>

class Velocity : public Module {
public:
    float horizontal = 0.0f;
    float vertical = 0.0f;

    Velocity() : Module("Velocity") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !playerObj) return;

        if (!m_cached) {
            m_hurtTimeF = env->GetFieldID(playerClass, Mappings::EntityLivingBase_hurtTime_Name, Mappings::EntityLivingBase_hurtTime_Sig);
            m_motionXF = env->GetFieldID(playerClass, Mappings::Entity_motionX_Name, Mappings::Entity_motionX_Sig);
            m_motionYF = env->GetFieldID(playerClass, Mappings::Entity_motionY_Name, Mappings::Entity_motionY_Sig);
            m_motionZF = env->GetFieldID(playerClass, Mappings::Entity_motionZ_Name, Mappings::Entity_motionZ_Sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
            m_cached = true;
        }

        if (m_hurtTimeF && m_motionXF && m_motionYF && m_motionZF) {
            int hurtTime = env->GetIntField(playerObj, m_hurtTimeF);
            
            // hurtTime is set to 10 the exact tick the player takes damage/knockback
            if (hurtTime == 10 && m_lastHurtTime != 10) {
                double mx = env->GetDoubleField(playerObj, m_motionXF);
                double my = env->GetDoubleField(playerObj, m_motionYF);
                double mz = env->GetDoubleField(playerObj, m_motionZF);
                
                env->SetDoubleField(playerObj, m_motionXF, mx * horizontal);
                env->SetDoubleField(playerObj, m_motionYF, my * vertical);
                env->SetDoubleField(playerObj, m_motionZF, mz * horizontal);
            }
            m_lastHurtTime = hurtTime;
        }
    }

    void onDisable() override {
        m_lastHurtTime = 0;
    }

private:
    bool m_cached = false;
    jfieldID m_hurtTimeF = nullptr;
    jfieldID m_motionXF = nullptr;
    jfieldID m_motionYF = nullptr;
    jfieldID m_motionZF = nullptr;
    int m_lastHurtTime = 0;
};
