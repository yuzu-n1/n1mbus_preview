#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  NoFall  –  Prevents fall damage
//
//  Technique:
//    • Each tick, if motionY < fallThreshold (player is falling fast enough
//      to take damage on landing), we clamp motionY to -0.0784 — the maximum
//      speed reached after one tick of free-fall, which is exactly below the
//      distance needed to begin taking damage in vanilla 1.8.9.
//
//  Alternative technique (packet-based) would require hooking Java packet
//  sending; we use the simpler motion-clamp approach here.
// ─────────────────────────────────────────────────────────────────────────────
class NoFall : public Module {
public:
    // motionY below which we reset the fall. Vanilla damage starts accumulating
    // above 3.5 blocks of fall distance, i.e. after ~4 ticks of gravity.
    float fallThreshold = -0.5f;

    NoFall() : Module("NoFall") {}

    void onUpdate(JNIEnv* env, jobject /*mcObj*/, jobject playerObj, jclass playerClass) override {
        if (!env || !playerObj || !playerClass) return;

        jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
        if (!entityClass) { _ex(env); return; }

        jfieldID mYf = env->GetFieldID(entityClass,
            Mappings::Entity_motionY_Name, Mappings::Entity_motionY_Sig);
        _ex(env);

        if (mYf) {
            double motionY = env->GetDoubleField(playerObj, mYf);
            if (motionY < (double)fallThreshold) {
                // Reset fall distance by keeping vertical speed to safe level
                env->SetDoubleField(playerObj, mYf, -0.0784);
            }
        }

        env->DeleteLocalRef(entityClass);
        _ex(env);
    }

private:
    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }
};
