#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include "../mapping_resolver.hpp"
#include <Windows.h>
#include <cmath>
#include <chrono>
#include <random>

class Scaffold : public Module {
public:
    int   mode        = 0;    // 0=Normal, 1=Legit, 2=NoShift, 3=Tower
    float pitch       = 79.5f;
    float minCps      = 8.0f;
    float maxCps      = 12.0f;
    bool  tower       = true;
    bool  expand      = true;

    Scaffold() : Module("Scaffold") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;
        _cache(env, mcObj, playerClass);
        if (!m_cached) {
            return;
        }

        // Read current state
        float curYaw   = env->GetFloatField(playerObj, m_yawF); _ex(env);
        float curPitch = env->GetFloatField(playerObj, m_pitchF); _ex(env);
        jboolean onGround = env->GetBooleanField(playerObj, m_onGroundF); _ex(env);
        double px = env->GetDoubleField(playerObj, m_posXF); _ex(env);
        double py = env->GetDoubleField(playerObj, m_posYF); _ex(env);
        double pz = env->GetDoubleField(playerObj, m_posZF); _ex(env);

        bool didSneak = false;
        bool didPlace = false;

        // Don't run if GUI is open
        if (m_currentScreenF) {
            jobject screenObj = env->GetObjectField(mcObj, m_currentScreenF);
            _ex(env);
            if (screenObj) { 
                env->DeleteLocalRef(screenObj); 
                return; // Gui is open
            }
        }

        // Get world object
        jobject worldObj = nullptr;
        if (m_theWorldF) {
            worldObj = env->GetObjectField(mcObj, m_theWorldF);
            _ex(env);
        }

        // ─── Auto-select block slot ──────────────────────────────────────
        _autoBlock(env, playerObj);

        // ─── Edge sneak / SafeWalk ───────────────────────────────────────
        if (worldObj) {
            bool nearEdge = false;
            if (onGround) {
                nearEdge = _isNearEdge(env, worldObj, playerObj, px, py, pz);
            }
            didSneak = nearEdge;

            if (nearEdge) {
                m_sneakTicks = 3; 
            } else if (m_sneakTicks > 0) {
                m_sneakTicks--;
                nearEdge = true;
            }

            if (mode == 0 || mode == 1) { // Normal, Eagle
                _setSneak(env, mcObj, nearEdge);
            } else if (mode == 2) { // NoShift
                // True SafeWalk without visual sneak by stopping motion
                if (nearEdge && m_motionXF && m_motionZF) {
                    env->SetDoubleField(playerObj, m_motionXF, 0.0); _ex(env);
                    env->SetDoubleField(playerObj, m_motionZF, 0.0); _ex(env);
                }
            }
        }

