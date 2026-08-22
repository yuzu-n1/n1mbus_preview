#pragma once
#include "Module.hpp"
#include "../minecraft_mappings.hpp"
#include "../jni_manager.hpp"
#include "../mapping_resolver.hpp"
#include <Windows.h>
#include <cmath>
#include <chrono>
#include <vector>
#include <imgui.h>

extern float g_ModelView[16];
extern float g_Projection[16];
extern GLint g_Viewport[4];
extern double g_ViewerX, g_ViewerY, g_ViewerZ;
extern bool g_MatricesValid;

class BedBreaker : public Module {
public:
    float radius = 25.0f;

    BedBreaker() : Module("BedBreaker") {}

    void onUpdate(JNIEnv* env, jobject mcObj, jobject playerObj, jclass playerClass) override {
        if (!env || !mcObj || !playerObj || !playerClass) return;
        _cache(env, mcObj, playerClass);
        if (!m_cached) return;

        // Reset targeting state
        m_isTargetingBed = false;

        jobject worldObj = nullptr;
        if (m_theWorldF) {
            worldObj = env->GetObjectField(mcObj, m_theWorldF);
            _ex(env);
        }
        if (!worldObj) return;

        // Get player position
        double px = env->GetDoubleField(playerObj, m_posXF); _ex(env);
        double py = env->GetDoubleField(playerObj, m_posYF); _ex(env);
        double pz = env->GetDoubleField(playerObj, m_posZF); _ex(env);

        // Get reach distance
        float reach = 4.5f; // fallback
        if (m_playerControllerF) {
            jobject pcObj = env->GetObjectField(mcObj, m_playerControllerF); _ex(env);
            if (pcObj && m_getBlockReachDistanceM) {
                reach = env->CallFloatMethod(pcObj, m_getBlockReachDistanceM); _ex(env);
            }
            if (pcObj) env->DeleteLocalRef(pcObj);
        }

        m_foundBeds.clear();

        // Scan for beds
        int r = (int)std::ceil(reach);
        for (int x = -r; x <= r; x++) {
            for (int y = -r; y <= r; y++) {
                for (int z = -r; z <= r; z++) {
                    int bx = (int)std::floor(px) + x;
                    int by = (int)std::floor(py) + y;
                    int bz = (int)std::floor(pz) + z;

                    // Create BlockPos
                    jobject posObj = env->NewObject(m_blockPosClass, m_blockPosInitM, (double)bx, (double)by, (double)bz); _ex(env);
                    if (!posObj) continue;

                    // Get BlockState
                    jobject stateObj = env->CallObjectMethod(worldObj, m_getBlockStateM, posObj); _ex(env);
                    if (stateObj) {
                        jobject blockObj = env->CallObjectMethod(stateObj, m_getBlockM); _ex(env);
                        if (blockObj) {
                            jint blockId = env->CallStaticIntMethod(m_blockClass, m_getIdFromBlockM, blockObj); _ex(env);
                            if (blockId == 26) { // Bed ID is 26
                                m_foundBeds.push_back({bx, by, bz});
                            }
                            env->DeleteLocalRef(blockObj);
                        }
                        env->DeleteLocalRef(stateObj);
                    }
                    env->DeleteLocalRef(posObj);
                }
            }
        }
        env->DeleteLocalRef(worldObj);
        
        // Find center of each bed (group by pairs)
        m_bedCenters.clear();
        std::vector<BedCenter> centers;
        std::vector<bool> visited(m_foundBeds.size(), false);
        
        for (size_t i = 0; i < m_foundBeds.size(); i++) {
            if (visited[i]) continue;
            visited[i] = true;
            
            int bx = m_foundBeds[i].x;
            int by = m_foundBeds[i].y;
            int bz = m_foundBeds[i].z;
            
            bool foundPair = false;
            for (size_t j = i + 1; j < m_foundBeds.size(); j++) {
                if (visited[j]) continue;
                int ox = m_foundBeds[j].x;
                int oy = m_foundBeds[j].y;
                int oz = m_foundBeds[j].z;
                
                // Adjacent block check
                if (oy == by && ((abs(ox - bx) == 1 && oz == bz) || (abs(oz - bz) == 1 && ox == bx))) {
                    visited[j] = true;
                    foundPair = true;
                    BedCenter c;
                    c.cx = (bx + ox + 1.0) / 2.0;
                    c.cy = by + 0.28125; // Bed height is 9/16 = 0.5625. Center is half of that.
                    c.cz = (bz + oz + 1.0) / 2.0;
                    c.blockX = bx; // Store one of the blocks for breaking
                    c.blockY = by;
                    c.blockZ = bz;
                    centers.push_back(c);
                    break;
                }
            }
            // If pair not found (maybe unloaded or partially broken), just use single block center
            if (!foundPair) {
                BedCenter c;
                c.cx = bx + 0.5;
                c.cy = by + 0.28125;
                c.cz = bz + 0.5;
                c.blockX = bx;
                c.blockY = by;
                c.blockZ = bz;
                centers.push_back(c);
            }
        }
        
        m_bedCenters = centers;
        
        // Execute breaking if targeting
        if (m_shouldOverrideMop && m_targetBedCenter.blockY != -1) {
            // Check if attack key is pressed
            bool attackPressed = false;
            if (m_gsF && m_keyBindAttackF && m_pressedF) {
                jobject gsObj = env->GetObjectField(mcObj, m_gsF); _ex(env);
                if (gsObj) {
                    jobject keyObj = env->GetObjectField(gsObj, m_keyBindAttackF); _ex(env);
                    if (keyObj) {
                        attackPressed = env->GetBooleanField(keyObj, m_pressedF); _ex(env);
                        env->DeleteLocalRef(keyObj);
                    }
                    env->DeleteLocalRef(gsObj);
                }
            }
            
            if (attackPressed && m_playerControllerF && m_enumFacingUp) {
                jobject pcObj = env->GetObjectField(mcObj, m_playerControllerF); _ex(env);
                if (pcObj) {
                    // Update progress before breaking
                    if (m_curBlockDamageMPF) {
                        m_curBreakProgress = env->GetFloatField(pcObj, m_curBlockDamageMPF); _ex(env);
                    }

                    if (m_blockPosF && m_blockPosClass) {
                        jobject targetPosObj = env->NewObject(m_blockPosClass, m_blockPosInitM, 
                                                              (double)m_targetBedCenter.blockX, 
                                                              (double)m_targetBedCenter.blockY, 
                                                              (double)m_targetBedCenter.blockZ); _ex(env);
                        if (targetPosObj) {
                            // Re-enable delay EVERY FRAME so vanilla doesn't run clickBlock on front block
                            if (m_blockHitDelayF) {
                                env->SetIntField(pcObj, m_blockHitDelayF, 5); _ex(env);
                            }

                            static ULONGLONG lastHitTime = 0;
                            ULONGLONG currentTime = GetTickCount64();
                            if (currentTime - lastHitTime >= 50) {
                                lastHitTime = currentTime;

                                // Suppress vanilla breaking by tricking blockHitDelay temporarily
                                if (m_blockHitDelayF) {
                                    env->SetIntField(pcObj, m_blockHitDelayF, 0); _ex(env);
                                }

                                if (m_onPlayerDamageBlockM) {
                                    env->CallBooleanMethod(pcObj, m_onPlayerDamageBlockM, targetPosObj, m_enumFacingUp); _ex(env);
                                }
                                
                                // Re-enable delay immediately
                                if (m_blockHitDelayF) {
                                    env->SetIntField(pcObj, m_blockHitDelayF, 5); _ex(env);
                                }
                                
                                if (m_swingItemM) {
                                    env->CallVoidMethod(playerObj, m_swingItemM); _ex(env);
                                }
                            }
                            env->DeleteLocalRef(targetPosObj);
                        }
                    }
                    env->DeleteLocalRef(pcObj);
                }
            }
        } else {
            // Update break progress for rendering if not actively attacking
            if (m_playerControllerF) {
                jobject pcObj = env->GetObjectField(mcObj, m_playerControllerF); _ex(env);
                if (pcObj && m_curBlockDamageMPF) {
                    m_curBreakProgress = env->GetFloatField(pcObj, m_curBlockDamageMPF); _ex(env);
                }
                if (pcObj) env->DeleteLocalRef(pcObj);
            }
        }
    }



