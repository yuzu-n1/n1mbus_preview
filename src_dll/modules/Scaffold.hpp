#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include <Windows.h>
#include <cmath>
#include <chrono>
#include <random>

class Scaffold : public Module {
public:
    Scaffold() : Module("Scaffold") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;

        // --- Cache JNI IDs ---
        if (!m_cached) {
            jclass mcClass = env->GetObjectClass(mcObj);
            
            // Player fields
            m_yawF      = env->GetFieldID(playerClass, Mappings::Entity_rotationYaw_Name, Mappings::Entity_rotationYaw_Sig);
            m_pitchF    = env->GetFieldID(playerClass, Mappings::Entity_rotationPitch_Name, Mappings::Entity_rotationPitch_Sig);
            m_onGroundF = env->GetFieldID(playerClass, Mappings::Entity_onGround_Name, Mappings::Entity_onGround_Sig);
            m_motionXF  = env->GetFieldID(playerClass, Mappings::Entity_motionX_Name, Mappings::Entity_motionX_Sig);
            m_motionYF  = env->GetFieldID(playerClass, Mappings::Entity_motionY_Name, Mappings::Entity_motionY_Sig);
            m_motionZF  = env->GetFieldID(playerClass, Mappings::Entity_motionZ_Name, Mappings::Entity_motionZ_Sig);
            m_posXF     = env->GetFieldID(playerClass, Mappings::Entity_posX_Name, Mappings::Entity_posX_Sig);
            m_posYF     = env->GetFieldID(playerClass, Mappings::Entity_posY_Name, Mappings::Entity_posY_Sig);
            m_posZF     = env->GetFieldID(playerClass, Mappings::Entity_posZ_Name, Mappings::Entity_posZ_Sig);
            m_jumpM     = env->GetMethodID(playerClass, Mappings::EntityLivingBase_jump_Name, Mappings::EntityLivingBase_jump_Sig);

            
            // AutoBlock fields
            m_inventoryF = env->GetFieldID(playerClass, Mappings::EntityPlayer_inventory_Name, Mappings::EntityPlayer_inventory_Sig);
            jclass invClass = JniManager::FindClassWithLoader(env, Mappings::InventoryPlayer_Class);
            if (invClass) {
                m_currentItemF = env->GetFieldID(invClass, Mappings::InventoryPlayer_currentItem_Name, Mappings::InventoryPlayer_currentItem_Sig);
                m_getStackInSlotM = env->GetMethodID(invClass, Mappings::InventoryPlayer_getStackInSlot_Name, Mappings::InventoryPlayer_getStackInSlot_Sig);
                env->DeleteLocalRef(invClass);
            }
            jclass itemStackClass = JniManager::FindClassWithLoader(env, Mappings::ItemStack_Class);
            if (itemStackClass) {
                m_getItemM = env->GetMethodID(itemStackClass, Mappings::ItemStack_getItem_Name, Mappings::ItemStack_getItem_Sig);
                env->DeleteLocalRef(itemStackClass);
            }
            jclass itemBlockCls = JniManager::FindClassWithLoader(env, Mappings::ItemBlock_Class);
            if (itemBlockCls) {
                m_itemBlockClass = (jclass)env->NewGlobalRef(itemBlockCls);
                env->DeleteLocalRef(itemBlockCls);
            }

            // Silent Rotation Agent
            jclass agentLocal = JniManager::FindClassWithLoader(env, "n1mbus/N1mbusAgent");
            if (agentLocal) {
                m_agentClass = (jclass)env->NewGlobalRef(agentLocal);
                m_setSilentRotation = env->GetStaticMethodID(m_agentClass, "setSilentRotation", "(FF)V");
                m_setSilentActive   = env->GetStaticMethodID(m_agentClass, "setSilentActive", "(Z)V");
                env->DeleteLocalRef(agentLocal);
            }

            // Raycast / Emulate Click
            m_rightClickDelayF = env->GetFieldID(mcClass, Mappings::Minecraft_rightClickDelayTimer_Name, Mappings::Minecraft_rightClickDelayTimer_Sig);
            m_rightClickM      = env->GetMethodID(mcClass, Mappings::Minecraft_rightClickMouse_Name, Mappings::Minecraft_rightClickMouse_Sig);
            m_entityRendererF  = env->GetFieldID(mcClass, Mappings::Minecraft_entityRenderer_Name, Mappings::Minecraft_entityRenderer_Sig);
            
            if (m_entityRendererF) {
                jobject erObj = env->GetObjectField(mcObj, m_entityRendererF);
                if (erObj) {
                    jclass erClass = env->GetObjectClass(erObj);
                    m_getMouseOverM = env->GetMethodID(erClass, Mappings::EntityRenderer_getMouseOver_Name, Mappings::EntityRenderer_getMouseOver_Sig);
                    env->DeleteLocalRef(erClass);
                    env->DeleteLocalRef(erObj);
                }
            }

            m_objectMouseOverF = env->GetFieldID(mcClass, Mappings::Minecraft_objectMouseOver_Name, Mappings::Minecraft_objectMouseOver_Sig);
            jclass mopClass = JniManager::FindClassWithLoader(env, Mappings::MovingObjectPosition_Class);
            if (mopClass) {
                m_typeOfHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_typeOfHit_Name, Mappings::MovingObjectPosition_typeOfHit_Sig);
                m_sideHitF = env->GetFieldID(mopClass, Mappings::MovingObjectPosition_sideHit_Name, Mappings::MovingObjectPosition_sideHit_Sig);
                env->DeleteLocalRef(mopClass);
            }
            jclass enumFacingClass = JniManager::FindClassWithLoader(env, Mappings::EnumFacing_Class);
            if (enumFacingClass) {
                jfieldID upF = env->GetStaticFieldID(enumFacingClass, Mappings::EnumFacing_UP_Name, Mappings::EnumFacing_UP_Sig);
                if (upF) {
                    jobject upEnum = env->GetStaticObjectField(enumFacingClass, upF);
                    m_enumFacingUP = env->NewGlobalRef(upEnum);
                    env->DeleteLocalRef(upEnum);
                }
                env->DeleteLocalRef(enumFacingClass);
            }
            jclass mopTypeClass = JniManager::FindClassWithLoader(env, Mappings::MovingObjectType_Class);
            if (mopTypeClass) {
                m_typeBlockF = env->GetStaticFieldID(mopTypeClass, Mappings::MovingObjectType_BLOCK_Name, Mappings::MovingObjectType_BLOCK_Sig);
                if (m_typeBlockF) {
                    jobject blockEnum = env->GetStaticObjectField(mopTypeClass, m_typeBlockF);
                    m_typeBlockEnum = env->NewGlobalRef(blockEnum);
                    env->DeleteLocalRef(blockEnum);
                }
                env->DeleteLocalRef(mopTypeClass);
            }

