#pragma once
using EGLBoolean = unsigned int;
using EGLint = int;
using EGLContext = void*;
using EGLDisplay = void*;
using EGLSurface = void*;
#define EGL_NO_CONTEXT nullptr
#define EGL_DRAW 0x3059
#define EGL_CONTEXT_CLIENT_TYPE 0x3097
#define EGL_OPENGL_ES_API 0x30A0
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
extern "C" EGLContext eglGetCurrentContext();
extern "C" EGLSurface eglGetCurrentSurface(EGLint);
extern "C" EGLBoolean eglQueryContext(EGLDisplay, EGLContext, EGLint, EGLint*);
extern "C" EGLBoolean eglQuerySurface(EGLDisplay, EGLSurface, EGLint, EGLint*);
extern "C" EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface);
