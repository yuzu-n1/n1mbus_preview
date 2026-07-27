#include "hook.hpp"
#include <windows.h>
#include <gl/GL.h>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <mutex>
#include <math.h>
#include <cstdio>
#include <urlmon.h>
#include "MinHook.h"
#include "imgui.h"
#include "style.hpp"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
#include "jni_manager.hpp"
#include "minecraft_mappings.hpp"
#include "mapping_resolver.hpp"
#include "mc.hpp"
#include "mc_register.hpp"
#include "blur.hpp"
#include "image_loader.hpp"

#include "modules.hpp"
#include "plugin_loader.hpp"
#include "script_engine.hpp"
#pragma comment(lib, "opengl32.lib")

typedef BOOL(WINAPI* twglSwapBuffers)(HDC hDc);
twglSwapBuffers o_wglSwapBuffers = nullptr;
typedef BOOL(WINAPI* tSetCursorPos)(int X, int Y);
tSetCursorPos o_SetCursorPos = nullptr;
typedef BOOL(WINAPI* tGetCursorPos)(LPPOINT lpPoint);
tGetCursorPos o_GetCursorPos = nullptr;
typedef BOOL(WINAPI* tClipCursor)(const RECT* lpRect);
tClipCursor o_ClipCursor = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WNDPROC o_WndProc = nullptr;
HWND g_hWnd = nullptr;
bool g_Initialized = false;
bool g_ShowMenu = false;
HMODULE g_hModule = nullptr;
float g_MenuAlpha = 0.0f;
float g_GlobalTime = 0.0f;
bool g_HudEditorMode = false;
static bool g_JavaAgentAttempted = false;
static void ShowToast(const char* msg, float duration);

static std::string GetModuleDirectory(HMODULE moduleHandle) {
    char modulePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(moduleHandle, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    std::string path = modulePath;
    size_t slash = path.find_last_of("\\/");
    if (slash == std::string::npos) return {};
    path.resize(slash + 1);
    return path;
}

static void TraceJavaAgent(const char* message) {
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
}

ImFont* g_ToastFont = nullptr;
ImFont* g_IconFont = nullptr;
GLuint g_InfoIconTex = 0;
GLuint g_PaletteTex = 0;
GLuint g_BannerTex = 0;
GLuint g_ReloadIconTex = 0;
GLuint g_EditIconTex = 0;
int g_BannerW = 0, g_BannerH = 0;
int g_InfoIconW = 0, g_InfoIconH = 0;

int g_CurrentTab = 0, g_TargetTab = 0;
float g_TabSlideAnim = 1.0f;
float g_IndicatorY = 0.0f, g_IndicatorTargetY = 0.0f;
float g_TabHoverAnim[5] = {};
float g_SectionHeaderAnim = 0.0f;
float g_MenuScale = 0.0f;
float g_WidgetStagger[20] = {};
int g_PrevTab = -1;

struct ToggleState { bool value = false; float anim = 0.0f; };
ToggleState g_Toggles[25];
static std::map<std::string, ToggleState> g_PluginToggles;
static ToggleState* GetModToggle(const std::string& name);
bool g_ExpandStates[10] = { false, false, false, false, false, false, false, false, false, false };
float g_ExpandAnims[10] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
bool g_ComboOpen[8] = {};
float g_ComboOpenAnim[8] = {};
// [0]=KillAura reach  [1]=Fly speed  [2]=Speed mult  [3]=HUD fade
// [4]=HUD scale  [5]=HUD X  [6]=HUD Y  [7]=AimAssist speed [8]=AimAssist FOV
// [9]=AutoClicker Min CPS [10]=AutoClicker Max CPS
// [11]=TriggerBot Min CPS [12]=TriggerBot Max CPS [13]=TriggerBot Reach
// [14]=Velocity H  [15]=Velocity V
// [16]=KillAura Min CPS [17]=KillAura Max CPS [18]=KillAura FOV [19]=KillAura Aim
// [20]=ArrayList X [21]=ArrayList Y [22]=ArrayList Scale [23]=TargetHUD Range [24]=ArrayList Anim Speed
float g_SliderVals[25] = {3.0f, 1.0f, 1.5f, 10.0f, 1.0f, 40.0f, 40.0f, 5.0f, 90.0f, 10.0f, 14.0f, 8.0f, 12.0f, 3.0f, 0.0f, 0.0f, 10.0f, 14.0f, 360.0f, 10.0f, -1.0f, 10.0f, 0.0f, 64.0f, 1.0f};
int g_ComboSelections[8] = {};
float g_Colors[4][4] = {
    {0.26f, 0.56f, 1.0f, 1.0f},
    {1.0f, 0.3f, 0.3f, 1.0f},
    {0.0f, 1.0f, 0.5f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f}
};

// Forward-declared module instance pointers (initialized once in the first frame)
static class NoFall*    g_ModNoFall    = nullptr;
static class Speed*     g_ModSpeed     = nullptr;
static class KillAura*  g_ModKillAura  = nullptr;
static class Scaffold*  g_ModScaffold  = nullptr;
static class AimAssist* g_ModAimAssist = nullptr;
static class AutoClicker* g_ModAutoClicker = nullptr;
static class SprintReset* g_ModSprintReset = nullptr;
static class BridgeAssist* g_ModBridgeAssist = nullptr;
static class TriggerBot* g_ModTriggerBot = nullptr;
static class Velocity* g_ModVelocity = nullptr;

class ScriptEngine;
static std::unique_ptr<ScriptEngine> g_ScriptEngine;

std::string g_MCID = "Developer";
bool g_MCIDFetched = false;
GLuint g_SkinTexID = 0;

std::string g_TargetName = "";
float g_TargetHealth = 0.0f;
float g_TargetMaxHealth = 20.0f;
float g_TargetDistance = 0.0f;
int g_TargetArmor = 0;
std::string g_TargetHeldItem = "";
int g_TargetPing = -1;
int g_TargetPotionCount = 0;
float g_TargetInfoAlpha = 0.0f;
bool g_HasTarget = false;
bool g_TargetIsPlayer = false;
bool g_TargetLookAway = false;
float g_TargetLookAwayTimer = 0.0f;
int g_LastModuleCount = 0;

GLuint g_TargetSkinTexID = 0;

bool g_IsGuiOpen = false;
float g_ArrayListGlobalAlpha = 1.0f;
float g_TargetAttackTimer = 0.0f;
float g_ArrayListColors[2][4] = {
    {0.4f, 0.7f, 1.0f, 1.0f},
    {1.0f, 0.4f, 0.7f, 1.0f}
};
struct ModuleFadeState {
    std::string name;
    float progress; // 0→1 animation progress (with easing)
    bool present;
    bool fadingOut;
    float alpha; // computed from progress for convenience
};
static std::vector<ModuleFadeState> g_ModuleFadeStates;

struct Particle { float x, y, vx, vy, alpha, size; };
static Particle g_Particles[40];
static bool g_ParticlesInit = false;

thread_local bool g_BypassGetCursorPos = false;

// Debug ESP
static bool dbg_matricesValid = false;
static int dbg_listSize = -1;
static bool dbg_hasFields = false;
static int dbg_drawnCount = 0;
static int dbg_playerCount = 0;
static float dbg_mv0 = 0.0f;
static float dbg_proj0 = 0.0f;

// Toast Notification System
struct ToastNotification {
    char message[128];
    float timer;       // time remaining
    float duration;    // total duration
    float slideAnim;   // 0=offscreen, 1=fully visible
    bool active;
};
static ToastNotification g_Toasts[4];
static int g_ToastCount = 0;
static bool g_FirstFrame = true;

static void ShowToast(const char* msg, float duration = 3.5f) {
    if (g_ToastCount >= 4) return;
    ToastNotification& t = g_Toasts[g_ToastCount++];
    strncpy(t.message, msg, 127);
    t.message[127] = '\0';
    t.timer = duration;
    t.duration = duration;
    t.slideAnim = 0.0f;
    t.active = true;
}

static float Lerp(float a, float b, float t) { return a + (b - a) * (t < 0 ? 0 : (t > 1 ? 1 : t)); }
static float EaseOutCubic(float t) { t = 1.0f - t; return 1.0f - t * t * t; }
static float EaseOutQuint(float t) { t = 1.0f - t; return 1.0f - t * t * t * t * t; }
static float EaseOutBack(float t) { const float c1 = 1.70158f; const float c3 = c1 + 1.0f; return 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f); }
static float EaseOutElastic(float t) { if (t <= 0) return 0; if (t >= 1) return 1; return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * (2.0f * 3.14159f / 3.0f)) + 1.0f; }
static float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

static void InitParticles(float w, float h) {
    for (int i = 0; i < 40; i++) {
        g_Particles[i].x = (float)(rand() % (int)w); g_Particles[i].y = (float)(rand() % (int)h);
        g_Particles[i].vx = ((rand() % 100) / 100.0f - 0.5f) * 15.0f; g_Particles[i].vy = ((rand() % 100) / 100.0f - 0.5f) * 10.0f - 5.0f;
        g_Particles[i].alpha = 0.1f + (rand() % 30) / 100.0f; g_Particles[i].size = 1.0f + (rand() % 20) / 10.0f;
    }
    g_ParticlesInit = true;
}

static void UpdateAndDrawParticles(ImDrawList* dl, ImVec2 origin, float w, float h, float dt, float alpha) {
    if (!g_ParticlesInit) InitParticles(w, h);
    for (int i = 0; i < 40; i++) {
        Particle& p = g_Particles[i]; p.x += p.vx * dt; p.y += p.vy * dt;
        if (p.x < 0) p.x += w; if (p.x > w) p.x -= w;
        if (p.y < 0) p.y += h; if (p.y > h) p.y -= h;
        dl->AddCircleFilled(ImVec2(origin.x + p.x, origin.y + p.y), p.size, IM_COL32(120, 180, 255, (int)(p.alpha * alpha * 255)));
    }
    for (int i = 0; i < 40; i++) {
        for (int j = i + 1; j < 40; j++) {
            float dx = g_Particles[i].x - g_Particles[j].x, dy = g_Particles[i].y - g_Particles[j].y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < 80.0f) {
                float lineAlpha = (1.0f - dist / 80.0f) * 0.15f * alpha;
                dl->AddLine(ImVec2(origin.x + g_Particles[i].x, origin.y + g_Particles[i].y), ImVec2(origin.x + g_Particles[j].x, origin.y + g_Particles[j].y), IM_COL32(100, 160, 230, (int)(lineAlpha * 255)));
            }
        }
    }
}

static void RenderToastUI(float screenW, float screenH, float dt) {
    if (g_ToastCount == 0) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float toastW = 320.0f;
    float toastH = 64.0f;
    float startY = screenH - 30.0f - toastH;
    for (int i = 0; i < g_ToastCount; i++) {
        ToastNotification& t = g_Toasts[i];
        if (!t.active) continue;
        t.timer -= dt;
        if (t.timer <= 0.0f) {
            t.slideAnim -= dt * 6.0f;
            if (t.slideAnim <= 0.0f) { t.active = false; continue; }
        } else {
            t.slideAnim += dt * 6.0f;
            if (t.slideAnim > 1.0f) t.slideAnim = 1.0f;
        }
        float easedAnim = EaseOutBack(t.slideAnim);
        float offsetX = (1.0f - easedAnim) * (toastW + 50.0f);
        float x = screenW - 20.0f - toastW + offsetX;
        float y = startY - i * (toastH + 10.0f);
        
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + toastW, y + toastH), IM_COL32(18, 22, 28, 240), 6.0f);
        dl->AddRect(ImVec2(x, y), ImVec2(x + toastW, y + toastH), IM_COL32(40, 50, 65, 100), 6.0f);
        
        float prog = t.timer / t.duration;
        dl->AddRectFilled(ImVec2(x, y + toastH - 1.0f), ImVec2(x + toastW * prog, y + toastH), IM_COL32(80, 160, 255, 255), 4.0f, ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight);
        
        if (g_InfoIconTex) {
            dl->AddImage((void*)(intptr_t)g_InfoIconTex, ImVec2(x + 12, y + 12), ImVec2(x + 52, y + 52));
        }
        
        dl->AddText(ImGui::GetFont(), 22.0f, ImVec2(x + 62, y + 10), IM_COL32(230, 235, 245, 255), "Notification");
        dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(x + 62, y + 36), IM_COL32(160, 170, 180, 255), t.message);
    }
    if (g_Toasts[0].active == false && g_ToastCount > 0) {
        for (int i = 1; i < g_ToastCount; i++) g_Toasts[i - 1] = g_Toasts[i];
        g_ToastCount--;
    }
}

static void CloseMenu() {
    g_ShowMenu = false;
    g_HudEditorMode = false;
    if (g_hWnd) {
        RECT rect; GetClientRect(g_hWnd, &rect);
        POINT pt = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
        ClientToScreen(g_hWnd, &pt);
        if (o_SetCursorPos) o_SetCursorPos(pt.x, pt.y);
    }
}

// ---- Custom Widgets with Global Alpha Multiplier ----

static bool AnimatedToggle(const char* label, ToggleState& state, float dt, float alphaMultiplier) {
    ImGui::PushID(label);
    float fullW = ImGui::GetContentRegionAvail().x;
    if (fullW > 250.0f) fullW = 250.0f; // Max width for toggle row
    float toggleW = 34.0f, toggleH = 18.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    ImGui::InvisibleButton("##row", ImVec2(fullW, 20.0f));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // Hand cursor

    if (ImGui::IsItemClicked()) state.value = !state.value;
    
    state.anim = Lerp(state.anim, state.value ? 1.0f : 0.0f, dt * 14.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);
    
    ImU32 textCol = hovered ? IM_COL32(230,235,240,baseA) : IM_COL32(200,205,210,baseA);
    dl->AddText(ImVec2(pos.x, pos.y + 2), textCol, label);
    
    float toggleX = pos.x + fullW - toggleW;
    float r = toggleH * 0.5f;
    ImU32 trackCol = IM_COL32((int)Lerp(35,45,state.anim), (int)Lerp(45,120,state.anim), (int)Lerp(55,180,state.anim), baseA);
    dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), trackCol, r);
    if (hovered) dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), IM_COL32(255,255,255,(int)(15*alphaMultiplier)), r);
    
    float knobR = toggleH * 0.38f;
    float knobX = Lerp(toggleX + r, toggleX + toggleW - r, EaseOutBack(state.anim));
    dl->AddCircleFilled(ImVec2(knobX, pos.y + 1 + r), knobR, IM_COL32(245,248,252,baseA));
    dl->AddCircle(ImVec2(knobX, pos.y + 1 + r), knobR + 0.5f, IM_COL32(0,0,0,(int)(30*alphaMultiplier)));
    
    ImGui::PopID();
    return true;
}

static bool AnimatedModuleToggle(const char* label, Module* mod, float dt, float alphaMultiplier) {
    auto& state = g_PluginToggles[mod->getName()];
    if (auto* ts = GetModToggle(mod->getName())) {
        state.value = ts->value;
    } else {
        state.value = mod->isEnabled();
    }
    ImGui::PushID(label);
    float fullW = ImGui::GetContentRegionAvail().x;
    if (fullW > 250.0f) fullW = 250.0f;
    float toggleW = 34.0f, toggleH = 18.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##row", ImVec2(fullW, 20.0f));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) {
        if (auto* ts = GetModToggle(mod->getName())) {
            ts->value = !ts->value;
        } else {
            mod->toggle();
        }
        state.value = !state.value;
    }
    state.anim = Lerp(state.anim, state.value ? 1.0f : 0.0f, dt * 14.0f);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);
    ImU32 textCol = hovered ? IM_COL32(230,235,240,baseA) : IM_COL32(200,205,210,baseA);
    dl->AddText(ImVec2(pos.x, pos.y + 2), textCol, label);
    float toggleX = pos.x + fullW - toggleW;
    float r = toggleH * 0.5f;
    ImU32 trackCol = IM_COL32((int)Lerp(35,45,state.anim), (int)Lerp(45,120,state.anim), (int)Lerp(55,180,state.anim), baseA);
    dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), trackCol, r);
    if (hovered) dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), IM_COL32(255,255,255,(int)(15*alphaMultiplier)), r);
    float knobR = toggleH * 0.38f;
    float knobX = Lerp(toggleX + r, toggleX + toggleW - r, EaseOutBack(state.anim));
    dl->AddCircleFilled(ImVec2(knobX, pos.y + 1 + r), knobR, IM_COL32(245,248,252,baseA));
    dl->AddCircle(ImVec2(knobX, pos.y + 1 + r), knobR + 0.5f, IM_COL32(0,0,0,(int)(30*alphaMultiplier)));
    ImGui::PopID();
    return true;
}

static bool AnimatedExpandableToggle(const char* label, ToggleState& state, float dt, float alphaMultiplier, bool* expandState, float* expandAnim) {
    ImGui::PushID(label);
    float fullW = 250.0f, toggleW = 44.0f, toggleH = 22.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    ImGui::InvisibleButton("##row", ImVec2(fullW, 20.0f));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImVec2 clickPos = ImGui::GetIO().MousePos;
    bool clicked = ImGui::IsItemClicked();
    
    float toggleX = pos.x + fullW - toggleW;
    
    if (clicked) {
        if (clickPos.x > toggleX - 10) {
            state.value = !state.value;
        } else {
            *expandState = !*expandState;
        }
    }
    
    state.anim = Lerp(state.anim, state.value ? 1.0f : 0.0f, dt * 14.0f);
    *expandAnim = Lerp(*expandAnim, *expandState ? 1.0f : 0.0f, dt * 14.0f);
    
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);
    
    ImU32 arrowCol = IM_COL32(180,185,190,baseA);
    float cx = pos.x + 6;
    float cy = pos.y + 10;
    
    if (*expandState) {
        dl->AddTriangleFilled(ImVec2(cx-4, cy-2), ImVec2(cx+4, cy-2), ImVec2(cx, cy+4), arrowCol);
    } else {
        dl->AddTriangleFilled(ImVec2(cx-2, cy-4), ImVec2(cx+4, cy), ImVec2(cx-2, cy+4), arrowCol);
    }

    ImU32 textCol = hovered ? IM_COL32(230,235,240,baseA) : IM_COL32(200,205,210,baseA);
    dl->AddText(ImVec2(pos.x + 16, pos.y + 2), textCol, label);
    
    float r = toggleH * 0.5f;
    ImU32 trackCol = IM_COL32((int)Lerp(35,45,state.anim), (int)Lerp(45,120,state.anim), (int)Lerp(55,180,state.anim), baseA);
    dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), trackCol, r);
    if (hovered && clickPos.x > toggleX - 10) dl->AddRectFilled(ImVec2(toggleX, pos.y + 1), ImVec2(toggleX + toggleW, pos.y + 1 + toggleH), IM_COL32(255,255,255,(int)(15*alphaMultiplier)), r);
    
    float knobR = toggleH * 0.38f;
    float knobX = Lerp(toggleX + r, toggleX + toggleW - r, EaseOutBack(state.anim));
    dl->AddCircleFilled(ImVec2(knobX, pos.y + 1 + r), knobR, IM_COL32(245,248,252,baseA));
    dl->AddCircle(ImVec2(knobX, pos.y + 1 + r), knobR + 0.5f, IM_COL32(0,0,0,(int)(30*alphaMultiplier)));
    
    ImGui::PopID();
    return true;
}

