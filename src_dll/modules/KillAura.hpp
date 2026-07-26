#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  KillAura  –  Automatically attacks entities in range with Anti-Cheat bypasses
// ─────────────────────────────────────────────────────────────────────────────
class KillAura : public Module {
public:
    int   mode        = 0;    // 0=Single 1=Switch 2=Multi
    float reach       = 4.0f; // Max attack range in blocks
    int   minCps      = 10;
    int   maxCps      = 14;
    float fov         = 360.0f;
    float aimSpeed    = 10.0f; // 1-10

    KillAura() : Module("KillAura") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;

        // ── Get world + entity list ───────────────────────────────────────────
        jclass listClass = JniManager::FindClassWithLoader(env, "java/util/List");
        jclass entityClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/Entity");
        jclass livingClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/EntityLivingBase");
        jclass playerCls   = JniManager::FindClassWithLoader(env, "net/minecraft/entity/player/EntityPlayer");
        _ex(env);
        if (!listClass || !entityClass || !livingClass || !playerCls) {
            _cleanup(env, {listClass, entityClass, livingClass, playerCls}); return;
        }

        jfieldID worldField = MC::fieldID(env, env->GetObjectClass(mcObj), "Minecraft.theWorld");
        _ex(env);
        if (!worldField) { _cleanup(env, {listClass, entityClass, livingClass, playerCls}); return; }

        jobject worldObj = env->GetObjectField(mcObj, worldField);
        if (!worldObj)   { _cleanup(env, {listClass, entityClass, livingClass, playerCls}); return; }

