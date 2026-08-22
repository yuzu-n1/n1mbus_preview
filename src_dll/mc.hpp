// mc.hpp  –  Dynamic Minecraft JNI mapping registry
// 
// Usage:
//   MC::Class("net/minecraft/entity/EntityLivingBase")
//   MC::field("EntityLivingBase", "D")           // 型スキャン
//   MC::method("EntityLivingBase", "getHealth")  // 名前スキャン
//   MC::fieldID(env, obj, "EntityPlayer", "capabilities", "L...;")
//
// モジュールは MC::resolve() が完了した後に使用できる。

#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include "jni/jni.h"
#include "jni_manager.hpp"

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace MC_internal {

    inline std::string slashToDot(std::string s) {
        std::replace(s.begin(), s.end(), '/', '.'); return s;
    }
    inline std::string dotToSlash(std::string s) {
        std::replace(s.begin(), s.end(), '.', '/'); return s;
    }

    // Try a list of class name candidates and return the first that loads.
    // outName receives the canonical slash-form name.
    inline jclass findClass(JNIEnv* env,
                            const std::vector<std::string>& candidates,
                            std::string& outName)
    {
        for (const auto& c : candidates) {
            jclass cls = JniManager::FindClassWithLoader(env, c.c_str());
            if (cls) { outName = c; return cls; }
        }
        return nullptr;
    }

    // Return the runtime type name of a Field (dot-form, like "boolean"/"double"/…)
    inline std::string fieldTypeName(JNIEnv* env, jclass classClass, jobject fieldObj) {
        jmethodID getType = env->GetMethodID(classClass, "getType", "()Ljava/lang/Class;");
        jclass typeClass = (jclass)env->CallObjectMethod(fieldObj, getType);
        if (!typeClass) return "";

        jclass cc = env->FindClass("java/lang/Class");
        jmethodID nm = env->GetMethodID(cc, "getName", "()Ljava/lang/String;");
        jstring js = (jstring)env->CallObjectMethod(typeClass, nm);
        const char* ch = env->GetStringUTFChars(js, nullptr);
        std::string result = ch;
        env->ReleaseStringUTFChars(js, ch);
        env->DeleteLocalRef(js);
        env->DeleteLocalRef(typeClass);
        env->DeleteLocalRef(cc);
        return result;
    }

    // Sig letter/prefix → Java type dot-name
    inline std::string sigToTypeName(const std::string& sig) {
        if (sig == "Z") return "boolean";
        if (sig == "B") return "byte";
        if (sig == "C") return "char";
        if (sig == "S") return "short";
        if (sig == "I") return "int";
        if (sig == "J") return "long";
        if (sig == "F") return "float";
        if (sig == "D") return "double";
        if (!sig.empty() && sig[0] == 'L' && sig.back() == ';')
            return slashToDot(sig.substr(1, sig.size() - 2));
        return sig;
    }

    // Scan getDeclaredFields and return the first field whose type matches sig.
    // isStatic controls whether to look for static or instance fields.
    inline std::string scanFieldByType(JNIEnv* env, jclass clazz,
                                        const std::string& sig, bool isStatic = false)
    {
        jclass classClass = env->FindClass("java/lang/Class");
        jmethodID gdf = env->GetMethodID(classClass, "getDeclaredFields",
                                          "()[Ljava/lang/reflect/Field;");
        jobjectArray fields = (jobjectArray)env->CallObjectMethod(clazz, gdf);
        if (!fields) { env->DeleteLocalRef(classClass); return ""; }

        jclass fieldClass = env->FindClass("java/lang/reflect/Field");
        jmethodID getName  = env->GetMethodID(fieldClass, "getName", "()Ljava/lang/String;");
        jmethodID getMods  = env->GetMethodID(fieldClass, "getModifiers", "()I");
        jclass modClass    = env->FindClass("java/lang/reflect/Modifier");
        jmethodID isStaticM = env->GetStaticMethodID(modClass, "isStatic", "(I)Z");

        std::string want = sigToTypeName(sig);
        std::string result;
        jsize len = env->GetArrayLength(fields);

        for (jsize i = 0; i < len && result.empty(); i++) {
            jobject f = env->GetObjectArrayElement(fields, i);
            if (!f) continue;

            jint mods = env->CallIntMethod(f, getMods);
            bool isS  = env->CallStaticBooleanMethod(modClass, isStaticM, mods) != JNI_FALSE;
            if (isS != isStatic) { env->DeleteLocalRef(f); continue; }

            std::string typeName = fieldTypeName(env, classClass, f);
            if (typeName == want) {
                jstring js = (jstring)env->CallObjectMethod(f, getName);
                const char* ch = env->GetStringUTFChars(js, nullptr);
                result = ch;
                env->ReleaseStringUTFChars(js, ch);
                env->DeleteLocalRef(js);
            }
            env->DeleteLocalRef(f);
        }

        env->DeleteLocalRef(fields);
        env->DeleteLocalRef(fieldClass);
        env->DeleteLocalRef(modClass);
        env->DeleteLocalRef(classClass);
        return result;
    }

    // Try each candidate name with getDeclaredField; return the first that exists.
    inline std::string scanFieldByNames(JNIEnv* env, jclass clazz,
                                         const std::vector<std::string>& names)
    {
        jclass cc = env->FindClass("java/lang/Class");
        jmethodID gdf = env->GetMethodID(cc, "getDeclaredField",
                                          "(Ljava/lang/String;)Ljava/lang/reflect/Field;");
        for (const auto& n : names) {
            jstring js = env->NewStringUTF(n.c_str());
            jobject f  = env->CallObjectMethod(clazz, gdf, js);
            env->DeleteLocalRef(js);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (f) { env->DeleteLocalRef(f); env->DeleteLocalRef(cc); return n; }
        }
        env->DeleteLocalRef(cc);
        return "";
    }

    // Try each candidate name with GetMethodID; return the first that works.
    inline std::string scanMethodByNames(JNIEnv* env, jclass clazz,
                                          const std::vector<std::string>& names,
                                          const char* sig, bool isStatic = false)
    {
        for (const auto& n : names) {
            jmethodID mid = isStatic
                ? env->GetStaticMethodID(clazz, n.c_str(), sig)
                : env->GetMethodID(clazz, n.c_str(), sig);
            if (env->ExceptionCheck()) { env->ExceptionClear(); continue; }
            if (mid) return n;
        }
        return "";
    }

} // namespace MC_internal