static void MiniColorPicker(const char* label, float col[4], float alphaMultiplier) {
    ImGui::SameLine(0, 10);
    ImGui::PushID(label);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaMultiplier);
    
    if (ImGui::ColorButton("##cb", ImVec4(col[0], col[1], col[2], col[3]), ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoBorder, ImVec2(20, 20))) {
        ImGui::OpenPopup("ColorPopup");
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
    if (ImGui::BeginPopup("ColorPopup")) {
        ImGui::ColorPicker4("##picker", col, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs);
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    
    ImGui::PopStyleVar();
    ImGui::PopID();
}

static bool AnimatedSlider(const char* label, float* value, float minVal, float maxVal, const char* fmt, float dt, float alphaMultiplier) {
    ImGui::PushID(label);
    float sliderW = 250.0f, trackH = 6.0f, knobR = 7.0f;
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);

    dl->AddText(ImVec2(startPos.x, startPos.y), IM_COL32(200, 205, 210, baseA), label);
    char valBuf[32]; snprintf(valBuf, sizeof(valBuf), fmt, *value);
    ImVec2 valSize = ImGui::CalcTextSize(valBuf);
    dl->AddText(ImVec2(startPos.x + sliderW - valSize.x, startPos.y), IM_COL32(140, 155, 170, baseA), valBuf);

    float sliderY = startPos.y + 24;
    ImVec2 trackMin(startPos.x, sliderY), trackMax(startPos.x + sliderW, sliderY + trackH);

    ImGui::SetCursorScreenPos(ImVec2(startPos.x - knobR, sliderY - knobR - 2));
    ImGui::InvisibleButton("##slider", ImVec2(sliderW + knobR * 2, trackH + knobR * 2 + 4));
    bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); // Hand cursor

    if (active) {
        float t = (ImGui::GetIO().MousePos.x - trackMin.x) / sliderW;
        t = t < 0 ? 0 : (t > 1 ? 1 : t);
        *value = minVal + t * (maxVal - minVal);
    }
    float frac = (*value - minVal) / (maxVal - minVal);

    dl->AddRectFilled(trackMin, trackMax, IM_COL32(25, 35, 45, baseA), trackH * 0.5f);
    if (frac > 0.001f) {
        dl->AddRectFilled(trackMin, ImVec2(trackMin.x + sliderW * frac, trackMax.y), IM_COL32(50, 130, 190, baseA), trackH * 0.5f);
        if (active || hovered) dl->AddRectFilled(trackMin, ImVec2(trackMin.x + sliderW * frac, trackMax.y), IM_COL32(80, 170, 230, (int)(40*alphaMultiplier)), trackH * 0.5f);
    }

    float knobX = trackMin.x + sliderW * frac;
    float knobY2 = sliderY + trackH * 0.5f;
    float aKnobR = active ? knobR + 2.0f : (hovered ? knobR + 1.0f : knobR);
    if (active) dl->AddCircleFilled(ImVec2(knobX, knobY2), aKnobR + 5.0f, IM_COL32(50, 130, 190, (int)(30*alphaMultiplier)));
    dl->AddCircleFilled(ImVec2(knobX, knobY2), aKnobR, IM_COL32(235, 240, 248, baseA));
    dl->AddCircle(ImVec2(knobX, knobY2), aKnobR + 0.5f, IM_COL32(0, 0, 0, (int)(25*alphaMultiplier)));

    ImGui::SetCursorScreenPos(ImVec2(startPos.x, sliderY + trackH + knobR + 6));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::PopID();
    return active;
}

static bool StyledCombo(const char* label, int* current, const char* const items[], int count, float alphaMultiplier, int comboIdx = 0) {
    ImGui::PushID(label);
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int baseA = (int)(255 * alphaMultiplier);
    float rowW = 250.0f;
    float rowH = 22.0f;

    // Header row
    ImGui::InvisibleButton("##combobtn", ImVec2(rowW, rowH));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) g_ComboOpen[comboIdx] = !g_ComboOpen[comboIdx];

    // Label on left
    ImU32 textCol = hovered ? IM_COL32(230, 235, 240, baseA) : IM_COL32(200, 205, 210, baseA);
    dl->AddText(ImVec2(startPos.x, startPos.y + 3), textCol, label);

    // Selected text on right
    const char* selectedText = (*current >= 0 && *current < count) ? items[*current] : "---";
    ImVec2 selSize = ImGui::CalcTextSize(selectedText);
    dl->AddText(ImVec2(startPos.x + rowW - selSize.x - 20, startPos.y + 3), IM_COL32(140, 155, 170, baseA), selectedText);

    // Arrow
    float ax = startPos.x + rowW - 8, ay = startPos.y + 11;
    g_ComboOpenAnim[comboIdx] = Lerp(g_ComboOpenAnim[comboIdx], g_ComboOpen[comboIdx] ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime * 14.0f);
    float arrowAng = g_ComboOpenAnim[comboIdx];
    if (arrowAng < 0.5f) {
        dl->AddTriangleFilled(ImVec2(ax - 4, ay - 2), ImVec2(ax + 4, ay - 2), ImVec2(ax, ay + 3), IM_COL32(150, 165, 180, baseA));
    } else {
        dl->AddTriangleFilled(ImVec2(ax - 4, ay + 2), ImVec2(ax + 4, ay + 2), ImVec2(ax, ay - 3), IM_COL32(150, 165, 180, baseA));
    }

    bool changed = false;
    float itemH = 22.0f;
    float totalDropH = count * itemH + 8.0f;
    float currentDropH = totalDropH * EaseOutCubic(g_ComboOpenAnim[comboIdx]);

    if (currentDropH > 0.01f) {
        float dropY = startPos.y + rowH;
        dl->PushClipRect(ImVec2(startPos.x, dropY), ImVec2(startPos.x + rowW, dropY + currentDropH), true);
        
        // Background for options (Optional: gives a subtle indented look)
        dl->AddRectFilled(ImVec2(startPos.x + 8, dropY), ImVec2(startPos.x + rowW - 8, dropY + currentDropH - 4), IM_COL32(15, 20, 25, (int)(100 * alphaMultiplier)), 6.0f);
        
        for (int i = 0; i < count; i++) {
            float itemY = dropY + 4.0f + i * itemH;
            ImGui::SetCursorScreenPos(ImVec2(startPos.x + 10, itemY));
            ImGui::PushID(i);
            ImGui::InvisibleButton("##item", ImVec2(rowW - 20, itemH));
            bool itemHov = ImGui::IsItemHovered();
            if (itemHov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            bool selected = (i == *current);
            if (itemHov) {
                dl->AddRectFilled(ImVec2(startPos.x + 10, itemY), ImVec2(startPos.x + rowW - 10, itemY + itemH), IM_COL32(255, 255, 255, (int)(12 * alphaMultiplier)), 4.0f);
            }
            ImU32 txtCol = selected ? IM_COL32(130, 200, 255, baseA) : IM_COL32(170, 180, 190, baseA);
            
            // Draw bullet or indicator for selection
            if (selected) {
                dl->AddCircleFilled(ImVec2(startPos.x + 22, itemY + itemH * 0.5f), 3.5f, IM_COL32(130, 200, 255, baseA));
                dl->AddCircleFilled(ImVec2(startPos.x + 22, itemY + itemH * 0.5f), 6.5f, IM_COL32(130, 200, 255, (int)(60 * alphaMultiplier)));
            } else {
                dl->AddCircle(ImVec2(startPos.x + 22, itemY + itemH * 0.5f), 3.5f, IM_COL32(100, 110, 120, baseA), 0, 1.5f);
            }
            
            dl->AddText(ImVec2(startPos.x + 36, itemY + 3), txtCol, items[i]);

            if (ImGui::IsItemClicked()) {
                *current = i;
                changed = true;
                // Does NOT automatically close, keeping it expanded as requested
            }
            ImGui::PopID();
        }
        dl->PopClipRect();
    }

    ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + rowH + currentDropH));
    ImGui::Dummy(ImVec2(0, 0));
    ImGui::PopID();
    return changed;
}

// Legacy wrapper for old call sites
static bool AnimatedCombo(const char* label, int* current, const char* const items[], int count, float alphaMultiplier) {
    return StyledCombo(label, current, items, count, alphaMultiplier, 0);
}

// ---- Hooks ----

BOOL WINAPI hk_SetCursorPos(int X, int Y) { if (g_ShowMenu || g_HudEditorMode) return TRUE; return o_SetCursorPos(X, Y); }
BOOL WINAPI hk_GetCursorPos(LPPOINT lpPoint) {
    if ((g_ShowMenu || g_HudEditorMode) && !g_BypassGetCursorPos && g_hWnd) {
        RECT rect; GetClientRect(g_hWnd, &rect);
        POINT pt = { (rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2 };
        ClientToScreen(g_hWnd, &pt);
        lpPoint->x = pt.x; lpPoint->y = pt.y; return TRUE;
    }
    return o_GetCursorPos(lpPoint);
}
BOOL WINAPI hk_ClipCursor(const RECT* lpRect) { if (g_ShowMenu || g_HudEditorMode) return o_ClipCursor(NULL); return o_ClipCursor(lpRect); }

// ── Keybind helpers ──

struct BindEntry { int vk = 0; };
static std::map<std::string, BindEntry> g_ModuleBinds;

static const char* VkName(int vk) {
    switch (vk) {
        case 0x08: return "BkSp"; case 0x09: return "Tab"; case 0x0D: return "Enter";
        case 0x10: return "Shift"; case 0x11: return "Ctrl"; case 0x12: return "Alt";
        case 0x1B: return "Esc"; case 0x20: return "Space";
        case 0x21: return "PgUp"; case 0x22: return "PgDn";
        case 0x23: return "End"; case 0x24: return "Home";
        case 0x25: return "Left"; case 0x26: return "Up"; case 0x27: return "Right"; case 0x28: return "Down";
        case 0x2D: return "Ins"; case 0x2E: return "Del";
        default:
            if (vk >= 0x30 && vk <= 0x39) { static char buf[2]; buf[0] = '0' + (vk - 0x30); buf[1] = 0; return buf; }
            if (vk >= 0x41 && vk <= 0x5A) { static char buf[2]; buf[0] = 'A' + (vk - 0x41); buf[1] = 0; return buf; }
            if (vk >= 0x70 && vk <= 0x7B) { static char buf[8]; snprintf(buf, sizeof(buf), "F%d", vk - 0x70 + 1); return buf; }
            if (vk == 0xA0) return "LShift"; if (vk == 0xA1) return "RShift";
            if (vk == 0xA2) return "LCtrl";  if (vk == 0xA3) return "RCtrl";
            if (vk == 0xA4) return "LAlt";   if (vk == 0xA5) return "RAlt";
            return "?";
    }
}

// Map module names to their ToggleState pointer for keybind dispatch
static ToggleState* GetModToggle(const std::string& name) {
    struct { const char* name; ToggleState* ts; } map[] = {
        {"KillAura",    &g_Toggles[0]},  {"Velocity",     &g_Toggles[1]},
        {"AimAssist",   &g_Toggles[2]},  {"AutoClicker",  &g_Toggles[3]},
        {"AutoSprint",  &g_Toggles[4]},  {"Fly",          &g_Toggles[5]},
        {"SprintReset", &g_Toggles[6]},  {"NoFall",       &g_Toggles[7]},
        {"ESP",         &g_Toggles[8]},  {"TargetHUD",    &g_Toggles[17]},
        {"Speed",       &g_Toggles[18]}, {"Scaffold",     &g_Toggles[19]},
        {"BridgeAssist",&g_Toggles[20]}, {"TriggerBot",   &g_Toggles[21]},
    };
    for (auto& e : map) if (name == e.name) return e.ts;
    return nullptr;
}

static std::string g_BindListening;

static void ModuleKeybindWidget(const std::string& modName, ToggleState* fallbackTs, float dt, float alpha) {
    auto& entry = g_ModuleBinds[modName];
    bool listening = (g_BindListening == modName);
    ImGui::PushID(modName.c_str());

    float fullW = ImGui::GetContentRegionAvail().x;
    if (fullW > 250.0f) fullW = 250.0f;
    ImVec2 pos = ImGui::GetCursorScreenPos();

    char label[64];
    if (listening) snprintf(label, sizeof(label), "Bind: ...");
    else if (entry.vk) snprintf(label, sizeof(label), "Bind: %s", VkName(entry.vk));
    else snprintf(label, sizeof(label), "Bind: None");

    ImGui::InvisibleButton("##bindrow", ImVec2(fullW, 18));
    bool hovered = ImGui::IsItemHovered();
    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsItemClicked()) {
        g_BindListening = listening ? "" : modName;
    }

    ImU32 col = listening ? IM_COL32(100,200,255,(int)(255*alpha))
        : hovered ? IM_COL32(180,190,200,(int)(255*alpha))
        : IM_COL32(140,150,160,(int)(255*alpha));
    ImGui::GetWindowDrawList()->AddText(ImVec2(pos.x, pos.y), col, label);

    if (!listening && g_EditIconTex) {
        float iconSize = 14.0f;
        float textW = ImGui::CalcTextSize(label).x;
        ImVec2 iconPos(pos.x + textW + 6, pos.y + 2);
        ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)g_EditIconTex,
            iconPos, ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
            ImVec2(0,0), ImVec2(1,1),
            IM_COL32(255,255,255,(int)(180*alpha)));
    }
    ImGui::PopID();
}

// Module* overload – uses Module keybind + fallback ToggleState for dispatch
static void ModuleKeybindWidget(Module* mod, float dt, float alpha) {
    (void)mod; (void)dt; (void)alpha; // unused – use string overload
}

void DispatchKeybinds(int vk) {
    if (g_ShowMenu || g_HudEditorMode) return;
    for (auto& [name, entry] : g_ModuleBinds) {
        if (entry.vk == vk) {
            if (auto* ts = GetModToggle(name)) {
                ts->value = !ts->value;
            }
        }
    }
}

LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_KEYDOWN) {
        // Bind listening
        if (!g_BindListening.empty() && g_ShowMenu) {
            if (wParam == VK_ESCAPE) { g_ModuleBinds[g_BindListening].vk = 0; g_BindListening.clear(); }
            else {
                g_ModuleBinds[g_BindListening].vk = (int)wParam;
                g_BindListening.clear();
            }
            return 0;
        }
        DispatchKeybinds((int)wParam);
    }
    if (msg == WM_KEYDOWN && wParam == VK_INSERT) {
        if (g_ShowMenu || g_HudEditorMode) CloseMenu(); else { g_ShowMenu = true; if (o_ClipCursor) o_ClipCursor(NULL); }
    }
    if (msg == WM_KEYDOWN && wParam == VK_ESCAPE && (g_ShowMenu || g_HudEditorMode)) { CloseMenu(); return 0; }
    if (g_ShowMenu || g_HudEditorMode) {
        if (msg == WM_SETCURSOR) {
            // Apply proper ImGui cursor shapes instead of forcing IDC_ARROW
            ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
            LPTSTR win_cursor = IDC_ARROW;
            switch (cursor) {
                case ImGuiMouseCursor_Hand: win_cursor = IDC_HAND; break;
                case ImGuiMouseCursor_TextInput: win_cursor = IDC_IBEAM; break;
                case ImGuiMouseCursor_ResizeNS: win_cursor = IDC_SIZENS; break;
                case ImGuiMouseCursor_ResizeEW: win_cursor = IDC_SIZEWE; break;
                case ImGuiMouseCursor_ResizeNESW: win_cursor = IDC_SIZENESW; break;
                case ImGuiMouseCursor_ResizeNWSE: win_cursor = IDC_SIZENWSE; break;
                case ImGuiMouseCursor_NotAllowed: win_cursor = IDC_NO; break;
                default: win_cursor = IDC_ARROW; break;
            }
            SetCursor(LoadCursor(NULL, win_cursor)); 
            return TRUE; 
        }
        if (msg == WM_ACTIVATE || msg == WM_ACTIVATEAPP || msg == WM_KILLFOCUS) {
            ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam); return 0;
        }
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
        if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) return true;
        if (msg >= WM_KEYFIRST && msg <= WM_KEYLAST) return true; // block keyboard to game
    }
    return CallWindowProc(o_WndProc, hWnd, msg, wParam, lParam);
}

bool WorldToScreen(float x, float y, float z, const float* modelView, const float* projection, int width, int height, float& screenX, float& screenY) {
    float clipX = x * modelView[0] + y * modelView[4] + z * modelView[8] + modelView[12];
    float clipY = x * modelView[1] + y * modelView[5] + z * modelView[9] + modelView[13];
    float clipZ = x * modelView[2] + y * modelView[6] + z * modelView[10] + modelView[14];
    float clipW = x * modelView[3] + y * modelView[7] + z * modelView[11] + modelView[15];

    float projX = clipX * projection[0] + clipY * projection[4] + clipZ * projection[8] + clipW * projection[12];
    float projY = clipX * projection[1] + clipY * projection[5] + clipZ * projection[9] + clipW * projection[13];
    float projZ = clipX * projection[2] + clipY * projection[6] + clipZ * projection[10] + clipW * projection[14];
    float projW = clipX * projection[3] + clipY * projection[7] + clipZ * projection[11] + clipW * projection[15];

    if (projW < 0.1f) return false;

    float ndcX = projX / projW;
    float ndcY = projY / projW;

    screenX = (width / 2.0f) * (ndcX + 1.0f);
    screenY = (height / 2.0f) * (1.0f - ndcY);

    return true;
}

