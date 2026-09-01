#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include "../mapping_resolver.hpp"
#include <cmath>
#include <chrono>
#include <random>
#include <algorithm>

extern bool g_IsGuiOpen;
extern bool g_ShowMenu;

// ─────────────────────────────────────────────────────────────────────────────
//  KillAura  –  Automatically attacks entities in range with Anti-Cheat bypasses
// ─────────────────────────────────────────────────────────────────────────────
class KillAura : public Module {
public:
    int   mode        = 0;    // 0=Single 1=Switch 2=Multi
    int   priority    = 0;    // 0=Distance 1=Health 2=Angle
    float reach       = 4.0f; // Max attack range in blocks
    int   minCps      = 10;
    int   maxCps      = 14;
    float fov         = 360.0f;
    float aimSpeed    = 10.0f; // 1-10
    
    bool  autoDisable     = false;
    int   autoDisableTime = 2000; // ms
    bool  teams           = false;

    KillAura() : Module("KillAura") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;

        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (autoDisable && now < m_pauseUntil) return;
        if (g_IsGuiOpen || g_ShowMenu) return; // Do not attack while UI is open

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
        jfieldID prevYawF = MC::fieldID(env, entityClass, "Entity.prevRotationYaw");
        jfieldID prevPitchF = MC::fieldID(env, entityClass, "Entity.prevRotationPitch");
        jfieldID yawHeadF = MC::fieldID(env, livingClass, "EntityLivingBase.rotationYawHead");
        jfieldID renderYawOffsetF = MC::fieldID(env, livingClass, "EntityLivingBase.renderYawOffset");
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

        if (!m_methodsResolved) {
            std::string getIdName = MappingResolver::FindMethodFromNames(env, entityClass, {"getEntityId", "func_145782_y", "F"}, "()I");
            if (!getIdName.empty()) m_getIdMeth = env->GetMethodID(entityClass, getIdName.c_str(), "()I");
            
            std::string getHealthName = MappingResolver::FindMethodFromNames(env, livingClass, {"getHealth", "func_110143_aJ", "bn"}, "()F");
            if (!getHealthName.empty()) m_getHealthMeth = env->GetMethodID(livingClass, getHealthName.c_str(), "()F");
            
            if (env->ExceptionCheck()) env->ExceptionClear();
            m_methodsResolved = true;
        }

        // ── Find & Collect target candidates ──────────────────────────────────
        struct TargetCandidate {
            jobject obj;
            int id;
            double distSq;
            float health;
            float yawDiff;
            double ex, ey, ez;
        };

        std::vector<TargetCandidate> candidates;
        double maxDistSq = (double)reach * reach;

        bool readyToAttack = false;
        if (m_lastClickTime == 0) m_lastClickTime = now;
        if (m_nextCps <= 0) m_nextCps = 10;
        
        if ((now - m_lastClickTime) >= (1000 / m_nextCps)) {
            readyToAttack = true;
        }
        
        bool lastTargetFound = false;
        bool lastTargetDead = false;

        for (int i = 0; i < size; i++) {
            jobject entObj = env->CallObjectMethod(listObj, getMeth, i);
            if (!entObj) continue;

            if (env->IsSameObject(entObj, playerObj)) { env->DeleteLocalRef(entObj); continue; }
            if (!env->IsInstanceOf(entObj, livingClass)) { env->DeleteLocalRef(entObj); continue; }
            if (!env->IsInstanceOf(entObj, playerCls)) { env->DeleteLocalRef(entObj); continue; }
            
            if (teams && MappingResolver::CallIsOnSameTeam(env, playerObj, entObj, entityClass)) {
                env->DeleteLocalRef(entObj);
                continue;
            }

            double ex = pXf ? env->GetDoubleField(entObj, pXf) : 0.0;
            double ey = pYf ? env->GetDoubleField(entObj, pYf) : 0.0;
            double ez = pZf ? env->GetDoubleField(entObj, pZf) : 0.0;
            _ex(env);

            int entId = m_getIdMeth ? env->CallIntMethod(entObj, m_getIdMeth) : -1;
            _ex(env);
            
            float health = 20.0f;
            if (m_getHealthMeth) {
                health = env->CallFloatMethod(entObj, m_getHealthMeth);
                _ex(env);
            }

            if (entId != -1 && entId == m_lastTargetId) {
                lastTargetFound = true;
                if (health <= 0.0f) lastTargetDead = true;
            }

            double dx = ex - myX, dy = (ey + 1.0) - (myY + 1.62), dz = ez - myZ;
            double distSq = dx*dx + dy*dy + dz*dz;

            if (distSq > maxDistSq) { env->DeleteLocalRef(entObj); continue; }

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

            // Valid candidate
            candidates.push_back({ entObj, entId, distSq, health, yawDiff, ex, ey, ez });
        }
        
