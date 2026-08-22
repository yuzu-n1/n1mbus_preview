#pragma once
#include "../plugin_api.hpp"
#include "../mc.hpp"
#include <cmath>
#include <chrono>

class SafeWalk : public Module {
private:
    jfieldID m_thePlayerF = nullptr, m_theWorldF = nullptr;
    jfieldID m_posXF = nullptr, m_posYF = nullptr, m_posZF = nullptr;
    jfieldID m_onGroundF = nullptr;
    jfieldID m_gsF = nullptr, m_sneakKeyF = nullptr, m_pressedF = nullptr;
    jmethodID m_isAirBlockM = nullptr, m_blockPosInitM = nullptr;
    jclass m_blockPosClassGlobal = nullptr;
    
    int m_sneakTicks = 0; // Coyote time

    bool _isAirBlock(JNIEnv* env, jobject worldObj, int x, int y, int z) {
        if (!m_isAirBlockM || !m_blockPosClassGlobal) return false;
        jobject posObj = env->NewObject(m_blockPosClassGlobal, m_blockPosInitM, (double)x, (double)y, (double)z);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return false; }
        if (!posObj) return false;
        
        jboolean isAir = env->CallBooleanMethod(worldObj, m_isAirBlockM, posObj);
        if (env->ExceptionCheck()) { env->ExceptionClear(); isAir = JNI_FALSE; }
        