        // ─── Block placement ─────────────────────────────────────────────
        if (worldObj) {
            PlaceTarget target;
            bool needsPlace = _findPlaceTarget(env, worldObj, px, py, pz, curYaw, target);
            if (needsPlace) {
                float targetYaw, targetPitch;
                _calcLookAt(px, py + 1.62, pz, target.hitX, target.hitY, target.hitZ, targetYaw, targetPitch);

                env->SetFloatField(playerObj, m_yawF, targetYaw); _ex(env);
                env->SetFloatField(playerObj, m_pitchF, targetPitch); _ex(env);

                // Try to place the block
                bool placed = _tryPlace(env, mcObj);

                if (placed) {
                    didPlace = true;

                    // AntiCheat bypass for NoShift mode: Spoof START_SNEAKING packet
                    if (mode == 2 && m_getNetHandlerM && m_addToSendQueueM && m_packetClass && m_packetActionClass && m_packetInit) {
                        jobject netHandler = env->CallObjectMethod(mcObj, m_getNetHandlerM); _ex(env);
                        if (netHandler) {
                            jobject actionObj = env->GetStaticObjectField(m_packetActionClass, m_actionStartSneaking); _ex(env);
                            if (actionObj) {
                                jobject packet = env->NewObject(m_packetClass, m_packetInit, playerObj, actionObj); _ex(env);
                                if (packet) {
                                    env->CallVoidMethod(netHandler, m_addToSendQueueM, packet); _ex(env);
                                    env->DeleteLocalRef(packet);
                                }
                                env->DeleteLocalRef(actionObj);
                            }
                            
                            // Send STOP_SNEAKING packet immediately after placing
                            actionObj = env->GetStaticObjectField(m_packetActionClass, m_actionStopSneaking); _ex(env);
                            if (actionObj) {
                                jobject packet = env->NewObject(m_packetClass, m_packetInit, playerObj, actionObj); _ex(env);
                                if (packet) {
                                    env->CallVoidMethod(netHandler, m_addToSendQueueM, packet); _ex(env);
                                    env->DeleteLocalRef(packet);
                                }
                                env->DeleteLocalRef(actionObj);
                            }
                            env->DeleteLocalRef(netHandler);
                        }
                    }

                    // For Eagle mode, un-sneak immediately after placing
                    if (mode == 1) {
                        _setSneak(env, mcObj, false);
                        m_sneakTicks = 0; // Reset coyote time
                    }
                }

                env->SetFloatField(playerObj, m_yawF, curYaw); _ex(env);
                env->SetFloatField(playerObj, m_pitchF, curPitch); _ex(env);
            }

            // ─── Tower ───────────────────────────────────────────────────
            bool spaceHeld = tower && (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
            if (spaceHeld && m_motionYF && m_motionXF && m_motionZF) {
                env->SetDoubleField(playerObj, m_motionXF, 0.0); _ex(env);
                env->SetDoubleField(playerObj, m_motionZF, 0.0); _ex(env);
                if (onGround) {
                    env->SetDoubleField(playerObj, m_motionYF, 0.42); _ex(env);
                }
            }

            env->DeleteLocalRef(worldObj);
        }

        static auto lastLog = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - lastLog).count() > 1000) {
            lastLog = std::chrono::steady_clock::now();
        }

        _ex(env);
    }

    void onDisable() override {
        JNIEnv* env = JniManager::GetEnv();
        if (!env) return;
        // Release sneak
        if (m_gsF && m_sneakKeyF && m_pressedF) {
            jclass mcClass = JniManager::FindClassWithLoader(env, Mappings::Minecraft_Class);
            if (mcClass) {
                jmethodID getMc = env->GetStaticMethodID(mcClass, Mappings::Minecraft_getMinecraft_Name, Mappings::Minecraft_getMinecraft_Sig);
                _ex(env);
                if (getMc) {
                    jobject mcObj = env->CallStaticObjectMethod(mcClass, getMc);
                    _ex(env);
                    if (mcObj) {
                        _setSneak(env, mcObj, false);
                        env->DeleteLocalRef(mcObj);
                    }
                }
                env->DeleteLocalRef(mcClass);
            }
        }
        _ex(env);
    }