    void onRender2D(JNIEnv* env, jobject mcObj, int displayWidth, int displayHeight, float partialTicks) override {
        if (!m_cached || !g_MatricesValid) return;
        
        // Get ImGui draw list
        ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
        
        float screenCX = displayWidth / 2.0f;
        float screenCY = displayHeight / 2.0f;
        
        float closestDistSq = 9999999.0f;
        BedCenter bestBed;
        bestBed.blockY = -1;
        
        m_shouldOverrideMop = false;
        
        for (const auto& bed : m_bedCenters) {
            // Project 3D to 2D
            float px, py, pz;
            // Subtract viewer position to get coordinates relative to camera
            double relX = bed.cx - g_ViewerX;
            double relY = bed.cy - g_ViewerY;
            double relZ = bed.cz - g_ViewerZ;
            
            if (!project((float)relX, (float)relY, (float)relZ, g_ModelView, g_Projection, g_Viewport, &px, &py, &pz)) {
                continue; // Behind camera
            }
            
            // Adjust to GUI coordinates (OpenGL origin is bottom-left, ImGui is top-left)
            // Let's use ImGui window size directly for scaling
            ImVec2 wndSize = ImGui::GetIO().DisplaySize;
            float guiX = (px / g_Viewport[2]) * wndSize.x;
            float guiY = (1.0f - (py / g_Viewport[3])) * wndSize.y;
            
            // Calculate distance to crosshair
            float cx = wndSize.x / 2.0f;
            float cy = wndSize.y / 2.0f;
            float dx = guiX - cx;
            float dy = guiY - cy;
            float distSq = dx*dx + dy*dy;
            
            bool hovered = distSq <= (radius * radius);
            if (hovered && distSq < closestDistSq) {
                closestDistSq = distSq;
                bestBed = bed;
                m_shouldOverrideMop = true;
            }
            
            // Draw
            ImU32 innerColor = hovered ? IM_COL32(0, 255, 0, 180) : IM_COL32(255, 0, 0, 180);
            ImU32 outerBgColor = IM_COL32(128, 128, 128, 128);
            ImU32 outerFgColor = IM_COL32(0, 255, 0, 255);
            
            // Inner circle
            draw_list->AddCircleFilled(ImVec2(guiX, guiY), radius * 0.8f, innerColor);
            
            // Outer empty ring
            draw_list->AddCircle(ImVec2(guiX, guiY), radius, outerBgColor, 32, 2.0f);
            
            // Progress ring
            if (hovered && m_curBreakProgress > 0.0f) {
                float progress = (std::min)(m_curBreakProgress, 1.0f);
                float pi = 3.14159265358979323846f;
                draw_list->PathClear();
                draw_list->PathArcTo(ImVec2(guiX, guiY), radius, -pi/2.0f, -pi/2.0f + (progress * 2.0f * pi), 32);
                draw_list->PathStroke(outerFgColor, false, 3.0f);
            }
        }
        
        m_targetBedCenter = bestBed;
    }

private:
    struct BedBlock {
        int x, y, z;
    };
    
