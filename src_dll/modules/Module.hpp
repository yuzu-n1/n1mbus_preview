#pragma once
#include <string>
#include <windows.h>
#include "../jni/jni.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Module  – lightweight base for every Nimbus feature
// ─────────────────────────────────────────────────────────────────────────────
class Module {
public:
    explicit Module(const char* name, int keybind = 0)
        : m_name(name), m_enabled(false), m_keybind(keybind) {}

    virtual ~Module() = default;

    // ── lifecycle ────────────────────────────────────────────────────────────
    // Called every wglSwapBuffers frame while the module is enabled.
    // env / mcObj / playerObj / playerClass may be nullptr if JNI is unavailable.
    virtual void onUpdate(JNIEnv* env,
                          jobject mcObj,
                          jobject playerObj,
                          jclass  playerClass) {}

    // Called when the module is toggled ON.
    virtual void onEnable()  {}

    // Called when the module is toggled OFF.
    virtual void onDisable() {}

    // ── helpers ──────────────────────────────────────────────────────────────
    const std::string& getName()    const { return m_name;    }
    bool               isEnabled()  const { return m_enabled; }
    int                getKeybind() const { return m_keybind; }

    void setEnabled(bool v) {
        if (v == m_enabled) return;
        m_enabled = v;
        if (m_enabled) onEnable(); else onDisable();
    }

    void toggle() { setEnabled(!m_enabled); }

    void setKeybind(int vk) { m_keybind = vk; }

protected:
    std::string m_name;
    bool        m_enabled;
    int         m_keybind; // VK_ code, 0 = none
};
