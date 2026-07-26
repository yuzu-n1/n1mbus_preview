#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <windows.h>
#include <chrono>
#include <random>

extern HWND g_hWnd;
extern bool g_ShowMenu;

class TriggerBot : public Module {
public:
    int minCps = 8;
    int maxCps = 12;
    float reach = 3.0f;
    bool playersOnly = false;
    bool visibleOnly = true;

    TriggerBot() : Module("TriggerBot") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!g_hWnd || GetForegroundWindow() != g_hWnd) return;
        if (!env || !mcObj) return;

        // Prevent clicking while DLL UI is open
        if (g_ShowMenu) return;

        jclass mcClass = env->GetObjectClass(mcObj);
        if (!mcClass) return;
        
        // Prevent clicking while Minecraft menu (inventory, chat, etc.) is open
        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (currentScreenF) {
            jobject currentScreenObj = env->GetObjectField(mcObj, currentScreenF);
            if (currentScreenObj) {
                env->DeleteLocalRef(currentScreenObj);
                env->DeleteLocalRef(mcClass);
                return; // Minecraft menu is open
            }
        }
        
        jfieldID mopF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (!mopF) { env->DeleteLocalRef(mcClass); return; }

        jobject mopObj = env->GetObjectField(mcObj, mopF);
        if (!mopObj) { env->DeleteLocalRef(mcClass); return; }

        jclass mopClass = env->GetObjectClass(mopObj);
        jfieldID entityHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_entityHit_Name, Mappings::MovingObjectPosition_entityHit_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();

        if (!entityHitF) {
            env->DeleteLocalRef(mopClass); env->DeleteLocalRef(mopObj); env->DeleteLocalRef(mcClass);
            return;
        }

        jobject entityHitObj = env->GetObjectField(mopObj, entityHitF);
        bool shouldClick = false;

        if (entityHitObj) {
            jclass entityLivingBaseClass = JniManager::FindClassWithLoader(env, Mappings::EntityLivingBase_Class);
            jclass entityPlayerClass = JniManager::FindClassWithLoader(env, Mappings::EntityPlayer_Class);
            
            if (env->IsInstanceOf(entityHitObj, entityLivingBaseClass)) {
                if (!playersOnly || (playersOnly && entityPlayerClass && env->IsInstanceOf(entityHitObj, entityPlayerClass))) {
                    
                    // Simple reach check using objectMouseOver
                    // Ideally we check hitVec distance to player eyes, but objectMouseOver already uses blockReachDistance (usually 3.0) for entities.
                    // If we need strict reach control, we calculate distance.
                    jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
                    if (entityClass) {
                        jfieldID pXF = env->GetFieldID(entityClass, Mappings::Entity_posX_Name, Mappings::Entity_posX_Sig);
                        jfieldID pYF = env->GetFieldID(entityClass, Mappings::Entity_posY_Name, Mappings::Entity_posY_Sig);
                        jfieldID pZF = env->GetFieldID(entityClass, Mappings::Entity_posZ_Name, Mappings::Entity_posZ_Sig);
                        
                        if (pXF && pYF && pZF) {
                            double eX = env->GetDoubleField(entityHitObj, pXF);
                            double eY = env->GetDoubleField(entityHitObj, pYF);
                            double eZ = env->GetDoubleField(entityHitObj, pZF);
                            
                            double pX = env->GetDoubleField(playerObj, pXF);
                            double pY = env->GetDoubleField(playerObj, pYF) + 1.62;
                            double pZ = env->GetDoubleField(playerObj, pZF);
                            
                            double dX = eX - pX, dY = eY - pY, dZ = eZ - pZ;
                            double dist = std::sqrt(dX*dX + dY*dY + dZ*dZ);
                            
                            if (dist <= reach) {
                                shouldClick = true;
                            }
                        } else {
                            shouldClick = true; // Fallback
                        }
                        env->DeleteLocalRef(entityClass);
                    } else {
                        shouldClick = true;
                    }

                    // Visibility Check
                    if (shouldClick && visibleOnly) {
                        jmethodID canSeeMethod = env->GetMethodID(entityLivingBaseClass, Mappings::EntityLivingBase_canEntityBeSeen_Name, Mappings::EntityLivingBase_canEntityBeSeen_Sig);
                        if (canSeeMethod) {
                            jboolean canSee = env->CallBooleanMethod(playerObj, canSeeMethod, entityHitObj);
                            if (env->ExceptionCheck()) env->ExceptionClear();
                            if (!canSee) shouldClick = false;
                        }
                    }
                }
            }

            if (entityLivingBaseClass) env->DeleteLocalRef(entityLivingBaseClass);
            if (entityPlayerClass) env->DeleteLocalRef(entityPlayerClass);
            env->DeleteLocalRef(entityHitObj);
        }

        env->DeleteLocalRef(mopClass);
        env->DeleteLocalRef(mopObj);
        env->DeleteLocalRef(mcClass);
        if (env->ExceptionCheck()) env->ExceptionClear();

        if (!shouldClick) return;

        // CPS timing
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (m_lastClickTime == 0) m_lastClickTime = now;
        if (m_nextCps <= 0) m_nextCps = 10;

        if ((now - m_lastClickTime) < (1000 / m_nextCps)) return;

        // Click
        POINT pos;
        GetCursorPos(&pos);
        ScreenToClient(g_hWnd, &pos);
        PostMessage(g_hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pos.x, pos.y));
        PostMessage(g_hWnd, WM_LBUTTONUP, 0, MAKELPARAM(pos.x, pos.y));

        m_lastClickTime = now;

        int cMin = minCps, cMax = maxCps;
        if (cMin > cMax) std::swap(cMin, cMax);
        if (cMin == cMax) { m_nextCps = cMin; return; }
        m_nextCps = cMin + (rand() % (cMax - cMin + 1));
    }

private:
    long long m_lastClickTime = 0;
    int m_nextCps = 10;
};
