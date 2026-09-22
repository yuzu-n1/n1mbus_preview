#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../mapping_resolver.hpp"
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
        
        _resolveLcField(env, mcClass);
        
        // Prevent clicking while Minecraft menu (inventory, chat, etc.) is open
        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        if (currentScreenF) {
            jobject currentScreenObj = env->GetObjectField(mcObj, currentScreenF);
            if (currentScreenObj) {
                env->DeleteLocalRef(currentScreenObj);
                m_pend.firedAt = 0;
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

        if (!shouldClick) { m_pend.firedAt = 0; return; }

        // CPS timing
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (m_lastClickTime == 0) m_lastClickTime = now;
        if (m_nextCps <= 0) m_nextCps = 10;

        jmethodID meth = _clickMethod(env, mcObj);

        // Settle a pending native click (JNI fallback if the game dropped it)
        _settle(env, mcObj, meth);

        if ((now - m_lastClickTime) < (1000 / m_nextCps)) return;

        if (m_pend.canFire()) {
            _fire(env, mcObj, meth);
            m_lastClickTime = now;

            int cMin = minCps, cMax = maxCps;
            if (cMin > cMax) std::swap(cMin, cMax);
            if (cMin == cMax) { m_nextCps = cMin; return; }
            m_nextCps = cMin + (rand() % (cMax - cMin + 1));
        }
    }

private:
    long long m_lastClickTime = 0;
    int m_nextCps = 10;

    struct Pend {
        long long firedAt = 0;
        int  lcBase = -1;
        bool sawChange = false;
        bool jniFired = false;
        bool canFire() const { return firedAt == 0; }
    };
    Pend m_pend;

    static jfieldID s_lcField;
    static bool     s_lcTried;

    static jmethodID _clickMethod(JNIEnv* env, jobject mcObj) {
        jclass mcCls = env->GetObjectClass(mcObj);
        if (!mcCls) return nullptr;
        jmethodID m = env->GetMethodID(mcCls, Mappings::Minecraft_clickMouse_Name, Mappings::Minecraft_clickMouse_Sig);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(mcCls);
        return m;
    }

    static void _resolveLcField(JNIEnv* env, jclass mcClass) {
        if (s_lcTried) return;
        s_lcTried = true;
        std::string nm = MappingResolver::FindFieldFromNames(env, mcClass, {"leftClickCounter", "field_71471_av"});
        if (!nm.empty()) {
            s_lcField = env->GetFieldID(mcClass, nm.c_str(), "I");
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
    }

    static int _readLc(JNIEnv* env, jobject mcObj) {
        if (!s_lcField) return -1;
        int v = env->GetIntField(mcObj, s_lcField);
        if (env->ExceptionCheck()) { env->ExceptionClear(); return -1; }
        return v;
    }

    static long long _nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    void _fire(JNIEnv* env, jobject mcObj, jmethodID meth) {
        m_pend.firedAt = _nowMs();
        m_pend.sawChange = false;
        m_pend.jniFired = false;
        m_pend.lcBase = _readLc(env, mcObj);
        if (m_pend.lcBase == -1) {
            // Can't verify native registration → rely on JNI directly.
            m_pend.firedAt = 0;
            if (meth) env->CallVoidMethod(mcObj, meth);
            if (env->ExceptionCheck()) env->ExceptionClear();
            return;
        }
        // Real native click (visible to Lunar-style CPS counters)
        INPUT in[2] = {};
        in[0].type = INPUT_MOUSE;
        in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
        in[1].type = INPUT_MOUSE;
        in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
        SendInput(2, in, sizeof(INPUT));
    }

    void _settle(JNIEnv* env, jobject mcObj, jmethodID meth) {
        if (m_pend.firedAt == 0) return;
        int lc = _readLc(env, mcObj);
        if (!m_pend.sawChange && lc != -1 && lc != m_pend.lcBase) m_pend.sawChange = true;
        if (m_pend.sawChange) { m_pend.firedAt = 0; return; }
        if ((_nowMs() - m_pend.firedAt) >= 80 && !m_pend.jniFired) {
            m_pend.jniFired = true;
            if (meth) env->CallVoidMethod(mcObj, meth);
            if (env->ExceptionCheck()) env->ExceptionClear();
            m_pend.firedAt = 0;
        }
    }
};

jfieldID TriggerBot::s_lcField = nullptr;
bool     TriggerBot::s_lcTried = false;