BOOL WINAPI hk_wglSwapBuffers(HDC hDc) {
    HWND currentHwnd = WindowFromDC(hDc);
    if (g_Initialized && currentHwnd != g_hWnd && currentHwnd != NULL) {
        // Window changed (Fullscreen F11 toggle)
        // Completely shutdown ImGui and force a full re-initialization
        if (o_WndProc) {
            SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)o_WndProc);
            o_WndProc = nullptr;
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_Initialized = false;
    }

    if (!g_Initialized) {
        g_hWnd = WindowFromDC(hDc);
        if (g_hWnd) {
            o_WndProc = (WNDPROC)SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            ImFontConfig fontCfg; fontCfg.OversampleH = 3; fontCfg.OversampleV = 3;
            std::string dllDir = GetModuleDirectory(g_hModule);
            std::string fontPath = dllDir + "assets\\Outfit\\static\\Outfit-Bold.ttf";
            ImFont* loaded = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &fontCfg);
            if (!loaded) {
                fontPath = dllDir + "assets\\Outfit\\Outfit-VariableFont_wght.ttf";
                loaded = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &fontCfg);
            }
            if (!loaded) loaded = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &fontCfg);

            g_ToastFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 24.0f, &fontCfg);
            if (!g_ToastFont) g_ToastFont = loaded;

            static const ImWchar icon_ranges[] = { 0xE000, 0xF8FF, 0 };
            ImFontConfig iconCfg; iconCfg.OversampleH = 2; iconCfg.OversampleV = 2;
            g_IconFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segmdl2.ttf", 28.0f, &iconCfg, icon_ranges);

            LoadGLExtensions();
            g_InfoIconTex = LoadTextureFromFile((dllDir + "assets\\info_circle.png").c_str(), &g_InfoIconW, &g_InfoIconH);
            g_PaletteTex = LoadTextureFromFile((dllDir + "assets\\palette.png").c_str(), nullptr, nullptr);
            g_BannerTex = LoadTextureFromFile((dllDir + "assets\\nimbus-banner-1920x721.png").c_str(), &g_BannerW, &g_BannerH);
            g_ReloadIconTex = LoadTextureFromFile((dllDir + "assets\\reload.png").c_str(), nullptr, nullptr);
            g_EditIconTex = LoadTextureFromFile((dllDir + "assets\\edit.png").c_str(), nullptr, nullptr);

            SetupN1mbusStyle();
            ImGui_ImplWin32_Init(g_hWnd);
            ImGui_ImplOpenGL3_Init();
            g_Initialized = true;
        }
    }

    if (g_Initialized) {
        g_BypassGetCursorPos = true;
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        g_BypassGetCursorPos = false;

        ImGuiIO& io = ImGui::GetIO();
        io.MouseDrawCursor = false;
        float dt = io.DeltaTime;
        g_GlobalTime += dt;

        if (g_ShowMenu) {
            InitBlur((int)io.DisplaySize.x, (int)io.DisplaySize.y);
        }

        float targetAlpha = g_ShowMenu ? 1.0f : 0.0f;
        g_MenuAlpha += (targetAlpha - g_MenuAlpha) * (dt * 12.0f);
        if (!g_ShowMenu && g_MenuAlpha < 0.01f) g_MenuAlpha = 0.0f;

        // Menu scale animation (open: 0.92 -> 1.0 with overshoot)
        float targetScale = g_ShowMenu ? 1.0f : 0.92f;
        g_MenuScale += (targetScale - g_MenuScale) * (dt * 10.0f);
        if (!g_ShowMenu && g_MenuScale < 0.93f) g_MenuScale = 0.92f;

        // Tab switching with stagger reset
        if (g_TargetTab != g_CurrentTab) {
            g_TabSlideAnim -= dt * 10.0f;
            if (g_TabSlideAnim <= 0) {
                g_PrevTab = g_CurrentTab;
                g_CurrentTab = g_TargetTab;
                g_TabSlideAnim = 0;
                // Reset widget stagger animations for entrance
                for (int w = 0; w < 20; w++) g_WidgetStagger[w] = 0.0f;
            }
        } else {
            if (g_TabSlideAnim < 1) { g_TabSlideAnim += dt * 8.0f; if (g_TabSlideAnim > 1) g_TabSlideAnim = 1; }
        }
        // Stagger widget entrance
        for (int w = 0; w < 20; w++) {
            float staggerDelay = w * 0.06f; // Each widget delayed by 60ms
            float tabProgress = g_TabSlideAnim;
            float widgetTarget = (tabProgress > staggerDelay) ? 1.0f : 0.0f;
            g_WidgetStagger[w] += (widgetTarget - g_WidgetStagger[w]) * (dt * 10.0f);
        }
        float headerTarget = (g_TabSlideAnim > 0.15f) ? 1.0f : 0.0f;
        g_SectionHeaderAnim = Lerp(g_SectionHeaderAnim, headerTarget, dt * 8.0f);
        g_IndicatorY = Lerp(g_IndicatorY, g_IndicatorTargetY, dt * 14.0f);

        // ---- Module Logic (JNI) ----
        static bool wasFlying = false;
        // Check if JNI is needed
        bool espMaster = g_Toggles[8].value;
        bool espPlayer = espMaster && g_Toggles[10].value;
        bool espHostile = espMaster && g_Toggles[11].value;
        bool espPassive = espMaster && g_Toggles[16].value;
        int espMode = g_ComboSelections[3]; // 0=2D, 1=3D, 2=Chams, 3=Glow
        bool needJni = !g_MCIDFetched || g_Toggles[4].value || g_Toggles[5].value || wasFlying || espPlayer || espHostile || espPassive || g_Toggles[17].value
                       || g_Toggles[0].value || g_Toggles[7].value || g_Toggles[18].value || g_Toggles[19].value
                       || g_Toggles[2].value || g_Toggles[3].value || g_Toggles[6].value || g_Toggles[20].value || g_Toggles[21].value || g_Toggles[1].value;
        if (needJni) {
            JNIEnv* env = JniManager::GetEnv();
            if (!env) return o_wglSwapBuffers(hDc);

            // Resolve mappings dynamically here on the OpenGL thread where ClassLoader is valid!
            static bool s_MappingsResolved = false;
            if (!s_MappingsResolved) {
                MappingResolver::ResolveAll();   // legacy: keeps Mappings:: namespace working
                MC_Register::all();              // register all MC:: requests
                MC::resolve();               // dynamic scan
                s_MappingsResolved = true;
            }

            if (env) {
                jclass mcClass = JniManager::FindClassWithLoader(env, Mappings::Minecraft_Class);
                if (mcClass) {
                    jmethodID getMc = env->GetStaticMethodID(mcClass, Mappings::Minecraft_getMinecraft_Name, Mappings::Minecraft_getMinecraft_Sig);
                    if (getMc) {
                        jobject mcObj = env->CallStaticObjectMethod(mcClass, getMc);
                        if (mcObj) {
                            if (!g_JavaAgentAttempted) {
                                g_JavaAgentAttempted = true;
                                std::string jarPath = GetModuleDirectory(g_hModule) + "n1mbus-agent.jar";
                                if (JniManager::AddJarToContextClassLoader(env, jarPath.c_str())) {
                                    TraceJavaAgent("[Java] n1mbus-agent.jar loaded");
                                } else {
                                    TraceJavaAgent("[Java] n1mbus-agent.jar load failed");
                                }
                                jclass agentClass = JniManager::FindClassWithLoader(env, "n1mbus/N1mbusAgent");
                                if (agentClass) {
                                    jmethodID injectFromMinecraft = env->GetStaticMethodID(agentClass, "injectFromMinecraft", "(Ljava/lang/Object;)V");
                                    if (injectFromMinecraft) {
                                        env->CallStaticVoidMethod(agentClass, injectFromMinecraft, mcObj);
                                        if (!env->ExceptionCheck()) {
                                            TraceJavaAgent("[Java] N1mbusAgent injected");
                                        } else {
                                            env->ExceptionClear();
                                            TraceJavaAgent("[Java] N1mbusAgent call failed");
                                        }
                                    }
                                    env->DeleteLocalRef(agentClass);
                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                } else {
                                    TraceJavaAgent("[Java] N1mbusAgent not found");
                                }
                            }
                            
                            auto fetchSkinId = [&](jobject playerTarget) -> GLuint {
                                GLuint texID = 0;
                                jclass acpClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/entity/AbstractClientPlayer");
                                if (!acpClass) return 0;
                                if (env->IsInstanceOf(playerTarget, acpClass)) {
                                    jmethodID getLoc = MC::methodID(env, acpClass, "AbstractClientPlayer.getLocationSkin");
                                    if (getLoc) {
                                        jobject resLoc = env->CallObjectMethod(playerTarget, getLoc);
                                        if (resLoc) {
                                            jmethodID getTexMgr = MC::methodID(env, mcClass, "Minecraft.getTextureManager");
                                            if (getTexMgr) {
                                                jobject texMgr = env->CallObjectMethod(mcObj, getTexMgr);
                                                if (texMgr) {
                                                    jclass texMgrClass = env->GetObjectClass(texMgr);
                                                    jmethodID getTex = MC::methodID(env, texMgrClass, "TextureManager.getTexture");
                                                    if (getTex) {
                                                        jobject texObj = env->CallObjectMethod(texMgr, getTex, resLoc);
                                                        if (texObj) {
                                                            jclass texObjClass = env->GetObjectClass(texObj);
                                                            jmethodID getGlId = MC::methodID(env, texObjClass, "ITextureObject.getGlTextureId");
                                                            if (getGlId) {
                                                                texID = (GLuint)env->CallIntMethod(texObj, getGlId);
                                                                if (texID != 0) {
                                                                    glBindTexture(GL_TEXTURE_2D, texID);
                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                                                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                                                                    glBindTexture(GL_TEXTURE_2D, 0); // Unbind
                                                                }
                                                            }
                                                            env->DeleteLocalRef(texObjClass);
                                                            env->DeleteLocalRef(texObj);
                                                        }
                                                    }
                                                    env->DeleteLocalRef(texMgrClass);
                                                    env->DeleteLocalRef(texMgr);
                                                }
                                            }
                                            env->DeleteLocalRef(resLoc);
                                        }
                                    }
                                }
                                env->DeleteLocalRef(acpClass);
                                if (env->ExceptionCheck()) env->ExceptionClear();
                                return texID;
                            };

                            if (!g_MCIDFetched) {
                                jmethodID getSession = MC::methodID(env, mcClass, "Minecraft.getSession");
                                if (getSession) {
                                    jobject sessionObj = env->CallObjectMethod(mcObj, getSession);
                                    if (sessionObj) {
                                        jclass sessionClass = env->GetObjectClass(sessionObj);
                                        jmethodID getUsername = MC::methodID(env, sessionClass, "Session.getUsername");
                                        if (getUsername) {
                                            jstring nameStr = (jstring)env->CallObjectMethod(sessionObj, getUsername);
                                            if (nameStr) {
                                                const char* nameChars = env->GetStringUTFChars(nameStr, nullptr);
                                                if (nameChars) {
                                                    g_MCID = nameChars;
                                                    env->ReleaseStringUTFChars(nameStr, nameChars);
                                                    g_MCIDFetched = true;

                                                    if (g_MCID != "Developer" && !g_MCID.empty()) {
                                                        // No external download needed, JNI handles skin fetching.
                                                    }
                                                }
                                            }
                                        }
                                        env->DeleteLocalRef(sessionClass);
                                        env->DeleteLocalRef(sessionObj);
                                    }
                                }
                                if (env->ExceptionCheck()) {
                                    env->ExceptionClear();
                                    g_MCIDFetched = true;
                                }
                            }

                            // Old objectMouseOver target selection removed.
                            // TargetHUD selection is now handled in the ESP loop for extended range crosshair targeting.

                            jfieldID thePlayerField = MC::fieldID(env, mcClass, "Minecraft.thePlayer");
                            if (thePlayerField) {
                                jobject playerObj = env->GetObjectField(mcObj, thePlayerField);
                                if (playerObj) {
                                    g_SkinTexID = fetchSkinId(playerObj);
                                    
                                    jclass playerClass = env->GetObjectClass(playerObj);
                                    
                                    // Update g_IsGuiOpen unconditionally
                                    bool isGuiOpen = false;
                                    jfieldID currentScreenField = MC::fieldID(env, mcClass, "Minecraft.currentScreen");
                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                    if (currentScreenField) {
                                        jobject currentScreenObj = env->GetObjectField(mcObj, currentScreenField);
                                        if (currentScreenObj) {
                                            isGuiOpen = true;
                                            env->DeleteLocalRef(currentScreenObj);
                                        }
                                    }
                                    g_IsGuiOpen = isGuiOpen;

                                    if (g_Toggles[4].value) {
                                        if (GetAsyncKeyState('W') & 0x8000) {
                                            jmethodID setSprinting = MC::methodID(env, playerClass, "Entity.setSprinting");
                                            if (setSprinting) {
                                                env->CallVoidMethod(playerObj, setSprinting, JNI_TRUE);
                                            }
                                        }
                                    }
                                    
                                    // Flight (Index 5)
                                    if (g_Toggles[5].value) {
                                        wasFlying = true;
                                        int mode = g_ComboSelections[1]; // 0: Vanilla, 1: Creative, 2: Glide, 3: Freeze
                                        
                                        // 1. Common Flight Setup for all modes
                                        jfieldID capField = MC::fieldID(env, playerClass, "EntityPlayer.capabilities");
                                        if (capField) {
                                            jobject capObj = env->GetObjectField(playerObj, capField);
                                            if (capObj) {
                                                jclass capClass = env->GetObjectClass(capObj);
                                                jfieldID isFlyingField = MC::fieldID(env, capClass, "PlayerCapabilities.isFlying");
                                                jfieldID allowFlyingField = MC::fieldID(env, capClass, "PlayerCapabilities.allowFlying");
                                                jfieldID flySpeedField = MC::fieldID(env, capClass, "PlayerCapabilities.flySpeed");
                                                
                                                if (allowFlyingField) env->SetBooleanField(capObj, allowFlyingField, JNI_TRUE);
                                                if (isFlyingField) env->SetBooleanField(capObj, isFlyingField, JNI_TRUE);
                                                if (flySpeedField) {
                                                    float speed = g_SliderVals[1] * 0.05f;
                                                    env->SetFloatField(capObj, flySpeedField, speed);
                                                }
                                                
                                                env->DeleteLocalRef(capClass);
                                                env->DeleteLocalRef(capObj);
                                            }
                                        }

                                        // 2. Mode specific modifications
                                        if (mode == 2 || mode == 3) {
                                            jclass entityClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/Entity");
                                            if (entityClass) {
                                                jfieldID motionYField = MC::fieldID(env, entityClass, "Entity.motionY");
                                                jfieldID motionXField = MC::fieldID(env, entityClass, "Entity.motionX");
                                                jfieldID motionZField = MC::fieldID(env, entityClass, "Entity.motionZ");

                                                if (mode == 2) { // Glide
                                                    if (motionYField) {
                                                        // Force gentle downward drift if not explicitly flying up or down
                                                        if (!(GetAsyncKeyState(VK_SPACE) & 0x8000) && !(GetAsyncKeyState(VK_LSHIFT) & 0x8000)) {
                                                            env->SetDoubleField(playerObj, motionYField, -0.05);
                                                        }
                                                    }
                                                } else if (mode == 3) { // Freeze
                                                    bool space = (GetAsyncKeyState(VK_SPACE) & 0x8000);
                                                    bool shift = (GetAsyncKeyState(VK_LSHIFT) & 0x8000);
                                                    bool horiz = (GetAsyncKeyState('W') & 0x8000) || (GetAsyncKeyState('A') & 0x8000) || 
                                                                 (GetAsyncKeyState('S') & 0x8000) || (GetAsyncKeyState('D') & 0x8000);
                                                    
                                                    // Freeze mode specific vertical scaling
                                                    if (motionYField) {
                                                        double verticalSpeed = g_SliderVals[1] * 0.25; // Scale vertical speed
                                                        if (space) {
                                                            env->SetDoubleField(playerObj, motionYField, verticalSpeed);
                                                        } else if (shift) {
                                                            env->SetDoubleField(playerObj, motionYField, -verticalSpeed);
                                                        } else {
                                                            env->SetDoubleField(playerObj, motionYField, 0.0);
                                                        }
                                                    }
                                                    
                                                    // Freeze horizontal momentum if no keys pressed
                                                    if (!horiz) {
                                                        if (motionXField) env->SetDoubleField(playerObj, motionXField, 0.0);
                                                        if (motionZField) env->SetDoubleField(playerObj, motionZField, 0.0);
                                                    }
                                                }
                                                env->DeleteLocalRef(entityClass);
                                            }
                                        }
                                    } else if (wasFlying) {
                                        // Flight toggled OFF -> Reset state
                                        wasFlying = false;
                                        jfieldID capField = MC::fieldID(env, playerClass, "EntityPlayer.capabilities");
                                        if (capField) {
                                            jobject capObj = env->GetObjectField(playerObj, capField);
                                            if (capObj) {
                                                jclass capClass = env->GetObjectClass(capObj);
                                                jfieldID allowFlyingField = MC::fieldID(env, capClass, "PlayerCapabilities.allowFlying");
                                                jfieldID isFlyingField = MC::fieldID(env, capClass, "PlayerCapabilities.isFlying");
                                                jfieldID flySpeedField = MC::fieldID(env, capClass, "PlayerCapabilities.flySpeed");
                                                
                                                if (allowFlyingField) env->SetBooleanField(capObj, allowFlyingField, JNI_FALSE);
                                                if (isFlyingField) env->SetBooleanField(capObj, isFlyingField, JNI_FALSE);
                                                if (flySpeedField) env->SetFloatField(capObj, flySpeedField, 0.05f);
                                                
                                                env->DeleteLocalRef(capClass);
                                                env->DeleteLocalRef(capObj);
                                            }
                                        }
                                    }

                                    // Sync UI state -> modules, then tick all enabled ones
                                    if (g_ModKillAura) {
                                        g_ModKillAura->setEnabled(g_Toggles[0].value);
                                        g_ModKillAura->mode  = g_ComboSelections[0];
                                        g_ModKillAura->reach = g_SliderVals[0];
                                        g_ModKillAura->minCps = (int)g_SliderVals[16];
                                        g_ModKillAura->maxCps = (int)g_SliderVals[17];
                                        g_ModKillAura->fov = g_SliderVals[18];
                                        g_ModKillAura->aimSpeed = g_SliderVals[19];
                                    }
                                    if (g_ModNoFall)   g_ModNoFall->setEnabled(g_Toggles[7].value);
                                    if (g_ModSpeed) {
                                        g_ModSpeed->setEnabled(g_Toggles[18].value);
                                        g_ModSpeed->mode       = g_ComboSelections[4];
                                        g_ModSpeed->multiplier = g_SliderVals[2];
                                    }
                                    if (g_ModScaffold) g_ModScaffold->setEnabled(g_Toggles[19].value);
                                    if (g_ModAimAssist) {
                                        g_ModAimAssist->setEnabled(g_Toggles[2].value);
                                        g_ModAimAssist->speed = g_SliderVals[7];
                                        g_ModAimAssist->fov = g_SliderVals[8];
                                    }
                                    if (g_ModAutoClicker) {
                                        g_ModAutoClicker->setEnabled(g_Toggles[3].value);
                                        g_ModAutoClicker->minCps = (int)g_SliderVals[9];
                                        g_ModAutoClicker->maxCps = (int)g_SliderVals[10];
                                    }
                                    if (g_ModTriggerBot) {
                                        g_ModTriggerBot->setEnabled(g_Toggles[21].value);
                                        g_ModTriggerBot->minCps = (int)g_SliderVals[11];
                                        g_ModTriggerBot->maxCps = (int)g_SliderVals[12];
                                        g_ModTriggerBot->reach = g_SliderVals[13];
                                    }
                                    if (g_ModVelocity) {
                                        g_ModVelocity->setEnabled(g_Toggles[1].value);
                                        g_ModVelocity->horizontal = g_SliderVals[14] / 100.0f;
                                        g_ModVelocity->vertical = g_SliderVals[15] / 100.0f;
                                    }
                                    if (g_ModSprintReset) g_ModSprintReset->setEnabled(g_Toggles[6].value);
                                    if (g_ModBridgeAssist) g_ModBridgeAssist->setEnabled(g_Toggles[20].value);

                                    // Set JNI context for Lua script modules
                                    SetLuaJniContext(env, mcObj, playerObj);
                                    ModuleManager::get().tickAll(env, mcObj, playerObj, playerClass);
                                    ClearLuaJniContext();

                                    // ESP Logic & TargetHUD crosshair tracking
                                    bool needsTargetTracking = g_Toggles[17].value;
                                    if (espPlayer || espHostile || espPassive || needsTargetTracking) {
                                        jclass listClass = JniManager::FindClassWithLoader(env, "java/util/List");
                                        jclass entityClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/Entity");
                                        jclass entityLivingBaseClass = JniManager::FindClassWithLoader(env, "net/minecraft/entity/EntityLivingBase");
                                        if (env->ExceptionCheck()) env->ExceptionClear();

                                        if (!g_IsGuiOpen && listClass && entityClass && entityLivingBaseClass) {
                                            // Get partialTicks
                                            float partialTicks = 1.0f;
                                            jfieldID timerField = MC::fieldID(env, mcClass, "Minecraft.timer");
                                            if (env->ExceptionCheck()) env->ExceptionClear();
                                            if (timerField) {
                                                jobject timerObj = env->GetObjectField(mcObj, timerField);
                                                if (timerObj) {
                                                    jclass timerClass = env->GetObjectClass(timerObj);
                                                    if (timerClass) {
                                                        jfieldID ptField = MC::fieldID(env, timerClass, "Timer.renderPartialTicks");
                                                        if (env->ExceptionCheck()) env->ExceptionClear();
                                                        if (ptField) partialTicks = env->GetFloatField(timerObj, ptField);
                                                        env->DeleteLocalRef(timerClass);
                                                    }
                                                    env->DeleteLocalRef(timerObj);
                                                }
                                            }

                                            // Get viewer position from player's own interpolated position
                                            jfieldID pXField = MC::fieldID(env, entityClass, "Entity.posX");
                                            jfieldID pYField = MC::fieldID(env, entityClass, "Entity.posY");
                                            jfieldID pZField = MC::fieldID(env, entityClass, "Entity.posZ");
                                            jfieldID lpXField = MC::fieldID(env, entityClass, "Entity.lastTickPosX");
                                            jfieldID lpYField = MC::fieldID(env, entityClass, "Entity.lastTickPosY");
                                            jfieldID lpZField = MC::fieldID(env, entityClass, "Entity.lastTickPosZ");
                                            jfieldID hField = MC::fieldID(env, entityClass, "Entity.height");
                                            jfieldID wField = MC::fieldID(env, entityClass, "Entity.width");
                                            if (env->ExceptionCheck()) env->ExceptionClear();

                                            if (pXField && pYField && pZField && lpXField && lpYField && lpZField && hField) {
                                                // Calculate viewer position from local player
                                                double myPosX = env->GetDoubleField(playerObj, pXField);
                                                double myPosY = env->GetDoubleField(playerObj, pYField);
                                                double myPosZ = env->GetDoubleField(playerObj, pZField);
                                                double myLPX = env->GetDoubleField(playerObj, lpXField);
                                                double myLPY = env->GetDoubleField(playerObj, lpYField);
                                                double myLPZ = env->GetDoubleField(playerObj, lpZField);
                                                double viewerX = myLPX + (myPosX - myLPX) * partialTicks;
                                                double viewerY = myLPY + (myPosY - myLPY) * partialTicks;
                                                double viewerZ = myLPZ + (myPosZ - myLPZ) * partialTicks;

                                                // Get MVP matrices from ActiveRenderInfo (3D projection)
                                                float modelView[16];
                                                float projection[16];
                                                bool matricesValid = false;
                                                
                                                jclass renderInfoClass = JniManager::FindClassWithLoader(env, "net/minecraft/client/renderer/ActiveRenderInfo");
                                                if (env->ExceptionCheck()) env->ExceptionClear();
                                                if (renderInfoClass) {
                                                    const char* mvName   = MC::field("ActiveRenderInfo.MODELVIEW");
                                                    const char* projName = MC::field("ActiveRenderInfo.PROJECTION");
                                                    static const char* fbSig = "Ljava/nio/FloatBuffer;";
                                                    
                                                    jfieldID projField = (projName && projName[0]) ? env->GetStaticFieldID(renderInfoClass, projName, fbSig) : nullptr;
                                                    jfieldID mvField   = (mvName   && mvName[0])   ? env->GetStaticFieldID(renderInfoClass, mvName,   fbSig) : nullptr;
                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                    
                                                    if (projField && mvField) {
                                                        jobject projObj = env->GetStaticObjectField(renderInfoClass, projField);
                                                        jobject mvObj   = env->GetStaticObjectField(renderInfoClass, mvField);
                                                        if (env->ExceptionCheck()) env->ExceptionClear();
                                                        
                                                        if (projObj && mvObj) {
                                                            jclass fbConcreteClass = env->GetObjectClass(projObj);
                                                            if (fbConcreteClass) {
                                                                jmethodID getFloat = env->GetMethodID(fbConcreteClass, "get", "(I)F");
                                                                if (getFloat) {
                                                                    bool ok = true;
                                                                    for (int i = 0; i < 16; i++) {
                                                                        projection[i] = env->CallFloatMethod(projObj, getFloat, i);
                                                                        modelView[i]  = env->CallFloatMethod(mvObj,   getFloat, i);
                                                                        if (env->ExceptionCheck()) { env->ExceptionClear(); ok = false; break; }
                                                                    }
                                                                    if (ok) {
                                                                        matricesValid = true;
                                                                        dbg_mv0   = modelView[0];
                                                                        dbg_proj0 = projection[0];
                                                                    }
                                                                }
                                                                env->DeleteLocalRef(fbConcreteClass);
                                                            }
                                                            env->DeleteLocalRef(projObj);
                                                            env->DeleteLocalRef(mvObj);
                                                        }
                                                    }
                                                    env->DeleteLocalRef(renderInfoClass);
                                                }

                                                if (!matricesValid) {
                                                    glGetFloatv(GL_MODELVIEW_MATRIX,  modelView);
                                                    glGetFloatv(GL_PROJECTION_MATRIX, projection);
                                                    dbg_mv0   = modelView[0];
                                                    dbg_proj0 = projection[0];
                                                    if (projection[0] != 0.0f || projection[5] != 0.0f)
                                                        matricesValid = true;
                                                }

                                                dbg_matricesValid = matricesValid;

                                                {
                                                    GLint viewport[4];
                                                    glGetIntegerv(GL_VIEWPORT, viewport);
                                                    int screenW = viewport[2];
                                                    int screenH = viewport[3];

                                                    jfieldID worldField = MC::fieldID(env, mcClass, "Minecraft.theWorld");
                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                    if (worldField) {
                                                        jobject worldObj = env->GetObjectField(mcObj, worldField);
                                                        if (worldObj) {
                                                            jclass worldClass = env->GetObjectClass(worldObj);
                                                            jfieldID listField = MC::fieldID(env, worldClass, "World.loadedEntityList");
                                                            if (env->ExceptionCheck()) env->ExceptionClear();
                                                            if (listField) {
                                                                jobject listObj = env->GetObjectField(worldObj, listField);
                                                                if (listObj) {
                                                                    jclass listConcreteClass = env->GetObjectClass(listObj);
                                                                    jmethodID sizeMeth = env->GetMethodID(listConcreteClass, "size", "()I");
                                                                    jmethodID getMeth  = env->GetMethodID(listConcreteClass, "get",  "(I)Ljava/lang/Object;");
                                                                    env->DeleteLocalRef(listConcreteClass);
                                                                    if (env->ExceptionCheck()) env->ExceptionClear();

                                                                    jclass playerCls = JniManager::FindClassWithLoader(env, "net/minecraft/entity/player/EntityPlayer");
                                                                    jclass mobCls = JniManager::FindClassWithLoader(env, "net/minecraft/entity/monster/EntityMob");
                                                                    jclass animalCls = JniManager::FindClassWithLoader(env, "net/minecraft/entity/passive/EntityAnimal");
                                                                    if (env->ExceptionCheck()) env->ExceptionClear();

                                                                    jmethodID getHealth = MC::methodID(env, entityLivingBaseClass, "EntityLivingBase.getHealth");
                                                                    jmethodID getMaxHealth = MC::methodID(env, entityLivingBaseClass, "EntityLivingBase.getMaxHealth");
                                                                    
                                                                    // Methods for new TargetHUD info
                                                                    jmethodID getTotalArmorValue = MC::methodID(env, playerCls, "EntityPlayer.getTotalArmorValue");
                                                                    jmethodID getHeldItem = MC::methodID(env, entityLivingBaseClass, "EntityLivingBase.getHeldItem");
                                                                    jclass itemStackCls = JniManager::FindClassWithLoader(env, "net/minecraft/item/ItemStack");
                                                                    jmethodID getDisplayName = MC::methodID(env, itemStackCls, "ItemStack.getDisplayName");
                                                                    
                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                    
                                                                    dbg_hasFields = (sizeMeth && getMeth && playerCls && mobCls && animalCls);
                                                                    int drawnCount = 0;
                                                                    int playerCount = 0;

                                                                    if (sizeMeth && getMeth && playerCls && mobCls && animalCls) {
                                                                        int size = env->CallIntMethod(listObj, sizeMeth);
                                                                        dbg_listSize = size;
                ImDrawList* dl = ImGui::GetForegroundDrawList();
                                                                        
                                                                        float bestTargetDist2D = 99999.0f;
                                                                        jobject bestTargetObj = nullptr;
                                                                        float bestTargetDistance3D = 0.0f;

                                                                        for (int i = 0; i < size; i++) {
                                                                            jobject entObj = env->CallObjectMethod(listObj, getMeth, i);
                                                                            if (entObj) {
                                                                                if (!env->IsSameObject(entObj, playerObj)) {
                                                                                    bool isPlayer = env->IsInstanceOf(entObj, playerCls);
                                                                                    bool isHostile = env->IsInstanceOf(entObj, mobCls);
                                                                                    bool isPassive = env->IsInstanceOf(entObj, animalCls);
                                                                                    
                                                                                    if (isPlayer) {
                                                                                        playerCount++;
                                                                                    }
                                                                                    
                                                                                    bool drawESP = (isPlayer && espPlayer) || (isHostile && espHostile) || (isPassive && espPassive);

                                                                                    if (drawESP || (needsTargetTracking && (isPlayer || isHostile || isPassive))) {
                                                                                        double x = env->GetDoubleField(entObj, pXField);
                                                                                        double y = env->GetDoubleField(entObj, pYField);
                                                                                        double z = env->GetDoubleField(entObj, pZField);
                                                                                        double lx = lpXField ? env->GetDoubleField(entObj, lpXField) : x;
                                                                                        double ly = lpYField ? env->GetDoubleField(entObj, lpYField) : y;
                                                                                        double lz = lpZField ? env->GetDoubleField(entObj, lpZField) : z;
                                                                                        float h = hField ? env->GetFloatField(entObj, hField) : 1.8f;
                                                                                        float w = wField ? env->GetFloatField(entObj, wField) : 0.6f;

                                                                                        double interpX = lx + (x - lx) * partialTicks - viewerX;
                                                                                        double interpY = ly + (y - ly) * partialTicks - viewerY;
                                                                                        double interpZ = lz + (z - lz) * partialTicks - viewerZ;
                                                                                        
                                                                                        float dist3D = sqrt((float)(interpX*interpX + interpY*interpY + interpZ*interpZ));
                                                                                        
                                                                                        float sBottomX = 0, sBottomY = 0, sTopX = 0, sTopY = 0;
                                                                                        bool okBottom = WorldToScreen((float)interpX, (float)interpY, (float)interpZ, modelView, projection, screenW, screenH, sBottomX, sBottomY);
                                                                                        bool okTop = WorldToScreen((float)interpX, (float)(interpY + h + 0.1), (float)interpZ, modelView, projection, screenW, screenH, sTopX, sTopY);
                                                                                        
                                                                                        if (needsTargetTracking && dist3D <= g_SliderVals[23] && okBottom && okTop) {
                                                                                            float cx = screenW / 2.0f;
                                                                                            float cy = screenH / 2.0f;
                                                                                            float boxH = sBottomY - sTopY;
                                                                                            if (boxH > 2.0f && boxH < screenH) {
                                                                                                float boxW = boxH * 0.65f;
                                                                                                float left = sTopX - boxW / 2;
                                                                                                float right = sTopX + boxW / 2;
                                                                                                float top = sTopY;
                                                                                                float bottom = sBottomY;
                                                                                                
                                                                                                if (cx >= left - 20 && cx <= right + 20 && cy >= top - 20 && cy <= bottom + 20) {
                                                                                                    float boxCX = (left + right) / 2.0f;
                                                                                                    float boxCY = (top + bottom) / 2.0f;
                                                                                                    float dist2D = sqrt((cx - boxCX)*(cx - boxCX) + (cy - boxCY)*(cy - boxCY));
                                                                                                    if (dist2D < bestTargetDist2D) {
                                                                                                        bestTargetDist2D = dist2D;
                                                                                                        bestTargetDistance3D = dist3D;
                                                                                                        if (bestTargetObj) env->DeleteLocalRef(bestTargetObj);
                                                                                                        bestTargetObj = env->NewLocalRef(entObj);
                                                                                                    }
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                        
                                                                                        if (drawESP) {

                                                                                            bool is3dBox = (espMode == 1) && matricesValid;
                                                                                            bool isChams = (espMode == 2) && matricesValid;
                                                                                            bool isGlow = (espMode == 3) && matricesValid;

                                                                                            ImU32 color = IM_COL32(255, 255, 255, 255);
                                                                                            if (isPlayer) color = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Colors[0][0], g_Colors[0][1], g_Colors[0][2], g_Colors[0][3]));
                                                                                            else if (isHostile) color = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Colors[1][0], g_Colors[1][1], g_Colors[1][2], g_Colors[1][3]));
                                                                                            else if (isPassive) color = ImGui::ColorConvertFloat4ToU32(ImVec4(g_Colors[2][0], g_Colors[2][1], g_Colors[2][2], g_Colors[2][3]));

                                                                                    if (is3dBox) {
                                                                                        float hw = w / 2.0f;
                                                                                        float px[8], py[8];
                                                                                        bool valid = true;
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)interpY, (float)(interpZ - hw), modelView, projection, screenW, screenH, px[0], py[0]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)interpY, (float)(interpZ + hw), modelView, projection, screenW, screenH, px[1], py[1]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)interpY, (float)(interpZ + hw), modelView, projection, screenW, screenH, px[2], py[2]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)interpY, (float)(interpZ - hw), modelView, projection, screenW, screenH, px[3], py[3]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)(interpY + h), (float)(interpZ - hw), modelView, projection, screenW, screenH, px[4], py[4]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)(interpY + h), (float)(interpZ + hw), modelView, projection, screenW, screenH, px[5], py[5]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)(interpY + h), (float)(interpZ + hw), modelView, projection, screenW, screenH, px[6], py[6]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)(interpY + h), (float)(interpZ - hw), modelView, projection, screenW, screenH, px[7], py[7]);
                                                                                        
                                                                                        if (valid) {
                                                                                            auto DrawLine3D = [&](int i, int j) {
                                                                                                dl->AddLine(ImVec2(px[i], py[i]), ImVec2(px[j], py[j]), IM_COL32(0,0,0,180), 3.5f);
                                                                                                dl->AddLine(ImVec2(px[i], py[i]), ImVec2(px[j], py[j]), color, 1.5f);
                                                                                            };
                                                                                            DrawLine3D(0, 1); DrawLine3D(1, 2); DrawLine3D(2, 3); DrawLine3D(3, 0);
                                                                                            DrawLine3D(4, 5); DrawLine3D(5, 6); DrawLine3D(6, 7); DrawLine3D(7, 4);
                                                                                            DrawLine3D(0, 4); DrawLine3D(1, 5); DrawLine3D(2, 6); DrawLine3D(3, 7);
                                                                                            
                                                                                            if (g_Toggles[9].value && getHealth && getMaxHealth && env->IsInstanceOf(entObj, entityLivingBaseClass)) {
                                                                                                float leftX = px[0], topY = py[0], botY = py[0];
                                                                                                for (int i = 1; i < 8; i++) {
                                                                                                    if (px[i] < leftX) leftX = px[i];
                                                                                                    if (py[i] < topY) topY = py[i];
                                                                                                    if (py[i] > botY) botY = py[i];
                                                                                                }
                                                                                                float boxH = botY - topY;
                                                                                                if (boxH > 2.0f) {
                                                                                                    float hp = env->CallFloatMethod(entObj, getHealth);
                                                                                                    float maxHp = env->CallFloatMethod(entObj, getMaxHealth);
                                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                                    float hpPerc = maxHp > 0 ? (hp / maxHp) : 0.0f;
                                                                                                    hpPerc = hpPerc < 0 ? 0 : (hpPerc > 1 ? 1 : hpPerc);
                                                                                                    ImU32 hpCol = IM_COL32((int)(255*(1-hpPerc)), (int)(255*hpPerc), 0, 255);
                                                                                                    float hpHeight = boxH * hpPerc;
                                                                                                    dl->AddRectFilled(ImVec2(leftX - 6, botY - boxH), ImVec2(leftX - 2, botY), IM_COL32(0,0,0,180));
                                                                                                    dl->AddRectFilled(ImVec2(leftX - 5, botY - hpHeight), ImVec2(leftX - 3, botY), hpCol);
                                                                                                }
                                                                                            }
                                                                                            drawnCount++;
                                                                                        }
                                                                                    } else if (isChams || isGlow) {
                                                                                        float hw = w / 2.0f;
                                                                                        float px[8], py[8];
                                                                                        bool valid = true;
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)interpY, (float)(interpZ - hw), modelView, projection, screenW, screenH, px[0], py[0]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)interpY, (float)(interpZ + hw), modelView, projection, screenW, screenH, px[1], py[1]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)interpY, (float)(interpZ + hw), modelView, projection, screenW, screenH, px[2], py[2]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)interpY, (float)(interpZ - hw), modelView, projection, screenW, screenH, px[3], py[3]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)(interpY + h), (float)(interpZ - hw), modelView, projection, screenW, screenH, px[4], py[4]);
                                                                                        valid &= WorldToScreen((float)(interpX - hw), (float)(interpY + h), (float)(interpZ + hw), modelView, projection, screenW, screenH, px[5], py[5]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)(interpY + h), (float)(interpZ + hw), modelView, projection, screenW, screenH, px[6], py[6]);
                                                                                        valid &= WorldToScreen((float)(interpX + hw), (float)(interpY + h), (float)(interpZ - hw), modelView, projection, screenW, screenH, px[7], py[7]);
                                                                                        
                                                                                        if (valid) {
                                                                                            ImVec4 cv = ImGui::ColorConvertU32ToFloat4(color);
                                                                                            int fillAlpha = isChams ? 90 : 50;
                                                                                            ImU32 fillCol = IM_COL32((int)(cv.x*255), (int)(cv.y*255), (int)(cv.z*255), fillAlpha);
                                                                                            
                                                                                            auto FillQuad = [&](int a, int b, int c, int d) {
                                                                                                dl->AddQuadFilled(ImVec2(px[a],py[a]), ImVec2(px[b],py[b]), ImVec2(px[c],py[c]), ImVec2(px[d],py[d]), fillCol);
                                                                                            };
                                                                                            FillQuad(0,1,2,3); FillQuad(4,5,6,7); FillQuad(0,1,5,4);
                                                                                            FillQuad(2,3,7,6); FillQuad(0,3,7,4); FillQuad(1,2,6,5);
                                                                                            
                                                                                            if (isGlow) {
                                                                                                ImU32 glowCol = IM_COL32((int)(cv.x*255), (int)(cv.y*255), (int)(cv.z*255), 35);
                                                                                                auto DrawGlowLine = [&](int i, int j) {
                                                                                                    dl->AddLine(ImVec2(px[i], py[i]), ImVec2(px[j], py[j]), glowCol, 6.0f);
                                                                                                    dl->AddLine(ImVec2(px[i], py[i]), ImVec2(px[j], py[j]), color, 1.5f);
                                                                                                };
                                                                                                DrawGlowLine(0,1); DrawGlowLine(1,2); DrawGlowLine(2,3); DrawGlowLine(3,0);
                                                                                                DrawGlowLine(4,5); DrawGlowLine(5,6); DrawGlowLine(6,7); DrawGlowLine(7,4);
                                                                                                DrawGlowLine(0,4); DrawGlowLine(1,5); DrawGlowLine(2,6); DrawGlowLine(3,7);
                                                                                            } else {
                                                                                                auto DrawLine3D = [&](int i, int j) {
                                                                                                    dl->AddLine(ImVec2(px[i], py[i]), ImVec2(px[j], py[j]), color, 1.5f);
                                                                                                };
                                                                                                DrawLine3D(0,1); DrawLine3D(1,2); DrawLine3D(2,3); DrawLine3D(3,0);
                                                                                                DrawLine3D(4,5); DrawLine3D(5,6); DrawLine3D(6,7); DrawLine3D(7,4);
                                                                                                DrawLine3D(0,4); DrawLine3D(1,5); DrawLine3D(2,6); DrawLine3D(3,7);
                                                                                            }
                                                                                            
                                                                                            if (g_Toggles[9].value && getHealth && getMaxHealth && env->IsInstanceOf(entObj, entityLivingBaseClass)) {
                                                                                                float leftX = px[0], topY = py[0], botY = py[0];
                                                                                                for (int i = 1; i < 8; i++) {
                                                                                                    if (px[i] < leftX) leftX = px[i];
                                                                                                    if (py[i] < topY) topY = py[i];
                                                                                                    if (py[i] > botY) botY = py[i];
                                                                                                }
                                                                                                float boxH = botY - topY;
                                                                                                if (boxH > 2.0f) {
                                                                                                    float hp = env->CallFloatMethod(entObj, getHealth);
                                                                                                    float maxHp = env->CallFloatMethod(entObj, getMaxHealth);
                                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                                    float hpPerc = maxHp > 0 ? (hp / maxHp) : 0.0f;
                                                                                                    hpPerc = hpPerc < 0 ? 0 : (hpPerc > 1 ? 1 : hpPerc);
                                                                                                    ImU32 hpCol = IM_COL32((int)(255*(1-hpPerc)), (int)(255*hpPerc), 0, 255);
                                                                                                    float hpHeight = boxH * hpPerc;
                                                                                                    dl->AddRectFilled(ImVec2(leftX - 6, botY - boxH), ImVec2(leftX - 2, botY), IM_COL32(0,0,0,180));
                                                                                                    dl->AddRectFilled(ImVec2(leftX - 5, botY - hpHeight), ImVec2(leftX - 3, botY), hpCol);
                                                                                                }
                                                                                            }
                                                                                            drawnCount++;
                                                                                        }
                                                                                    } else {
                                                                                        // 2D box – always works since matrices from glGetFloatv
                                                                                        float sBottomX, sBottomY, sTopX, sTopY;
                                                                                        if (WorldToScreen((float)interpX, (float)interpY, (float)interpZ, modelView, projection, screenW, screenH, sBottomX, sBottomY) &&
                                                                                            WorldToScreen((float)interpX, (float)(interpY + h + 0.1), (float)interpZ, modelView, projection, screenW, screenH, sTopX, sTopY)) {

                                                                                            float boxH = sBottomY - sTopY;
                                                                                            if (boxH > 2.0f && boxH < screenH) {
                                                                                                float boxW = boxH * 0.65f;
                                                                                                float left = sTopX - boxW / 2;
                                                                                                float right = sTopX + boxW / 2;
                                                                                                float top = sTopY;
                                                                                                float bottom = sBottomY;

                                                                                                dl->AddRect(ImVec2(left, top), ImVec2(right, bottom), IM_COL32(0,0,0,180), 0.0f, 0, 3.5f);
                                                                                                dl->AddRect(ImVec2(left, top), ImVec2(right, bottom), color, 0.0f, 0, 1.5f);

                                                                                                if (g_Toggles[9].value && getHealth && getMaxHealth && env->IsInstanceOf(entObj, entityLivingBaseClass)) {
                                                                                                    float hp = env->CallFloatMethod(entObj, getHealth);
                                                                                                    float maxHp = env->CallFloatMethod(entObj, getMaxHealth);
                                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                                    float hpPerc = maxHp > 0 ? (hp / maxHp) : 0.0f;
                                                                                                    hpPerc = hpPerc < 0 ? 0 : (hpPerc > 1 ? 1 : hpPerc);
                                                                                                    ImU32 hpCol = IM_COL32((int)(255*(1-hpPerc)), (int)(255*hpPerc), 0, 255);
                                                                                                    float hpHeight = boxH * hpPerc;
                                                                                                    dl->AddRectFilled(ImVec2(left - 6, bottom - boxH), ImVec2(left - 2, bottom), IM_COL32(0,0,0,180));
                                                                                                    dl->AddRectFilled(ImVec2(left - 5, bottom - hpHeight), ImVec2(left - 3, bottom), hpCol);
                                                                                                }
                                                                                                drawnCount++;
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                    } // end drawESP
                                                                                    } // end drawESP || targetTracking
                                                                                } // end !IsSameObject
                                                                                env->DeleteLocalRef(entObj);
                                                                            } // end entObj
                                                                        } // end for loop
                                                                        
                                                                        // -- After loop: resolve bestTarget for TargetHUD --
                                                                        g_HasTarget = false;
                                                                        if (bestTargetObj) {
                                                                            jclass entClass = env->GetObjectClass(bestTargetObj);
                                                                            jmethodID getName = MC::methodID(env, entClass, "Entity.getName");
                                                                            if (getName) {
                                                                                jstring nameStr = (jstring)env->CallObjectMethod(bestTargetObj, getName);
                                                                                if (nameStr) {
                                                                                    const char* nameChars = env->GetStringUTFChars(nameStr, nullptr);
                                                                                    if (nameChars) {
                                                                                        g_TargetName = nameChars;
                                                                                        env->ReleaseStringUTFChars(nameStr, nameChars);
                                                                                        if (!g_TargetName.empty()) {
                                                                                            g_TargetSkinTexID = fetchSkinId(bestTargetObj);
                                                                                        }
                                                                                    }
                                                                                    env->DeleteLocalRef(nameStr);
                                                                                }
                                                                            }
                                                                            if (getHealth && getMaxHealth) {
                                                                                g_TargetHealth    = env->CallFloatMethod(bestTargetObj, getHealth);
                                                                                g_TargetMaxHealth = env->CallFloatMethod(bestTargetObj, getMaxHealth);
                                                                                if (env->ExceptionCheck()) env->ExceptionClear();
                                                                            }
                                                                            g_TargetDistance = bestTargetDistance3D;
                                                                            // Armor
                                                                            if (getTotalArmorValue) {
                                                                                g_TargetArmor = env->CallIntMethod(bestTargetObj, getTotalArmorValue);
                                                                                if (env->ExceptionCheck()) { env->ExceptionClear(); g_TargetArmor = 0; }
                                                                            } else {
                                                                                g_TargetArmor = 0;
                                                                            }
                                                                            // Held item
                                                                            g_TargetHeldItem = "";
                                                                            if (getHeldItem) {
                                                                                jobject heldItemObj = env->CallObjectMethod(bestTargetObj, getHeldItem);
                                                                                if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                if (heldItemObj && getDisplayName) {
                                                                                    jstring dispStr = (jstring)env->CallObjectMethod(heldItemObj, getDisplayName);
                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                    if (dispStr) {
                                                                                        const char* dispChars = env->GetStringUTFChars(dispStr, nullptr);
                                                                                        if (dispChars) {
                                                                                            g_TargetHeldItem = dispChars;
                                                                                            env->ReleaseStringUTFChars(dispStr, dispChars);
                                                                                        }
                                                                                        env->DeleteLocalRef(dispStr);
                                                                                    }
                                                                                    env->DeleteLocalRef(heldItemObj);
                                                                                }
                                                                            }
                                                                             // Ping & Potions
                                                                             g_TargetPing = -1;
                                                                             g_TargetPotionCount = 0;
                                                                             g_TargetIsPlayer = env->IsInstanceOf(bestTargetObj, playerCls) ? true : false;
                                                                             
                                                                             if (g_TargetIsPlayer) {
                                                                                jmethodID getNetHandler = MC::methodID(env, mcClass, "Minecraft.getNetHandler");
                                                                                if (getNetHandler && !g_TargetName.empty()) {
                                                                                    jobject netHandler = env->CallObjectMethod(mcObj, getNetHandler);
                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                    if (netHandler) {
                                                                                        jmethodID getPlayerInfo = MC::methodID(env, env->GetObjectClass(netHandler), "NetHandlerPlayClient.getPlayerInfo");
                                                                                        if (getPlayerInfo) {
                                                                                            jstring pNameStr = env->NewStringUTF(g_TargetName.c_str());
                                                                                            jobject playerInfo = env->CallObjectMethod(netHandler, getPlayerInfo, pNameStr);
                                                                                            if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                            if (playerInfo) {
                                                                                                jmethodID getResponseTime = MC::methodID(env, env->GetObjectClass(playerInfo), "NetworkPlayerInfo.getResponseTime");
                                                                                                if (getResponseTime) {
                                                                                                    g_TargetPing = env->CallIntMethod(playerInfo, getResponseTime);
                                                                                                    if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                                }
                                                                                                env->DeleteLocalRef(playerInfo);
                                                                                            }
                                                                                            env->DeleteLocalRef(pNameStr);
                                                                                        }
                                                                                        env->DeleteLocalRef(netHandler);
                                                                                    }
                                                                                }
                                                                            }
                                                                            
                                                                            jmethodID getPotions = MC::methodID(env, entClass, "EntityLivingBase.getActivePotionEffects");
                                                                            if (getPotions) {
                                                                                jobject potCol = env->CallObjectMethod(bestTargetObj, getPotions);
                                                                                if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                if (potCol) {
                                                                                    jclass colClass = env->FindClass("java/util/Collection");
                                                                                    if (colClass) {
                                                                                        jmethodID sizeMeth = env->GetMethodID(colClass, "size", "()I");
                                                                                        if (sizeMeth) {
                                                                                            g_TargetPotionCount = env->CallIntMethod(potCol, sizeMeth);
                                                                                            if (env->ExceptionCheck()) env->ExceptionClear();
                                                                                        }
                                                                                        env->DeleteLocalRef(colClass);
                                                                                    }
                                                                                    env->DeleteLocalRef(potCol);
                                                                                }
                                                                            }

                                                                            env->DeleteLocalRef(entClass);
                                                                            env->DeleteLocalRef(bestTargetObj);
                                                                            g_HasTarget = true;
                                                                        }
                                                                        if (itemStackCls) env->DeleteLocalRef(itemStackCls);
                                                                    } // end sizeMeth && getMeth check
                                                                    if (playerCls) env->DeleteLocalRef(playerCls);
                                                                if (mobCls) env->DeleteLocalRef(mobCls);
                                                                if (animalCls) env->DeleteLocalRef(animalCls);
                                                                env->DeleteLocalRef(listObj);
                                                                
                                                                dbg_drawnCount = drawnCount;
                                                                dbg_playerCount = playerCount;
                                                            }
                                                        }
                                                        env->DeleteLocalRef(worldClass);
                                                        env->DeleteLocalRef(worldObj);
                                                    }
                                                }
                                                }
                                                }
                                        }
                                        if (listClass) env->DeleteLocalRef(listClass);
                                        if (entityClass) env->DeleteLocalRef(entityClass);
                                        if (entityLivingBaseClass) env->DeleteLocalRef(entityLivingBaseClass);
                                    }
                                    
                                    env->DeleteLocalRef(playerClass);
                                    env->DeleteLocalRef(playerObj);
                                }
                            }
                            env->DeleteLocalRef(mcObj);
                        }
                    }
                    env->DeleteLocalRef(mcClass);
                }
            }
        }

        // ── Module sync (unconditional, outside JNI block) ──
        // Must run every frame so module state stays in sync with toggles,
        // even when needJni is false (e.g., last module just turned off).
        if (g_ModKillAura) {
            g_ModKillAura->setEnabled(g_Toggles[0].value);
            g_ModKillAura->mode  = g_ComboSelections[0];
            g_ModKillAura->reach = g_SliderVals[0];
            g_ModKillAura->minCps = (int)g_SliderVals[16];
            g_ModKillAura->maxCps = (int)g_SliderVals[17];
            g_ModKillAura->fov = g_SliderVals[18];
            g_ModKillAura->aimSpeed = g_SliderVals[19];
        }
        if (g_ModNoFall)   g_ModNoFall->setEnabled(g_Toggles[7].value);
        if (g_ModSpeed) {
            g_ModSpeed->setEnabled(g_Toggles[18].value);
            g_ModSpeed->mode       = g_ComboSelections[4];
            g_ModSpeed->multiplier = g_SliderVals[2];
        }
        if (g_ModScaffold) g_ModScaffold->setEnabled(g_Toggles[19].value);
        if (g_ModAimAssist) {
            g_ModAimAssist->setEnabled(g_Toggles[2].value);
            g_ModAimAssist->speed = g_SliderVals[7];
            g_ModAimAssist->fov = g_SliderVals[8];
        }
        if (g_ModAutoClicker) {
            g_ModAutoClicker->setEnabled(g_Toggles[3].value);
            g_ModAutoClicker->minCps = (int)g_SliderVals[9];
            g_ModAutoClicker->maxCps = (int)g_SliderVals[10];
        }
        if (g_ModTriggerBot) {
            g_ModTriggerBot->setEnabled(g_Toggles[21].value);
            g_ModTriggerBot->minCps = (int)g_SliderVals[11];
            g_ModTriggerBot->maxCps = (int)g_SliderVals[12];
            g_ModTriggerBot->reach = g_SliderVals[13];
        }
        if (g_ModVelocity) {
            g_ModVelocity->setEnabled(g_Toggles[1].value);
            g_ModVelocity->horizontal = g_SliderVals[14] / 100.0f;
            g_ModVelocity->vertical = g_SliderVals[15] / 100.0f;
        }
        if (g_ModSprintReset) g_ModSprintReset->setEnabled(g_Toggles[6].value);
        if (g_ModBridgeAssist) g_ModBridgeAssist->setEnabled(g_Toggles[20].value);

        if (g_ShowMenu && o_ClipCursor) o_ClipCursor(NULL);
        
        if (g_ShowMenu && ImGui::IsKeyPressed(ImGuiKey_Escape)) { CloseMenu(); }

        // Show injection toast on first frame
        if (g_FirstFrame) {
            ShowToast("Finished Loading", 5.0f);
            g_FirstFrame = false;
        }

        // ---- Render Toast Notifications (bottom-right, Vape style) ----
        {
            float screenW = io.DisplaySize.x;
            float screenH = io.DisplaySize.y;
            float margin = 8.0f;
            float bottomPad = 4.0f;
            int aliveCount = 0;

            ImDrawList* fgDl = ImGui::GetForegroundDrawList();

            for (int i = 0; i < g_ToastCount; i++) {
                ToastNotification& t = g_Toasts[i];
                if (!t.active) continue;

                t.timer -= dt;
                float targetSlide = 1.0f;
                if (t.timer <= 0.0f) { t.active = false; continue; }
                if (t.timer > t.duration - 0.35f) {
                    float introT = (t.duration - t.timer) / 0.35f;
                    targetSlide = introT;
                } else if (t.timer < 0.6f) {
                    targetSlide = t.timer / 0.6f;
                }
                t.slideAnim = Lerp(t.slideAnim, targetSlide, dt * 14.0f);
                float eased = EaseOutCubic(t.slideAnim < 0.0f ? 0.0f : (t.slideAnim > 1.0f ? 1.0f : t.slideAnim));

                // Font & Sizes
                ImFont* font = ImGui::GetFont();
                ImFont* tFont = g_ToastFont ? g_ToastFont : font;
                float fontSize = 32.0f; // Big main text
                float hintSizeY = 20.0f; // Big hint text
                ImVec2 msgSize = tFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, t.message);

                // Vape Layout but SCALED UP slightly more
                float iconSize = 38.0f;
                float iconSpacing = 18.0f;
                float padX = 26.0f;
                float padY = 20.0f;
                float textSpacingY = 4.0f;

                bool showHint = (i == 0 && g_FirstFrame == false);
                const char* hint = "Press Insert to open GUI";
                ImVec2 hintSize = font->CalcTextSizeA(hintSizeY, FLT_MAX, 0.0f, hint);

                float textW = msgSize.x;
                if (showHint && hintSize.x > textW) textW = hintSize.x;

                float toastW = padX + iconSize + iconSpacing + textW + padX;
                float toastH = padY + fontSize + (showHint ? textSpacingY + hintSizeY : 0.0f) + padY;

                // Position: bottom-right, slide in from right
                float xPos = screenW - margin - toastW * eased;
                float yPos = screenH - bottomPad - toastH - aliveCount * (toastH + 8.0f);

                ImVec2 p0(xPos, yPos);
                ImVec2 p1(xPos + toastW, yPos + toastH);

                // Initialize Blur if not already
                InitBlur((int)screenW, (int)screenH);

                // Vape Background with Blur (Glass effect)
                fgDl->PushClipRect(p0, p1, true);
                fgDl->AddCallback(RenderBlurCallback, nullptr);
                fgDl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
                fgDl->AddRectFilled(p0, p1, IM_COL32(18, 18, 18, (int)(180 * eased)), 4.0f);
                fgDl->PopClipRect();

                // Ultra-thin subtle border
                fgDl->AddRect(p0, p1, IM_COL32(0, 0, 0, (int)(100 * eased)), 4.0f, 0, 1.0f); 

                // Vape Icon (Centered vertically, white/grey)
                ImU32 iconColor = IM_COL32(230, 230, 230, (int)(255 * eased));
                float iconX = p0.x + padX;
                float iconY = p0.y + (toastH - iconSize) * 0.5f;

                if (g_InfoIconTex != 0) {
                    fgDl->AddImage((ImTextureID)(intptr_t)g_InfoIconTex, ImVec2(iconX, iconY), ImVec2(iconX + iconSize, iconY + iconSize), ImVec2(0,0), ImVec2(1,1), iconColor);
                } else if (g_IconFont) {
                    fgDl->AddText(g_IconFont, iconSize, ImVec2(iconX, iconY), iconColor, (const char*)u8"\xEE\x9C\xBE");
                } else {
                    float cx = iconX + iconSize * 0.5f, cy = iconY + iconSize * 0.5f;
                    fgDl->AddLine(ImVec2(cx - 4, cy), ImVec2(cx - 1, cy + 4), iconColor, 1.5f);
                    fgDl->AddLine(ImVec2(cx - 1, cy + 4), ImVec2(cx + 6, cy - 5), iconColor, 1.5f);
                }

                // Text
                float textX = iconX + iconSize + iconSpacing;
                float textY = p0.y + padY;
                fgDl->AddText(tFont, fontSize, ImVec2(textX, textY), IM_COL32(250, 250, 250, (int)(255 * eased)), t.message);

                if (showHint) {
                    fgDl->AddText(font, hintSizeY, ImVec2(textX, textY + fontSize + textSpacingY), IM_COL32(150, 150, 150, (int)(220 * eased)), hint);
                }

                // Vape Progress Bar (Ultra thin 1.0px green line at bottom)
                float progress = t.timer / t.duration;
                if (progress > 0.0f && progress <= 1.0f) {
                    ImU32 progCol = IM_COL32(40, 220, 80, (int)(255 * eased));
                    fgDl->PushClipRect(p0, p1, true);
                    fgDl->AddRectFilled(ImVec2(p0.x, p1.y - 1.0f), ImVec2(p0.x + toastW * progress, p1.y), progCol, 4.0f, ImDrawFlags_RoundCornersBottomLeft | ImDrawFlags_RoundCornersBottomRight);
                    fgDl->PopClipRect();
                }

                aliveCount++;
            }

            // Compact array: remove dead toasts
            if (g_ToastCount > 0) {
                int write = 0;
                for (int read = 0; read < g_ToastCount; read++) {
                    if (g_Toasts[read].active) {
                        if (write != read) g_Toasts[write] = g_Toasts[read];
                        write++;
                    }
                }
                g_ToastCount = write;
            }
        }

        CURSORINFO ci = { sizeof(CURSORINFO) }; GetCursorInfo(&ci);
        bool inWorld = (ci.flags == 0);

        if (inWorld || g_ShowMenu) {
            ImGui::SetNextWindowPos(ImVec2(20, 20));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.08f, 0.10f, 0.88f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            if (ImGui::Begin("Watermark", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "N I M B U S");
                ImGui::SameLine(); ImGui::TextDisabled(" | %d FPS", (int)io.Framerate);
                RenderToastUI(io.DisplaySize.x, io.DisplaySize.y, dt);
            }
            ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();
        }
        
        // ArrayList rendering (enabled modules list)
        // Single unified path: ImGui window with NoInputs when menu-closed

        // Target HUD Rendering – 4s attack grace period (left-click only)
        // g_TargetAttackTimer resets only on LEFT-CLICK + onCrosshair (real attack).
        // HUD stays visible for 4s after last attack, then fades out.
        // HUD does NOT show just from looking at an entity – must attack first.
        bool hideHUD = g_IsGuiOpen && !g_HudEditorMode;
        float thSpeed = g_SliderVals[3] * 0.3f;
        
        static bool prevTargetHudToggle = false;
        bool curTargetHudToggle = g_Toggles[17].value;
        if (curTargetHudToggle && !prevTargetHudToggle) {
            g_TargetInfoAlpha = 0.0f;
            g_TargetAttackTimer = 999.0f; // start hidden, must attack to show
        }
        prevTargetHudToggle = curTargetHudToggle;
        
        bool onCrosshair = (g_HasTarget && g_TargetDistance < g_SliderVals[23]);
        bool leftClickAttack = onCrosshair && ImGui::GetIO().MouseDown[0];
        
        // Update attack timer: reset only on attack, count up otherwise
        if (leftClickAttack && !g_HudEditorMode) {
            g_TargetAttackTimer = 0.0f;
        } else if (!g_HudEditorMode) {
            g_TargetAttackTimer += dt;
        }
        
        // HUD only shows when attacking or within 4s of last attack
        bool attackActive = (g_TargetAttackTimer < 4.0f);
        bool shouldShowHUD = (g_HudEditorMode || (g_HasTarget && (leftClickAttack || attackActive)));
        
        // Alpha update
        if (curTargetHudToggle && shouldShowHUD && !hideHUD) {
            g_TargetInfoAlpha += dt * thSpeed;
            if (g_TargetInfoAlpha > 1.0f) g_TargetInfoAlpha = 1.0f;
        } else {
            g_TargetInfoAlpha -= dt * (thSpeed * 2.0f);
            if (g_TargetInfoAlpha < 0.0f) { g_TargetInfoAlpha = 0.0f; g_TargetAttackTimer = 0.0f; }
        }

        if (curTargetHudToggle && g_TargetInfoAlpha > 0.01f && !hideHUD) {
            float alphaAnim = g_TargetInfoAlpha;
            float scaleAnim = 1.0f;
            
            if (shouldShowHUD) {
                float bounceAnim = EaseOutElastic(alphaAnim);
                scaleAnim = bounceAnim;
                if (scaleAnim < 0.01f) scaleAnim = 0.01f;
                alphaAnim = alphaAnim < 0.2f ? (alphaAnim / 0.2f) : 1.0f;
            } else {
                float popT = 1.0f - alphaAnim;
                float popCurve = popT * popT * popT;
                scaleAnim = 1.0f + (popCurve * 1.2f);
                alphaAnim = alphaAnim * alphaAnim * alphaAnim;
            }
            
            float thScale = g_SliderVals[4] * scaleAnim;
            if (thScale < 0.01f) thScale = 0.01f;
            
            float width = 300.0f * thScale;
            float height = 145.0f * thScale;
            
            // Adjust position so it expands/shrinks exactly from its center
            float baseScale = g_SliderVals[4];
            float scaleDiff = scaleAnim - 1.0f;
            float centerOffsetX = 300.0f * baseScale * scaleDiff * 0.5f;
            float centerOffsetY = 90.0f * baseScale * scaleDiff * 0.5f;
            
            float startX = io.DisplaySize.x * 0.5f;
            float startY = io.DisplaySize.y * 0.5f;
            
            // ALWAYS update position when scaling so the top-left corner moves to keep the center fixed.
            static bool thDragging = false;
            ImGui::SetNextWindowPos(ImVec2(startX + g_SliderVals[5] - centerOffsetX, startY + g_SliderVals[6] - centerOffsetY), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
            
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alphaAnim);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing;
            if (!g_HudEditorMode) flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
            else if (thDragging) flags |= ImGuiWindowFlags_NoMove;
            
            ImGui::Begin("TargetHUD", nullptr, flags);
            ImVec2 wp = ImGui::GetWindowPos();
            
            // Handle custom resize logic BEFORE drawing anything that might cover the grip
            if (g_HudEditorMode) {
                if (ImGui::IsWindowFocused() && !ImGui::IsMouseDown(0)) {
                    g_SliderVals[5] = wp.x - startX;
                    g_SliderVals[6] = wp.y - startY;
                }
            }
            
            ImDrawList* dl = ImGui::GetWindowDrawList();
            
            dl->AddCallback(RenderBlurCallback, nullptr);
            dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            dl->AddRectFilled(wp, ImVec2(wp.x + width, wp.y + height), IM_COL32(15, 20, 25, (int)(160 * alphaAnim)), 12.0f);
            dl->AddRect(wp, ImVec2(wp.x + width, wp.y + height), IM_COL32(255, 255, 255, (int)(15 * alphaAnim)), 12.0f);
            
            std::string dispName = (g_HudEditorMode && !g_HasTarget) ? "HUD Preview" : g_TargetName;
            float dispHealth = (g_HudEditorMode && !g_HasTarget) ? 20.0f : g_TargetHealth;
            float dispMaxHealth = (g_HudEditorMode && !g_HasTarget) ? 20.0f : g_TargetMaxHealth;
            
            float hpFrac = dispMaxHealth > 0 ? (dispHealth / dispMaxHealth) : 0.0f;
            if (hpFrac < 0) hpFrac = 0; if (hpFrac > 1) hpFrac = 1;
            
            // Icon handling
            float iconS = 64.0f * thScale;
            float iconX = wp.x + 15.0f * thScale;
            float iconY = wp.y + (height - iconS) * 0.5f;
            
            dl->AddRectFilled(ImVec2(iconX, iconY), ImVec2(iconX + iconS, iconY + iconS), IM_COL32(30, 40, 50, (int)(150 * alphaAnim)), 8.0f);
            
            ImFont* hudFont = g_ToastFont ? g_ToastFont : ImGui::GetFont();
            
            if (g_TargetSkinTexID) {
                dl->AddCallback([](const ImDrawList*, const ImDrawCmd*){
                    typedef void (APIENTRY * PFNGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
                    static PFNGLBINDSAMPLERPROC my_glBindSampler = (PFNGLBINDSAMPLERPROC)wglGetProcAddress("glBindSampler");
                    if (my_glBindSampler) my_glBindSampler(0, 0);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                }, nullptr);
                dl->AddImageRounded((void*)(intptr_t)g_TargetSkinTexID, ImVec2(iconX, iconY), ImVec2(iconX + iconS, iconY + iconS), ImVec2(8.0f/64.0f, 8.0f/64.0f), ImVec2(16.0f/64.0f, 16.0f/64.0f), IM_COL32(255,255,255,(int)(255 * alphaAnim)), 8.0f);
                dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
            } else {
                // Fallback initial
                char initial[2] = { dispName.empty() ? '?' : dispName[0], '\0' };
                ImVec2 initSize = hudFont->CalcTextSizeA(36.0f * thScale, FLT_MAX, 0.0f, initial);
                dl->AddText(hudFont, 36.0f * thScale, ImVec2(iconX + (iconS - initSize.x)*0.5f, iconY + (iconS - initSize.y)*0.5f), IM_COL32(200, 210, 220, (int)(255 * alphaAnim)), initial);
            }
            
            // Text offset
            float textX = iconX + iconS + 15.0f * thScale;
            
            float nameFS = 28.0f * thScale;
            float hpFS = 18.0f * thScale;
            float nameY = wp.y + 24.0f * thScale;
            
            char hpText[32]; snprintf(hpText, sizeof(hpText), "%.1f HP", dispHealth);
            ImVec2 hpTextSize = hudFont->CalcTextSizeA(hpFS, FLT_MAX, 0.0f, hpText);
            
            // Clip name text so it doesn't overlap HP
            float maxNameW = (wp.x + width - 15.0f * thScale - hpTextSize.x - 8.0f * thScale) - textX;
            dl->PushClipRect(ImVec2(textX, nameY), ImVec2(textX + maxNameW, nameY + nameFS + 5.0f * thScale), true);
            dl->AddText(hudFont, nameFS, ImVec2(textX, nameY), IM_COL32(230, 240, 250, (int)(255 * alphaAnim)), dispName.c_str());
            dl->PopClipRect();
            
            ImU32 hpColText = IM_COL32((int)(255 * (1.0f - hpFrac) + 80 * hpFrac), (int)(80 * (1.0f - hpFrac) + 255 * hpFrac), 80, (int)(255 * alphaAnim));
            dl->AddText(hudFont, hpFS, ImVec2(wp.x + width - 15.0f * thScale - hpTextSize.x, nameY + (nameFS - hpFS)), hpColText, hpText);
            
            float barX = textX;
            float barY = nameY + nameFS + 8.0f * thScale;
            float barW = width - (textX - wp.x) - 15.0f * thScale;
            float barH = 8.0f * thScale;
            
            dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW, barY + barH), IM_COL32(0, 0, 0, (int)(200 * alphaAnim)), 3.0f);
            ImU32 hpCol = IM_COL32((int)(255 * (1.0f - hpFrac)), (int)(200 * hpFrac), (int)(50 + 50 * hpFrac), (int)(255 * alphaAnim));
            
            static float smoothHpFrac = 0.0f;
            smoothHpFrac = Lerp(smoothHpFrac, hpFrac, ImGui::GetIO().DeltaTime * 10.0f);
            
            if (smoothHpFrac > hpFrac) {
                // Damage flash white/red
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * smoothHpFrac, barY + barH), IM_COL32(255, 100, 100, (int)(200 * alphaAnim)), 3.0f);
            }
            
            if (smoothHpFrac > 0.01f) {
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * hpFrac, barY + barH), hpCol, 3.0f);
                dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barW * hpFrac, barY + barH), (hpCol & 0x00FFFFFF) | ((int)(60 * alphaAnim) << 24), 6.0f);
            }
            
            // Info rows below health bar
            float infoY = barY + barH + 8.0f * thScale;
            float infoFS = 14.0f * thScale;
            
            bool isPreview = (g_HudEditorMode && !g_HasTarget);
            float dispDist = isPreview ? 12.34f : g_TargetDistance;
            int dispArmor = isPreview ? 15 : g_TargetArmor;
            int dispPing = isPreview ? 45 : g_TargetPing;
            int dispPots = isPreview ? 2 : g_TargetPotionCount;
            std::string dispHeld = isPreview ? "Diamond Sword" : (g_TargetHeldItem.empty() ? "None" : g_TargetHeldItem);
            
            // Row 1: Distance | Armor
            char distText[64];
            snprintf(distText, sizeof(distText), "%.2fm", dispDist);
            char armorText[32];
            snprintf(armorText, sizeof(armorText), "Armor: %d", dispArmor);
            dl->AddText(hudFont, infoFS, ImVec2(barX, infoY), IM_COL32(160, 200, 255, (int)(200 * alphaAnim)), distText);
            ImVec2 armorSize = hudFont->CalcTextSizeA(infoFS, FLT_MAX, 0.0f, armorText);
            dl->AddText(hudFont, infoFS, ImVec2(wp.x + width - 15.0f * thScale - armorSize.x, infoY), IM_COL32(200, 220, 255, (int)(200 * alphaAnim)), armorText);
            
            // Row 2: Ping | Potions
            float infoY2 = infoY + infoFS + 4.0f * thScale;
            char pingText[64];
            ImU32 pingCol = IM_COL32(150, 150, 150, (int)(200 * alphaAnim));
            if (dispPing >= 0) {
                snprintf(pingText, sizeof(pingText), "%d ms", dispPing);
                if (dispPing < 100) pingCol = IM_COL32(100, 255, 100, (int)(200 * alphaAnim));
                else if (dispPing < 200) pingCol = IM_COL32(255, 200, 50, (int)(200 * alphaAnim));
                else pingCol = IM_COL32(255, 100, 100, (int)(200 * alphaAnim));
            } else if (g_TargetIsPlayer) {
                snprintf(pingText, sizeof(pingText), "-- ms");
            } else {
                snprintf(pingText, sizeof(pingText), "NPC");
            }
            dl->AddText(hudFont, infoFS, ImVec2(barX, infoY2), pingCol, pingText);
            
            char potText[32];
            snprintf(potText, sizeof(potText), "Buffs: %d", dispPots);
            ImVec2 potSize = hudFont->CalcTextSizeA(infoFS, FLT_MAX, 0.0f, potText);
            dl->AddText(hudFont, infoFS, ImVec2(wp.x + width - 15.0f * thScale - potSize.x, infoY2), IM_COL32(255, 150, 200, (int)(200 * alphaAnim)), potText);
            
            // Row 3: Held Item
            float infoY3 = infoY2 + infoFS + 4.0f * thScale;
            char heldText[128];
            snprintf(heldText, sizeof(heldText), "%s", dispHeld.c_str());
            dl->AddText(hudFont, infoFS, ImVec2(barX, infoY3), IM_COL32(255, 220, 160, (int)(200 * alphaAnim)), heldText);
            
            if (g_HudEditorMode) {
                ImVec2 br = ImVec2(wp.x + width, wp.y + height);
                float gripSize = 18.0f * thScale;
                
                ImVec2 gripMin(br.x - gripSize, br.y - gripSize);
                bool hovered = ImGui::IsMouseHoveringRect(gripMin, br);
                if (hovered && ImGui::IsMouseClicked(0)) thDragging = true;
                if (!ImGui::IsMouseDown(0)) thDragging = false;

                if (thDragging) {
                    float dx = ImGui::GetIO().MouseDelta.x;
                    g_SliderVals[4] += dx / 300.0f;
                    if (g_SliderVals[4] < 0.5f) g_SliderVals[4] = 0.5f;
                    if (g_SliderVals[4] > 3.0f) g_SliderVals[4] = 3.0f;
                }
                if (hovered || thDragging) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                    dl->AddTriangleFilled(ImVec2(br.x - gripSize, br.y), ImVec2(br.x, br.y - gripSize), ImVec2(br.x, br.y), IM_COL32(255, 255, 255, (int)(200 * alphaAnim)));
                } else {
                    dl->AddTriangleFilled(ImVec2(br.x - gripSize, br.y), ImVec2(br.x, br.y - gripSize), ImVec2(br.x, br.y), IM_COL32(255, 255, 255, (int)(80 * alphaAnim)));
                }
            }
            
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
        }

        // ArrayList (Active Modules) Rendering – single ImGui window path
        if (g_Toggles[22].value) {
            if (g_SliderVals[20] < 0.0f) g_SliderVals[20] = io.DisplaySize.x - 150.0f; // Default X
            if (g_SliderVals[22] < 0.5f) g_SliderVals[22] = 1.0f; // Default Scale

            auto& mods = ModuleManager::get().all();
            std::vector<std::string> activeMods;
            for (auto& m : mods) {
                if (m->isEnabled()) {
                    activeMods.push_back(m->getName());
                }
            }

            static bool alDragging = false;
            ImGuiWindowFlags alFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground;
            if (!g_HudEditorMode) alFlags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMove;
            else if (alDragging) alFlags |= ImGuiWindowFlags_NoMove;

            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            
            ImGui::SetNextWindowPos(ImVec2(g_SliderVals[20], g_SliderVals[21]), alDragging ? ImGuiCond_Always : ImGuiCond_Appearing);
            
            if (ImGui::Begin("ArrayList", nullptr, alFlags)) {
                if (g_HudEditorMode && ImGui::IsWindowFocused() && !ImGui::IsMouseDown(0)) {
                    ImVec2 wp = ImGui::GetWindowPos();
                    g_SliderVals[20] = wp.x;
                    g_SliderVals[21] = wp.y;
                }

                float scale = g_SliderVals[22];
                float fontSize = 28.0f * scale;
                float padX = 8.0f * scale;
                float padY = 4.0f * scale;
                float itemH = fontSize + padY * 2.0f;

                ImFont* font = g_ToastFont ? g_ToastFont : ImGui::GetFont();
                std::sort(activeMods.begin(), activeMods.end(), [font, fontSize](const std::string& a, const std::string& b) {
                    return font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, a.c_str()).x > font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, b.c_str()).x;
                });

                if (activeMods.empty() && g_HudEditorMode) {
                    ImGui::SetCursorPos(ImVec2(10, 10));
                    ImGui::TextColored(ImVec4(1, 1, 1, 0.5f), "ArrayList [Empty]");
                    ImGui::Dummy(ImVec2(150, 40));
                } else if (!activeMods.empty()) {
                    float maxWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, activeMods[0].c_str()).x;
                    int n = (int)activeMods.size();
                    float barW = 4.0f * scale;
                    float rounding = 5.0f * scale;  // Smaller rounding for top-left
                    float totalH = (float)n * itemH;
                    // Dummy includes both background area AND bar width
                    float contentW = maxWidth + padX * 2.0f;

                    ImGui::Dummy(ImVec2(contentW + barW, totalH));
                    ImVec2 rMin2 = ImGui::GetItemRectMin();
                    // Background ends at bgRight; bar spans bgRight to barRight
                    float bgRight = rMin2.x + contentW;
                    float barRight = bgRight + barW;
                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    // Pass 1: Per-item backgrounds (stop BEFORE bar area)
                    for (int j = 0; j < n; j++) {
                        float w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, activeMods[j].c_str()).x;
                        float bLeft = rMin2.x + maxWidth - w;
                        float itemTop = rMin2.y + (float)j * itemH;

                        ImDrawFlags bgFlags = ImDrawFlags_RoundCornersNone;
                        if (j == 0) bgFlags |= ImDrawFlags_RoundCornersTopLeft;
                        if (j == n - 1) {
                            bgFlags |= ImDrawFlags_RoundCornersBottomLeft;
                        } else {
                            float nextW = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, activeMods[j+1].c_str()).x;
                            if (w - nextW > rounding) bgFlags |= ImDrawFlags_RoundCornersBottomLeft;
                        }

                        dl->AddRectFilled(
                            ImVec2(bLeft, itemTop), ImVec2(bgRight, itemTop + itemH),
                            IM_COL32(18, 18, 18, 190), rounding, bgFlags);
                    }

                    // Pass 2: Single unified right color bar + Pass 3: Texts (settings-aware)
                    bool rainbowMode = g_Toggles[24].value;
                    int gradientMode = g_ComboSelections[5];
                    float rainbowHue = fmod(g_GlobalTime * (g_SliderVals[24] * 0.3f), 1.0f);
                    
                    // Color bar
                    if (rainbowMode) {
                        float hueBar = std::fmod(g_GlobalTime * 0.3f + (float)(n-1) * 0.025f, 1.0f);
                        float rBar, gBar, bBar;
                        ImGui::ColorConvertHSVtoRGB(hueBar, 0.7f, 1.0f, rBar, gBar, bBar);
                        dl->AddRectFilled(
                            ImVec2(bgRight, rMin2.y), ImVec2(barRight, rMin2.y + totalH),
                            IM_COL32((int)(rBar*255),(int)(gBar*255),(int)(bBar*255),255),
                            rounding, ImDrawFlags_RoundCornersRight);
                    } else {
                        ImVec4& barC1 = *(ImVec4*)g_ArrayListColors[0];
                        dl->AddRectFilled(
                            ImVec2(bgRight, rMin2.y), ImVec2(barRight, rMin2.y + totalH),
                            IM_COL32((int)(barC1.x*255),(int)(barC1.y*255),(int)(barC1.z*255),255),
                            rounding, ImDrawFlags_RoundCornersRight);
                    }
                    
                    // Texts
                    for (int j = 0; j < n; j++) {
                        float w = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, activeMods[j].c_str()).x;
                        float bLeft = rMin2.x + maxWidth - w;
                        float itemTop = rMin2.y + (float)j * itemH;
                        
                        ImU32 txtColor;
                        if (rainbowMode) {
                            float hue = fmod(rainbowHue + (float)j * 0.08f, 1.0f);
                            float r, g, b;
                            ImGui::ColorConvertHSVtoRGB(hue, 0.8f, 1.0f, r, g, b);
                            txtColor = IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),255);
                        } else if (gradientMode == 1 || gradientMode == 2) {
                            float t = (float)j / (std::max)(n - 1, 1);
                            ImVec4& c1 = *(ImVec4*)g_ArrayListColors[0];
                            ImVec4& c2 = *(ImVec4*)g_ArrayListColors[1];
                            ImVec4 bc(c1.x + (c2.x-c1.x)*t, c1.y + (c2.y-c1.y)*t, c1.z + (c2.z-c1.z)*t, 1.0f);
                            txtColor = IM_COL32((int)(bc.x*255),(int)(bc.y*255),(int)(bc.z*255),255);
                        } else {
                            ImVec4& base = *(ImVec4*)g_ArrayListColors[0];
                            txtColor = IM_COL32((int)(base.x*255),(int)(base.y*255),(int)(base.z*255),255);
                        }
                        dl->AddText(font, fontSize, ImVec2(bLeft + padX, itemTop + padY), txtColor, activeMods[j].c_str());
                    }
                }

                if (g_HudEditorMode) {
                    ImVec2 wp = ImGui::GetWindowPos();
                    ImVec2 ws = ImGui::GetWindowSize();
                    ImVec2 br = ImVec2(wp.x + ws.x, wp.y + ws.y);
                    float gripSize = 18.0f;

                    ImVec2 gripMin(br.x - gripSize, br.y - gripSize);
                    bool hovered = ImGui::IsMouseHoveringRect(gripMin, br);
                    if (hovered && ImGui::IsMouseClicked(0)) alDragging = true;
                    if (!ImGui::IsMouseDown(0)) alDragging = false;

                    if (alDragging) {
                        float dx = ImGui::GetIO().MouseDelta.x;
                        g_SliderVals[22] += dx / 200.0f;
                        if (g_SliderVals[22] < 0.5f) g_SliderVals[22] = 0.5f;
                        if (g_SliderVals[22] > 3.0f) g_SliderVals[22] = 3.0f;
                    }
                    if (hovered || alDragging) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(br.x - gripSize, br.y), ImVec2(br.x, br.y - gripSize), ImVec2(br.x, br.y), IM_COL32(255, 255, 255, 200));
                    } else {
                        ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(br.x - gripSize, br.y), ImVec2(br.x, br.y - gripSize), ImVec2(br.x, br.y), IM_COL32(255, 255, 255, 80));
                    }
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }


        if (g_HudEditorMode) {
            // Draw a subtle helper text fixed at the top
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 50.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0.4f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);
            ImGui::Begin("HudEditorTip", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs);
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Drag HUD to move, drag bottom-right corner to resize. (ESC to exit)");
            ImGui::End();
            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
        }

        if (g_MenuAlpha > 0.005f) {
            float screenW = io.DisplaySize.x, screenH = io.DisplaySize.y;
            float menuW = screenW * 0.55f, menuH = screenH * 0.62f;
            menuW = menuW < 750 ? 750 : (menuW > 1200 ? 1200 : menuW);
            menuH = menuH < 500 ? 500 : (menuH > 800 ? 800 : menuH);
            float sidebarW = menuW * 0.26f;
            sidebarW = sidebarW < 180 ? 180 : (sidebarW > 260 ? 260 : sidebarW);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, g_MenuAlpha);

                ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(io.DisplaySize);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.30f * g_MenuAlpha));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
                ImGui::Begin("##DimLayer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs);
                ImGui::End(); ImGui::PopStyleVar(); ImGui::PopStyleColor();

                ImGui::SetNextWindowSize(ImVec2(menuW, menuH), ImGuiCond_Always);
                float scaleEased = EaseOutBack(g_MenuScale);
                float slideY = (1.0f - EaseOutQuint(g_MenuAlpha)) * 40.0f;
                ImVec2 center = ImGui::GetMainViewport()->GetCenter();
                ImGui::SetNextWindowPos(ImVec2(center.x, center.y + slideY), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);

                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::Begin("N1mbusMenu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar)) {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
                    
                    float MAlpha = g_MenuAlpha;

                    dl->AddCallback(RenderBlurCallback, nullptr);
                    dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
                    dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + ws.y), IM_COL32(8, 12, 18, (int)(100 * MAlpha)), 10.0f);

                    UpdateAndDrawParticles(dl, wp, ws.x, ws.y, dt, MAlpha * 0.8f);

                    dl->AddRectFilled(wp, ImVec2(wp.x + sidebarW, wp.y + ws.y), IM_COL32(8, 14, 18, (int)(245 * MAlpha)), 10.0f, ImDrawFlags_RoundCornersLeft);
                    // Gradient fade from sidebar to content
                    float gradW = 35.0f;
                    ImU32 gradL = IM_COL32(8, 14, 18, (int)(200 * MAlpha));
                    ImU32 gradR = IM_COL32(8, 14, 18, 0);
                    dl->AddRectFilledMultiColor(
                        ImVec2(wp.x + sidebarW, wp.y),
                        ImVec2(wp.x + sidebarW + gradW, wp.y + ws.y),
                        gradL, gradR, gradR, gradL
                    );

                    // Banner image at top of sidebar
                    if (g_BannerTex) {
                        float bannerW = sidebarW - 40.0f;
                        float bannerH = bannerW * (float)g_BannerH / (float)g_BannerW;
                        dl->AddImage((void*)(intptr_t)g_BannerTex,
                            ImVec2(wp.x + 20, wp.y + 25),
                            ImVec2(wp.x + 20 + bannerW, wp.y + 25 + bannerH),
                            ImVec2(0,0), ImVec2(1,1),
                            IM_COL32(255,255,255,(int)(255*MAlpha)));
                    } else {
                        ImGui::SetCursorPos(ImVec2(30, 35));
                        ImGui::TextColored(ImVec4(0.95f, 0.95f, 1.0f, MAlpha), "N I M B U S");
                        ImGui::SetCursorPos(ImVec2(30, 58));
                        ImGui::TextColored(ImVec4(0.3f, 0.45f, 0.55f, MAlpha), "Pre Edition");
                    }
                    dl->AddLine(ImVec2(wp.x + 25, wp.y + (g_BannerTex ? 25 + (sidebarW - 40) * g_BannerH / g_BannerW + 15 : 88)), ImVec2(wp.x + sidebarW - 25, wp.y + (g_BannerTex ? 25 + (sidebarW - 40) * g_BannerH / g_BannerW + 15 : 88)), IM_COL32(255, 255, 255, (int)(12 * MAlpha)));

                    const char* tabNames[] = { "Combat", "Movement", "Render", "Misc", "Plugins" };
                    const char* tabIcons[] = { ">>", "~~", "<>", "::", "[]" };
                    float tabStartY = 105.0f, tabH = 50.0f;

                    for (int i = 0; i < 5; i++) {
                        float itemY = tabStartY + i * tabH;
                        ImVec2 tabMin(wp.x + 8, wp.y + itemY), tabMax(wp.x + sidebarW - 8, wp.y + itemY + tabH - 4);
                        ImGui::SetCursorPos(ImVec2(8, itemY));
                        ImGui::InvisibleButton(tabNames[i], ImVec2(sidebarW - 16, tabH - 4));
                        bool hovered = ImGui::IsItemHovered();
                        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                        if (ImGui::IsItemClicked() && g_TargetTab != i) g_TargetTab = i;
                        float targetHover = (hovered || g_TargetTab == i) ? 1.0f : 0.0f;
                        g_TabHoverAnim[i] = Lerp(g_TabHoverAnim[i], targetHover, dt * 14.0f);
                        if (g_TabHoverAnim[i] > 0.01f) dl->AddRectFilled(tabMin, tabMax, IM_COL32(255, 255, 255, (int)(12 * g_TabHoverAnim[i] * MAlpha)), 8.0f);
                        if (g_TargetTab == i) g_IndicatorTargetY = itemY + 10.0f;
                        ImGui::SetCursorPos(ImVec2(25, itemY + 13));
                        ImGui::TextColored(ImVec4(0.3f+0.2f*g_TabHoverAnim[i], 0.5f+0.1f*g_TabHoverAnim[i], 0.65f+0.1f*g_TabHoverAnim[i], MAlpha), "%s", tabIcons[i]);
                        ImGui::SameLine(0, 15); ImGui::SetCursorPosY(itemY + 13);
                        float tb = 0.5f + 0.45f * g_TabHoverAnim[i];
                        ImGui::TextColored(ImVec4(tb, tb, tb + 0.05f, MAlpha), "%s", tabNames[i]);
                    }

                    float pulse = 0.85f + 0.15f * sinf(g_GlobalTime * 3.0f);
                    float indX = wp.x + 3, indY = wp.y + g_IndicatorY, indH = tabH - 24.0f;
                    dl->AddRectFilled(ImVec2(indX, indY), ImVec2(indX + 3, indY + indH), IM_COL32((int)(200*pulse), (int)(220*pulse), (int)(255*pulse), (int)(220 * MAlpha)), 2.0f);
                    dl->AddRectFilled(ImVec2(indX - 3, indY - 2), ImVec2(indX + 8, indY + indH + 2), IM_COL32(150, 200, 255, (int)(35 * pulse * MAlpha)), 4.0f);

                    float bottomY = ws.y - 65.0f;
                    dl->AddLine(ImVec2(wp.x + 25, wp.y + bottomY - 10), ImVec2(wp.x + sidebarW - 25, wp.y + bottomY - 10), IM_COL32(255, 255, 255, (int)(8 * MAlpha)));
                    
                    // User Profile Section
                    float avatarS = 36.0f;
                    float avatarX = wp.x + 25.0f;
                    float avatarY = wp.y + bottomY + 5.0f;
                    
                    // Avatar background / icon
                    ImU32 avatarCol = IM_COL32(40, 120, 200, (int)(255 * MAlpha));
                    dl->AddRectFilled(ImVec2(avatarX, avatarY), ImVec2(avatarX + avatarS, avatarY + avatarS), IM_COL32(25, 35, 45, (int)(255 * MAlpha)), 8.0f);
                    
                    if (g_SkinTexID) {
                        dl->AddCallback([](const ImDrawList*, const ImDrawCmd*){
                            typedef void (APIENTRY * PFNGLBINDSAMPLERPROC) (GLuint unit, GLuint sampler);
                            static PFNGLBINDSAMPLERPROC my_glBindSampler = (PFNGLBINDSAMPLERPROC)wglGetProcAddress("glBindSampler");
                            if (my_glBindSampler) my_glBindSampler(0, 0);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                        }, nullptr);
                        dl->AddImageRounded((void*)(intptr_t)g_SkinTexID, ImVec2(avatarX, avatarY), ImVec2(avatarX + avatarS, avatarY + avatarS), ImVec2(8.0f/64.0f, 8.0f/64.0f), ImVec2(16.0f/64.0f, 16.0f/64.0f), IM_COL32(255,255,255,(int)(255*MAlpha)), 8.0f);
                        dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
                    } else {
                        // Draw initial letter of MCID
                        char initial[2] = { g_MCID.empty() ? '?' : g_MCID[0], '\0' };
                        ImVec2 initSize = ImGui::GetFont()->CalcTextSizeA(20.0f, FLT_MAX, 0.0f, initial);
                        dl->AddText(ImGui::GetFont(), 20.0f, ImVec2(avatarX + (avatarS - initSize.x)*0.5f, avatarY + (avatarS - initSize.y)*0.5f), avatarCol, initial);
                    }
                    
                    // Palette button
                    ImGui::SetCursorPos(ImVec2(sidebarW - 40, bottomY + 8));
                    ImGui::InvisibleButton("PaletteBtn", ImVec2(30, 30));
                    bool palHov = ImGui::IsItemHovered();
                    if (palHov) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip("Open HUD Editor");
                    }
                    if (ImGui::IsItemClicked()) {
                        g_HudEditorMode = true;
                        g_ShowMenu = false;
                    }
                    if (g_PaletteTex) {
                        dl->AddImage((void*)(intptr_t)g_PaletteTex, ImVec2(wp.x + sidebarW - 40, wp.y + bottomY + 8), ImVec2(wp.x + sidebarW - 10, wp.y + bottomY + 38), ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,(int)(255*(palHov?1.0f:0.5f)*MAlpha)));
                    } else {
                        dl->AddRectFilled(ImVec2(wp.x + sidebarW - 40, wp.y + bottomY + 8), ImVec2(wp.x + sidebarW - 10, wp.y + bottomY + 38), IM_COL32(40, 50, 60, (int)(255*MAlpha)), 4.0f);
                        dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(wp.x + sidebarW - 35, wp.y + bottomY + 16), IM_COL32(255,255,255,(int)(255*MAlpha)), "HUD");
                    }
                    
                    // Username texts
                    ImGui::SetCursorPos(ImVec2(avatarX - wp.x + avatarS + 12, bottomY + 8));
                    ImGui::TextColored(ImVec4(0.8f, 0.85f, 0.9f, MAlpha), "%s", g_MCID.c_str());
                    ImGui::SetCursorPos(ImVec2(avatarX - wp.x + avatarS + 12, bottomY + 24));
                    ImGui::TextColored(ImVec4(0.4f, 0.5f, 0.6f, MAlpha), "Premium User");
                    
                    dl->AddLine(ImVec2(wp.x + sidebarW, wp.y + 20), ImVec2(wp.x + sidebarW, wp.y + ws.y - 20), IM_COL32(255, 255, 255, (int)(8 * MAlpha)));

                    // ---- CONTENT ----
                    ImGui::SetCursorPos(ImVec2(sidebarW, 0));
                    ImGui::BeginChild("ContentArea", ImVec2(ws.x - sidebarW, ws.y), false, ImGuiWindowFlags_NoScrollbar);

                    float modAlpha = EaseOutQuint(g_TabSlideAnim);
                    float modOffsetX = (1.0f - modAlpha) * 35.0f;
                    float headerOffsetX = (1.0f - EaseOutQuint(g_SectionHeaderAnim)) * 20.0f;

                    float contentAlpha = MAlpha * modAlpha;

                    ImGui::SetCursorPos(ImVec2(45 + headerOffsetX, 45));
                    const char* sectionNames[] = { "COMBAT", "MOVEMENT", "VISUAL", "UTILITY", "PLUGINS" };
                    ImVec2 headerPos = ImGui::GetCursorScreenPos();
                    ImGui::TextColored(ImVec4(0.35f, 0.50f, 0.60f, contentAlpha), "%s", sectionNames[g_CurrentTab]);
                    ImVec2 headerSize = ImGui::CalcTextSize(sectionNames[g_CurrentTab]);
                    float lineW = headerSize.x * EaseOutQuint(g_SectionHeaderAnim);
                    dl->AddLine(ImVec2(headerPos.x, headerPos.y + headerSize.y + 6), ImVec2(headerPos.x + lineW, headerPos.y + headerSize.y + 6), IM_COL32(50, 130, 190, (int)(100 * g_SectionHeaderAnim * contentAlpha)));

                    ImGui::SetCursorPos(ImVec2(45, 85));
                    ImGui::BeginGroup();

                    // Helper macro for staggered widget animation
                    #define WIDGET_ANIM(idx) \
                        float wA##idx = EaseOutQuint(g_WidgetStagger[idx]); \
                        float wOff##idx = (1.0f - wA##idx) * 20.0f; \
                        float wAlpha##idx = contentAlpha * wA##idx; \
                        ImGui::SetCursorPosX(45 + wOff##idx);

                    #define MODULE_BIND(idx, ename) do { \
                        ImGui::SetCursorPosX(60 + wOff##idx); \
                        ModuleKeybindWidget(ename, GetModToggle(ename), dt, wAlpha##idx * 0.7f); \
                    } while(0)

                    if (g_CurrentTab == 0) {
                        WIDGET_ANIM(0) AnimatedExpandableToggle("KillAura", g_Toggles[0], dt, wAlpha0, &g_ExpandStates[2], &g_ExpandAnims[2]);
                        MODULE_BIND(0, "KillAura");
                        if (g_ExpandAnims[2] > 0.01f) {
                            float ea = wAlpha0 * g_ExpandAnims[2];
                            ImGui::Indent(20.0f);
                            static const char* auraMode[] = { "Single", "Switch", "Multi" };
                            StyledCombo("Mode", &g_ComboSelections[0], auraMode, 3, ea, 0); ImGui::Spacing();
                            AnimatedSlider("Reach", &g_SliderVals[0], 3.0f, 6.0f, "%.1f blocks", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Min CPS", &g_SliderVals[16], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Max CPS", &g_SliderVals[17], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("FOV", &g_SliderVals[18], 10.0f, 360.0f, "%.0f deg", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Aim Speed", &g_SliderVals[19], 1.0f, 10.0f, "%.1f", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        WIDGET_ANIM(1) AnimatedExpandableToggle("Velocity", g_Toggles[1], dt, wAlpha1, &g_ExpandStates[7], &g_ExpandAnims[7]);
                        MODULE_BIND(1, "Velocity");
                        if (g_ExpandAnims[7] > 0.01f) {
                            float ea = wAlpha1 * g_ExpandAnims[7];
                            ImGui::Indent(20.0f);
                            AnimatedSlider("Horizontal", &g_SliderVals[14], 0.0f, 100.0f, "%.0f %%", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Vertical", &g_SliderVals[15], 0.0f, 100.0f, "%.0f %%", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        WIDGET_ANIM(2) AnimatedExpandableToggle("AimAssist", g_Toggles[2], dt, wAlpha2, &g_ExpandStates[5], &g_ExpandAnims[5]);
                        MODULE_BIND(2, "AimAssist");
                        if (g_ExpandAnims[5] > 0.01f) {
                            float ea = wAlpha2 * g_ExpandAnims[5];
                            ImGui::Indent(20.0f);
                            AnimatedSlider("Speed", &g_SliderVals[7], 1.0f, 10.0f, "%.1f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("FOV", &g_SliderVals[8], 10.0f, 360.0f, "%.0f deg", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        WIDGET_ANIM(3) AnimatedExpandableToggle("AutoClicker", g_Toggles[3], dt, wAlpha3, &g_ExpandStates[6], &g_ExpandAnims[6]);
                        MODULE_BIND(3, "AutoClicker");
                        if (g_ExpandAnims[6] > 0.01f) {
                            float ea = wAlpha3 * g_ExpandAnims[6];
                            ImGui::Indent(20.0f);
                            AnimatedSlider("Min CPS", &g_SliderVals[9], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Max CPS", &g_SliderVals[10], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        WIDGET_ANIM(4) AnimatedExpandableToggle("TriggerBot", g_Toggles[21], dt, wAlpha4, &g_ExpandStates[8], &g_ExpandAnims[8]);
                        MODULE_BIND(4, "TriggerBot");
                        if (g_ExpandAnims[8] > 0.01f) {
                            float ea = wAlpha4 * g_ExpandAnims[8];
                            ImGui::Indent(20.0f);
                            AnimatedSlider("Min CPS", &g_SliderVals[11], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Max CPS", &g_SliderVals[12], 1.0f, 25.0f, "%.0f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Reach", &g_SliderVals[13], 3.0f, 6.0f, "%.1f blocks", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        WIDGET_ANIM(5) AnimatedExpandableToggle("TargetHUD", g_Toggles[17], dt, wAlpha5, &g_ExpandStates[3], &g_ExpandAnims[3]);
                        MODULE_BIND(5, "TargetHUD");
                        if (g_ExpandAnims[3] > 0.01f) {
                            float ea = wAlpha5 * g_ExpandAnims[3];
                            ImGui::Indent(20.0f);
                            AnimatedSlider("Fade Speed", &g_SliderVals[3], 2.0f, 25.0f, "%.1f", dt, ea); ImGui::Spacing();
                            AnimatedSlider("Target Range", &g_SliderVals[23], 5.0f, 100.0f, "%.0fm", dt, ea);
                            ImGui::Unindent(20.0f);
                        }
                    } else if (g_CurrentTab == 1) {
                        WIDGET_ANIM(0) AnimatedToggle("AutoSprint", g_Toggles[4], dt, wAlpha0); ImGui::Spacing();

                        WIDGET_ANIM(1) AnimatedExpandableToggle("Flight", g_Toggles[5], dt, wAlpha1, &g_ExpandStates[1], &g_ExpandAnims[1]);
                        if (g_ExpandAnims[1] > 0.01f) {
                            float ea = wAlpha1 * g_ExpandAnims[1];
                            ImGui::Indent(20.0f);
                            static const char* flyMode[] = { "Vanilla", "Creative", "Glide", "Freeze" };
                            StyledCombo("Fly Mode", &g_ComboSelections[1], flyMode, 4, ea, 1); ImGui::Spacing();
                            AnimatedSlider("Speed", &g_SliderVals[1], 1.0f, 5.0f, "%.1f x", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();

                        WIDGET_ANIM(2) AnimatedExpandableToggle("Speed", g_Toggles[18], dt, wAlpha2, &g_ExpandStates[4], &g_ExpandAnims[4]);
                        if (g_ExpandAnims[4] > 0.01f) {
                            float ea = wAlpha2 * g_ExpandAnims[4];
                            ImGui::Indent(20.0f);
                            static const char* speedMode[] = { "Ground", "Boost", "BHop" };
                            StyledCombo("Mode", &g_ComboSelections[4], speedMode, 3, ea, 4); ImGui::Spacing();
                            AnimatedSlider("Multiplier", &g_SliderVals[2], 1.0f, 4.0f, "%.1f x", dt, ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();

                        WIDGET_ANIM(3) AnimatedToggle("SprintReset", g_Toggles[6], dt, wAlpha3); ImGui::Spacing();
                        WIDGET_ANIM(4) AnimatedToggle("NoFall", g_Toggles[7], dt, wAlpha4); ImGui::Spacing();
                        WIDGET_ANIM(5) AnimatedToggle("Scaffold", g_Toggles[19], dt, wAlpha5); ImGui::Spacing();
                        WIDGET_ANIM(6) AnimatedToggle("BridgeAssist", g_Toggles[20], dt, contentAlpha * EaseOutQuint(g_WidgetStagger[6]));
                    } else if (g_CurrentTab == 2) {
                        WIDGET_ANIM(0) AnimatedExpandableToggle("ESP", g_Toggles[8], dt, wAlpha0, &g_ExpandStates[0], &g_ExpandAnims[0]);
                        if (g_ExpandAnims[0] > 0.01f) {
                            float ea = wAlpha0 * g_ExpandAnims[0];
                            ImGui::Indent(20.0f);
                            
                            static const char* espModes[] = { "2D Box", "3D Box" };
                            StyledCombo("Mode", &g_ComboSelections[3], espModes, 2, ea, 3); ImGui::Spacing();
                            
                            AnimatedToggle("Player", g_Toggles[10], dt, ea); MiniColorPicker("##pc", g_Colors[0], ea); ImGui::Spacing();
                            AnimatedToggle("Hostile", g_Toggles[11], dt, ea); MiniColorPicker("##hc", g_Colors[1], ea); ImGui::Spacing();
                            AnimatedToggle("Passive", g_Toggles[16], dt, ea); MiniColorPicker("##ac", g_Colors[2], ea); ImGui::Spacing();
                            AnimatedToggle("Health Bar", g_Toggles[9], dt, ea);
                            
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                        
                        // ArrayList settings (visual module)
                        WIDGET_ANIM(2) AnimatedExpandableToggle("ArrayList", g_Toggles[22], dt, wAlpha2, &g_ExpandStates[9], &g_ExpandAnims[9]);
                        if (g_ExpandAnims[9] > 0.01f) {
                            float ea = wAlpha2 * g_ExpandAnims[9];
                            ImGui::Indent(20.0f);
                            AnimatedToggle("Rainbow Mode", g_Toggles[24], dt, ea); ImGui::Spacing();
                            static const char* gradOpts[] = { "Off", "Horizontal", "Vertical" };
                            StyledCombo("Gradient", &g_ComboSelections[5], gradOpts, 3, ea, 5); ImGui::Spacing();
                            AnimatedSlider("Anim Speed", &g_SliderVals[24], 0.5f, 5.0f, "%.1fx", dt, ea); ImGui::Spacing();
                            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, ea), "Colors"); MiniColorPicker("##alc1", g_ArrayListColors[0], ea); 
                            ImGui::SameLine(0, 4); MiniColorPicker("##alc2", g_ArrayListColors[1], ea); ImGui::Spacing();
                            ImGui::Unindent(20.0f);
                        }
                        ImGui::Spacing();
                    } else if (g_CurrentTab == 3) {
                        WIDGET_ANIM(0) AnimatedToggle("AutoTool", g_Toggles[12], dt, wAlpha0); ImGui::Spacing();
                        WIDGET_ANIM(1) static const char* antibot[] = { "Off", "Basic", "Advanced" };
                        StyledCombo("AntiBot", &g_ComboSelections[2], antibot, 3, wAlpha1, 2); ImGui::Spacing();
                        ImGui::Spacing(); ImGui::Spacing();
                        WIDGET_ANIM(2)
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.12f, 0.12f, wAlpha2));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.50f, 0.18f, 0.18f, wAlpha2));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.60f, 0.22f, 0.22f, wAlpha2));
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, wAlpha2));
                        if (ImGui::Button("Unload Client", ImVec2(250, 48))) {
                            std::thread([]() { N1mbusHook::Uninitialize(); FreeLibraryAndExitThread(g_hModule, 0); }).detach();
                        }
                        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::PopStyleColor(4);
                    } else if (g_CurrentTab == 4) {
                        auto& mods = ModuleManager::get().all();
                        // Only show plugin/script modules (wrapped in PluginModuleAdapter)
                        int idx = 0;
                        for (auto& m : mods) {
                            if (!dynamic_cast<PluginModuleAdapter*>(m.get())) continue;
                            float wa = EaseOutQuint(g_WidgetStagger[idx]);
                            float wOff = (1.0f - wa) * 20.0f;
                            float wAlpha = contentAlpha * wa;
                            ImGui::SetCursorPosX(45 + wOff);
                            AnimatedModuleToggle(m->getName().c_str(), m.get(), dt, wAlpha);
                            ImGui::SetCursorPosX(60 + wOff);
                            ModuleKeybindWidget(m->getName(), GetModToggle(m->getName()), dt, wAlpha * 0.7f);
                            ImGui::Spacing();
                            idx++;
                        }
                        if (idx == 0) {
                            ImGui::SetCursorPosX(45);
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, contentAlpha), "No modules");
                        }
                        ImGui::Spacing(); ImGui::Spacing();
                        ImGui::SetCursorPosX(45);
                        if (g_ReloadIconTex) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.1f));
                            float iconSize = 20.0f;
                            ImVec2 rPos = ImGui::GetCursorScreenPos();
                            if (ImGui::InvisibleButton("##reload", ImVec2(iconSize + 10, iconSize + 10))) {
                                std::string rDir = GetModuleDirectory(g_hModule);
                                // Remove old plugin/script modules (keep built-in)
                                const char* builtin[] = {
                                    "KillAura","AimAssist","AutoClicker","NoFall","Speed",
                                    "Scaffold","SprintReset","BridgeAssist","TriggerBot","Velocity"
                                };
                                std::unordered_set<std::string> bi;
                                for (auto& n : builtin) bi.insert(n);
                                ModuleManager::get().removeIf([&](Module* m) {
                                    return bi.find(m->getName()) == bi.end();
                                });
                                // Reload plugins
                                auto newPlugins = PluginLoader_ReloadAll(rDir + "\\plugins");
                                for (auto* plugin : newPlugins) {
                                    std::vector<PluginModule*> mods;
                                    plugin->GetModules(mods);
                                    for (auto* m : mods)
                                        ModuleManager::get().add<PluginModuleAdapter>(m);
                                }
                                // Reload scripts (same Lua state, old refs stay valid)
                                auto newScripts = g_ScriptEngine->ReloadScripts(rDir + "\\scripts");
                                for (auto* m : newScripts)
                                    ModuleManager::get().add<PluginModuleAdapter>(m);
                            }
                            bool rHov = ImGui::IsItemHovered();
                            ImGui::GetWindowDrawList()->AddImage((void*)(intptr_t)g_ReloadIconTex,
                                rPos, ImVec2(rPos.x + iconSize, rPos.y + iconSize),
                                ImVec2(0,0), ImVec2(1,1),
                                IM_COL32(255,255,255,(int)((rHov ? 255 : 180) * contentAlpha)));
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,contentAlpha*0.8f), "Reload scripts & plugins");
                            ImGui::PopStyleColor(2);
                        }
                    }

                    ImGui::EndGroup();
                    ImGui::EndChild();
                }
                ImGui::End();
                ImGui::PopStyleColor(); // Pop WindowBg
                ImGui::PopStyleVar(3); // WindowPadding, WindowBorderSize, WindowRounding
                ImGui::PopStyleVar();
        }

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        GLint last_polygon_mode[2]; glGetIntegerv(GL_POLYGON_MODE, last_polygon_mode);
        GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
        GLint last_scissor_box[4]; glGetIntegerv(GL_SCISSOR_BOX, last_scissor_box);
        glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
        glOrtho(0, last_viewport[2], last_viewport[3], 0, -1, 1);
        glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glMatrixMode(GL_MODELVIEW); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
        glPopAttrib(); glBindTexture(GL_TEXTURE_2D, last_texture);
        glPolygonMode(GL_FRONT, last_polygon_mode[0]); glPolygonMode(GL_BACK, last_polygon_mode[1]);
        glViewport(last_viewport[0], last_viewport[1], last_viewport[2], last_viewport[3]);
        glScissor(last_scissor_box[0], last_scissor_box[1], last_scissor_box[2], last_scissor_box[3]);
    }
    return o_wglSwapBuffers(hDc);
}

