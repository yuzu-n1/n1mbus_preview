#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include "jni_manager.hpp"
#include "minecraft_mappings.hpp"

class MappingResolver {
private:
    static std::string SlashToDot(const std::string& str) {
        std::string res = str;
        std::replace(res.begin(), res.end(), '/', '.');
        return res;
    }

    static std::string DotToSlash(const std::string& str) {
        std::string res = str;
        std::replace(res.begin(), res.end(), '.', '/');
        return res;
    }

public:
    // Try each candidate class name and return the first one that loads
    static jclass FindClassFromCandidates(JNIEnv* env, const std::vector<std::string>& candidates, std::string& outResolvedName) {
        for (const auto& candidate : candidates) {
            jclass clazz = JniManager::FindClassWithLoader(env, candidate.c_str());
            if (clazz) {
                outResolvedName = candidate;
                return clazz;
            }
        }
        return nullptr;
    }

    // Scan all declared fields of a class and return the name of the first field
    // whose type matches targetTypeSignature (e.g. "Lnet/minecraft/...;" or "Z"/"D"/"I"/"F")
    static std::string FindFieldByType(JNIEnv* env, jclass clazz, const std::string& targetTypeSignature, bool isStatic = false) {
        jclass classClass = env->FindClass("java/lang/Class");
        if (!classClass) return "";
        jmethodID getDeclaredFieldsMethod = env->GetMethodID(classClass, "getDeclaredFields", "()[Ljava/lang/reflect/Field;");
        jobjectArray fields = (jobjectArray)env->CallObjectMethod(clazz, getDeclaredFieldsMethod);
        if (!fields) {
            env->DeleteLocalRef(classClass);
            return "";
        }

        jsize len = env->GetArrayLength(fields);
        jclass fieldClass = env->FindClass("java/lang/reflect/Field");
        jmethodID getNameMethod = env->GetMethodID(fieldClass, "getName", "()Ljava/lang/String;");
        jmethodID getTypeMethod = env->GetMethodID(fieldClass, "getType", "()Ljava/lang/Class;");
        jmethodID getModifiersMethod = env->GetMethodID(fieldClass, "getModifiers", "()I");
        jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
        jmethodID isStaticMethod = env->GetStaticMethodID(modifierClass, "isStatic", "(I)Z");

        std::string resolvedFieldName = "";

        // Convert JNI signature format to Java class name
        std::string expectedTypeName = targetTypeSignature;
        if (!expectedTypeName.empty() && expectedTypeName[0] == 'L' && expectedTypeName.back() == ';') {
            expectedTypeName = expectedTypeName.substr(1, expectedTypeName.length() - 2);
            expectedTypeName = SlashToDot(expectedTypeName);
        } else {
            if (expectedTypeName == "Z") expectedTypeName = "boolean";
            else if (expectedTypeName == "D") expectedTypeName = "double";
            else if (expectedTypeName == "I") expectedTypeName = "int";
            else if (expectedTypeName == "F") expectedTypeName = "float";
        }

        for (jsize i = 0; i < len; i++) {
            jobject field = env->GetObjectArrayElement(fields, i);
            if (!field) continue;

            jint modifiers = env->CallIntMethod(field, getModifiersMethod);
            jboolean staticCheck = env->CallStaticBooleanMethod(modifierClass, isStaticMethod, modifiers);
            if ((staticCheck != JNI_FALSE) != isStatic) {
                env->DeleteLocalRef(field);
                continue;
            }

            jclass fieldTypeClass = (jclass)env->CallObjectMethod(field, getTypeMethod);
            if (!fieldTypeClass) {
                env->DeleteLocalRef(field);
                continue;
            }

            jclass classLoaderClass = env->FindClass("java/lang/Class");
            jmethodID getNameClassMethod = env->GetMethodID(classLoaderClass, "getName", "()Ljava/lang/String;");
            jstring typeNameJStr = (jstring)env->CallObjectMethod(fieldTypeClass, getNameClassMethod);

            const char* typeNameChars = env->GetStringUTFChars(typeNameJStr, nullptr);
            std::string typeName = typeNameChars;
            env->ReleaseStringUTFChars(typeNameJStr, typeNameChars);
            env->DeleteLocalRef(typeNameJStr);
            env->DeleteLocalRef(classLoaderClass);

            if (typeName == expectedTypeName) {
                jstring fieldNameJStr = (jstring)env->CallObjectMethod(field, getNameMethod);
                const char* fieldNameChars = env->GetStringUTFChars(fieldNameJStr, nullptr);
                resolvedFieldName = fieldNameChars;
                env->ReleaseStringUTFChars(fieldNameJStr, fieldNameChars);
                env->DeleteLocalRef(fieldNameJStr);
                env->DeleteLocalRef(fieldTypeClass);
                env->DeleteLocalRef(field);
                break;
            }

            env->DeleteLocalRef(fieldTypeClass);
            env->DeleteLocalRef(field);
        }

        env->DeleteLocalRef(modifierClass);
        env->DeleteLocalRef(fieldClass);
        env->DeleteLocalRef(fields);
        env->DeleteLocalRef(classClass);
        return resolvedFieldName;
    }