        if (autoDisable && m_lastTargetId != -1) {
            if (!lastTargetFound || lastTargetDead) {
                m_pauseUntil = now + autoDisableTime;
                m_lastTargetId = -1;
                
                for (auto& c : candidates) env->DeleteLocalRef(c.obj);
                if (controllerObj) env->DeleteLocalRef(controllerObj);
                _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj, listObj});
                return;
            }
        }

        // ── Sort candidates by Priority ───────────────────────────────────────
        // 0 = Distance (closest first)
        // 1 = Health (lowest HP first, secondary by distance)
        // 2 = Angle / FOV (closest to crosshair first)
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), [this](const TargetCandidate& a, const TargetCandidate& b) {
                if (this->priority == 1) { // Lowest Health
                    if (std::abs(a.health - b.health) > 0.05f) {
                        return a.health < b.health;
                    }
                    return a.distSq < b.distSq;
                } else if (this->priority == 2) { // Angle (Crosshair)
                    return a.yawDiff < b.yawDiff;
                } else { // Distance (default)
                    return a.distSq < b.distSq;
                }
            });
        }

        // ── Execution based on Mode ───────────────────────────────────────────
        if (mode == 2) {
            // Multi mode: attack up to 4 candidates in priority order
            if (readyToAttack && !candidates.empty()) {
                int multiHitCount = 0;
                for (size_t i = 0; i < candidates.size() && multiHitCount < 4; i++) {
                    const auto& target = candidates[i];
                    double adx = target.ex - myX, ady = (target.ey + 1.0) - (myY + 1.62), adz = target.ez - myZ;
                    float aimYaw = (float)(std::atan2(adz, adx) * 180.0 / 3.14159265358979) - 90.0f;
                    float aimPitch = (float)-(std::atan2(ady, std::sqrt(adx*adx + adz*adz)) * 180.0 / 3.14159265358979);

                    float origYaw = yawF ? env->GetFloatField(playerObj, yawF) : 0.0f;
                    float origPitch = pitchF ? env->GetFloatField(playerObj, pitchF) : 0.0f;
                    float origPrevYaw = prevYawF ? env->GetFloatField(playerObj, prevYawF) : 0.0f;
                    float origPrevPitch = prevPitchF ? env->GetFloatField(playerObj, prevPitchF) : 0.0f;

                    if (yawF) env->SetFloatField(playerObj, yawF, aimYaw);
                    if (pitchF) env->SetFloatField(playerObj, pitchF, aimPitch);

                    _notifyAgentSilentRotation(env, aimYaw, aimPitch);

                    if (swingMeth) env->CallVoidMethod(playerObj, swingMeth);
                    env->CallVoidMethod(controllerObj, attackMeth, playerObj, target.obj);
                    _ex(env);

                    if (yawF) env->SetFloatField(playerObj, yawF, origYaw);
                    if (pitchF) env->SetFloatField(playerObj, pitchF, origPitch);
                    if (prevYawF) env->SetFloatField(playerObj, prevYawF, origPrevYaw);
                    if (prevPitchF) env->SetFloatField(playerObj, prevPitchF, origPrevPitch);

                    if (yawHeadF) env->SetFloatField(playerObj, yawHeadF, aimYaw);
                    if (renderYawOffsetF) env->SetFloatField(playerObj, renderYawOffsetF, aimYaw);

                    multiHitCount++;
                    m_lastTargetId = target.id;
                }

                m_lastClickTime = now;
                int cMin = minCps, cMax = maxCps;
                if (cMin > cMax) std::swap(cMin, cMax);
                if (cMin == cMax) { m_nextCps = cMin; }
                else m_nextCps = cMin + (rand() % (cMax - cMin + 1));
            }
        } else if (!candidates.empty()) {
            // Single (mode==0) or Switch (mode==1)
            size_t targetIdx = 0;
            if (mode == 1) {
                targetIdx = m_switchIndex % candidates.size();
            }
            const auto& bestTarget = candidates[targetIdx];

            double dx = bestTarget.ex - myX, dy = (bestTarget.ey + 1.0) - (myY + 1.62), dz = bestTarget.ez - myZ;
            float yawTo = (float)(std::atan2(dz, dx) * 180.0 / 3.14159265358979) - 90.0f;
            float pitchTo = (float)-(std::atan2(dy, std::sqrt(dx*dx + dz*dz)) * 180.0 / 3.14159265358979);

            float yDiff = std::fmod(yawTo - pYaw, 360.0f);
            if (yDiff > 180.0f) yDiff -= 360.0f;
            if (yDiff < -180.0f) yDiff += 360.0f;
            float pDiff = pitchTo - pPitch;

            float factor = aimSpeed / 10.0f; 
            float stepYaw = yDiff * factor;
            float stepPitch = pDiff * factor;

            stepYaw -= std::fmod(stepYaw, gcd);
            stepPitch -= std::fmod(stepPitch, gcd);

            float serverYaw = pYaw + stepYaw;
            float serverPitch = pPitch + stepPitch;

            m_serverYaw = serverYaw;
            m_serverPitch = serverPitch;

            if (readyToAttack) {
                float origPrevYaw = prevYawF ? env->GetFloatField(playerObj, prevYawF) : 0.0f;
                float origPrevPitch = prevPitchF ? env->GetFloatField(playerObj, prevPitchF) : 0.0f;

                if (yawF) env->SetFloatField(playerObj, yawF, serverYaw);
                if (pitchF) env->SetFloatField(playerObj, pitchF, serverPitch);

                _notifyAgentSilentRotation(env, serverYaw, serverPitch);

                if (swingMeth) {
                    env->CallVoidMethod(playerObj, swingMeth);
                    _ex(env);
                }
                
                env->CallVoidMethod(controllerObj, attackMeth, playerObj, bestTarget.obj);
                _ex(env);

                if (yawF) env->SetFloatField(playerObj, yawF, pYaw);
                if (pitchF) env->SetFloatField(playerObj, pitchF, pPitch);
                if (prevYawF) env->SetFloatField(playerObj, prevYawF, origPrevYaw);
                if (prevPitchF) env->SetFloatField(playerObj, prevPitchF, origPrevPitch);
                
                m_lastTargetId = bestTarget.id;
                
                if (mode == 1) {
                    m_switchIndex = (m_switchIndex + 1) % candidates.size();
                }

                m_lastClickTime = now;
                int cMin = minCps, cMax = maxCps;
                if (cMin > cMax) std::swap(cMin, cMax);
                if (cMin == cMax) { m_nextCps = cMin; }
                else m_nextCps = cMin + (rand() % (cMax - cMin + 1));
            }

            if (yawHeadF) env->SetFloatField(playerObj, yawHeadF, serverYaw);
            if (renderYawOffsetF) env->SetFloatField(playerObj, renderYawOffsetF, serverYaw);
        }

        // Cleanup local refs of candidates
        for (auto& c : candidates) {
            env->DeleteLocalRef(c.obj);
        }

        if (controllerObj) env->DeleteLocalRef(controllerObj);

        _cleanup(env, {listClass, entityClass, livingClass, playerCls, worldClass, worldObj, listObj});
    }