// ─────────────────────────────────────────────────────────────────────────────
//  MappingRequest  –  one pending field or method to resolve
// ─────────────────────────────────────────────────────────────────────────────
struct FieldRequest {
    std::string key;            // storage key (e.g. "EntityPlayer.capabilities")
    std::vector<std::string> classNames;   // e.g. {"net/minecraft/entity/player/EntityPlayer","qn"}
    std::string typeSig;        // JNI type sig for type-scan, e.g. "Lnet/minecraft/entity/player/PlayerCapabilities;"
    std::vector<std::string> nameHints;    // extra name candidates (SRG / deobf)
};

struct MethodRequest {
    std::string key;
    std::vector<std::string> classNames;
    std::string methodSig;      // JNI sig, e.g. "()F"
    std::vector<std::string> nameHints;
    bool isStatic = false;
};

// ─────────────────────────────────────────────────────────────────────────────
//  MC  –  The global mapping registry
// ─────────────────────────────────────────────────────────────────────────────
namespace MC {

    // ── storage ──────────────────────────────────────────────────────────────
    inline std::unordered_map<std::string, std::string> g_fields;   // key → field name
    inline std::unordered_map<std::string, std::string> g_methods;  // key → method name
    inline std::unordered_map<std::string, std::string> g_classes;  // short name → resolved slash path
    inline std::unordered_map<std::string, std::string> g_sigs;     // key → resolved sig
    inline bool g_resolved = false;

    // ── pending requests (filled before resolve()) ────────────────────────────
    inline std::vector<FieldRequest>  g_fieldRequests;
    inline std::vector<MethodRequest> g_methodRequests;

    // ── accessors ─────────────────────────────────────────────────────────────

    // Return resolved field name, or "" if not found
    inline const char* field(const char* key) {
        auto it = g_fields.find(key);
        return it != g_fields.end() ? it->second.c_str() : "";
    }

    // Return resolved method name, or "" if not found
    inline const char* method(const char* key) {
        auto it = g_methods.find(key);
        return it != g_methods.end() ? it->second.c_str() : "";
    }

    // Return resolved class path (slash form), or "" if not found
    inline const char* cls(const char* shortName) {
        auto it = g_classes.find(shortName);
        return it != g_classes.end() ? it->second.c_str() : "";
    }

    // Return resolved signature for a field or method
    inline const char* sig(const char* key) {
        auto it = g_sigs.find(key);
        return it != g_sigs.end() ? it->second.c_str() : "";
    }

    // ── registration (call before resolve()) ──────────────────────────────────

    // Register a field to be resolved.
    // typeSig: JNI type descriptor used for type-scan (e.g. "D", "Z", "Lnet/...;")
    // nameHints: extra candidate names (SRG obfuscated names, readable names, etc.)
    inline void registerField(const char* key,
                               std::vector<std::string> classNames,
                               const char* typeSig,
                               std::vector<std::string> nameHints = {})
    {
        g_fieldRequests.push_back({key, std::move(classNames), typeSig, std::move(nameHints)});
    }