    // Try each candidate field name and return the first one that exists in the class
    static std::string FindFieldFromNames(JNIEnv* env, jclass clazz, const std::vector<std::string>& nameCandidates) {
        jclass classClass = env->FindClass("java/lang/Class");
        jmethodID getFieldMethod = env->GetMethodID(classClass, "getDeclaredField", "(Ljava/lang/String;)Ljava/lang/reflect/Field;");

        for (const auto& name : nameCandidates) {
            jstring jName = env->NewStringUTF(name.c_str());
            jobject field = env->CallObjectMethod(clazz, getFieldMethod, jName);
            env->DeleteLocalRef(jName);

            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            } else if (field) {
                env->DeleteLocalRef(field);
                env->DeleteLocalRef(classClass);
                return name;
            }
        }
        env->DeleteLocalRef(classClass);
        return "";
    }

    // Scan all declared methods of a class and return the name of the first method
    // that takes 0 arguments and returns the specified type.
    static std::string FindMethodByReturnTypeAndNoArgs(JNIEnv* env, jclass clazz, const std::string& targetTypeSignature, bool isStatic = false) {
        jclass classClass = env->FindClass("java/lang/Class");
        if (!classClass) {
            return "";
        }
        jmethodID getDeclaredMethodsMethod = env->GetMethodID(classClass, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
        jobjectArray methods = (jobjectArray)env->CallObjectMethod(clazz, getDeclaredMethodsMethod);
        if (!methods) {
            env->DeleteLocalRef(classClass);
            return "";
        }

        jsize len = env->GetArrayLength(methods);

        jclass methodClass = env->FindClass("java/lang/reflect/Method");
        jmethodID getNameMethod = env->GetMethodID(methodClass, "getName", "()Ljava/lang/String;");
        jmethodID getReturnTypeMethod = env->GetMethodID(methodClass, "getReturnType", "()Ljava/lang/Class;");
        jmethodID getParameterTypesMethod = env->GetMethodID(methodClass, "getParameterTypes", "()[Ljava/lang/Class;");
        jmethodID getModifiersMethod = env->GetMethodID(methodClass, "getModifiers", "()I");
        jclass modifierClass = env->FindClass("java/lang/reflect/Modifier");
        jmethodID isStaticMethod = env->GetStaticMethodID(modifierClass, "isStatic", "(I)Z");

        std::string resolvedMethodName = "";

        // Convert JNI signature format to Java class name
        std::string expectedTypeName = targetTypeSignature;
        if (!expectedTypeName.empty() && expectedTypeName[0] == 'L' && expectedTypeName.back() == ';') {
            expectedTypeName = expectedTypeName.substr(1, expectedTypeName.length() - 2);
            expectedTypeName = SlashToDot(expectedTypeName);
        }

        for (jsize i = 0; i < len; i++) {
            jobject methodObj = env->GetObjectArrayElement(methods, i);
            if (!methodObj) continue;

            jstring methodNameJStr = (jstring)env->CallObjectMethod(methodObj, getNameMethod);
            const char* methodNameChars = env->GetStringUTFChars(methodNameJStr, nullptr);
            std::string currentMethodName = methodNameChars;
            env->ReleaseStringUTFChars(methodNameJStr, methodNameChars);
            env->DeleteLocalRef(methodNameJStr);

            jint modifiers = env->CallIntMethod(methodObj, getModifiersMethod);
            jboolean staticCheck = env->CallStaticBooleanMethod(modifierClass, isStaticMethod, modifiers);
            bool isCurrentStatic = (staticCheck != JNI_FALSE);

            jobjectArray paramTypes = (jobjectArray)env->CallObjectMethod(methodObj, getParameterTypesMethod);
            int paramCount = paramTypes ? env->GetArrayLength(paramTypes) : 0;
            if (paramTypes) env->DeleteLocalRef(paramTypes);

            jclass returnTypeClass = (jclass)env->CallObjectMethod(methodObj, getReturnTypeMethod);
            std::string typeName = "unknown";
            if (returnTypeClass) {
                jmethodID getNameClassMethod = env->GetMethodID(classClass, "getName", "()Ljava/lang/String;");
                jstring typeNameJStr = (jstring)env->CallObjectMethod(returnTypeClass, getNameClassMethod);
                const char* typeNameChars = env->GetStringUTFChars(typeNameJStr, nullptr);
                typeName = typeNameChars;
                env->ReleaseStringUTFChars(typeNameJStr, typeNameChars);
                env->DeleteLocalRef(typeNameJStr);
                env->DeleteLocalRef(returnTypeClass);
            }

            if (isCurrentStatic == isStatic && paramCount == 0 && typeName == expectedTypeName && resolvedMethodName.empty()) {
                resolvedMethodName = currentMethodName;
            }

            env->DeleteLocalRef(methodObj);
        }

        env->DeleteLocalRef(modifierClass);
        env->DeleteLocalRef(methodClass);
        env->DeleteLocalRef(methods);
        env->DeleteLocalRef(classClass);
        return resolvedMethodName;
    }

    // Try each candidate method name and return the first one that exists in the class
    static std::string FindMethodFromNames(JNIEnv* env, jclass clazz, const std::vector<std::string>& nameCandidates, const char* sig, bool isStatic = false) {
        for (const auto& name : nameCandidates) {
            jmethodID mid = isStatic ? env->GetStaticMethodID(clazz, name.c_str(), sig) 
                                     : env->GetMethodID(clazz, name.c_str(), sig);
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            } else if (mid) {
                return name;
            }
        }
        return "";
    }

    // Resolve all mappings dynamically at runtime
    static bool ResolveAll() {
        JNIEnv* env = JniManager::GetEnv();
        if (!env) return false;

        auto Log = [](const char* fmt, ...) { }; // no-op

        Log("--- Starting mapping resolution ---\n");

        std::string resolvedMC, resolvedPlayer, resolvedWorld, resolvedScreen;
        std::string resolvedTimer, resolvedSettings, resolvedRenderer;
        std::string resolvedCap, resolvedKB;

        // --- Class resolution (SRG name first, Notch name as fallback) ---
        jclass mcClass = FindClassFromCandidates(env,
            {"net/minecraft/client/Minecraft", "ave"}, resolvedMC);
        if (!mcClass) return false;

        jclass playerClass = FindClassFromCandidates(env,
            {"net/minecraft/client/entity/EntityPlayerSP", "bew"}, resolvedPlayer);
        jclass worldClass = FindClassFromCandidates(env,
            {"net/minecraft/client/multiplayer/WorldClient", "bdb"}, resolvedWorld);
        jclass screenClass = FindClassFromCandidates(env,
            {"net/minecraft/client/gui/GuiScreen", "axu"}, resolvedScreen);
        jclass timerClass = FindClassFromCandidates(env,
            {"net/minecraft/util/Timer", "bas"}, resolvedTimer);
        jclass settingsClass = FindClassFromCandidates(env,
            {"net/minecraft/client/settings/GameSettings", "avs"}, resolvedSettings);
        jclass rendererClass = FindClassFromCandidates(env,
            {"net/minecraft/client/renderer/EntityRenderer", "el"}, resolvedRenderer);
        jclass capClass = FindClassFromCandidates(env,
            {"net/minecraft/entity/player/PlayerCapabilities", "ob"}, resolvedCap);
        jclass kbClass = FindClassFromCandidates(env,
            {"net/minecraft/client/settings/KeyBinding", "avt"}, resolvedKB);

        Log("Resolved MC: %s\n", resolvedMC.c_str());
        Log("Resolved Player: %s\n", resolvedPlayer.c_str());
        Log("Resolved World: %s\n", resolvedWorld.c_str());
        Log("Resolved Screen: %s\n", resolvedScreen.c_str());
        Log("Resolved Timer: %s\n", resolvedTimer.c_str());
        Log("Resolved Settings: %s\n", resolvedSettings.c_str());
        Log("Resolved Renderer: %s\n", resolvedRenderer.c_str());
        Log("Resolved Capabilities: %s\n", resolvedCap.c_str());
        Log("Resolved KeyBinding: %s\n", resolvedKB.c_str());

        // --- Build dynamic signatures ---
        static std::string mcSig       = "()L" + DotToSlash(resolvedMC) + ";";
        static std::string playerSig   = "L" + DotToSlash(resolvedPlayer) + ";";
        static std::string worldSig    = "L" + DotToSlash(resolvedWorld) + ";";
        static std::string screenSig   = "L" + DotToSlash(resolvedScreen) + ";";
        static std::string timerSig    = "L" + DotToSlash(resolvedTimer) + ";";
        static std::string settingsSig = "L" + DotToSlash(resolvedSettings) + ";";
        static std::string rendererSig = "L" + DotToSlash(resolvedRenderer) + ";";
        static std::string capSig      = "L" + DotToSlash(resolvedCap) + ";";
        static std::string kbSig       = "L" + DotToSlash(resolvedKB) + ";";

        // --- Update Mappings pointers ---
        Mappings::Minecraft_Class             = _strdup(DotToSlash(resolvedMC).c_str());
        Mappings::Minecraft_getMinecraft_Sig  = _strdup(mcSig.c_str());
        Mappings::Minecraft_thePlayer_Sig     = _strdup(playerSig.c_str());
        Mappings::Minecraft_theWorld_Sig      = _strdup(worldSig.c_str());
        Mappings::Minecraft_currentScreen_Sig = _strdup(screenSig.c_str());
        Mappings::Minecraft_timer_Sig         = _strdup(timerSig.c_str());
        Mappings::Minecraft_gameSettings_Sig  = _strdup(settingsSig.c_str());
        Mappings::Minecraft_entityRenderer_Sig= _strdup(rendererSig.c_str());
        Mappings::EntityPlayer_capabilities_Sig = _strdup(capSig.c_str());
        Mappings::GameSettings_keyBindSneak_Sig = _strdup(kbSig.c_str());
        Mappings::GameSettings_keyBindJump_Sig  = _strdup(kbSig.c_str());

        // Dynamic classes
        std::string resolvedMOP;
        jclass mopClass = FindClassFromCandidates(env,
            {"net/minecraft/util/MovingObjectPosition", "auh"}, resolvedMOP);
        if (mopClass) {
            Mappings::Minecraft_objectMouseOver_Sig = _strdup(("L" + DotToSlash(resolvedMOP) + ";").c_str());
            env->DeleteLocalRef(mopClass);
        }
        std::string resolvedEntityForSig;
        jclass entityClassForSig = FindClassFromCandidates(env,
            {"net/minecraft/entity/Entity", "pk"}, resolvedEntityForSig);
        if (entityClassForSig) {
            Mappings::MovingObjectPosition_entityHit_Sig = _strdup(("L" + DotToSlash(resolvedEntityForSig) + ";").c_str());
            env->DeleteLocalRef(entityClassForSig);
        }
        std::string resolvedMOT;
        jclass motClass = FindClassFromCandidates(env,
            {"net/minecraft/util/MovingObjectPosition$MovingObjectType", "auh$a"}, resolvedMOT);
        if (motClass) {
            Mappings::MovingObjectPosition_typeOfHit_Sig = _strdup(("L" + DotToSlash(resolvedMOT) + ";").c_str());
            env->DeleteLocalRef(motClass);
        }

        // --- Field name resolution by type scan ---
        static std::string thePlayer_Name = FindFieldByType(env, mcClass, playerSig);
        if (!thePlayer_Name.empty()) Mappings::Minecraft_thePlayer_Name = _strdup(thePlayer_Name.c_str());
        Log("thePlayer mapping: %s\n", thePlayer_Name.c_str());

        static std::string theWorld_Name = FindFieldByType(env, mcClass, worldSig);
        if (!theWorld_Name.empty()) Mappings::Minecraft_theWorld_Name = _strdup(theWorld_Name.c_str());
        Log("theWorld mapping: %s\n", theWorld_Name.c_str());

        static std::string currentScreen_Name = FindFieldByType(env, mcClass, screenSig);
        if (!currentScreen_Name.empty()) Mappings::Minecraft_currentScreen_Name = _strdup(currentScreen_Name.c_str());
        Log("currentScreen mapping: %s\n", currentScreen_Name.c_str());

        static std::string timer_Name = FindFieldByType(env, mcClass, timerSig);
        if (!timer_Name.empty()) Mappings::Minecraft_timer_Name = _strdup(timer_Name.c_str());
        Log("timer mapping: %s\n", timer_Name.c_str());

        static std::string gameSettings_Name = FindFieldByType(env, mcClass, settingsSig);
        if (!gameSettings_Name.empty()) Mappings::Minecraft_gameSettings_Name = _strdup(gameSettings_Name.c_str());
        Log("gameSettings mapping: %s\n", gameSettings_Name.c_str());

        static std::string entityRenderer_Name = FindFieldByType(env, mcClass, rendererSig);
        if (!entityRenderer_Name.empty()) Mappings::Minecraft_entityRenderer_Name = _strdup(entityRenderer_Name.c_str());
        Log("entityRenderer mapping: %s\n", entityRenderer_Name.c_str());

        // --- Method resolution by name candidates ---
        static std::string getMinecraft_Name = FindMethodFromNames(env, mcClass, {"func_71410_x", "getMinecraft", "A", "B", "C"}, Mappings::Minecraft_getMinecraft_Sig, true);
        if (getMinecraft_Name.empty()) {
            getMinecraft_Name = FindMethodByReturnTypeAndNoArgs(env, mcClass, "L" + DotToSlash(resolvedMC) + ";", true);
        }
        if (!getMinecraft_Name.empty()) Mappings::Minecraft_getMinecraft_Name = _strdup(getMinecraft_Name.c_str());
        Log("getMinecraft mapping: %s\n", getMinecraft_Name.c_str());

        static std::string rcDelay_Name = FindFieldFromNames(env, mcClass,
            {"field_71467_ac", "rightClickDelayTimer", "ap"});
        if (!rcDelay_Name.empty()) Mappings::Minecraft_rightClickDelayTimer_Name = _strdup(rcDelay_Name.c_str());

        // TargetHUD requires objectMouseOver
        static std::string objectMouseOver_Name = FindFieldFromNames(env, mcClass,
            {"field_71476_x", "objectMouseOver", "s"});
        if (!objectMouseOver_Name.empty()) Mappings::Minecraft_objectMouseOver_Name = _strdup(objectMouseOver_Name.c_str());
        Log("objectMouseOver mapping: %s\n", objectMouseOver_Name.c_str());

        // --- Other class member resolution ---
        // capabilities and inventory are declared on EntityPlayer (superclass), not EntityPlayerSP
        {
            std::string resolvedEntityPlayer;
            jclass entityPlayerClass = FindClassFromCandidates(env,
                {"net/minecraft/entity/player/EntityPlayer", "qn"}, resolvedEntityPlayer);
            if (entityPlayerClass) {
                static std::string cap_Name = FindFieldFromNames(env, entityPlayerClass,
                    {"field_71075_bZ", "capabilities"});
                if (!cap_Name.empty()) Mappings::EntityPlayer_capabilities_Name = _strdup(cap_Name.c_str());
                Log("capabilities mapping: %s\n", cap_Name.c_str());

                static std::string inv_Name = FindFieldFromNames(env, entityPlayerClass,
                    {"field_71071_by", "inventory"});
                if (!inv_Name.empty()) Mappings::EntityPlayer_inventory_Name = _strdup(inv_Name.c_str());
                Log("inventory mapping: %s\n", inv_Name.c_str());

                env->DeleteLocalRef(entityPlayerClass);
            } else {
                Log("EntityPlayer class not found!\n");
            }
        }
        if (capClass) {
            // PlayerCapabilities has 3 booleans: isCreativeMode, disableDamage, allowFlying, isFlying
            // Use name candidates instead of type scan to avoid ambiguity
            static std::string isFlying_Name = FindFieldFromNames(env, capClass,
                {"field_75100_b", "isFlying", "b"});
            if (!isFlying_Name.empty()) Mappings::PlayerCapabilities_isFlying_Name = _strdup(isFlying_Name.c_str());
            Log("isFlying mapping: %s\n", isFlying_Name.c_str());

            static std::string allowFlying_Name = FindFieldFromNames(env, capClass,
                {"field_75101_c", "allowFlying", "c"});
            if (!allowFlying_Name.empty()) Mappings::PlayerCapabilities_allowFlying_Name = _strdup(allowFlying_Name.c_str());
            Log("allowFlying mapping: %s\n", allowFlying_Name.c_str());

            static std::string flySpeed_Name = FindFieldFromNames(env, capClass,
                {"field_75096_f", "flySpeed", "f"});
            if (!flySpeed_Name.empty()) Mappings::PlayerCapabilities_flySpeed_Name = _strdup(flySpeed_Name.c_str());
            Log("flySpeed mapping: %s\n", flySpeed_Name.c_str());
        }
        if (kbClass) {
            static std::string pressed_Name = FindFieldFromNames(env, kbClass,
                {"field_74513_e", "pressed", "pressed", "e"});
            if (!pressed_Name.empty()) Mappings::KeyBinding_pressed_Name = _strdup(pressed_Name.c_str());
            Log("pressed mapping: %s\n", pressed_Name.c_str());
        }

        // Entity class
        std::string resolvedEntity;
        jclass entityClass = FindClassFromCandidates(env,
            {"net/minecraft/entity/Entity", "pk"}, resolvedEntity);
        if (entityClass) {
            static std::string onGround_Name = FindFieldFromNames(env, entityClass,
                {"field_70122_E", "onGround"});
            if (!onGround_Name.empty()) Mappings::Entity_onGround_Name = _strdup(onGround_Name.c_str());
            Log("onGround mapping: %s\n", onGround_Name.c_str());

            static std::string motionX_Name = FindFieldFromNames(env, entityClass,
                {"field_70159_w", "motionX"});
            if (!motionX_Name.empty()) Mappings::Entity_motionX_Name = _strdup(motionX_Name.c_str());
            Log("motionX mapping: %s\n", motionX_Name.c_str());

            static std::string motionY_Name = FindFieldFromNames(env, entityClass,
                {"field_70181_x", "motionY"});
            if (!motionY_Name.empty()) Mappings::Entity_motionY_Name = _strdup(motionY_Name.c_str());
            Log("motionY mapping: %s\n", motionY_Name.c_str());

            static std::string motionZ_Name = FindFieldFromNames(env, entityClass,
                {"field_70179_y", "motionZ"});
            if (!motionZ_Name.empty()) Mappings::Entity_motionZ_Name = _strdup(motionZ_Name.c_str());
            Log("motionZ mapping: %s\n", motionZ_Name.c_str());

            static std::string rotYaw_Name = FindFieldFromNames(env, entityClass,
                {"field_70177_z", "rotationYaw"});
            if (!rotYaw_Name.empty()) Mappings::Entity_rotationYaw_Name = _strdup(rotYaw_Name.c_str());
            Log("rotationYaw mapping: %s\n", rotYaw_Name.c_str());

            static std::string rotPitch_Name = FindFieldFromNames(env, entityClass,
                {"field_70125_A", "rotationPitch"});
            if (!rotPitch_Name.empty()) Mappings::Entity_rotationPitch_Name = _strdup(rotPitch_Name.c_str());
            Log("rotationPitch mapping: %s\n", rotPitch_Name.c_str());

            static std::string posX_Name = FindFieldFromNames(env, entityClass,
                {"field_70165_t", "posX"});
            if (!posX_Name.empty()) Mappings::Entity_posX_Name = _strdup(posX_Name.c_str());

            static std::string posY_Name = FindFieldFromNames(env, entityClass,
                {"field_70163_u", "posY"});
            if (!posY_Name.empty()) Mappings::Entity_posY_Name = _strdup(posY_Name.c_str());

            static std::string posZ_Name = FindFieldFromNames(env, entityClass,
                {"field_70161_v", "posZ"});
            if (!posZ_Name.empty()) Mappings::Entity_posZ_Name = _strdup(posZ_Name.c_str());

            static std::string hurtTime_Name = FindFieldFromNames(env, entityClass,
                {"field_70737_aN", "hurtTime"});
            // hurtTime is on EntityLivingBase, try there first via entityClass (may not be found),
            // we'll also search in livingBaseClass below
            if (!hurtTime_Name.empty()) Mappings::EntityLivingBase_hurtTime_Name = _strdup(hurtTime_Name.c_str());
            Log("hurtTime (from Entity) mapping: %s\n", hurtTime_Name.c_str());

            static std::string setSprinting_Name = FindMethodFromNames(env, entityClass,
                {"func_70031_b", "setSprinting"}, Mappings::Entity_setSprinting_Sig);
            if (!setSprinting_Name.empty()) Mappings::Entity_setSprinting_Name = _strdup(setSprinting_Name.c_str());
            Log("setSprinting mapping: %s\n", setSprinting_Name.c_str());

            env->DeleteLocalRef(entityClass);
        }

        // Minecraft click methods
        static std::string clickMouse_Name = FindMethodFromNames(env, mcClass,
            {"func_147116_af", "clickMouse"}, "()V");
        if (!clickMouse_Name.empty()) Mappings::Minecraft_clickMouse_Name = _strdup(clickMouse_Name.c_str());
        Log("clickMouse mapping: %s\n", clickMouse_Name.c_str());

        static std::string rightClick_Name = FindMethodFromNames(env, mcClass,
            {"func_147121_ag", "rightClickMouse"}, "()V");
        if (!rightClick_Name.empty()) Mappings::Minecraft_rightClickMouse_Name = _strdup(rightClick_Name.c_str());
        Log("rightClickMouse mapping: %s\n", rightClick_Name.c_str());

        // EntityLivingBase
        std::string resolvedLivingBase;
        jclass livingBaseClass = FindClassFromCandidates(env,
            {"net/minecraft/entity/EntityLivingBase", "pr"}, resolvedLivingBase);
        if (livingBaseClass) {
            static std::string getHealth_Name = FindMethodFromNames(env, livingBaseClass,
                {"func_110143_aJ", "getHealth"}, Mappings::EntityLivingBase_getHealth_Sig);
            if (!getHealth_Name.empty()) Mappings::EntityLivingBase_getHealth_Name = _strdup(getHealth_Name.c_str());
            Log("getHealth mapping: %s\n", getHealth_Name.c_str());

            static std::string getMaxHealth_Name = FindMethodFromNames(env, livingBaseClass,
                {"func_110138_aP", "getMaxHealth"}, Mappings::EntityLivingBase_getMaxHealth_Sig);
            if (!getMaxHealth_Name.empty()) Mappings::EntityLivingBase_getMaxHealth_Name = _strdup(getMaxHealth_Name.c_str());
            Log("getMaxHealth mapping: %s\n", getMaxHealth_Name.c_str());

            static std::string canSee_Name = FindMethodFromNames(env, livingBaseClass,
                {"func_70685_l", "canEntityBeSeen"}, Mappings::EntityLivingBase_canEntityBeSeen_Sig);
            if (!canSee_Name.empty()) Mappings::EntityLivingBase_canEntityBeSeen_Name = _strdup(canSee_Name.c_str());
            Log("canEntityBeSeen mapping: %s\n", canSee_Name.c_str());

            static std::string swingItem_Name = FindMethodFromNames(env, livingBaseClass,
                {"func_71038_i", "swingItem"}, Mappings::EntityLivingBase_swingItem_Sig);
            if (!swingItem_Name.empty()) Mappings::EntityLivingBase_swingItem_Name = _strdup(swingItem_Name.c_str());
            Log("swingItem mapping: %s\n", swingItem_Name.c_str());

            static std::string jump_Name = FindMethodFromNames(env, livingBaseClass,
                {"func_70664_aZ", "jump"}, Mappings::EntityLivingBase_jump_Sig);
            if (!jump_Name.empty()) Mappings::EntityLivingBase_jump_Name = _strdup(jump_Name.c_str());
            Log("jump mapping: %s\n", jump_Name.c_str());

            // hurtTime is on EntityLivingBase
            static std::string hurtTime_lb_Name = FindFieldFromNames(env, livingBaseClass,
                {"field_70737_aN", "hurtTime"});
            if (!hurtTime_lb_Name.empty()) Mappings::EntityLivingBase_hurtTime_Name = _strdup(hurtTime_lb_Name.c_str());
            Log("hurtTime (from LivingBase) mapping: %s\n", hurtTime_lb_Name.c_str());

            env->DeleteLocalRef(livingBaseClass);
        }

        // World class fields
        {
            std::string resolvedWorldSuper;
            jclass worldSuperClass = FindClassFromCandidates(env,
                {"net/minecraft/world/World", "adm"}, resolvedWorldSuper);
            
            if (worldSuperClass) {
                static std::string loadedEntityList_Name = FindFieldFromNames(env, worldSuperClass,
                    {"field_72996_f", "loadedEntityList"});
                if (!loadedEntityList_Name.empty()) Mappings::World_loadedEntityList_Name = _strdup(loadedEntityList_Name.c_str());
                Log("loadedEntityList mapping: %s\n", loadedEntityList_Name.c_str());

                static std::string isAirBlock_Name = FindMethodFromNames(env, worldSuperClass,
                    {"func_175623_d", "isAirBlock"}, Mappings::World_isAirBlock_Sig);
                if (!isAirBlock_Name.empty()) Mappings::World_isAirBlock_Name = _strdup(isAirBlock_Name.c_str());
                Log("isAirBlock mapping: %s\n", isAirBlock_Name.c_str());
                
                env->DeleteLocalRef(worldSuperClass);
            } else {
                Log("World superclass not found!\n");
            }
        }

        // Entity.getName
        {
            std::string resolvedEntity2;
            jclass entityClass2 = FindClassFromCandidates(env,
                {"net/minecraft/entity/Entity", "pk"}, resolvedEntity2);
            if (entityClass2) {
                static std::string getName_Name = FindMethodFromNames(env, entityClass2,
                    {"func_70005_c_", "getName"}, Mappings::Entity_getName_Sig);
                if (!getName_Name.empty()) Mappings::Entity_getName_Name = _strdup(getName_Name.c_str());
                Log("getName mapping: %s\n", getName_Name.c_str());
                env->DeleteLocalRef(entityClass2);
            }
        }

        // MovingObjectPosition
        {
            std::string resolvedMOP;
            jclass mopClass = FindClassFromCandidates(env,
                {"net/minecraft/util/MovingObjectPosition", "auh"}, resolvedMOP);
            if (mopClass) {
                static std::string entityHit_Name = FindFieldFromNames(env, mopClass,
                    {"field_72308_g", "entityHit"});
                if (!entityHit_Name.empty()) Mappings::MovingObjectPosition_entityHit_Name = _strdup(entityHit_Name.c_str());
                Log("entityHit mapping: %s\n", entityHit_Name.c_str());

                static std::string typeOfHit_Name = FindFieldFromNames(env, mopClass,
                    {"field_72313_a", "typeOfHit"});
                if (!typeOfHit_Name.empty()) Mappings::MovingObjectPosition_typeOfHit_Name = _strdup(typeOfHit_Name.c_str());
                Log("typeOfHit mapping: %s\n", typeOfHit_Name.c_str());

                env->DeleteLocalRef(mopClass);
            } else {
                Log("MovingObjectPosition class not found!\n");
            }
        }

        Log("--- Mapping resolution complete ---\n");

        // Cleanup
        if (mcClass)       env->DeleteLocalRef(mcClass);
        if (playerClass)   env->DeleteLocalRef(playerClass);
        if (worldClass)    env->DeleteLocalRef(worldClass);
        if (screenClass)   env->DeleteLocalRef(screenClass);
        if (timerClass)    env->DeleteLocalRef(timerClass);
        if (settingsClass) env->DeleteLocalRef(settingsClass);
        if (rendererClass) env->DeleteLocalRef(rendererClass);
        if (capClass)      env->DeleteLocalRef(capClass);
        if (kbClass)       env->DeleteLocalRef(kbClass);

        return true;
    }
};