private:
    long long m_lastClickTime = 0;
    int m_nextCps = 10;
    int m_switchIndex  = 0;
    
    long long m_pauseUntil = 0;
    int m_lastTargetId = -1;
    bool m_methodsResolved = false;
    jmethodID m_getIdMeth = nullptr;
    jmethodID m_getHealthMeth = nullptr;
    jmethodID m_isOnSameTeamMeth = nullptr;

    // Silent rotation: server-side spoofed angles (not applied to client camera)
    float m_serverYaw = 0.0f;
    float m_serverPitch = 0.0f;

    static void _notifyAgentSilentRotation(JNIEnv* env, float yaw, float pitch) {
        if (!env) return;
        jclass agentClass = JniManager::FindClassWithLoader(env, "n1mbus/N1mbusAgent");
        if (agentClass) {
            jmethodID setRot = env->GetStaticMethodID(agentClass, "setSilentRotation", "(FF)V");
            jmethodID setActive = env->GetStaticMethodID(agentClass, "setSilentActive", "(Z)V");
            if (setRot && setActive) {
                env->CallStaticVoidMethod(agentClass, setRot, yaw, pitch);
                env->CallStaticVoidMethod(agentClass, setActive, JNI_TRUE);
            }
            if (env->ExceptionCheck()) env->ExceptionClear();
            env->DeleteLocalRef(agentClass);
        }
    }

    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }

    static void _cleanup(JNIEnv* env, std::initializer_list<jobject> refs) {
        for (jobject r : refs) if (r) env->DeleteLocalRef(r);
    }
};