    struct BedCenter {
        double cx, cy, cz;
        int blockX, blockY, blockZ;
    };

    bool m_cached = false;
    std::vector<BedBlock> m_foundBeds;
    std::vector<BedCenter> m_bedCenters;
    
    bool m_isTargetingBed = false;
    BedCenter m_targetBedCenter;
    bool m_shouldOverrideMop = false;
    float m_curBreakProgress = 0.0f;

    jfieldID m_posXF = nullptr, m_posYF = nullptr, m_posZF = nullptr;
    jfieldID m_theWorldF = nullptr;
    jfieldID m_playerControllerF = nullptr;
    jfieldID m_objectMouseOverF = nullptr;
    
    jmethodID m_getBlockReachDistanceM = nullptr;
    jfieldID m_curBlockDamageMPF = nullptr;
    
    jclass m_blockPosClass = nullptr;
    jmethodID m_blockPosInitM = nullptr;
    
    jmethodID m_getBlockStateM = nullptr;
    jmethodID m_getBlockM = nullptr;
    
    jclass m_blockClass = nullptr;
    jmethodID m_getIdFromBlockM = nullptr;
    
    jclass m_mopClass = nullptr;
    jfieldID m_typeOfHitF = nullptr;
    jfieldID m_blockPosF = nullptr;
    jobject m_typeBlockEnum = nullptr;
    
    jfieldID m_gsF = nullptr;
    jfieldID m_keyBindAttackF = nullptr;
    jfieldID m_pressedF = nullptr;