        jclass  worldClass = env->GetObjectClass(worldObj);
        jfieldID listField  = MC::fieldID(env, worldClass, "World.loadedEntityList");
        _ex(env);
        if (!listField) {
            _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj}); return;
        }

        jobject listObj = env->GetObjectField(worldObj, listField);
        if (!listObj) {
            _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj}); return;
        }

        jclass listConcreteClass = env->GetObjectClass(listObj);
        jmethodID sizeMeth = env->GetMethodID(listConcreteClass, "size", "()I");
        jmethodID getMeth  = env->GetMethodID(listConcreteClass, "get", "(I)Ljava/lang/Object;");
        env->DeleteLocalRef(listConcreteClass);
        jmethodID canSeeMeth = MC::methodID(env, livingClass, "EntityLivingBase.canEntityBeSeen");
        _ex(env);

        // ── Get GameSettings for GCD patch ────────────────────────────────────
        jfieldID gsF = MC::fieldID(env, env->GetObjectClass(mcObj), "Minecraft.gameSettings");
        jobject gsObj = gsF ? env->GetObjectField(mcObj, gsF) : nullptr;
        float sensitivity = 0.5f;
        if (gsObj) {
            jfieldID sensF = MC::fieldID(env, env->GetObjectClass(gsObj), "GameSettings.mouseSensitivity");
            if (sensF) sensitivity = env->GetFloatField(gsObj, sensF);
            env->DeleteLocalRef(gsObj);
        }
        float f_sens = sensitivity * 0.6f + 0.2f;
        float gcd = f_sens * f_sens * f_sens * 8.0f * 0.15f;

        // ── Player position & rotation ────────────────────────────────────────
        jfieldID pXf = MC::fieldID(env, entityClass, "Entity.posX");
        jfieldID pYf = MC::fieldID(env, entityClass, "Entity.posY");
        jfieldID pZf = MC::fieldID(env, entityClass, "Entity.posZ");
        jfieldID yawF = MC::fieldID(env, entityClass, "Entity.rotationYaw");
        jfieldID pitchF = MC::fieldID(env, entityClass, "Entity.rotationPitch");
        _ex(env);

        double myX = pXf ? env->GetDoubleField(playerObj, pXf) : 0.0;
        double myY = pYf ? env->GetDoubleField(playerObj, pYf) : 0.0;
        double myZ = pZf ? env->GetDoubleField(playerObj, pZf) : 0.0;
        float pYaw = yawF ? env->GetFloatField(playerObj, yawF) : 0.0f;
        float pPitch = pitchF ? env->GetFloatField(playerObj, pitchF) : 0.0f;

        // ── Attack method ──────────────────────────────────────────────────────
        jfieldID controllerField = MC::fieldID(env, env->GetObjectClass(mcObj), "Minecraft.playerController");
        jobject controllerObj = controllerField ? env->GetObjectField(mcObj, controllerField) : nullptr;
        jmethodID attackMeth = nullptr;
        if (controllerObj) {
            attackMeth = MC::methodID(env, env->GetObjectClass(controllerObj), "PlayerControllerMP.attackEntity");
        }
        jmethodID swingMeth = MC::methodID(env, playerClass, "EntityLivingBase.swingItem");
        _ex(env);

        if (!sizeMeth || !getMeth || !attackMeth) {
            if (controllerObj) env->DeleteLocalRef(controllerObj);
            _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj, listObj}); return;
        }

        int size = env->CallIntMethod(listObj, sizeMeth);
        _ex(env);

        // ── Find targets ──────────────────────────────────────────────────────
        jobject bestTarget = nullptr;
        double  bestDist   = (double)reach * reach;

        if (mode == 1 && m_switchIndex >= size) m_switchIndex = 0;
        
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        bool readyToAttack = false;
        
        if (m_lastClickTime == 0) m_lastClickTime = now;
        if (m_nextCps <= 0) m_nextCps = 10;
        
        if ((now - m_lastClickTime) >= (1000 / m_nextCps)) {
            readyToAttack = true;
        }
        
        int multiHitCount = 0;

        for (int i = 0; i < size; i++) {
            jobject entObj = env->CallObjectMethod(listObj, getMeth, i);
            if (!entObj) continue;

            if (env->IsSameObject(entObj, playerObj)) { env->DeleteLocalRef(entObj); continue; }
            if (!env->IsInstanceOf(entObj, livingClass)) { env->DeleteLocalRef(entObj); continue; }
            if (!env->IsInstanceOf(entObj, playerCls)) { env->DeleteLocalRef(entObj); continue; } // Defaulting to players only for now

            double ex = pXf ? env->GetDoubleField(entObj, pXf) : 0.0;
            double ey = pYf ? env->GetDoubleField(entObj, pYf) : 0.0;
            double ez = pZf ? env->GetDoubleField(entObj, pZf) : 0.0;
            _ex(env);

            double dx = ex - myX, dy = (ey + 1.0) - (myY + 1.62), dz = ez - myZ;
            double distSq = dx*dx + dy*dy + dz*dz;

            if (distSq > bestDist) { env->DeleteLocalRef(entObj); continue; }

            // FOV Check
            float yawTo = (float)(std::atan2(dz, dx) * 180.0 / 3.14159265358979) - 90.0f;
            float yawDiff = std::fmod(std::abs(yawTo - pYaw), 360.0f);
            if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;
            if (yawDiff > fov / 2.0f) { env->DeleteLocalRef(entObj); continue; }

            // LoS Check
            if (canSeeMeth) {
                jboolean canSee = env->CallBooleanMethod(playerObj, canSeeMeth, entObj);
                _ex(env);
                if (!canSee) { env->DeleteLocalRef(entObj); continue; }
            }

            if (mode == 0) {
                if (distSq < bestDist) {
                    if (bestTarget) env->DeleteLocalRef(bestTarget);
                    bestTarget = entObj;
                    bestDist = distSq;
                    continue; 
                }
            } else if (mode == 1) {
                if (i == m_switchIndex) {
                    bestTarget = env->NewLocalRef(entObj);
                    if (readyToAttack) m_switchIndex = (m_switchIndex + 1) % size;
                    env->DeleteLocalRef(entObj);
                    break;
                }
            } else {
                if (readyToAttack && multiHitCount < 4) {
                    if (swingMeth) env->CallVoidMethod(playerObj, swingMeth);
                    env->CallVoidMethod(controllerObj, attackMeth, playerObj, entObj);
                    _ex(env);
                    multiHitCount++;
                    
                    // Delay CPS calculation until after the loop
                }
            }

            env->DeleteLocalRef(entObj);
        }
        
        if (mode == 2 && readyToAttack && multiHitCount > 0) {
            m_lastClickTime = now;
            int cMin = minCps, cMax = maxCps;
            if (cMin > cMax) std::swap(cMin, cMax);
            if (cMin == cMax) { m_nextCps = cMin; }
            else m_nextCps = cMin + (rand() % (cMax - cMin + 1));
        }

        // Single / Switch mode logic
        if (bestTarget) {
            // 1. Aim at target
            double ex = env->GetDoubleField(bestTarget, pXf);
            double ey = env->GetDoubleField(bestTarget, pYf) + 1.0;
            double ez = env->GetDoubleField(bestTarget, pZf);
            
            double dx = ex - myX, dy = ey - (myY + 1.62), dz = ez - myZ;

            float yawTo = (float)(std::atan2(dz, dx) * 180.0 / 3.14159265358979) - 90.0f;
            float pitchTo = (float)-(std::atan2(dy, std::sqrt(dx*dx + dz*dz)) * 180.0 / 3.14159265358979);

            float yDiff = std::fmod(yawTo - pYaw, 360.0f);
            if (yDiff > 180.0f) yDiff -= 360.0f;
            if (yDiff < -180.0f) yDiff += 360.0f;
            float pDiff = pitchTo - pPitch;

            float factor = aimSpeed / 10.0f; 
            float stepYaw = yDiff * factor;
            float stepPitch = pDiff * factor;

            // GCD Patch for GrimAC
            stepYaw -= std::fmod(stepYaw, gcd);
            stepPitch -= std::fmod(stepPitch, gcd);

            if (yawF && pitchF) {
                env->SetFloatField(playerObj, yawF, pYaw + stepYaw);
                env->SetFloatField(playerObj, pitchF, pPitch + stepPitch);
            }

            // 2. Attack target if cooldown met
            if (readyToAttack) {
                if (swingMeth) {
                    env->CallVoidMethod(playerObj, swingMeth);
                    _ex(env);
                }
                
                // Post attack click
                env->CallVoidMethod(controllerObj, attackMeth, playerObj, bestTarget);
                _ex(env);
                
                m_lastClickTime = now;
                int cMin = minCps, cMax = maxCps;
                if (cMin > cMax) std::swap(cMin, cMax);
                if (cMin == cMax) { m_nextCps = cMin; }
                else m_nextCps = cMin + (rand() % (cMax - cMin + 1));
            }

            env->DeleteLocalRef(bestTarget);
        } else {
            if (mode == 1) m_switchIndex++; // Move to next if current is invalid
        }

        if (controllerObj) env->DeleteLocalRef(controllerObj);

        _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj, listObj});
    }

private:
    long long m_lastClickTime = 0;
    int m_nextCps = 10;
    int m_switchIndex  = 0;

    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }

    static void _cleanup(JNIEnv* env, std::initializer_list<jobject> refs) {
        for (jobject r : refs) if (r) env->DeleteLocalRef(r);
    }
};
