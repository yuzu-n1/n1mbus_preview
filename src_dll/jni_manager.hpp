#pragma once
#include <windows.h>
#include "jni/jni.h"

class JniManager {
public:
    static bool Initialize();
    static JNIEnv* GetEnv();
    static jclass FindClassWithLoader(JNIEnv* env, const char* className);
    static bool AddJarToContextClassLoader(JNIEnv* env, const char* jarPath);
private:
    static JavaVM* jvm;
};