    jmethodID m_clickBlockM = nullptr;
    jmethodID m_onPlayerDamageBlockM = nullptr;
    jmethodID m_swingItemM = nullptr;
    jobject m_enumFacingUp = nullptr;
    
    jfieldID m_blockHitDelayF = nullptr;

    static void _ex(JNIEnv* e) { if (e->ExceptionCheck()) e->ExceptionClear(); }

    void _cache(JNIEnv* env, jobject mcObj, jclass playerClass) {
        if (m_cached) return;

        jclass mcClass = env->GetObjectClass(mcObj); _ex(env);
        if (!mcClass) return;

        m_posXF = MC::fieldID(env, playerClass, "Entity.posX");
        m_posYF = MC::fieldID(env, playerClass, "Entity.posY");
        m_posZF = MC::fieldID(env, playerClass, "Entity.posZ");
        if (!m_posXF || !m_posYF || !m_posZF) return;

        m_theWorldF = MC::fieldID(env, mcClass, "Minecraft.theWorld");
        m_playerControllerF = MC::fieldID(env, mcClass, "Minecraft.playerController");
        m_objectMouseOverF = MC::fieldID(env, mcClass, "Minecraft.objectMouseOver");
        
        m_gsF = MC::fieldID(env, mcClass, "Minecraft.gameSettings");
        jclass gsClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/settings/GameSettings");
        if (!gsClass) { env->ExceptionClear(); gsClass = env->FindClass("avs"); }
        if (gsClass) {
            m_keyBindAttackF = env->GetFieldID(gsClass, "keyBindAttack", "Lnet/minecraft/client/settings/KeyBinding;");
            if (!m_keyBindAttackF) { env->ExceptionClear(); m_keyBindAttackF = env->GetFieldID(gsClass, "field_74312_F", "Lavt;"); }
            env->DeleteLocalRef(gsClass);
        }
        
        jclass kbClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/settings/KeyBinding");
        if (!kbClass) { env->ExceptionClear(); kbClass = env->FindClass("avt"); }
        if (kbClass) {
            m_pressedF = env->GetFieldID(kbClass, "pressed", "Z");
            if (!m_pressedF) { env->ExceptionClear(); m_pressedF = env->GetFieldID(kbClass, "field_74513_e", "Z"); }
            if (!m_pressedF) { env->ExceptionClear(); m_pressedF = env->GetFieldID(kbClass, "e", "Z"); }
            env->DeleteLocalRef(kbClass);
        }

        jclass pcClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/multiplayer/PlayerControllerMP");
        if (!pcClass) { env->ExceptionClear(); pcClass = env->FindClass("bda"); }
        if (pcClass) {
            m_getBlockReachDistanceM = env->GetMethodID(pcClass, "getBlockReachDistance", "()F");
            if (!m_getBlockReachDistanceM) { env->ExceptionClear(); m_getBlockReachDistanceM = env->GetMethodID(pcClass, "func_78757_d", "()F"); }
            if (!m_getBlockReachDistanceM) { env->ExceptionClear(); m_getBlockReachDistanceM = env->GetMethodID(pcClass, "d", "()F"); }
            
            m_curBlockDamageMPF = env->GetFieldID(pcClass, "curBlockDamageMP", "F");
            if (!m_curBlockDamageMPF) { env->ExceptionClear(); m_curBlockDamageMPF = env->GetFieldID(pcClass, "field_78770_f", "F"); }
            if (!m_curBlockDamageMPF) { env->ExceptionClear(); m_curBlockDamageMPF = env->GetFieldID(pcClass, "e", "F"); }
            
            m_blockHitDelayF = env->GetFieldID(pcClass, "blockHitDelay", "I");
            if (!m_blockHitDelayF) { env->ExceptionClear(); m_blockHitDelayF = env->GetFieldID(pcClass, "field_78781_i", "I"); }
            if (!m_blockHitDelayF) { env->ExceptionClear(); m_blockHitDelayF = env->GetFieldID(pcClass, "g", "I"); }
            
            m_clickBlockM = env->GetMethodID(pcClass, "clickBlock", "(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z");
            if (!m_clickBlockM) { env->ExceptionClear(); m_clickBlockM = env->GetMethodID(pcClass, "func_180511_b", "(Lcj;Lcq;)Z"); }
            if (!m_clickBlockM) { env->ExceptionClear(); m_clickBlockM = env->GetMethodID(pcClass, "b", "(Lcj;Lcq;)Z"); }
            
            m_onPlayerDamageBlockM = env->GetMethodID(pcClass, "onPlayerDamageBlock", "(Lnet/minecraft/util/BlockPos;Lnet/minecraft/util/EnumFacing;)Z");
            if (!m_onPlayerDamageBlockM) { env->ExceptionClear(); m_onPlayerDamageBlockM = env->GetMethodID(pcClass, "func_180512_c", "(Lcj;Lcq;)Z"); }
            if (!m_onPlayerDamageBlockM) { env->ExceptionClear(); m_onPlayerDamageBlockM = env->GetMethodID(pcClass, "c", "(Lcj;Lcq;)Z"); }
            
            env->DeleteLocalRef(pcClass);
        }
        
        m_swingItemM = MC::methodID(env, playerClass, "EntityLivingBase.swingItem");
        
        jclass facingClass = JniManager::FindClassWithLoader(env, "net/minecraft/util/EnumFacing");
        if (!facingClass) { env->ExceptionClear(); facingClass = env->FindClass("cq"); }
        if (facingClass) {
            jfieldID upF = env->GetStaticFieldID(facingClass, "UP", "Lnet/minecraft/util/EnumFacing;");
            if (!upF) { env->ExceptionClear(); upF = env->GetStaticFieldID(facingClass, "field_176728_b", "Lcq;"); }
            if (!upF) { env->ExceptionClear(); upF = env->GetStaticFieldID(facingClass, "b", "Lcq;"); }
            if (upF) {
                jobject upObj = env->GetStaticObjectField(facingClass, upF); _ex(env);
                if (upObj) {
                    m_enumFacingUp = env->NewGlobalRef(upObj); _ex(env);
                    env->DeleteLocalRef(upObj);
                }
            }
            env->DeleteLocalRef(facingClass);
        }
        
        jclass bpClass = JniManager::FindClassWithLoader(env, "net/minecraft/util/BlockPos");
        if (!bpClass) { env->ExceptionClear(); bpClass = env->FindClass("cj"); }
        if (bpClass) {
            m_blockPosClass = (jclass)env->NewGlobalRef(bpClass); _ex(env);
            m_blockPosInitM = env->GetMethodID(bpClass, "<init>", "(DDD)V"); _ex(env);
            env->DeleteLocalRef(bpClass);
        }
        
        jclass worldClass = JniManager::FindClassWithLoader(env, "net/minecraft/world/World");
        if (!worldClass) { env->ExceptionClear(); worldClass = env->FindClass("adm"); }
        if (worldClass) {
            m_getBlockStateM = env->GetMethodID(worldClass, "getBlockState", "(Lnet/minecraft/util/BlockPos;)Lnet/minecraft/block/state/IBlockState;");
            if (!m_getBlockStateM) { env->ExceptionClear(); m_getBlockStateM = env->GetMethodID(worldClass, "func_180495_p", "(Lcj;)Lalz;"); }
            if (!m_getBlockStateM) { env->ExceptionClear(); m_getBlockStateM = env->GetMethodID(worldClass, "p", "(Lcj;)Lalz;"); }
            env->DeleteLocalRef(worldClass);
        }
        
        jclass ibsClass = JniManager::FindClassWithLoader(env, "net/minecraft/block/state/IBlockState");
        if (!ibsClass) { env->ExceptionClear(); ibsClass = env->FindClass("alz"); }
        if (ibsClass) {
            m_getBlockM = env->GetMethodID(ibsClass, "getBlock", "()Lnet/minecraft/block/Block;");
            if (!m_getBlockM) { env->ExceptionClear(); m_getBlockM = env->GetMethodID(ibsClass, "func_177230_c", "()Lafh;"); }
            if (!m_getBlockM) { env->ExceptionClear(); m_getBlockM = env->GetMethodID(ibsClass, "c", "()Lafh;"); }
            env->DeleteLocalRef(ibsClass);
        }
        
        jclass bClass = JniManager::FindClassWithLoader(env, "net/minecraft/block/Block");
        if (!bClass) { env->ExceptionClear(); bClass = env->FindClass("afh"); }
        if (bClass) {
            m_blockClass = (jclass)env->NewGlobalRef(bClass); _ex(env);
            m_getIdFromBlockM = env->GetStaticMethodID(bClass, "getIdFromBlock", "(Lnet/minecraft/block/Block;)I");
            if (!m_getIdFromBlockM) { env->ExceptionClear(); m_getIdFromBlockM = env->GetStaticMethodID(bClass, "func_149682_b", "(Lafh;)I"); }
            if (!m_getIdFromBlockM) { env->ExceptionClear(); m_getIdFromBlockM = env->GetStaticMethodID(bClass, "a", "(Lafh;)I"); }
            env->DeleteLocalRef(bClass);
        }
        
        jclass mopCls = JniManager::FindClassWithLoader(env, "net/minecraft/util/MovingObjectPosition");
        if (!mopCls) { env->ExceptionClear(); mopCls = env->FindClass("auh"); }
        if (mopCls) {
            m_mopClass = (jclass)env->NewGlobalRef(mopCls); _ex(env);
            m_typeOfHitF = env->GetFieldID(mopCls, "typeOfHit", "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;");
            if (!m_typeOfHitF) { env->ExceptionClear(); m_typeOfHitF = env->GetFieldID(mopCls, "field_72313_a", "Lauh$a;"); }
            if (!m_typeOfHitF) { env->ExceptionClear(); m_typeOfHitF = env->GetFieldID(mopCls, "a", "Lauh$a;"); }
            
            m_blockPosF = env->GetFieldID(mopCls, "blockPos", "Lnet/minecraft/util/BlockPos;");
            if (!m_blockPosF) { env->ExceptionClear(); m_blockPosF = env->GetFieldID(mopCls, "field_178783_e", "Lcj;"); }
            if (!m_blockPosF) { env->ExceptionClear(); m_blockPosF = env->GetFieldID(mopCls, "e", "Lcj;"); }
            env->DeleteLocalRef(mopCls);
        }
        
        jclass mopTypeCls = JniManager::FindClassWithLoader(env, "net/minecraft/util/MovingObjectPosition$MovingObjectType");
        if (!mopTypeCls) { env->ExceptionClear(); mopTypeCls = env->FindClass("auh$a"); }
        if (mopTypeCls) {
            jfieldID typeBlockF = env->GetStaticFieldID(mopTypeCls, "BLOCK", "Lnet/minecraft/util/MovingObjectPosition$MovingObjectType;"); _ex(env);
            if (!typeBlockF) { env->ExceptionClear(); typeBlockF = env->GetStaticFieldID(mopTypeCls, "b", "Lauh$a;"); }
            if (typeBlockF) {
                jobject blockEnum = env->GetStaticObjectField(mopTypeCls, typeBlockF); _ex(env);
                if (blockEnum) {
                    m_typeBlockEnum = env->NewGlobalRef(blockEnum); _ex(env);
                    env->DeleteLocalRef(blockEnum);
                }
            }
            env->DeleteLocalRef(mopTypeCls);
        }

        m_cached = true;
    }
    
