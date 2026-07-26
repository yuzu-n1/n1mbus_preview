#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <gl/GL.h>
#include <dwmapi.h>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_opengl3.h"
#include "../src_dll/style.hpp"
#include "../src_dll/image_loader.hpp"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "dwmapi.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct PopParticle {
    ImVec2 pos; ImVec2 vel; float radius; float alpha;
};
std::vector<PopParticle> g_PopParticles;

struct TargetCard {
    DWORD pid = 0;
    std::string title, version;
    float appearAnim = 0.0f;
    bool popping = false;
    float popAnim = 0.0f;
    ImVec2 popCenter;
    bool injectTriggered = false;
    bool burstSpawned = false;
};
std::vector<TargetCard> g_Cards;
DWORD g_LastRefreshTick = 0;

std::vector<std::string> g_Logs;
std::string g_DllPath;

float g_MinHover = 0.0f, g_CloseHover = 0.0f;
GLuint g_BannerTex = 0;
int g_BannerW = 0, g_BannerH = 0;

static std::string ToLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return text;
}

static std::string ExeDir() {
    char exePath[MAX_PATH] = {};
    DWORD length = GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return {};
    std::string path = exePath;
    size_t slash = path.find_last_of("\\/");
    if (slash != std::string::npos) path.resize(slash + 1);
    return path;
}

static std::string GetLocalDllPath() { return ExeDir() + "n1mbus_dll.dll"; }

static std::string ExtractVersion(const std::string& title) {
    auto pos = title.find("1.");
    if (pos == std::string::npos) return "";
    std::string ver;
    for (size_t i = pos; i < title.size(); i++) {
        char c = title[i]; if (isdigit((unsigned char)c) || c == '.') ver += c; else break;
    }
    while (!ver.empty() && ver.back() == '.') ver.pop_back();
    return ver;
}

void LogMessage(const std::string& msg) { g_Logs.push_back(msg); }

struct ACCENT_POLICY { int AccentState; int AccentFlags; int GradientColor; int AnimationId; };
struct WINDOWCOMPOSITIONATTRIBDATA { int Attrib; PVOID pvData; SIZE_T cbData; };
using SetWindowCompositionAttributeFn = BOOL(WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

static void EnableSoftTransparency(HWND hwnd) {
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    HMODULE dm = GetModuleHandleA("dwmapi.dll");
    if (dm) {
        auto setAttr = (HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD))
            GetProcAddress(dm, "DwmSetWindowAttribute");
        if (setAttr) {
            BOOL dark = TRUE; setAttr(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            DWORD corner = 2; setAttr(hwnd, 33, &corner, sizeof(corner));
            return;
        }
    }
    HMODULE u32 = GetModuleHandleA("user32.dll");
    auto setWca = u32 ? (SetWindowCompositionAttributeFn)GetProcAddress(u32, "SetWindowCompositionAttribute") : nullptr;
    if (setWca) {
        ACCENT_POLICY p = {}; p.AccentState = 3; p.GradientColor = 0xFF101820;
        WINDOWCOMPOSITIONATTRIBDATA d = {}; d.Attrib = 19; d.pvData = &p; d.cbData = sizeof(p);
        setWca(hwnd, &d); return;
    }
    MARGINS m = { -1 }; DwmExtendFrameIntoClientArea(hwnd, &m);
}

static float Lerp(float a, float b, float t) {
    return a + (b - a) * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
}

struct WindowMatchContext { DWORD pid = 0; std::string title; };

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* ctx = (WindowMatchContext*)lParam;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER)) return TRUE;
    DWORD pid; GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;
    char t[512] = {}; GetWindowTextA(hwnd, t, sizeof(t));
    if (t[0]) ctx->title = t;
    return FALSE;
}

static std::string GetWindowTitleForPid(DWORD pid) {
    WindowMatchContext ctx; ctx.pid = pid;
    EnumWindows(EnumWindowsProc, (LPARAM)&ctx);
    return ctx.title;
}

