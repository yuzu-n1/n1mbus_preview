#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <cmath>

class AimAssist : public Module {
public:
    float speed = 5.0f;     // 1-10 smoothness
    float reach = 4.0f;     // max distance
    float fov = 90.0f;      // field-of-view cone
    bool playersOnly = false;
    bool visibleOnly = true; // LoS check (no wall aim)

    AimAssist() : Module("AimAssist") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;
        // Only aim when left button is held (attacking)
        if (!(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) return;

        jclass mcClass = env->GetObjectClass(mcObj);

        // GUI Check (do not aim if inventory/chat/menu is open)
        jfieldID currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);
        if (currentScreenF) {
            jobject screenObj = env->GetObjectField(mcObj, currentScreenF);
            if (screenObj) {
                env->DeleteLocalRef(screenObj);
                env->DeleteLocalRef(mcClass);
                return;
            }
        }

        jfieldID worldField = env->GetFieldID(mcClass, Mappings::Minecraft_theWorld_Name, Mappings::Minecraft_theWorld_Sig);
        if (!worldField) { _ex(env); env->DeleteLocalRef(mcClass); return; }

        jobject worldObj = env->GetObjectField(mcObj, worldField);
        if (!worldObj) { _ex(env); env->DeleteLocalRef(mcClass); return; }