    // Matrix multiplication and projection to 2D screen coordinates
    bool project(float objX, float objY, float objZ, 
                 const GLfloat* modelview, const GLfloat* projection, const GLint* viewport, 
                 float* winX, float* winY, float* winZ) {
        float in[4] = { objX, objY, objZ, 1.0f };
        float out[4];
        float out2[4];
        
        // modelview * in
        for (int i=0; i<4; i++) {
            out[i] = in[0]*modelview[0*4+i] + in[1]*modelview[1*4+i] + in[2]*modelview[2*4+i] + in[3]*modelview[3*4+i];
        }
        // projection * out
        for (int i=0; i<4; i++) {
            out2[i] = out[0]*projection[0*4+i] + out[1]*projection[1*4+i] + out[2]*projection[2*4+i] + out[3]*projection[3*4+i];
        }
        if (out2[3] == 0.0f) return false;
        if (out2[3] < 0.0f) return false; // Behind camera
        
        out2[0] /= out2[3];
        out2[1] /= out2[3];
        out2[2] /= out2[3];
        
        // Map x, y to range 0-1
        *winX = viewport[0] + (1.0f + out2[0]) * viewport[2] / 2.0f;
        *winY = viewport[1] + (1.0f + out2[1]) * viewport[3] / 2.0f;
        *winZ = (1.0f + out2[2]) / 2.0f;
        
        return true;
    }
};