private:
    bool m_cached = false;

    jfieldID m_yawF = nullptr, m_pitchF = nullptr, m_onGroundF = nullptr;
    jfieldID m_motionXF = nullptr, m_motionYF = nullptr, m_motionZF = nullptr;
    jfieldID m_posXF = nullptr, m_posYF = nullptr, m_posZF = nullptr;

    jfieldID m_inventoryF = nullptr, m_currentItemF = nullptr;
    jmethodID m_getStackInSlotM = nullptr, m_getItemM = nullptr;
    jclass m_itemBlockClass = nullptr;

    jmethodID m_rightClickM = nullptr;
    jfieldID m_entityRendererF = nullptr;
    jmethodID m_getMouseOverM = nullptr;
    jfieldID m_objectMouseOverF = nullptr;
    jfieldID m_typeOfHitF = nullptr;
    jobject m_typeBlockEnum = nullptr;

    jfieldID m_theWorldF = nullptr;
    jmethodID m_isAirBlockM = nullptr;
    jclass m_blockPosClassGlobal = nullptr;
    jmethodID m_blockPosInitM = nullptr;

    jfieldID m_gsF = nullptr, m_currentScreenF = nullptr;
    jfieldID m_sneakKeyF = nullptr, m_pressedF = nullptr;
    jfieldID m_rightClickDelayTimerF = nullptr;

    jmethodID m_getNetHandlerM = nullptr;
    jmethodID m_addToSendQueueM = nullptr;
    jclass m_packetActionClass = nullptr;
    jfieldID m_actionStartSneaking = nullptr;
    jfieldID m_actionStopSneaking = nullptr;
    jclass m_packetClass = nullptr;
    jmethodID m_packetInit = nullptr;

    int m_sneakTicks = 0;

    std::chrono::steady_clock::time_point m_lastClickTime{};

    struct PlaceTarget {
        double hitX, hitY, hitZ;
        int blockX, blockY, blockZ;
    };

    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }

    // ─── Cache with proper exception clearing between EVERY JNI call ─────
    void _cache(JNIEnv* env, jobject mcObj, jclass playerClass) {
        if (m_cached) return;

        jclass mcClass = env->GetObjectClass(mcObj);
        _ex(env);
        if (!mcClass) return;

        // Entity fields
        m_yawF      = MC::fieldID(env, playerClass, "Entity.rotationYaw");
        m_pitchF    = MC::fieldID(env, playerClass, "Entity.rotationPitch");
        m_onGroundF = MC::fieldID(env, playerClass, "Entity.onGround");
        m_motionXF  = MC::fieldID(env, playerClass, "Entity.motionX");
        m_motionYF  = MC::fieldID(env, playerClass, "Entity.motionY");
        m_motionZF  = MC::fieldID(env, playerClass, "Entity.motionZ");
        m_posXF     = MC::fieldID(env, playerClass, "Entity.posX");
        m_posYF     = MC::fieldID(env, playerClass, "Entity.posY");
        m_posZF     = MC::fieldID(env, playerClass, "Entity.posZ");

        if (!m_yawF || !m_pitchF || !m_onGroundF || !m_posXF || !m_posYF || !m_posZF ||
            !m_motionXF || !m_motionYF || !m_motionZF) {
            return; // Don't set m_cached, will retry next tick
        }

        // Inventory
        m_inventoryF = MC::fieldID(env, playerClass, "EntityPlayer.inventory");
        jclass invClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/player/InventoryPlayer"); _ex(env);
        if (invClass) {
            m_currentItemF = MC::fieldID(env, invClass, "InventoryPlayer.currentItem");
            m_getStackInSlotM = env->GetMethodID(invClass, Mappings::InventoryPlayer_getStackInSlot_Name, Mappings::InventoryPlayer_getStackInSlot_Sig); _ex(env);
            env->DeleteLocalRef(invClass);
        }
        jclass itemStackClass = JniManager::FindClassWithLoader(env, Mappings::ItemStack_Class); _ex(env);
        if (itemStackClass) {
            m_getItemM = env->GetMethodID(itemStackClass, Mappings::ItemStack_getItem_Name, Mappings::ItemStack_getItem_Sig); _ex(env);
            env->DeleteLocalRef(itemStackClass);
        }
        jclass itemBlockCls = JniManager::FindClassWithLoader(env, Mappings::ItemBlock_Class); _ex(env);
        if (itemBlockCls) {
            m_itemBlockClass = (jclass)env->NewGlobalRef(itemBlockCls); _ex(env);
            env->DeleteLocalRef(itemBlockCls);
        }

        // Minecraft methods/fields
        m_rightClickM       = MC::methodID(env, mcClass, "Minecraft.rightClickMouse");
        m_entityRendererF   = MC::fieldID(env, mcClass, "Minecraft.entityRenderer");
        if (m_entityRendererF) {
            jobject erObj = env->GetObjectField(mcObj, m_entityRendererF); _ex(env);
            if (erObj) {
                jclass erClass = env->GetObjectClass(erObj); _ex(env);
                if (erClass) {
                    m_getMouseOverM = env->GetMethodID(erClass, Mappings::EntityRenderer_getMouseOver_Name, Mappings::EntityRenderer_getMouseOver_Sig); _ex(env);
                    env->DeleteLocalRef(erClass);
                }
                env->DeleteLocalRef(erObj);
            }
        }
        m_objectMouseOverF = MC::fieldID(env, mcClass, "Minecraft.objectMouseOver");

        // MovingObjectPosition
        jclass mopClass = JniManager::FindClassWithLoader(env, "net/minecraft/util/MovingObjectPosition"); _ex(env);
        if (mopClass) {
            m_typeOfHitF = MC::fieldID(env, mopClass, "MovingObjectPosition.typeOfHit");
            env->DeleteLocalRef(mopClass);
        }
        jclass mopTypeClass = JniManager::FindClassWithLoader(env, "net/minecraft/util/MovingObjectPosition$MovingObjectType"); _ex(env);
        if (!mopTypeClass) { env->ExceptionClear(); mopTypeClass = env->FindClass("auh$a"); }
        if (mopTypeClass) {
            jfieldID typeBlockF = env->GetStaticFieldID(mopTypeClass, "BLOCK", "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;"); _ex(env);
            if (!typeBlockF) { env->ExceptionClear(); typeBlockF = env->GetStaticFieldID(mopTypeClass, "b", "Lauh$a;"); }
            if (typeBlockF) {
                jobject blockEnum = env->GetStaticObjectField(mopTypeClass, typeBlockF); _ex(env);
                if (blockEnum) {
                    m_typeBlockEnum = env->NewGlobalRef(blockEnum); _ex(env);
                    env->DeleteLocalRef(blockEnum);
                }
            }
            env->DeleteLocalRef(mopTypeClass);
        }

        // World
        m_theWorldF = MC::fieldID(env, mcClass, "Minecraft.theWorld");
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
            m_blockPosInitM = env->GetMethodID(blockPosClass, "<init>", Mappings::BlockPos_Init_Sig); _ex(env);
            env->DeleteLocalRef(blockPosClass);
        }

        // GameSettings / sneak key
        m_gsF            = MC::fieldID(env, mcClass, "Minecraft.gameSettings");
        m_currentScreenF = MC::fieldID(env, mcClass, "Minecraft.currentScreen");
        m_rightClickDelayTimerF = MC::fieldID(env, mcClass, "Minecraft.rightClickDelayTimer");

        if (m_gsF) {
            jobject gsObj = env->GetObjectField(mcObj, m_gsF); _ex(env);
            if (gsObj) {
                jclass gsClass = env->GetObjectClass(gsObj); _ex(env);
                if (gsClass) {
                    m_sneakKeyF = MC::fieldID(env, gsClass, "GameSettings.keyBindSneak");
                    env->DeleteLocalRef(gsClass);
                }
                env->DeleteLocalRef(gsObj);
            }
        }
        jclass keyBindClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/settings/KeyBinding"); _ex(env);
        if (!keyBindClass) { env->ExceptionClear(); keyBindClass = env->FindClass("avt"); }
        if (keyBindClass) {
            m_pressedF = MC::fieldID(env, keyBindClass, "KeyBinding.pressed");
            env->DeleteLocalRef(keyBindClass);
        }

        // Networking
        m_getNetHandlerM = MC::methodID(env, mcClass, "Minecraft.getNetHandler");
        jclass nhClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/network/NetHandlerPlayClient"); _ex(env);
        if (!nhClass) { env->ExceptionClear(); nhClass = env->FindClass("bcy"); }
        if (nhClass) {
            m_addToSendQueueM = env->GetMethodID(nhClass, "addToSendQueue", "(Lnet/minecraft/network/Packet;)V");
            if (!m_addToSendQueueM) {
                env->ExceptionClear();
                m_addToSendQueueM = env->GetMethodID(nhClass, "a", "(Lff;)V");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(nhClass);
        }

        jclass pktClass = JniManager::FindClassWithLoader(env, "net/minecraft/network/play/client/C0BPacketEntityAction"); _ex(env);
        if (!pktClass) { env->ExceptionClear(); pktClass = env->FindClass("ip"); }
        if (pktClass) {
            m_packetClass = (jclass)env->NewGlobalRef(pktClass); _ex(env);
            m_packetInit = env->GetMethodID(pktClass, "<init>", "(Lnet/minecraft/entity/Entity;Lnet/minecraft/network/play/client/C0BPacketEntityAction$Action;)V");
            if (!m_packetInit) {
                env->ExceptionClear();
                m_packetInit = env->GetMethodID(pktClass, "<init>", "(Lpk;Lip$a;)V");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(pktClass);
        }

        jclass actionCls = JniManager::FindClassWithLoader(env, "net/minecraft/network/play/client/C0BPacketEntityAction$Action"); _ex(env);
        if (!actionCls) { env->ExceptionClear(); actionCls = env->FindClass("ip$a"); }
        if (actionCls) {
            m_packetActionClass = (jclass)env->NewGlobalRef(actionCls); _ex(env);
            m_actionStartSneaking = env->GetStaticFieldID(actionCls, "START_SNEAKING", "Lnet/minecraft/network/play/client/C0BPacketEntityAction$Action;");
            if (!m_actionStartSneaking) {
                env->ExceptionClear();
                m_actionStartSneaking = env->GetStaticFieldID(actionCls, "START_SNEAKING", "Lip$a;");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            m_actionStopSneaking = env->GetStaticFieldID(actionCls, "STOP_SNEAKING", "Lnet/minecraft/network/play/client/C0BPacketEntityAction$Action;");
            if (!m_actionStopSneaking) {
                env->ExceptionClear();
                m_actionStopSneaking = env->GetStaticFieldID(actionCls, "STOP_SNEAKING", "Lip$a;");
                if (env->ExceptionCheck()) env->ExceptionClear();
            }
            env->DeleteLocalRef(actionCls);
        }

        env->DeleteLocalRef(mcClass);
        _ex(env);

        m_cached = true;
    }

    // ─── Air block check ─────────────────────────────────────────────────
    bool _isAirBlock(JNIEnv* env, jobject worldObj, int x, int y, int z) {
        if (!m_blockPosClassGlobal || !m_blockPosInitM || !m_isAirBlockM) return false;
        _ex(env);
        jobject bp = env->NewObject(m_blockPosClassGlobal, m_blockPosInitM, (double)x, (double)y, (double)z);
        _ex(env);
        if (!bp) return false;
        jboolean air = env->CallBooleanMethod(worldObj, m_isAirBlockM, bp);
        _ex(env);
        env->DeleteLocalRef(bp);
        return air == JNI_TRUE;
    }

    // ─── Edge detection ──────────────────────────────────────────────────
    bool _isNearEdge(JNIEnv* env, jobject worldObj, jobject playerObj, double px, double py, double pz) {
        if (!m_isAirBlockM || !m_blockPosClassGlobal) return false;
        int ty = (int)std::floor(py) - 1;

        double mx = 0.0, mz = 0.0;
        if (m_motionXF && m_motionZF) {
            mx = env->GetDoubleField(playerObj, m_motionXF);
            mz = env->GetDoubleField(playerObj, m_motionZF);
        }
        if (env->ExceptionCheck()) env->ExceptionClear();

        double predX = px + mx * 3.0;
        double predZ = pz + mz * 3.0;
        
        double fracX = px - std::floor(px);
        double fracZ = pz - std::floor(pz);
        
        bool edgeX = (fracX < 0.25 && _isAirBlock(env, worldObj, std::floor(px)-1, ty, std::floor(pz))) || 
                     (fracX > 0.75 && _isAirBlock(env, worldObj, std::floor(px)+1, ty, std::floor(pz)));
        bool edgeZ = (fracZ < 0.25 && _isAirBlock(env, worldObj, std::floor(px), ty, std::floor(pz)-1)) || 
                     (fracZ > 0.75 && _isAirBlock(env, worldObj, std::floor(px), ty, std::floor(pz)+1));

        bool movingToAir = _isAirBlock(env, worldObj, (int)std::floor(predX), ty, (int)std::floor(predZ));
        return (edgeX || edgeZ) && movingToAir;
    }

    // ─── Sneak control ───────────────────────────────────────────────────
    void _setSneak(JNIEnv* env, jobject mcObj, bool sneak) {
        if (!m_gsF || !m_sneakKeyF || !m_pressedF) return;

        // If user is holding shift manually, don't interfere
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) return;

        jobject gsObj = env->GetObjectField(mcObj, m_gsF); _ex(env);
        if (!gsObj) return;
        jobject sneakKeyObj = env->GetObjectField(gsObj, m_sneakKeyF); _ex(env);
        if (sneakKeyObj) {
            env->SetBooleanField(sneakKeyObj, m_pressedF, sneak ? JNI_TRUE : JNI_FALSE); _ex(env);
            env->DeleteLocalRef(sneakKeyObj);
        }
        env->DeleteLocalRef(gsObj);
    }

    // ─── Auto-select block from hotbar ───────────────────────────────────
    void _autoBlock(JNIEnv* env, jobject playerObj) {
        if (!m_inventoryF || !m_currentItemF || !m_getStackInSlotM || !m_getItemM || !m_itemBlockClass) return;

        jobject inventory = env->GetObjectField(playerObj, m_inventoryF); _ex(env);
        if (!inventory) return;

        int currentSlot = env->GetIntField(inventory, m_currentItemF); _ex(env);

        // Check if current slot already has a block
        jobject currentStack = env->CallObjectMethod(inventory, m_getStackInSlotM, currentSlot); _ex(env);
        if (currentStack) {
            jobject currentItem = env->CallObjectMethod(currentStack, m_getItemM); _ex(env);
            bool hasBlock = currentItem && env->IsInstanceOf(currentItem, m_itemBlockClass);
            _ex(env);
            if (currentItem) env->DeleteLocalRef(currentItem);
            env->DeleteLocalRef(currentStack);
            if (hasBlock) { env->DeleteLocalRef(inventory); return; }
        }

        // Find first block in hotbar
        for (int i = 0; i < 9; i++) {
            jobject stack = env->CallObjectMethod(inventory, m_getStackInSlotM, i); _ex(env);
            if (!stack) continue;
            jobject item = env->CallObjectMethod(stack, m_getItemM); _ex(env);
            bool isBlock = item && env->IsInstanceOf(item, m_itemBlockClass);
            _ex(env);
            if (item) env->DeleteLocalRef(item);
            env->DeleteLocalRef(stack);
            if (isBlock) {
                env->SetIntField(inventory, m_currentItemF, i); _ex(env);
                break;
            }
        }
        env->DeleteLocalRef(inventory);
    }

    // ─── Find where to place a block ─────────────────────────────────────
    bool _findPlaceTarget(JNIEnv* env, jobject worldObj, double px, double py, double pz,
                          float curYaw, PlaceTarget& out) {
        int playerBlockX = (int)std::floor(px);
        int playerBlockY = (int)std::floor(py) - 1;
        int playerBlockZ = (int)std::floor(pz);

        // Check if we even need to place (block below is air)
        bool belowIsAir = _isAirBlock(env, worldObj, playerBlockX, playerBlockY, playerBlockZ);
        
        // Also check forward direction
        int fwdX = playerBlockX, fwdZ = playerBlockZ;
        bool fwdIsAir = false;
        if (expand) {
            float yawRad = curYaw * (3.14159265358979323846f / 180.0f);
            int fdx = (int)std::round(-std::sin(yawRad));
            int fdz = (int)std::round(std::cos(yawRad));
            if (fdx != 0 || fdz != 0) {
                fwdX = playerBlockX + fdx;
                fwdZ = playerBlockZ + fdz;
                fwdIsAir = _isAirBlock(env, worldObj, fwdX, playerBlockY, fwdZ);
            }
        }

        if (!belowIsAir && !fwdIsAir) return false;

        // Try to place at the first available target
        int targetX = belowIsAir ? playerBlockX : fwdX;
        int targetZ = belowIsAir ? playerBlockZ : fwdZ;

        // Search for an adjacent solid block to place against
        struct FaceOff { int dx, dy, dz; };
        static const FaceOff faces[] = {
            {0, -1, 0},  // Below
            {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, // Sides
            {0, 1, 0},  // Above
        };

        for (auto& f : faces) {
            int sx = targetX + f.dx, sy = playerBlockY + f.dy, sz = targetZ + f.dz;
            if (!_isAirBlock(env, worldObj, sx, sy, sz)) {
                out.blockX = sx; out.blockY = sy; out.blockZ = sz;
                out.hitX = sx + 0.5 + (-f.dx) * 0.5;
                out.hitY = sy + 0.5 + (-f.dy) * 0.5;
                out.hitZ = sz + 0.5 + (-f.dz) * 0.5;
                return true;
            }
        }

        // Extended search: diagonal blocks
        static const FaceOff extFaces[] = {
            {0, -1, -1}, {0, -1, 1}, {-1, -1, 0}, {1, -1, 0},
            {-1, 0, -1}, {1, 0, -1}, {-1, 0, 1}, {1, 0, 1},
            {0, -2, 0},
        };
        for (auto& f : extFaces) {
            int sx = targetX + f.dx, sy = playerBlockY + f.dy, sz = targetZ + f.dz;
            if (!_isAirBlock(env, worldObj, sx, sy, sz)) {
                // Find the connecting face through an intermediary
                if (f.dy == -1 && (f.dx != 0 || f.dz != 0)) {
                    // Check the block directly below
                    if (!_isAirBlock(env, worldObj, targetX, playerBlockY - 1, targetZ)) {
                        out.blockX = targetX; out.blockY = playerBlockY - 1; out.blockZ = targetZ;
                        out.hitX = targetX + 0.5; out.hitY = playerBlockY; out.hitZ = targetZ + 0.5;
                        return true;
                    }
                    // Check the side block
                    if (f.dx != 0 && !_isAirBlock(env, worldObj, targetX + f.dx, playerBlockY, targetZ)) {
                        out.blockX = targetX + f.dx; out.blockY = playerBlockY; out.blockZ = targetZ;
                        out.hitX = (targetX + f.dx) + 0.5 + (-f.dx) * 0.5; out.hitY = playerBlockY + 0.5; out.hitZ = targetZ + 0.5;
                        return true;
                    }
                    if (f.dz != 0 && !_isAirBlock(env, worldObj, targetX, playerBlockY, targetZ + f.dz)) {
                        out.blockX = targetX; out.blockY = playerBlockY; out.blockZ = targetZ + f.dz;
                        out.hitX = targetX + 0.5; out.hitY = playerBlockY + 0.5; out.hitZ = (targetZ + f.dz) + 0.5 + (-f.dz) * 0.5;
                        return true;
                    }
                }
                if (f.dy == -2 && f.dx == 0 && f.dz == 0) {
                    if (!_isAirBlock(env, worldObj, targetX, playerBlockY - 1, targetZ)) {
                        out.blockX = targetX; out.blockY = playerBlockY - 1; out.blockZ = targetZ;
                        out.hitX = targetX + 0.5; out.hitY = playerBlockY; out.hitZ = targetZ + 0.5;
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // ─── Look-at calculation ─────────────────────────────────────────────
    static void _calcLookAt(double fromX, double fromY, double fromZ,
                            double toX, double toY, double toZ,
                            float& outYaw, float& outPitch) {
        double dx = toX - fromX;
        double dy = toY - fromY;
        double dz = toZ - fromZ;
        double hDist = std::sqrt(dx * dx + dz * dz);
        outYaw = (float)(std::atan2(dx, dz) * (-180.0 / 3.14159265358979323846));
        outPitch = (float)(-std::atan2(dy, hDist) * (180.0 / 3.14159265358979323846));
    }

    // ─── Place block (with CPS throttle) ─────────────────────────────────
    bool _tryPlace(JNIEnv* env, jobject mcObj) {
        if (!m_rightClickM || !m_entityRendererF || !m_getMouseOverM ||
            !m_objectMouseOverF || !m_typeOfHitF || !m_typeBlockEnum) return false;

        auto now = std::chrono::steady_clock::now();
        long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();
        int range = (int)maxCps - (int)minCps;
        int cps = (int)minCps + (range > 0 ? (rand() % (range + 1)) : 0);
        if (cps < 1) cps = 1;
        if (ms < (1000 / cps)) return false;

        // Update raycast with our spoofed rotation
        jobject erObj = env->GetObjectField(mcObj, m_entityRendererF); _ex(env);
        if (!erObj) return false;
        env->CallVoidMethod(erObj, m_getMouseOverM, 1.0f); _ex(env);
        env->DeleteLocalRef(erObj);

        // Check if we're looking at a block
        jobject mopObj = env->GetObjectField(mcObj, m_objectMouseOverF); _ex(env);
        if (!mopObj) return false;
        jobject hitType = env->GetObjectField(mopObj, m_typeOfHitF); _ex(env);
        bool hitBlock = hitType && env->IsSameObject(hitType, m_typeBlockEnum);
        _ex(env);
        if (hitType) env->DeleteLocalRef(hitType);
        env->DeleteLocalRef(mopObj);

        if (!hitBlock) return false;

        // Reset click delay and place
        if (m_rightClickDelayTimerF) {
            env->SetIntField(mcObj, m_rightClickDelayTimerF, 0); _ex(env);
        }

        env->CallVoidMethod(mcObj, m_rightClickM); _ex(env);
        m_lastClickTime = now;
        return true;
    }
};
