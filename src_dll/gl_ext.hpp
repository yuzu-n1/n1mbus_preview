#pragma once
#include <windows.h>
#include <gl/GL.h>
#include <stddef.h>

// Basic OpenGL 2.0+ types
typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

// Constants for Shaders and FBOs
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_FRAMEBUFFER                    0x8D40
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#define GL_CURRENT_PROGRAM                0x8B8D

// Function Pointers
typedef GLuint(WINAPI * PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint(WINAPI * PFNGLCREATESHADERPROC) (GLenum type);
typedef void (WINAPI * PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (WINAPI * PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef void (WINAPI * PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (WINAPI * PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (WINAPI * PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (WINAPI * PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (WINAPI * PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (WINAPI * PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (WINAPI * PFNGLUSEPROGRAMPROC) (GLuint program);
typedef GLint(WINAPI * PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (WINAPI * PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (WINAPI * PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
typedef void (WINAPI * PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);

typedef void (WINAPI * PFNGLGENFRAMEBUFFERSPROC) (GLsizei n, GLuint *framebuffers);
typedef void (WINAPI * PFNGLBINDFRAMEBUFFERPROC) (GLenum target, GLuint framebuffer);
typedef void (WINAPI * PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum(WINAPI * PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void (WINAPI * PFNGLDELETEFRAMEBUFFERSPROC) (GLsizei n, const GLuint *framebuffers);
typedef void (WINAPI * PFNGLACTIVETEXTUREPROC) (GLenum texture);
typedef void (WINAPI * PFNGLGENERATEMIPMAPPROC) (GLenum target);

extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM2FPROC glUniform2f;
extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;

void LoadGLExtensions();