static bool IsMinecraftProcess(DWORD pid, const std::string& title) {
    // Primary: detect via loaded render modules (LWJGL)
    HANDLE hMod = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    if (hMod != INVALID_HANDLE_VALUE) {
        MODULEENTRY32 me32 = {}; me32.dwSize = sizeof(MODULEENTRY32);
        if (Module32First(hMod, &me32)) {
            do {
                std::string mod = ToLower(me32.szModule);
                if (mod.find("lwjgl") != std::string::npos) {
                    CloseHandle(hMod); return true;
                }
            } while (Module32Next(hMod, &me32));
        }
        CloseHandle(hMod);
    }
    // Fallback: window title check
    std::string c = ToLower(title);
    return c.find("minecraft") != std::string::npos || c.find("lwjgl") != std::string::npos;
}

struct ProcessCandidate { DWORD pid = 0; std::string exeName; std::string title; };

static std::vector<ProcessCandidate> GetCandidates() {
    std::vector<ProcessCandidate> candidates;
    PROCESSENTRY32 pe32 = {}; pe32.dwSize = sizeof(PROCESSENTRY32);
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (h == INVALID_HANDLE_VALUE) return candidates;
    if (Process32First(h, &pe32)) do {
        std::string exe = pe32.szExeFile;
        std::string lowerExe = ToLower(exe);
        if (lowerExe != "java.exe" && lowerExe != "javaw.exe") continue;
        std::string title = GetWindowTitleForPid(pe32.th32ProcessID);
        if (!IsMinecraftProcess(pe32.th32ProcessID, title)) continue;
        candidates.push_back({ pe32.th32ProcessID, exe, title });
    } while (Process32Next(h, &pe32));
    CloseHandle(h);
    std::sort(candidates.begin(), candidates.end(), [](auto& a, auto& b) {
        if (a.title != b.title) return a.title < b.title; return a.pid < b.pid;
    });
    return candidates;
}

static void SpawnBurst(ImVec2 center) {
    for (int i = 0; i < 50; i++) {
        float angle = ((float)rand() / RAND_MAX) * 6.28318f;
        float speed = 120.0f + ((float)rand() / RAND_MAX) * 250.0f;
        g_PopParticles.push_back({ center, ImVec2(cosf(angle)*speed, sinf(angle)*speed), 1.5f+((float)rand()/RAND_MAX)*7.0f, 1.0f });
    }
    for (int i = 0; i < 10; i++) {
        float angle = ((float)rand() / RAND_MAX) * 6.28318f;
        float speed = 60.0f + ((float)rand() / RAND_MAX) * 100.0f;
        g_PopParticles.push_back({ center, ImVec2(cosf(angle)*speed, sinf(angle)*speed), 6.0f+((float)rand()/RAND_MAX)*10.0f, 1.0f });
    }
}

