#include "jni_manager.hpp"
#include <string>

JavaVM* JniManager::jvm = nullptr;
typedef jint(JNICALL* tJNI_GetCreatedJavaVMs)(JavaVM**, jsize, jsize*);

bool JniManager::Initialize() {
    HMODULE hJvm = GetModuleHandleA("jvm.dll");
    if (!hJvm) return false;
    
    tJNI_GetCreatedJavaVMs getVMs = (tJNI_GetCreatedJavaVMs)GetProcAddress(hJvm, "JNI_GetCreatedJavaVMs");
    if (!getVMs) return false;
    
    jsize count = 0;
    getVMs(&jvm, 1, &count);
    return jvm != nullptr;
}

JNIEnv* JniManager::GetEnv() {
    if (!jvm) return nullptr;
    JNIEnv* env = nullptr;
    int status = jvm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        jvm->AttachCurrentThread((void**)&env, nullptr);
    }
    return env;
}

jclass JniManager::FindClassWithLoader(JNIEnv* env, const char* className) {
    jclass directClass = env->FindClass(className);
    if (directClass) return directClass;
    
    env->ExceptionClear();
    
    jclass threadClass = env->FindClass("java/lang/Thread");
    if (!threadClass) return nullptr;
    
    jmethodID currentThreadMethod = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
    jobject currentThread = env->CallStaticObjectMethod(threadClass, currentThreadMethod);
    if (!currentThread) return nullptr;
    
    jmethodID getContextClassLoaderMethod = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = env->CallObjectMethod(currentThread, getContextClassLoaderMethod);
    if (!classLoader) return nullptr;
    
    jclass classLoaderClass = env->GetObjectClass(classLoader);
    jmethodID loadClassMethod = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    
    // Replace '/' with '.' for ClassLoader.loadClass
    std::string dotName = className;
    for (size_t i = 0; i < dotName.length(); i++) {
        if (dotName[i] == '/') dotName[i] = '.';
    }
    
    jstring jClassName = env->NewStringUTF(dotName.c_str());
    jclass result = (jclass)env->CallObjectMethod(classLoader, loadClassMethod, jClassName);
    
    env->DeleteLocalRef(jClassName);
    env->DeleteLocalRef(classLoaderClass);
    env->DeleteLocalRef(classLoader);
    env->DeleteLocalRef(currentThread);
    env->DeleteLocalRef(threadClass);
    
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return nullptr;
    }
    return result;
}

bool JniManager::AddJarToContextClassLoader(JNIEnv* env, const char* jarPath) {
    if (!env || !jarPath || !jarPath[0]) return false;

    jclass fileClass = env->FindClass("java/io/File");
    if (!fileClass) { env->ExceptionClear(); return false; }
    jmethodID fileCtor = env->GetMethodID(fileClass, "<init>", "(Ljava/lang/String;)V");
    jmethodID toUriMethod = env->GetMethodID(fileClass, "toURI", "()Ljava/net/URI;");
    if (!fileCtor || !toUriMethod) { env->DeleteLocalRef(fileClass); env->ExceptionClear(); return false; }

    jstring jJarPath = env->NewStringUTF(jarPath);
    jobject fileObj = env->NewObject(fileClass, fileCtor, jJarPath);
    env->DeleteLocalRef(jJarPath);
    if (!fileObj) { env->DeleteLocalRef(fileClass); env->ExceptionClear(); return false; }

    jobject uriObj = env->CallObjectMethod(fileObj, toUriMethod);
    if (!uriObj) { env->DeleteLocalRef(fileObj); env->DeleteLocalRef(fileClass); env->ExceptionClear(); return false; }

    jclass uriClass = env->GetObjectClass(uriObj);
    jmethodID toUrlMethod = env->GetMethodID(uriClass, "toURL", "()Ljava/net/URL;");
    if (!toUrlMethod) {
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        env->ExceptionClear();
        return false;
    }

    jobject urlObj = env->CallObjectMethod(uriObj, toUrlMethod);
    if (!urlObj) {
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        env->ExceptionClear();
        return false;
    }

    jclass threadClass = env->FindClass("java/lang/Thread");
    jmethodID currentThreadMethod = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
    jobject currentThread = env->CallStaticObjectMethod(threadClass, currentThreadMethod);
    jmethodID getContextClassLoaderMethod = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
    jobject classLoader = env->CallObjectMethod(currentThread, getContextClassLoaderMethod);
    if (!classLoader) {
        env->DeleteLocalRef(threadClass);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        env->ExceptionClear();
        return false;
    }

    jclass urlClass = env->FindClass("java/net/URL");
    jclass urlClassLoaderClass = env->FindClass("java/net/URLClassLoader");
    if (!urlClass || !urlClassLoaderClass) {
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(threadClass);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        env->ExceptionClear();
        return false;
    }

    if (!env->IsInstanceOf(classLoader, urlClassLoaderClass)) {
        env->DeleteLocalRef(urlClassLoaderClass);
        env->DeleteLocalRef(urlClass);
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(threadClass);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        return false;
    }

    jmethodID addUrlMethod = env->GetMethodID(urlClassLoaderClass, "addURL", "(Ljava/net/URL;)V");
    if (!addUrlMethod) {
        env->DeleteLocalRef(urlClassLoaderClass);
        env->DeleteLocalRef(urlClass);
        env->DeleteLocalRef(classLoader);
        env->DeleteLocalRef(threadClass);
        env->DeleteLocalRef(urlObj);
        env->DeleteLocalRef(uriClass);
        env->DeleteLocalRef(uriObj);
        env->DeleteLocalRef(fileObj);
        env->DeleteLocalRef(fileClass);
        env->ExceptionClear();
        return false;
    }

    env->CallVoidMethod(classLoader, addUrlMethod, urlObj);
    bool ok = !env->ExceptionCheck();
    if (!ok) env->ExceptionClear();

    env->DeleteLocalRef(urlClassLoaderClass);
    env->DeleteLocalRef(urlClass);
    env->DeleteLocalRef(classLoader);
    env->DeleteLocalRef(threadClass);
    env->DeleteLocalRef(urlObj);
    env->DeleteLocalRef(uriClass);
    env->DeleteLocalRef(uriObj);
    env->DeleteLocalRef(fileObj);
    env->DeleteLocalRef(fileClass);
    return ok;
}
