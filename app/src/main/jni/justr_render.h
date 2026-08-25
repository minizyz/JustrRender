/*
 * JustrRender - Fold Craft Launcher Renderer Plugin
 * Native renderer with dual-backend support: Vulkan (preferred)
 * with automatic fallback to OpenGL ES 3.0.
 *
 * This library provides EGL context management, Vulkan WSI support,
 * and automatic backend selection for Minecraft: Java Edition on Android.
 */

#ifndef JUSTR_RENDER_H
#define JUSTR_RENDER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <stdbool.h>
#include <stdint.h>

#include "vulkan_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Renderer version */
#define JUSTR_RENDER_VERSION "1.1.0"
#define JUSTR_RENDERER_ID "justr_render"

/* Maximum number of EGL configs to enumerate */
#define JUSTR_MAX_CONFIGS 64

/* === Backend Selection === */

typedef enum {
    JUSTR_BACKEND_AUTO = 0,    /* Try Vulkan first, fallback to GLES */
    JUSTR_BACKEND_VULKAN,      /* Force Vulkan, fail if unavailable */
    JUSTR_BACKEND_OPENGLES,    /* Force OpenGL ES */
} justr_backend_t;

/* Active backend state */
typedef enum {
    JUSTR_BACKEND_NONE = 0,
    JUSTR_BACKEND_ACTIVE_VULKAN,
    JUSTR_BACKEND_ACTIVE_OPENGLES,
} justr_active_backend_t;

/* === Renderer Context === */

typedef struct {
    /* --- OpenGL ES backend state --- */
    EGLDisplay display;
    EGLConfig config;
    EGLContext context;
    EGLSurface surface;
    ANativeWindow *window;
    int width;
    int height;
    int red_size;
    int green_size;
    int blue_size;
    int alpha_size;
    int depth_size;
    int stencil_size;
    int sample_buffers;
    int samples;

    /* --- Backend management --- */
    justr_backend_t requested_backend;   /* What the user asked for */
    justr_active_backend_t active_backend; /* What is actually running */
    bool initialized;
    bool vsync;
    bool vulkan_available;               /* Cached probe result */
} justr_render_context_t;

/* Global renderer instance */
extern justr_render_context_t g_justr_ctx;

/* === Backend Selection API === */

/* Set preferred backend (call before init) */
void justr_set_backend(justr_backend_t backend);

/* Get currently active backend */
justr_active_backend_t justr_get_active_backend(void);

/* Get backend name string */
const char *justr_get_backend_name(void);

/* Probe if Vulkan is available (cached) */
bool justr_is_vulkan_available(void);

/* === Unified Renderer API === */

/* Initialize renderer with automatic backend selection */
EGLBoolean justr_render_init(ANativeWindow *window);

/* Terminate renderer (any backend) */
void justr_render_terminate(void);

/* Swap buffers (works for both backends) */
EGLBoolean justr_render_swap_buffers(void);

/* === EGL Bridge Functions (GLES backend) === */

EGLBoolean justr_egl_init(ANativeWindow *window);
EGLContext justr_egl_create_context(EGLDisplay display, EGLConfig config,
                                    EGLContext share_context,
                                    const EGLint *attrib_list);
EGLSurface justr_egl_create_window_surface(EGLDisplay display, EGLConfig config,
                                           ANativeWindow *window,
                                           const EGLint *attrib_list);
EGLBoolean justr_egl_make_current(EGLDisplay display, EGLSurface draw,
                                  EGLSurface read, EGLContext context);
EGLBoolean justr_egl_swap_buffers(EGLDisplay display, EGLSurface surface);
EGLBoolean justr_egl_swap_interval(EGLDisplay display, EGLint interval);
EGLBoolean justr_egl_destroy_context(EGLDisplay display, EGLContext context);
EGLBoolean justr_egl_destroy_surface(EGLDisplay display, EGLSurface surface);
void justr_egl_terminate(void);

/* === Info Queries === */

const char *justr_get_renderer_string(void);
const char *justr_get_version_string(void);
const char *justr_get_vendor_string(void);

/* === Utility Functions === */

bool justr_check_egl_error(const char *operation);
bool justr_check_gl_error(const char *operation);
void justr_get_surface_size(int *width, int *height);
void justr_set_vsync(bool enabled);
bool justr_get_vsync(void);

#ifdef __cplusplus
}
#endif

#endif /* JUSTR_RENDER_H */
