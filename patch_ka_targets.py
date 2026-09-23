import sys
import re

def patch_hook():
    with open('src_dll/hook.cpp', 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Expand arrays
    content = content.replace("bool g_ComboOpen[8] =", "bool g_ComboOpen[15] =")
    content = content.replace("float g_ComboOpenAnim[8] =", "float g_ComboOpenAnim[15] =")

    # 2. Inject StyledMultiCombo right after StyledCombo
    multi_combo_code = """
static bool StyledMultiCombo(const char* label, bool* states, const char* const items[], int count, float alphaMultiplier, int comboIdx) {
    ImGui::PushID(label);
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);
    float rowW = 250.0f;
    float rowH = 22.0f;

    ImGui::InvisibleButton("##combobtn", ImVec2(rowW, rowH));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) g_ComboOpen[comboIdx] = !g_ComboOpen[comboIdx];

    ImU32 textCol = hovered ? IM_COL32(230, 235, 240, baseA) : IM_COL32(200, 210, 210, baseA);
    DrawHighlightedText(dl, ImVec2(startPos.x, startPos.y + 3), textCol, IM_COL32(100, 180, 255, baseA), label, nullptr, g_SearchBuffer);

    std::string selStr = "";
    int selCount = 0;
    for(int i=0; i<count; i++) {
        if(states[i]) {
            if(selCount > 0) selStr += ", ";
            selStr += items[i];
            selCount++;
        }
    }
    if(selCount == 0) selStr = "None";
    if(selStr.length() > 20) selStr = std::to_string(selCount) + " selected";

    ImVec2 selSize = ImGui::CalcTextSize(selStr.c_str());
    dl->AddText(ImVec2(startPos.x + rowW - selSize.x - 20, startPos.y + 3), IM_COL32(140, 155, 170, baseA), selStr.c_str());

    float ax = startPos.x + rowW - 8, ay = startPos.y + 11;
    g_ComboOpenAnim[comboIdx] = Lerp(g_ComboOpenAnim[comboIdx], g_ComboOpen[comboIdx] ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float arrowAng = g_ComboOpenAnim[comboIdx];
    float angle_c = arrowAng * 3.14159265359f; 
    float s_c = sinf(angle_c), c_c = cosf(angle_c);
    
    ImVec2 cp1(-3.5f, -1.5f); ImVec2 cp2( 0.0f,  2.0f); ImVec2 cp3( 3.5f, -1.5f);
    ImVec2 cr1(cp1.x * c_c - cp1.y * s_c + ax, cp1.x * s_c + cp1.y * c_c + ay);
    ImVec2 cr2(cp2.x * c_c - cp2.y * s_c + ax, cp2.x * s_c + cp2.y * c_c + ay);
    ImVec2 cr3(cp3.x * c_c - cp3.y * s_c + ax, cp3.x * s_c + cp3.y * c_c + ay);
    
    dl->AddLine(cr1, cr2, IM_COL32(150, 165, 180, baseA), 2.0f);
    dl->AddLine(cr2, cr3, IM_COL32(150, 165, 180, baseA), 2.0f);

    bool changed = false;
    float itemH = 22.0f;
    float totalDropH = count * itemH + 8.0f;
    float currentDropH = totalDropH * EaseOutCubic(g_ComboOpenAnim[comboIdx]);

    if (currentDropH > 0.01f) {
        float dropY = startPos.y + rowH;
        dl->PushClipRect(ImVec2(startPos.x, dropY), ImVec2(startPos.x + rowW, dropY + currentDropH), true);
        dl->AddRectFilled(ImVec2(startPos.x + 8, dropY), ImVec2(startPos.x + rowW - 8, dropY + currentDropH - 4), IM_COL32(15, 20, 25, (int)(100 * alphaMultiplier)), 6.0f);
        
        for (int i = 0; i < count; i++) {
            float itemY = dropY + 4.0f + i * itemH;
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10, itemY));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##item", ImVec2(rowW - 20, itemH));
            bool itemHov = ImGui::IsItemHovered();
            if (itemHov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsItemClicked()) {
                states[i] = !states[i];
                changed = true;
                SaveConfig();
            }

            ImU32 itCol = states[i] ? IM_COL32(100, 180, 255, baseA) : (itemHov ? IM_COL32(230,230,230,baseA) : IM_COL32(170,170,170,baseA));
            dl->AddText(ImVec2(startPos.x + 35, itemY + 3), itCol, items[i]);
            
            // Draw checkbox
            float cbR = 6.0f;
            float cbX = startPos.x + 20;
            float cbY = itemY + itemH * 0.5f;
            dl->AddRectFilled(ImVec2(cbX - cbR, cbY - cbR), ImVec2(cbX + cbR, cbY + cbR), IM_COL32(30, 35, 40, baseA), 2.0f);
            if (states[i]) {
                dl->AddRectFilled(ImVec2(cbX - cbR+2, cbY - cbR+2), ImVec2(cbX + cbR-2, cbY + cbR-2), IM_COL32(100, 180, 255, baseA), 1.0f);
            }
            ImGui::PopID();
        }
        dl->PopClipRect();
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, dropY + currentDropH));
    } else {
        ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + rowH));
    }
    ImGui::PopID();
    return changed;
}
"""
    # Insert it before AnimatedToggle
    if "static bool StyledMultiCombo" not in content:
        content = content.replace("static bool AnimatedToggle", multi_combo_code + "\nstatic bool AnimatedToggle")

    # 3. Add to KillAura UI
    # We need to find the KillAura menu block and insert the Targets combo.
    # KillAura has: `AnimatedSlider("Reach"`
    target_combo_code = """
                              static const char* auraTargets[] = { "Players", "Hostile Mobs", "Neutral/Passive" };
                              KillAura* ka = (KillAura*)ModuleManager::get().getModule("KillAura");
                              bool targetStates[3] = { ka->targetPlayers, ka->targetHostiles, ka->targetPassives };
                              if (StyledMultiCombo("Targets", targetStates, auraTargets, 3, ea, 8)) {
                                  ka->targetPlayers = targetStates[0];
                                  ka->targetHostiles = targetStates[1];
                                  ka->targetPassives = targetStates[2];
                              }
                              ImGui::Spacing();
"""
    if "auraTargets[]" not in content:
        content = content.replace('StyledCombo("Priority", &g_ComboSelections[1], auraPriority, 3, ea, 1); ImGui::Spacing();', 
                                  'StyledCombo("Priority", &g_ComboSelections[1], auraPriority, 3, ea, 1); ImGui::Spacing();\n' + target_combo_code)

    with open('src_dll/hook.cpp', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Patched hook.cpp")

def patch_killaura():
    with open('src_dll/modules/KillAura.hpp', 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Add fields
    if "bool targetPlayers" not in content:
        content = content.replace("bool  teams           = false;", 
                                  "bool  teams           = false;\n    bool  targetPlayers    = true;\n    bool  targetHostiles   = false;\n    bool  targetPassives   = false;")
    
    # 2. Update logic
    old_logic = """            if (!env->IsInstanceOf(entObj, livingClass)) { env->DeleteLocalRef(entObj); continue; }
            if (!env->IsInstanceOf(entObj, playerCls)) { env->DeleteLocalRef(entObj); continue; }"""
    
    new_logic = """            if (!env->IsInstanceOf(entObj, livingClass)) { env->DeleteLocalRef(entObj); continue; }
            
            jclass mobCls      = JniManager::FindClassWithLoader(env, "net/minecraft/entity/monster/IMob");
            jclass animalCls   = JniManager::FindClassWithLoader(env, "net/minecraft/entity/passive/IAnimals");
            jclass villagerCls = JniManager::FindClassWithLoader(env, "net/minecraft/entity/passive/EntityVillager");
            jclass batCls      = JniManager::FindClassWithLoader(env, "net/minecraft/entity/passive/EntityBat");
            jclass squidCls    = JniManager::FindClassWithLoader(env, "net/minecraft/entity/passive/EntitySquid");

            bool isPlayer = env->IsInstanceOf(entObj, playerCls);
            bool isHostile = (mobCls && env->IsInstanceOf(entObj, mobCls));
            bool isPassive = (animalCls && env->IsInstanceOf(entObj, animalCls)) ||
                             (villagerCls && env->IsInstanceOf(entObj, villagerCls)) ||
                             (batCls && env->IsInstanceOf(entObj, batCls)) ||
                             (squidCls && env->IsInstanceOf(entObj, squidCls));

            if (!isPlayer && !isHostile && !isPassive) {
                env->DeleteLocalRef(entObj);
                continue;
            }
            if (isPlayer && !targetPlayers) { env->DeleteLocalRef(entObj); continue; }
            if (isHostile && !targetHostiles) { env->DeleteLocalRef(entObj); continue; }
            if (isPassive && !targetPassives) { env->DeleteLocalRef(entObj); continue; }"""
    
    if old_logic in content:
        content = content.replace(old_logic, new_logic)
        print("Patched KillAura.hpp logic")
    else:
        print("Could not find old logic in KillAura.hpp")

    with open('src_dll/modules/KillAura.hpp', 'w', encoding='utf-8') as f:
        f.write(content)

patch_hook()
patch_killaura()