bool InjectDLL(DWORD pid, const std::string& title) {
    LogMessage("[*] Injecting into " + title + " (PID " + std::to_string(pid) + ")");
    if (pid == 0) { LogMessage("[-] Process not found."); return false; }
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) { LogMessage("[-] Failed to open process."); return false; }
    size_t pathLen = g_DllPath.size() + 1;
    void* pAlloc = VirtualAllocEx(hProcess, nullptr, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pAlloc) { LogMessage("[-] Alloc failed."); CloseHandle(hProcess); return false; }
    if (!WriteProcessMemory(hProcess, pAlloc, g_DllPath.c_str(), pathLen, nullptr)) {
        LogMessage("[-] Write failed."); VirtualFreeEx(hProcess, pAlloc, 0, MEM_RELEASE); CloseHandle(hProcess); return false;
    }
    FARPROC pLoadLibraryA = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)pLoadLibraryA, pAlloc, 0, nullptr);
    if (!hThread) { LogMessage("[-] Remote thread failed."); VirtualFreeEx(hProcess, pAlloc, 0, MEM_RELEASE); CloseHandle(hProcess); return false; }
    LogMessage("[+] Injected successfully!");
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, pAlloc, 0, MEM_RELEASE);
    CloseHandle(hThread); CloseHandle(hProcess);
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_NCCALCSIZE: return 0;
    case WM_NCHITTEST: {
        LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT) {
            POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
            // Exclude top-right buttons from drag area
            RECT win; GetClientRect(hwnd, &win);
            if (pt.y >= 8 && pt.y <= 44 && pt.x >= win.right - 80) return HTCLIENT;
            if (pt.y < 60) return HTCAPTION;
        }
        return hit;
    }
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_OWNDC, WindowProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, "N1mbusLoader", NULL };
    RegisterClassEx(&wc);

    int winW = 720, winH = 500;
    HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW | WS_EX_LAYERED, wc.lpszClassName, "N1mbus", WS_POPUP,
        (GetSystemMetrics(SM_CXSCREEN) - winW) / 2, (GetSystemMetrics(SM_CYSCREEN) - winH) / 2,
        winW, winH, NULL, NULL, wc.hInstance, NULL);

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA, 32 };
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    HGLRC hglrc = wglCreateContext(hdc); wglMakeCurrent(hdc, hglrc);
    EnableSoftTransparency(hwnd);
    ShowWindow(hwnd, nCmdShow); UpdateWindow(hwnd);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    g_DllPath = GetLocalDllPath();

    g_BannerTex = LoadTextureFromFile((ExeDir() + "assets\\nimbus-banner-1920x721.png").c_str(), &g_BannerW, &g_BannerH);

    ImFontConfig fcfg; fcfg.OversampleH = 3; fcfg.OversampleV = 3;
    std::string fpath = ExeDir() + "assets\\Outfit\\static\\Outfit-Bold.ttf";
    if (!io.Fonts->AddFontFromFileTTF(fpath.c_str(), 14.0f, &fcfg)) {
        fpath = ExeDir() + "assets\\Outfit\\Outfit-VariableFont_wght.ttf";
        if (!io.Fonts->AddFontFromFileTTF(fpath.c_str(), 14.0f, &fcfg))
            io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 14.0f, &fcfg);
    }

    SetupN1mbusStyle();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0, 0);
    style.FrameRounding = 4.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init();

    bool done = false;

    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg); DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        DWORD nowTick = GetTickCount();
        if (nowTick - g_LastRefreshTick > 2000) {
            auto candidates = GetCandidates();
            for (auto& card : g_Cards) {
                if (card.popping) continue;
                auto it = std::find_if(candidates.begin(), candidates.end(),
                    [&card](auto& c) { return c.pid == card.pid; });
                if (it == candidates.end()) card.popping = true;
            }
            for (auto& cand : candidates) {
                auto it = std::find_if(g_Cards.begin(), g_Cards.end(),
                    [&cand](auto& c) { return c.pid == cand.pid && !c.popping; });
                if (it == g_Cards.end()) {
                    TargetCard card;
                    card.pid = cand.pid;
                    card.title = cand.title.empty() ? cand.exeName : cand.title;
                    card.version = ExtractVersion(cand.title);
                    if (card.version.empty()) card.version = ExtractVersion(cand.exeName);
                    g_Cards.push_back(card);
                }
            }
            g_LastRefreshTick = nowTick;
        }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
        float dt = ImGui::GetIO().DeltaTime;

        for (auto& card : g_Cards) {
            if (card.popping) {
                card.popAnim = Lerp(card.popAnim, 1.0f, dt * 5.0f);
                if (card.injectTriggered && card.popAnim > 0.4f) {
                    card.injectTriggered = false;
                    InjectDLL(card.pid, card.title);
                }
            } else {
                card.appearAnim = Lerp(card.appearAnim, 1.0f, dt * 4.0f);
            }
        }
        g_Cards.erase(std::remove_if(g_Cards.begin(), g_Cards.end(),
            [](auto& c) { return c.popping && c.popAnim > 0.95f; }), g_Cards.end());

        for (auto& p : g_PopParticles) {
            p.pos.x += p.vel.x * dt; p.pos.y += p.vel.y * dt;
            p.vel.x *= 0.94f; p.vel.y *= 0.94f;
            p.alpha = Lerp(p.alpha, 0.0f, dt * 2.0f);
        }
        g_PopParticles.erase(std::remove_if(g_PopParticles.begin(), g_PopParticles.end(),
            [](auto& p) { return p.alpha < 0.01f; }), g_PopParticles.end());

        // Decay hover when not in button block
        g_MinHover = Lerp(g_MinHover, 0.0f, dt * 6.0f);
        g_CloseHover = Lerp(g_CloseHover, 0.0f, dt * 6.0f);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetWindowPos();
        ImVec2 s = ImGui::GetWindowSize();

        // Background
        dl->AddRectFilledMultiColor(p, ImVec2(p.x + s.x, p.y + s.y),
            IM_COL32(10, 24, 32, 255), IM_COL32(18, 40, 50, 255),
            IM_COL32(8, 18, 24, 255), IM_COL32(10, 22, 30, 255));

        // Window border
        float ei = 3.0f;
        dl->AddRect(ImVec2(p.x+ei, p.y+ei), ImVec2(p.x+s.x-ei, p.y+s.y-ei),
            IM_COL32(50, 140, 200, 100), 8.0f, 0, 2.0f);
        dl->AddRect(ImVec2(p.x+ei, p.y+ei), ImVec2(p.x+s.x-ei, p.y+s.y-ei),
            IM_COL32(100, 200, 255, 35), 8.0f, 0, 5.0f);

        // Info text (above banner)
        const char* info = "N1mbus Injector - Launch Minecraft Java Edition to begin";
        ImVec2 infoSz = ImGui::CalcTextSize(info);
        dl->AddText(ImVec2(p.x+(s.x-infoSz.x)*0.5f, p.y+45.0f), IM_COL32(70,110,140,130), info);

        // Banner
        float bannerBottom = 4.0f;
        if (g_BannerTex) {
            float bw = (float)g_BannerW, bh = (float)g_BannerH;
            if (bw > 280.0f) { bh *= 280.0f / bw; bw = 280.0f; }
            if (bh > 95.0f) { bw *= 95.0f / bh; bh = 95.0f; }
            ImGui::SetCursorPos(ImVec2((s.x - bw) * 0.5f, 62.0f));
            ImGui::Image((ImTextureID)(uint64_t)g_BannerTex, ImVec2(bw, bh));
            bannerBottom = 62.0f + bh + 16.0f;
        }

        // Custom window buttons (top-right)
        float bSz = 28.0f, bGap = 6.0f;
        float btnX = s.x - 14.0f;
        // Close
        ImGui::SetCursorScreenPos(ImVec2(p.x + btnX - bSz, p.y + 12.0f));
        ImVec2 cPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##close", ImVec2(bSz, bSz));
        g_CloseHover = ImGui::IsItemHovered() ? Lerp(g_CloseHover, 1.0f, dt * 20.0f) : Lerp(g_CloseHover, 0.0f, dt * 6.0f);
        if (ImGui::IsItemClicked()) PostMessage(hwnd, WM_CLOSE, 0, 0);
        dl->AddRectFilled(cPos, ImVec2(cPos.x+bSz, cPos.y+bSz), IM_COL32(190,45,45,(int)(70+120*g_CloseHover)), 6.0f);
        dl->AddLine(ImVec2(cPos.x+8,cPos.y+8), ImVec2(cPos.x+bSz-8,cPos.y+bSz-8), IM_COL32(220,220,230,180), 2.0f);
        dl->AddLine(ImVec2(cPos.x+bSz-8,cPos.y+8), ImVec2(cPos.x+8,cPos.y+bSz-8), IM_COL32(220,220,230,180), 2.0f);
        // Minimize
        ImGui::SetCursorScreenPos(ImVec2(p.x + btnX - bSz*2 - bGap, p.y + 12.0f));
        ImVec2 mPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##min", ImVec2(bSz, bSz));
        g_MinHover = ImGui::IsItemHovered() ? Lerp(g_MinHover, 1.0f, dt * 20.0f) : Lerp(g_MinHover, 0.0f, dt * 6.0f);
        if (ImGui::IsItemClicked()) ShowWindow(hwnd, SW_MINIMIZE);
        dl->AddRectFilled(mPos, ImVec2(mPos.x+bSz, mPos.y+bSz), IM_COL32(0,0,0,(int)(50+80*g_MinHover)), 6.0f);
        dl->AddLine(ImVec2(mPos.x+8, mPos.y+bSz*0.5f), ImVec2(mPos.x+bSz-8, mPos.y+bSz*0.5f), IM_COL32(180,190,200,170), 2.0f);

        // Target cards
        float cardW = 310.0f, cardH = 70.0f, cardGap = 10.0f;
        float totalH = g_Cards.size() * cardH + (g_Cards.size() - 1) * cardGap;
        float startY = bannerBottom + (s.y - bannerBottom - totalH) * 0.5f;
        if (startY < bannerBottom + 6.0f) startY = bannerBottom + 6.0f;
        float baseX = p.x + (s.x - cardW) * 0.5f;

        for (size_t i = 0; i < g_Cards.size(); i++) {
            auto& card = g_Cards[i];
            float cy = p.y + startY + i * (cardH + cardGap);
            float scale, alpha;

            if (card.popping) {
                float t = card.popAnim;
                if (t < 0.3f) { scale = 1.0f + (t / 0.3f) * 0.4f; alpha = 1.0f; }
                else { scale = 1.4f; alpha = 1.0f - (t - 0.3f) / 0.4f; }
                if (!card.burstSpawned && t > 0.2f) {
                    card.burstSpawned = true;
                    SpawnBurst(ImVec2(baseX + cardW * 0.5f, cy + cardH * 0.5f));
                }
            } else {
                scale = card.appearAnim; alpha = card.appearAnim;
            }

            if (alpha < 0.01f || scale < 0.01f) continue;

            float w = cardW * scale, h = cardH * scale;
            float x = baseX + (cardW - w) * 0.5f, y = cy + (cardH - h) * 0.5f;

            dl->AddRectFilled(ImVec2(x, y), ImVec2(x+w, y+h), IM_COL32(20,42,56,(int)(210*alpha)), 8.0f);
            dl->AddRect(ImVec2(x, y), ImVec2(x+w, y+h), IM_COL32(60,150,210,(int)(90*alpha)), 8.0f, 0, 1.5f);

            std::string label = card.title + (card.version.empty() ? "" : "  " + card.version);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize()*scale, ImVec2(x+12*scale, y+8*scale),
                IM_COL32(200,220,240,(int)(255*alpha)), label.c_str());
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize()*0.85f*scale,
                ImVec2(x+12*scale, y+(8+ImGui::GetFontSize()+4)*scale),
                IM_COL32(120,155,180,(int)(200*alpha)), ("PID: "+std::to_string(card.pid)).c_str());

            if (!card.popping) {
                ImGui::SetCursorScreenPos(ImVec2(x, y));
                ImGui::InvisibleButton(("##c"+std::to_string(card.pid)).c_str(), ImVec2(w, h));
                if (ImGui::IsItemClicked()) {
                    card.popping = true;
                    card.popCenter = ImVec2(x+w*0.5f, y+h*0.5f);
                    card.injectTriggered = true;
                }
                if (ImGui::IsItemHovered())
                    dl->AddRect(ImVec2(x, y), ImVec2(x+w, y+h), IM_COL32(100,190,255,(int)(160*alpha)), 8.0f, 0, 2.0f);
            }
        }

        // Burst particles
        for (auto& pp : g_PopParticles) {
            ImU32 col = IM_COL32(80+(int)((1-pp.alpha)*120), 180+(int)((1-pp.alpha)*60), 255, (int)(pp.alpha*230));
            dl->AddCircleFilled(pp.pos, pp.radius, col);
            dl->AddCircle(pp.pos, pp.radius*1.3f, IM_COL32(150,220,255,(int)(pp.alpha*60)), 0, 1.0f);
        }

        if (g_Cards.empty()) {
            const char* txt = "Scanning for Minecraft...";
            ImVec2 sz = ImGui::CalcTextSize(txt);
            dl->AddText(ImVec2(p.x+(s.x-sz.x)*0.5f, p.y+(s.y-bannerBottom)*0.5f+bannerBottom), IM_COL32(100,130,150,150), txt);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(hdc);
    }

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    if (g_BannerTex) glDeleteTextures(1, &g_BannerTex);
    wglMakeCurrent(NULL, NULL); wglDeleteContext(hglrc); ReleaseDC(hwnd, hdc); DestroyWindow(hwnd);
    return 0;
}
