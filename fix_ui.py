import re

with open('src_dll/hook.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

replacement_start = '''float savedY = ImGui::GetCursorPosY();
                              float slideOffset = (1.0f - (ea / (contentAlpha > 0.01f ? contentAlpha : 1.0f))) * 25.0f;
                              float startX = (220.0f > ImGui::GetWindowSize().x - 320.0f ? 220.0f : ImGui::GetWindowSize().x - 320.0f);
                              ImVec2 bgTopLeft = ImVec2(ImGui::GetWindowPos().x + startX - slideOffset, ImGui::GetWindowPos().y + 85.0f);
                              ImVec2 bgBottomRight = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
                              ImDrawList* dl = ImGui::GetWindowDrawList();
                              dl->AddRectFilled(bgTopLeft, bgBottomRight, IM_COL32(10, 14, 18, int(160 * ea)), 10.0f, ImDrawFlags_RoundCornersRight);
                              dl->AddLine(bgTopLeft, ImVec2(bgTopLeft.x, bgBottomRight.y), IM_COL32(255, 255, 255, int(30 * ea)));
                              ImGui::SetCursorPos(ImVec2(startX - slideOffset, 85.0f));
                              ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ea);
                              ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0,0,0,0));
                              BeginSmoothScrollChild(ImGui::GetID((void*)(intptr_t)savedY), ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground, dt);
                              if (g_ResetPanelScrolls) { ImGui::SetScrollY(0.0f); ImGui::GetStateStorage()->SetFloat(ImGui::GetID("smooth_scroll_target"), 0.0f); g_ResetPanelScrolls = false; }
                              ImGui::SetCursorPos(ImVec2(25, 25));
                              ImGui::BeginGroup();'''

replacement_end = '''ImGui::EndGroup();
                              ImGui::EndChild();
                              ImGui::PopStyleColor();
                              ImGui::PopStyleVar();
                              ImGui::SetCursorPos(ImVec2(45, savedY));'''

code = code.replace('ImGui::Indent(20.0f);', replacement_start)
code = code.replace('ImGui::Unindent(20.0f);', replacement_end)

if 'bool g_ResetPanelScrolls' not in code:
    code = code.replace('bool g_ShowMenu = false;', 'bool g_ShowMenu = false;\nbool g_ResetPanelScrolls = false;')

code = re.sub(r'ts->value = !ts->value;\s*ts->animState = ts->value \? 0.01f : 0.99f;', r'ts->value = !ts->value;\n                ts->animState = ts->value ? 0.01f : 0.99f;\n                if(ts->value) g_ResetPanelScrolls = true;', code)
code = re.sub(r'if \((.*?) && GetMessageExtraInfo\(\) == \(LPARAM\)0xA7C0C112UL\)\s*{\s*(.*?)->value = !(.*?)->value;\s*(.*?)->animState = (.*?)->value \? 0.01f : 0.99f;', r'if (\1 && GetMessageExtraInfo() == (LPARAM)0xA7C0C112UL) { \2->value = !\3->value; \4->animState = \5->value ? 0.01f : 0.99f; if(\2->value) g_ResetPanelScrolls = true;', code)

with open('src_dll/hook.cpp', 'w', encoding='utf-8') as f:
    f.write(code)

print("done")