            // Flawless Eagle checks
            m_theWorldF = env->GetFieldID(mcClass, Mappings::Minecraft_theWorld_Name, Mappings::Minecraft_theWorld_Sig);
            jclass worldClass = JniManager::FindClassWithLoader(env, Mappings::World_Class);
            if (worldClass) {
                m_isAirBlockM = env->GetMethodID(worldClass, Mappings::World_isAirBlock_Name, Mappings::World_isAirBlock_Sig);
                env->DeleteLocalRef(worldClass);
            }
            jclass blockPosClass = JniManager::FindClassWithLoader(env, Mappings::BlockPos_Class);
            if (blockPosClass) {
                m_blockPosClassGlobal = (jclass)env->NewGlobalRef(blockPosClass);
                m_blockPosInitM = env->GetMethodID(blockPosClass, "<init>", Mappings::BlockPos_Init_Sig);
                env->DeleteLocalRef(blockPosClass);
            }

            // SafeWalk / Settings / GUI
            m_gsF = env->GetFieldID(mcClass, Mappings::Minecraft_gameSettings_Name, Mappings::Minecraft_gameSettings_Sig);
            m_currentScreenF = env->GetFieldID(mcClass, Mappings::Minecraft_currentScreen_Name, Mappings::Minecraft_currentScreen_Sig);

            
            env->DeleteLocalRef(mcClass);
            if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
            m_cached = true;
        }

        float yaw = env->GetFloatField(playerObj, m_yawF);
        float pitch = env->GetFloatField(playerObj, m_pitchF);
        jboolean onGround = env->GetBooleanField(playerObj, m_onGroundF);

        // --- 1. AutoBlock ---
        if (m_inventoryF && m_currentItemF && m_getStackInSlotM && m_getItemM && m_itemBlockClass) {
            jobject inventory = env->GetObjectField(playerObj, m_inventoryF);
            if (inventory) {
                int currentSlot = env->GetIntField(inventory, m_currentItemF);
                bool hasBlock = false;
                
                // Check if current item is a block
                jobject currentStack = env->CallObjectMethod(inventory, m_getStackInSlotM, currentSlot);
                if (currentStack) {
                    jobject currentItem = env->CallObjectMethod(currentStack, m_getItemM);
                    if (currentItem && env->IsInstanceOf(currentItem, m_itemBlockClass)) hasBlock = true;
                    if (currentItem) env->DeleteLocalRef(currentItem);
                    env->DeleteLocalRef(currentStack);
                }

                // If not block, search hotbar (0-8)
                if (!hasBlock) {
                    for (int i = 0; i < 9; i++) {
                        jobject stack = env->CallObjectMethod(inventory, m_getStackInSlotM, i);
                        if (!stack) continue;
                        jobject item = env->CallObjectMethod(stack, m_getItemM);
                        bool isBlock = item && env->IsInstanceOf(item, m_itemBlockClass);
                        if (item) env->DeleteLocalRef(item);
                        env->DeleteLocalRef(stack);
                        if (isBlock) {
                            env->SetIntField(inventory, m_currentItemF, i);
                            break;
                        }
                    }
                }
                env->DeleteLocalRef(inventory);
            }
        }

        // Check if GUI is open (Inventory, Chat, Menu, etc)
        bool isGuiOpen = false;
        if (m_currentScreenF) {
            jobject screenObj = env->GetObjectField(mcObj, m_currentScreenF);
            if (screenObj) {
                isGuiOpen = true;
                env->DeleteLocalRef(screenObj);
            }
        }

        // --- 2. Auto Viewpoint Adjustment (Physical) & Auto Half-Shift ---
        // Only trigger when the player is HOLDING right mouse button AND no GUI is open.
        bool rightHeld = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 && !isGuiOpen;
        if (rightHeld) {
            // Physical Aim Assist (snap to GodBridge angle)
            float targetYaw = std::round(yaw / 45.0f) * 45.0f;
            float targetPitch = 79.5f;

            float diffYaw = targetYaw - yaw;
            while (diffYaw >  180.0f) diffYaw -= 360.0f;
            while (diffYaw < -180.0f) diffYaw += 360.0f;
            
            // Smoothly move the actual camera (0.3f speed)
            env->SetFloatField(playerObj, m_yawF, yaw + diffYaw * 0.3f);
            env->SetFloatField(playerObj, m_pitchF, pitch + (targetPitch - pitch) * 0.3f);

            // Clean up silent rotation state if it was active
            if (m_agentClass && m_setSilentActive) {
                env->CallStaticVoidMethod(m_agentClass, m_setSilentActive, JNI_FALSE);
            }

            // --- 3. Raycast & Emulate Click ---
            auto now = std::chrono::steady_clock::now();
            long long msSinceLastClick = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();
            int clickCooldownMs = 100 + (rand() % 41 - 20); // 80-120 ms = 8-12 CPS

            if (msSinceLastClick >= clickCooldownMs) {
                if (m_entityRendererF && m_rightClickM && m_getMouseOverM && m_objectMouseOverF && m_typeOfHitF && m_typeBlockEnum) {
                    jobject erObj = env->GetObjectField(mcObj, m_entityRendererF);
                    if (erObj) {
                        env->CallVoidMethod(erObj, m_getMouseOverM, 1.0f);
                        jobject mopObj = env->GetObjectField(mcObj, m_objectMouseOverF);
                        if (mopObj) {
                            jobject hitType = env->GetObjectField(mopObj, m_typeOfHitF);
                            if (hitType && env->IsSameObject(hitType, m_typeBlockEnum)) {
                                env->CallVoidMethod(mcObj, m_rightClickM);
                                m_lastClickTime = now;
                            }
                            if (hitType) env->DeleteLocalRef(hitType);
                            env->DeleteLocalRef(mopObj);
                        }
                        env->DeleteLocalRef(erObj);
                    }
                }
            }

            // --- 4. Flawless Eagle Logic ---
            bool overAir = true; // Assume air for safety
            if (m_theWorldF && m_isAirBlockM && m_blockPosClassGlobal && m_blockPosInitM) {
                jobject worldObj = env->GetObjectField(mcObj, m_theWorldF);
                if (worldObj) {
                    double pX = env->GetDoubleField(playerObj, m_posXF);
                    double pY = env->GetDoubleField(playerObj, m_posYF);
                    double pZ = env->GetDoubleField(playerObj, m_posZF);
                    
                    // Check the block exactly below the player's center
                    jobject blockPosObj = env->NewObject(m_blockPosClassGlobal, m_blockPosInitM, pX, pY - 1.0, pZ);
                    if (blockPosObj) {
                        overAir = env->CallBooleanMethod(worldObj, m_isAirBlockM, blockPosObj);
                        env->DeleteLocalRef(blockPosObj);
                    }
                    env->DeleteLocalRef(worldObj);
                }
            }
            
            // If the block directly below us is Air, we are off the edge -> Sneak!
            m_wasSneaking = overAir;

        } else {
            m_wasSneaking = false;
        }

        // --- 4. Tower ---
        bool spacePressed = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
        if (spacePressed && m_motionXF && m_motionZF && m_motionYF) {
            double mx = env->GetDoubleField(playerObj, m_motionXF);
            double mz = env->GetDoubleField(playerObj, m_motionZF);
            
            // If standing still horizontally and space is pressed, Tower up
            if (std::abs(mx) < 0.05 && std::abs(mz) < 0.05) {
                if (onGround) {
                    // Set jump motion manually
                    env->SetDoubleField(playerObj, m_motionYF, 0.41999998688697815);
                    // Also zero out horizontal motion to prevent drifting
                    env->SetDoubleField(playerObj, m_motionXF, 0.0);
                    env->SetDoubleField(playerObj, m_motionZF, 0.0);
                }
            }
        }

        // --- 5. Apply Half-Shift via GameSettings ---
        if (m_gsF) {

            jobject gsObj = env->GetObjectField(mcObj, m_gsF);
            if (gsObj) {
                if (!m_sneakFieldsCached) {
                    jclass gsClass = env->GetObjectClass(gsObj);
                    m_sneakKeyF = env->GetFieldID(gsClass, Mappings::GameSettings_keyBindSneak_Name, Mappings::GameSettings_keyBindSneak_Sig);
                    env->DeleteLocalRef(gsClass);
                    if (m_sneakKeyF) {
                        jobject sneakKeyObj = env->GetObjectField(gsObj, m_sneakKeyF);
                        if (sneakKeyObj) {
                            jclass keyBindClass = JniManager::FindClassWithLoader(env, Mappings::KeyBinding_Class);
                            if (keyBindClass) {
                                m_pressedF = env->GetFieldID(keyBindClass, Mappings::KeyBinding_pressed_Name, Mappings::KeyBinding_pressed_Sig);
                                env->DeleteLocalRef(keyBindClass);
                            }
                            env->DeleteLocalRef(sneakKeyObj);
                        }
                    }
                    m_sneakFieldsCached = true;
                }

                if (m_sneakKeyF && m_pressedF) {
                    jobject sneakKeyObj = env->GetObjectField(gsObj, m_sneakKeyF);
                    if (sneakKeyObj) {
                        if (m_wasSneaking) {
                            env->SetBooleanField(sneakKeyObj, m_pressedF, JNI_TRUE);
                        } else {
                            if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == 0) {
                                env->SetBooleanField(sneakKeyObj, m_pressedF, JNI_FALSE);
                            }
                        }
                        env->DeleteLocalRef(sneakKeyObj);
                    }
                }
                env->DeleteLocalRef(gsObj);
            }
        }
        
        if (env->ExceptionCheck()) env->ExceptionClear();
    }

    void onDisable() override {
        JNIEnv* env = JniManager::GetEnv();
        if (env && m_agentClass && m_setSilentActive) {
            env->CallStaticVoidMethod(m_agentClass, m_setSilentActive, JNI_FALSE);
            if (env->ExceptionCheck()) env->ExceptionClear();
        }
        m_wasSneaking = false;
        m_spoofYaw = 0.0f;
        m_spoofPitch = 0.0f;
    }

