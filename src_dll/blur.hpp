#pragma once
#include "gl_ext.hpp"
#include "imgui.h"

static GLuint blurFBO1 = 0, blurFBO2 = 0;
static GLuint blurTex0 = 0, blurTex1 = 0, blurTex2 = 0;
static GLuint blurProgram = 0;
static int blurW = 0, blurH = 0;
static bool blurReady = false;

static const char* blurVertSrc = R"(#version 120
void main() {
    gl_TexCoord[0] = gl_MultiTexCoord0;
    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
})";

static const char* blurFragSrc = R"(#version 120
uniform sampler2D u_Texture;
uniform vec2 u_Resolution;
uniform vec2 u_Direction;

float normpdf(float x, float sigma) {
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

void main() {
    vec2 uv = gl_TexCoord[0].st;
    vec3 result = vec3(0.0);
    float total = 0.0;
    const int kSize = 8;
    const float sigma = 5.0;
    
    for (int i = -kSize; i <= kSize; ++i) {
        float w = normpdf(float(i), sigma);
        result += w * texture2D(u_Texture, uv + (u_Direction * float(i)) / u_Resolution).rgb;
        total += w;
    }
    
    gl_FragColor = vec4(result / total, 1.0);
})";

static void InitBlur(int w, int h) {
    if (!blurReady) {
        LoadGLExtensions();
        if (!glCreateProgram) return;

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &blurVertSrc, NULL);
        glCompileShader(vs);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &blurFragSrc, NULL);
        glCompileShader(fs);

        blurProgram = glCreateProgram();
        glAttachShader(blurProgram, vs);
        glAttachShader(blurProgram, fs);
        glLinkProgram(blurProgram);

        glGenFramebuffers(1, &blurFBO1);
        glGenFramebuffers(1, &blurFBO2);
        glGenTextures(1, &blurTex0);
        glGenTextures(1, &blurTex1);
        glGenTextures(1, &blurTex2);

        blurReady = true;
    }

    if (w != blurW || h != blurH) {
        GLuint texIds[3] = { blurTex0, blurTex1, blurTex2 };
        for (int i = 0; i < 3; i++) {
            glBindTexture(GL_TEXTURE_2D, texIds[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); // GL_CLAMP_TO_EDGE
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO1);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex1, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO2);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTex2, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        blurW = w;
        blurH = h;
    }
}

static void DrawFullscreenQuad(int w, int h) {
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f((float)w, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f((float)w, (float)h);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, (float)h);
    glEnd();
}

static void RenderBlurCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd) {
    if (!blurReady || !blurProgram) return;

    ImVec4 cr = cmd->ClipRect;
    float rx = cr.x, ry = cr.y;
    float rw = cr.z - cr.x, rh = cr.w - cr.y;
    if (rw <= 1 || rh <= 1) return;

    // ---- Save ALL OpenGL state ----
    GLint prevFBO; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevProg; glGetIntegerv(GL_CURRENT_PROGRAM, &prevProg);
    GLint prevTex; glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTex);
    GLint prevViewport[4]; glGetIntegerv(GL_VIEWPORT, prevViewport);
    GLint prevScissorBox[4]; glGetIntegerv(GL_SCISSOR_BOX, prevScissorBox);
    GLboolean prevScissor = glIsEnabled(GL_SCISSOR_TEST);
    GLboolean prevBlend = glIsEnabled(GL_BLEND);
    GLboolean prevDepth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean prevCull = glIsEnabled(GL_CULL_FACE);

    // ---- Disable state that would interfere ----
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    // ---- Setup ortho projection for fullscreen quads ----
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glOrtho(0, blurW, 0, blurH, -1, 1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glViewport(0, 0, blurW, blurH);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // ---- Step 1 & 2: Copy and Blur (Only once per frame) ----
    static int lastBlurFrame = -1;
    int currentFrame = ImGui::GetFrameCount();
    
    if (currentFrame != lastBlurFrame) {
        lastBlurFrame = currentFrame;

        // Step 1: Copy current screen into blurTex0
        glBindTexture(GL_TEXTURE_2D, blurTex0);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, blurW, blurH);

        // Step 2: Multi-pass blur (2 passes for smooth result)
        glUseProgram(blurProgram);
        GLint locRes = glGetUniformLocation(blurProgram, "u_Resolution");
        GLint locDir = glGetUniformLocation(blurProgram, "u_Direction");
        GLint locTex = glGetUniformLocation(blurProgram, "u_Texture");
        glUniform2f(locRes, (float)blurW, (float)blurH);
        glUniform1i(locTex, 0);

        // Pass 1: Horizontal (tex0 -> FBO2/tex2)
        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO2);
        glBindTexture(GL_TEXTURE_2D, blurTex0);
        glUniform2f(locDir, 1.0f, 0.0f);
        DrawFullscreenQuad(blurW, blurH);

        // Pass 2: Vertical (tex2 -> FBO1/tex1)
        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO1);
        glBindTexture(GL_TEXTURE_2D, blurTex2);
        glUniform2f(locDir, 0.0f, 1.0f);
        DrawFullscreenQuad(blurW, blurH);

        // Pass 3: Horizontal again (tex1 -> FBO2/tex2)
        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO2);
        glBindTexture(GL_TEXTURE_2D, blurTex1);
        glUniform2f(locDir, 1.0f, 0.0f);
        DrawFullscreenQuad(blurW, blurH);

        // Pass 4: Vertical again (tex2 -> FBO1/tex1)
        glBindFramebuffer(GL_FRAMEBUFFER, blurFBO1);
        glBindTexture(GL_TEXTURE_2D, blurTex2);
        glUniform2f(locDir, 0.0f, 1.0f);
        DrawFullscreenQuad(blurW, blurH);
    }

    // ---- Step 3: Draw the blurred result onto screen (only the menu rect) ----
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glUseProgram(0); // Fixed-function pipeline
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, blurTex1);

    // ImGui Y is top-down, OpenGL Y is bottom-up
    // Convert ImGui coords to OpenGL coords for the ortho projection
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(0, blurW, blurH, 0, -1, 1); // Top-left origin to match ImGui
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glViewport(0, 0, blurW, blurH);

    // UV coords: map screen region to texture coords
    float u0 = rx / (float)blurW;
    float v0 = 1.0f - ry / (float)blurH; // flip Y
    float u1 = (rx + rw) / (float)blurW;
    float v1 = 1.0f - (ry + rh) / (float)blurH; // flip Y

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(u0, v0); glVertex2f(rx, ry);
    glTexCoord2f(u1, v0); glVertex2f(rx + rw, ry);
    glTexCoord2f(u1, v1); glVertex2f(rx + rw, ry + rh);
    glTexCoord2f(u0, v1); glVertex2f(rx, ry + rh);
    glEnd();

    // ---- Restore ALL OpenGL state ----
    glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW); glPopMatrix();

    glBindTexture(GL_TEXTURE_2D, prevTex);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    if (prevScissor) glEnable(GL_SCISSOR_TEST);
    glScissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);
    if (prevBlend) glEnable(GL_BLEND);
    if (prevDepth) glEnable(GL_DEPTH_TEST);
    if (prevCull) glEnable(GL_CULL_FACE);
    glUseProgram(prevProg);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}
