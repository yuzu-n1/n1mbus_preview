#pragma once
#include <windows.h>
#include <GL/gl.h>
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "stb_image.h"

#ifndef STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#endif
#include "stb_image_resize2.h"

inline GLuint LoadTextureFromMemory(const unsigned char* buffer, int len, int* out_width, int* out_height, bool forceWhite = true) {
    int image_width = 0;
    int image_height = 0;
    int channels = 0;
    unsigned char* image_data = stbi_load_from_memory(buffer, len, &image_width, &image_height, &channels, 4);
    if (image_data == NULL) return 0;

    // Resize to 64x64 on CPU to guarantee smooth downscaling in legacy OpenGL contexts
    int target_w = 64;
    int target_h = 64;
    unsigned char* resized_data = (unsigned char*)malloc(target_w * target_h * 4);
    stbir_resize_uint8_linear(image_data, image_width, image_height, 0,
                              resized_data, target_w, target_h, 0, STBIR_RGBA);
    stbi_image_free(image_data);
    image_data = resized_data;
    image_width = target_w;
    image_height = target_h;

    if (forceWhite) {
        for (int i = 0; i < image_width * image_height * 4; i += 4) {
            image_data[i]     = 255;
            image_data[i + 1] = 255;
            image_data[i + 2] = 255;
        }
    }

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);

    // Generate mipmap at runtime (no hard dependency on gl_ext.hpp)
    HMODULE hGl = GetModuleHandleA("opengl32.dll");
    if (hGl) {
        FARPROC proc = GetProcAddress(hGl, "glGenerateMipmap");
        if (!proc) {
            using WglGetProc = PROC(WINAPI*)(LPCSTR);
            auto wgl = (WglGetProc)GetProcAddress(hGl, "wglGetProcAddress");
            if (wgl) proc = wgl("glGenerateMipmap");
        }
        if (proc) ((void(WINAPI*)(GLenum))proc)(GL_TEXTURE_2D);
    }
    
    free(image_data);

    if (out_width) *out_width = image_width;
    if (out_height) *out_height = image_height;

    return image_texture;
}

inline GLuint LoadTextureFromFile(const char* filename, int* out_width, int* out_height) {
    int image_width = 0;
    int image_height = 0;
    int channels = 0;
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, &channels, 4);
    if (image_data == NULL) return 0;

    // The image is black on transparent. To colorize it dynamically via ImGui tinting,
    // we need it to be white on transparent.
    // Let us convert any RGB values to 255 (white) but preserve the alpha.
    for (int i = 0; i < image_width * image_height * 4; i += 4) {
        image_data[i]     = 255; // R
        image_data[i + 1] = 255; // G
        image_data[i + 2] = 255; // B
        // Alpha is preserved
    }

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F); // GL_CLAMP_TO_EDGE

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    
    stbi_image_free(image_data);

    if (out_width) *out_width = image_width;
    if (out_height) *out_height = image_height;

    return image_texture;
}