void N1mbusHook::Initialize(HMODULE hModule) {
    g_hModule = hModule;
    JniManager::Initialize();
    if (MH_Initialize() != MH_OK) return;
    HMODULE hOpengl32 = GetModuleHandleA("opengl32.dll");
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    void* p;
    p = (void*)GetProcAddress(hOpengl32, "wglSwapBuffers");
    if (p) { MH_CreateHook(p, (void**)&hk_wglSwapBuffers, reinterpret_cast<void**>(&o_wglSwapBuffers)); MH_EnableHook(p); }
    p = (void*)GetProcAddress(hUser32, "SetCursorPos");
    if (p) { MH_CreateHook(p, (void**)&hk_SetCursorPos, reinterpret_cast<void**>(&o_SetCursorPos)); MH_EnableHook(p); }
    p = (void*)GetProcAddress(hUser32, "GetCursorPos");
    if (p) { MH_CreateHook(p, (void**)&hk_GetCursorPos, reinterpret_cast<void**>(&o_GetCursorPos)); MH_EnableHook(p); }
    p = (void*)GetProcAddress(hUser32, "ClipCursor");
    if (p) { MH_CreateHook(p, (void**)&hk_ClipCursor, reinterpret_cast<void**>(&o_ClipCursor)); MH_EnableHook(p); }

    // Load C++ plugins
    char dirBuf[MAX_PATH];
    GetModuleFileNameA(hModule, dirBuf, MAX_PATH);
    std::string dllDir = dirBuf;
    dllDir = dllDir.substr(0, dllDir.find_last_of("\\/"));

    // Register built-in modules (always, before plugins/scripts)
    if (ModuleManager::get().all().empty()) {
        g_ModNoFall    = ModuleManager::get().add<NoFall>();
        g_ModSpeed     = ModuleManager::get().add<Speed>();
        g_ModKillAura  = ModuleManager::get().add<KillAura>();
        g_ModScaffold  = ModuleManager::get().add<Scaffold>();
        g_ModAimAssist = ModuleManager::get().add<AimAssist>();
        g_ModAutoClicker = ModuleManager::get().add<AutoClicker>();
        g_ModSprintReset = ModuleManager::get().add<SprintReset>();
        g_ModBridgeAssist = ModuleManager::get().add<BridgeAssist>();
        g_ModTriggerBot = ModuleManager::get().add<TriggerBot>();
        g_ModVelocity = ModuleManager::get().add<Velocity>();
    }

    auto plugins = LoadPlugins(dllDir + "\\plugins");
    for (auto* plugin : plugins) {
        char buf[256]; snprintf(buf, sizeof(buf), "[n1mbus] Plugin loaded: %s", plugin->GetName());
        OutputDebugStringA(buf);
        std::vector<PluginModule*> mods;
        plugin->GetModules(mods);
        for (auto* m : mods) {
            ModuleManager::get().add<PluginModuleAdapter>(m);
            char mBuf[256]; snprintf(mBuf, sizeof(mBuf), "[n1mbus]   Module: %s", m->GetName());
            OutputDebugStringA(mBuf);
        }
    }

    // Load Lua scripts
    if (!g_ScriptEngine) {
        g_ScriptEngine = std::make_unique<ScriptEngine>();
        g_ScriptEngine->Initialize();
    }
    {
        auto luaMods = g_ScriptEngine->LoadScripts(dllDir + "\\scripts");
        for (auto* m : luaMods) {
            ModuleManager::get().add<PluginModuleAdapter>(m);
            char buf[256]; snprintf(buf, sizeof(buf), "[n1mbus] Lua script: %s", m->GetName());
            OutputDebugStringA(buf);
        }
    }
}

void N1mbusHook::Uninitialize() {
    MH_DisableHook(MH_ALL_HOOKS); MH_Uninitialize();
    if (g_Initialized) {
        ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
        if (g_hWnd && o_WndProc) SetWindowLongPtr(g_hWnd, GWLP_WNDPROC, (LONG_PTR)o_WndProc);
    }
}
