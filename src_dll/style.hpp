#pragma once
#include "imgui.h"

inline void SetupN1mbusStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Ultra-Premium Sleek Deep Teal/Dark Palette (Website Vibe)
    colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.45f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.08f, 0.10f, 0.97f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.03f, 0.05f, 0.07f, 0.50f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.05f, 0.08f, 0.10f, 0.98f);
    colors[ImGuiCol_Border]                 = ImVec4(0.12f, 0.16f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.10f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.05f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.05f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.05f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.03f, 0.04f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.15f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.25f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.85f, 0.90f, 0.95f, 1.00f); 
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.85f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.12f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.18f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.25f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.12f, 0.18f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.18f, 0.25f, 0.32f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.25f, 0.32f, 0.40f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.15f, 0.20f, 0.25f, 0.50f);
    
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 6.0f;
    style.WindowPadding = ImVec2(24, 24);
    style.FramePadding = ImVec2(8, 4); // SMALLER FRAME PADDING = SMALLER CHECKBOXES
    style.ItemSpacing = ImVec2(12, 14);
    style.ItemInnerSpacing = ImVec2(12, 6);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
}