        jclass worldClass = env->GetObjectClass(worldObj);
        jfieldID loadedEntsField = env->GetFieldID(worldClass, Mappings::World_loadedEntityList_Name, Mappings::World_loadedEntityList_Sig);
        if (!loadedEntsField) { _ex(env); env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass); return; }

        jobject loadedEnts = env->GetObjectField(worldObj, loadedEntsField);
        if (!loadedEnts) { _ex(env); env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass); return; }

        jclass listClass = env->GetObjectClass(loadedEnts);
        jmethodID toArrayMethod = env->GetMethodID(listClass, "toArray", "()[Ljava/lang/Object;");
        if (!toArrayMethod) { _ex(env); env->DeleteLocalRef(listClass); env->DeleteLocalRef(loadedEnts); env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass); return; }

        jobjectArray entityArray = (jobjectArray)env->CallObjectMethod(loadedEnts, toArrayMethod);
        if (!entityArray) { _ex(env); env->DeleteLocalRef(listClass); env->DeleteLocalRef(loadedEnts); env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass); return; }

        int count = env->GetArrayLength(entityArray);

        // Load type-check classes
        jclass entityLivingBaseClass = JniManager::FindClassWithLoader(env, Mappings::EntityLivingBase_Class);
        jclass entityPlayerClass = JniManager::FindClassWithLoader(env, Mappings::EntityPlayer_Class);
        jclass entityClass = JniManager::FindClassWithLoader(env, Mappings::Entity_Class);
        _ex(env);
        if (!entityClass || !entityLivingBaseClass) {
            if (entityLivingBaseClass) env->DeleteLocalRef(entityLivingBaseClass);
            if (entityPlayerClass) env->DeleteLocalRef(entityPlayerClass);
            if (entityClass) env->DeleteLocalRef(entityClass);
            env->DeleteLocalRef(entityArray); env->DeleteLocalRef(listClass); env->DeleteLocalRef(loadedEnts);
            env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass);
            return;
        }

        jfieldID pXField = env->GetFieldID(entityClass, Mappings::Entity_posX_Name, Mappings::Entity_posX_Sig);
        jfieldID pYField = env->GetFieldID(entityClass, Mappings::Entity_posY_Name, Mappings::Entity_posY_Sig);
        jfieldID pZField = env->GetFieldID(entityClass, Mappings::Entity_posZ_Name, Mappings::Entity_posZ_Sig);
        jfieldID yawF = env->GetFieldID(entityClass, Mappings::Entity_rotationYaw_Name, Mappings::Entity_rotationYaw_Sig);
        jfieldID pitchF = env->GetFieldID(entityClass, Mappings::Entity_rotationPitch_Name, Mappings::Entity_rotationPitch_Sig);
        _ex(env);
        if (!pXField || !yawF || !pitchF) {
            env->DeleteLocalRef(entityLivingBaseClass); if (entityPlayerClass) env->DeleteLocalRef(entityPlayerClass);
            env->DeleteLocalRef(entityClass); env->DeleteLocalRef(entityArray); env->DeleteLocalRef(listClass);
            env->DeleteLocalRef(loadedEnts); env->DeleteLocalRef(worldClass); env->DeleteLocalRef(worldObj); env->DeleteLocalRef(mcClass);
            return;
        }

        // canEntityBeSeen method for LoS check
        jmethodID canSeeMethod = nullptr;
        if (visibleOnly) {
            canSeeMethod = env->GetMethodID(entityLivingBaseClass, Mappings::EntityLivingBase_canEntityBeSeen_Name, Mappings::EntityLivingBase_canEntityBeSeen_Sig);
            _ex(env);
        }

        double pX = env->GetDoubleField(playerObj, pXField);
        double pY = env->GetDoubleField(playerObj, pYField) + 1.62; // eye height
        double pZ = env->GetDoubleField(playerObj, pZField);
        float pYaw = env->GetFloatField(playerObj, yawF);
        float pPitch = env->GetFloatField(playerObj, pitchF);

        jobject targetObj = nullptr;
        double bestDist = reach;

        for (int i = 0; i < count; i++) {
            jobject entObj = env->GetObjectArrayElement(entityArray, i);
            if (!entObj || env->IsSameObject(entObj, playerObj)) {
                if (entObj) env->DeleteLocalRef(entObj);
                continue;
            }

            // Filter: must be EntityLivingBase (skip items, XP orbs, arrows etc.)
            if (!env->IsInstanceOf(entObj, entityLivingBaseClass)) {
                env->DeleteLocalRef(entObj);
                continue;
            }

            // Exclude ArmorStands (Holograms)
            jclass armorStandClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/item/EntityArmorStand");
            if (armorStandClass) {
                bool isArmorStand = env->IsInstanceOf(entObj, armorStandClass);
                env->DeleteLocalRef(armorStandClass);
                if (isArmorStand) {
                    env->DeleteLocalRef(entObj);
                    continue;
                }
            }

            // Players-only filter
            if (playersOnly && entityPlayerClass && !env->IsInstanceOf(entObj, entityPlayerClass)) {
                env->DeleteLocalRef(entObj);
                continue;
            }

            double eX = env->GetDoubleField(entObj, pXField);
            double eY = env->GetDoubleField(entObj, pYField) + 1.0;
            double eZ = env->GetDoubleField(entObj, pZField);

            double dX = eX - pX, dY = eY - pY, dZ = eZ - pZ;
            double dist = std::sqrt(dX*dX + dY*dY + dZ*dZ);
            if (dist > reach || dist < 0.5) { env->DeleteLocalRef(entObj); continue; }

            // FOV check
            float yawTo = (float)(std::atan2(dZ, dX) * 180.0 / 3.14159265358979) - 90.0f;
            float yawDiff = std::fmod(std::abs(yawTo - pYaw), 360.0f);
            if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;
            if (yawDiff > fov / 2.0f) { env->DeleteLocalRef(entObj); continue; }

            // Line-of-sight check (wall check)
            if (visibleOnly && canSeeMethod) {
                jboolean canSee = env->CallBooleanMethod(playerObj, canSeeMethod, entObj);
                _ex(env);
                if (!canSee) { env->DeleteLocalRef(entObj); continue; }
            }

            if (dist < bestDist) {
                bestDist = dist;
                if (targetObj) env->DeleteLocalRef(targetObj);
                targetObj = env->NewLocalRef(entObj);
            }
            env->DeleteLocalRef(entObj);
        }

        if (targetObj) {
            double eX = env->GetDoubleField(targetObj, pXField);
            double eY = env->GetDoubleField(targetObj, pYField) + 1.0;
            double eZ = env->GetDoubleField(targetObj, pZField);

            double dX = eX - pX, dY = eY - pY, dZ = eZ - pZ;

            float yawTo = (float)(std::atan2(dZ, dX) * 180.0 / 3.14159265358979) - 90.0f;
            float pitchTo = (float)-(std::atan2(dY, std::sqrt(dX*dX + dZ*dZ)) * 180.0 / 3.14159265358979);

            float yDiff = std::fmod(yawTo - pYaw, 360.0f);
            if (yDiff > 180.0f) yDiff -= 360.0f;
            if (yDiff < -180.0f) yDiff += 360.0f;
            float pDiff = pitchTo - pPitch;

            float factor = speed / 10.0f; // speed 1-10 -> 0.1-1.0 blend
            float stepYaw = yDiff * factor * 0.5f;
            float stepPitch = pDiff * factor * 0.5f;

            env->SetFloatField(playerObj, yawF, pYaw + stepYaw);
            env->SetFloatField(playerObj, pitchF, pPitch + stepPitch);

            env->DeleteLocalRef(targetObj);
        }

        // Cleanup
        if (entityLivingBaseClass) env->DeleteLocalRef(entityLivingBaseClass);
        if (entityPlayerClass) env->DeleteLocalRef(entityPlayerClass);
        env->DeleteLocalRef(entityClass);
        env->DeleteLocalRef(entityArray);
        env->DeleteLocalRef(listClass);
        env->DeleteLocalRef(loadedEnts);
        env->DeleteLocalRef(worldClass);
        env->DeleteLocalRef(worldObj);
        env->DeleteLocalRef(mcClass);
        _ex(env);
    }

private:
    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }
};