private:
    bool m_cached = false;
    bool m_sneakFieldsCached = false;

    // Player
    jfieldID m_yawF = nullptr;
    jfieldID m_pitchF = nullptr;
    jfieldID m_onGroundF = nullptr;
    jfieldID m_motionXF = nullptr;
    jfieldID m_motionYF = nullptr;
    jfieldID m_motionZF = nullptr;
    jfieldID m_posXF = nullptr;
    jfieldID m_posYF = nullptr;
    jfieldID m_posZF = nullptr;
    jmethodID m_jumpM = nullptr;

    // AutoBlock
    jfieldID m_inventoryF = nullptr;
    jfieldID m_currentItemF = nullptr;
    jmethodID m_getStackInSlotM = nullptr;
    jmethodID m_getItemM = nullptr;
    jclass m_itemBlockClass = nullptr;

    // Silent Rotation
    jclass m_agentClass = nullptr;
    jmethodID m_setSilentRotation = nullptr;
    jmethodID m_setSilentActive = nullptr;

    // Raycast / Click
    jfieldID m_rightClickDelayF = nullptr;
    jmethodID m_rightClickM = nullptr;
    jfieldID m_entityRendererF = nullptr;
    jmethodID m_getMouseOverM = nullptr;
    jfieldID m_objectMouseOverF = nullptr;
    jfieldID m_typeOfHitF = nullptr;
    jfieldID m_sideHitF = nullptr;
    jfieldID m_typeBlockF = nullptr;
    jobject m_typeBlockEnum = nullptr;
    jobject m_enumFacingUP = nullptr;

    // Eagle block checks
    jfieldID m_theWorldF = nullptr;
    jmethodID m_isAirBlockM = nullptr;
    jclass m_blockPosClassGlobal = nullptr;
    jmethodID m_blockPosInitM = nullptr;

    // GUI / Settings
    jfieldID m_gsF = nullptr;
    jfieldID m_sneakKeyF = nullptr;
    jfieldID m_pressedF = nullptr;
    jfieldID m_currentScreenF = nullptr;

    bool m_wasSneaking = false;
    float m_spoofYaw = 0.0f;
    float m_spoofPitch = 0.0f;
    bool m_spoofInitialized = false;
    std::chrono::steady_clock::time_point m_lastClickTime{};
};