    // Register a method to be resolved.
    inline void registerMethod(const char* key,
                                std::vector<std::string> classNames,
                                const char* methodSig,
                                std::vector<std::string> nameHints = {},
                                bool isStatic = false)
    {
        g_methodRequests.push_back({key, std::move(classNames), methodSig,
                                    std::move(nameHints), isStatic});
    }

    // ── resolve() ──────────────────────────────────────────────────────────────
    // Call once after DLL injection. Processes all registered requests.
    inline bool resolve() {
        JNIEnv* env = JniManager::GetEnv();
        if (!env) return false;

        FILE* log = nullptr;
        auto Log = [&](const char* fmt, ...) {
            if (!log) return;
            va_list a; va_start(a, fmt);
            vfprintf(log, fmt, a); va_end(a);
            fflush(log);
        };

        Log("=== MC mapping resolve start ===\n");

        // ── Fields ──────────────────────────────────────────────────────────
        for (auto& req : g_fieldRequests) {
            std::string resolved;
            jclass cls = MC_internal::findClass(env, req.classNames, resolved);
            if (!cls) {
                Log("[FIELD] %s  -> CLASS NOT FOUND (tried:", req.key.c_str());
                for (auto& n : req.classNames) Log(" %s", n.c_str());
                Log(")\n");
                continue;
            }

            std::string name;

            // 1) Try name hints first (fastest)
            if (!req.nameHints.empty()) {
                name = MC_internal::scanFieldByNames(env, cls, req.nameHints);
            }

            // 2) Fallback: scan all declared fields by type
            if (name.empty() && !req.typeSig.empty()) {
                name = MC_internal::scanFieldByType(env, cls, req.typeSig);
            }

            // 3) Also try combining: type-scan but only among nameHints if first failed
            //    (type scan already handles this as a full scan)

            if (!name.empty()) {
                g_fields[req.key] = name;
                // Store signature
                g_sigs[req.key + ".sig"] = req.typeSig;
                Log("[FIELD] %-40s -> %-30s (class: %s)\n",
                    req.key.c_str(), name.c_str(), resolved.c_str());
            } else {
                Log("[FIELD] %-40s -> NOT FOUND (class: %s, type: %s)\n",
                    req.key.c_str(), resolved.c_str(), req.typeSig.c_str());
            }

            env->DeleteLocalRef(cls);
        }

        // ── Methods ─────────────────────────────────────────────────────────
        for (auto& req : g_methodRequests) {
            std::string resolved;
            jclass cls = MC_internal::findClass(env, req.classNames, resolved);
            if (!cls) {
                Log("[METHOD] %s  -> CLASS NOT FOUND\n", req.key.c_str());
                continue;
            }

            std::string name;
            if (!req.nameHints.empty()) {
                name = MC_internal::scanMethodByNames(env, cls, req.nameHints,
                                                      req.methodSig.c_str(), req.isStatic);
            }

            if (!name.empty()) {
                g_methods[req.key] = name;
                g_sigs[req.key + ".sig"] = req.methodSig;
                Log("[METHOD] %-40s -> %-30s (class: %s)\n",
                    req.key.c_str(), name.c_str(), resolved.c_str());
            } else {
                Log("[METHOD] %-40s -> NOT FOUND (class: %s)\n",
                    req.key.c_str(), resolved.c_str());
            }

            env->DeleteLocalRef(cls);
        }

        Log("=== MC mapping resolve done ===\n");
        if (log) fclose(log);

        g_resolved = true;
        return true;
    }

    // ── Convenience JNI helpers ───────────────────────────────────────────────

    // Get a jfieldID from a cached mapping key.
    // clazz should be the actual class that declares the field (may differ from the lookup class).
    inline jfieldID fieldID(JNIEnv* env, jclass clazz,
                             const char* key, const char* fallbackSig = nullptr)
    {
        auto it = g_fields.find(key);
        if (it == g_fields.end()) return nullptr;
        const char* sigStr = fallbackSig;
        if (!sigStr) {
            auto si = g_sigs.find(std::string(key) + ".sig");
            if (si != g_sigs.end()) sigStr = si->second.c_str();
        }
        if (!sigStr) return nullptr;
        jfieldID fid = env->GetFieldID(clazz, it->second.c_str(), sigStr);
        if (env->ExceptionCheck()) env->ExceptionClear();
        return fid;
    }

    inline jmethodID methodID(JNIEnv* env, jclass clazz,
                               const char* key, bool isStatic = false)
    {
        auto it = g_methods.find(key);
        if (it == g_methods.end()) return nullptr;
        auto si = g_sigs.find(std::string(key) + ".sig");
        if (si == g_sigs.end()) return nullptr;
        jmethodID mid = isStatic
            ? env->GetStaticMethodID(clazz, it->second.c_str(), si->second.c_str())
            : env->GetMethodID(clazz, it->second.c_str(), si->second.c_str());
        if (env->ExceptionCheck()) env->ExceptionClear();
        return mid;
    }

} // namespace MC