        env->DeleteLocalRef(posObj);
        return isAir == JNI_TRUE;
    }

    bool _isNearEdge(JNIEnv* env, jobject worldObj, jobject playerObj, double px, double py, double pz) {
        if (!m_isAirBlockM || !m_blockPosClassGlobal) return false;

        int ty = (int)std::floor(py) - 1;

        // Predict future position based on motion
        double mx = 0.0, mz = 0.0;
        if (m_posXF) { // Reuse posXF check to ensure fields are mapped, but we need motion
            jclass playerClass = env->GetObjectClass(playerObj);
            if (playerClass) {
                jfieldID mxF = MC::fieldID(env, playerClass, "Entity.motionX");
                jfieldID mzF = MC::fieldID(env, playerClass, "Entity.motionZ");
                if (mxF && mzF) {
                    mx = env->GetDoubleField(playerObj, mxF);
                    mz = env->GetDoubleField(playerObj, mzF);
                }
                env->DeleteLocalRef(playerClass);
            }
        }
        if (env->ExceptionCheck()) env->ExceptionClear();

        // Check if the block we are moving towards is air
        // We look a bit ahead (3 ticks of motion or a small fixed distance)
        double predX = px + mx * 3.0;
        double predZ = pz + mz * 3.0;
        
        // Also check if we are physically very close to the edge (0.25 blocks from the edge)
        // A block is 0 to 1. The edge is when fractional part is < 0.25 or > 0.75
        double fracX = px - std::floor(px);
        double fracZ = pz - std::floor(pz);
        
        bool edgeX = (fracX < 0.25 && _isAirBlock(env, worldObj, std::floor(px)-1, ty, std::floor(pz))) || 
                     (fracX > 0.75 && _isAirBlock(env, worldObj, std::floor(px)+1, ty, std::floor(pz)));
        bool edgeZ = (fracZ < 0.25 && _isAirBlock(env, worldObj, std::floor(px), ty, std::floor(pz)-1)) || 
                     (fracZ > 0.75 && _isAirBlock(env, worldObj, std::floor(px), ty, std::floor(pz)+1));

        bool movingToAir = _isAirBlock(env, worldObj, (int)std::floor(predX), ty, (int)std::floor(predZ));

        return (edgeX || edgeZ) && movingToAir;
    }

    void _setSneak(JNIEnv* env, jobject mcObj, bool sneak) {
        if (!m_gsF || !m_sneakKeyF || !m_pressedF) return;

        // If user is physically holding shift, don't interfere
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) return;

        jobject gsObj = env->GetObjectField(mcObj, m_gsF); 
        if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
        if (!gsObj) return;
        
        jobject sneakKeyObj = env->GetObjectField(gsObj, m_sneakKeyF);
        if (env->ExceptionCheck()) { env->ExceptionClear(); env->DeleteLocalRef(gsObj); return; }
        
        if (sneakKeyObj) {
            env->SetBooleanField(sneakKeyObj, m_pressedF, sneak ? JNI_TRUE : JNI_FALSE);
            if (env->ExceptionCheck()) { env->ExceptionClear(); }
            env->DeleteLocalRef(sneakKeyObj);
        }
        env->DeleteLocalRef(gsObj);
    }

    void _ex(JNIEnv* env) {
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

public:
    SafeWalk() : Module("SafeWalk") {}

    void onEnable() override {
        m_sneakTicks = 0;
    }

    void onDisable() override {
        m_sneakTicks = 0;
    }

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!isEnabled()) return;

        if (!m_thePlayerF) {
            jclass mcClass = env->GetObjectClass(mcObj); _ex(env);
            if (!mcClass) return;
            
            m_thePlayerF = MC::fieldID(env, mcClass, "Minecraft.thePlayer");
            m_theWorldF  = MC::fieldID(env, mcClass, "Minecraft.theWorld");
            m_gsF        = MC::fieldID(env, mcClass, "Minecraft.gameSettings");
            
            m_posXF     = MC::fieldID(env, playerClass, "Entity.posX");
            m_posYF     = MC::fieldID(env, playerClass, "Entity.posY");
            m_posZF     = MC::fieldID(env, playerClass, "Entity.posZ");
            m_onGroundF = MC::fieldID(env, playerClass, "Entity.onGround");
            
            jclass worldClass = JniManager::FindClassWithLoader(env, "net/minecraft/world/World"); _ex(env);
            if (!worldClass) { env->ExceptionClear(); worldClass = env->FindClass("adm"); }
            if (worldClass) {
                m_isAirBlockM = MC::methodID(env, worldClass, "World.isAirBlock");
                env->DeleteLocalRef(worldClass);
            }
            
            jclass blockPosClass = JniManager::FindClassWithLoader(env, "net/minecraft/util/BlockPos"); _ex(env);
            if (!blockPosClass) { env->ExceptionClear(); blockPosClass = env->FindClass("cj"); }
            if (blockPosClass) {
                m_blockPosClassGlobal = (jclass)env->NewGlobalRef(blockPosClass); _ex(env);
                m_blockPosInitM = env->GetMethodID(blockPosClass, "<init>", "(DDD)V"); _ex(env);
                env->DeleteLocalRef(blockPosClass);
            }
            
            if (m_gsF) {
                jobject gsObj = env->GetObjectField(mcObj, m_gsF); _ex(env);
                if (gsObj) {
                    jclass gsClass = env->GetObjectClass(gsObj); _ex(env);
                    if (gsClass) {
                        m_sneakKeyF = MC::fieldID(env, gsClass, "GameSettings.keyBindSneak");
                        if (m_sneakKeyF) {
                            jobject keyObj = env->GetObjectField(gsObj, m_sneakKeyF); _ex(env);
                            if (keyObj) {
                                jclass keyClass = env->GetObjectClass(keyObj); _ex(env);
                                if (keyClass) {
                                    m_pressedF = MC::fieldID(env, keyClass, "KeyBinding.pressed");
                                    env->DeleteLocalRef(keyClass);
                                }
                                env->DeleteLocalRef(keyObj);
                            }
                        }
                        env->DeleteLocalRef(gsClass);
                    }
                    env->DeleteLocalRef(gsObj);
                }
            }
            
            env->DeleteLocalRef(mcClass);
        }

        if (!m_theWorldF || !m_posXF || !m_onGroundF) return;

        jobject worldObj = env->GetObjectField(mcObj, m_theWorldF); _ex(env);
        if (!worldObj) return;

        bool onGround = env->GetBooleanField(playerObj, m_onGroundF); _ex(env);
        
        double px = env->GetDoubleField(playerObj, m_posXF); _ex(env);
        double py = env->GetDoubleField(playerObj, m_posYF); _ex(env);
        double pz = env->GetDoubleField(playerObj, m_posZF); _ex(env);

        // Only apply SafeWalk if the player is currently on the ground
        bool nearEdge = false;
        if (onGround) {
            nearEdge = _isNearEdge(env, worldObj, playerObj, px, py, pz);
        }

        // Coyote time for shifting: stay shifted for a few ticks after detecting edge
        // This ensures the server acknowledges the shift and we don't accidentally fall due to ping.
        if (nearEdge) {
            m_sneakTicks = 2; 
        } else if (m_sneakTicks > 0) {
            m_sneakTicks--;
            nearEdge = true; // Force sneak to true while coyote time is active
        }

        _setSneak(env, mcObj, nearEdge);

        env->DeleteLocalRef(worldObj);
    }
};
