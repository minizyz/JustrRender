/*
 * JustrRender - Fold Craft Launcher Renderer Plugin
 * Native renderer with dual-backend support: Vulkan (preferred)
 * with automatic fallback to OpenGL ES 3.0.
 *
 * Features:
 *   - Dual-backend: Vulkan WSI + OpenGL ES (auto fallback)
 *   - FSR 1.0: spatial upscaling (EASU) + sharpening (RCAS)
 *   - VSync control, MSAA, custom render scale
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
#include "fsr_render.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JUSTR_RENDER_VERSION "1.2.0"
#define JUSTR_RENDERER_ID "justr_render"
#define JUSTR_MAX_CONFIGS 64

typedef enum {
    JUSTR_BACKEND_AUTO = 0,
    JUSTR_BACKEND_VULKAN,
    JUSTR_BACKEND_OPENGLES,
} justr_backend_t;

typedef enum {
    JUSTR_BACKEND_NONE = 0,
    JUSTR_BACKEND_ACTIVE_VULKAN,
    JUSTR_BACKEND_ACTIVE_OPENGLES,
} justr_active_backend_t;

typedef struct {
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

    justr_backend_t requested_backend;
    justr_active_backend_t active_backend;
    bool initialized;
    bool vsync;
    bool vulkan_available;

    justr_fsr_context_t fsr;
    bool fsr_enabled;
    justr_fsr_mode_t fsr_mode;
    float fsr_sharpening;
    float custom_scale;

    bool debug_log;
} justr_render_context_t;

extern justr_render_context_t g_justr_ctx;

void justr_set_backend(justr_backend_t backend);
justr_active_backend_t justr_get_active_backend(void);
const char *justr_get_backend_name(void);
bool justr_is_vulkan_available(void);

EGLBoolean justr_render_init(ANativeWindow *window);
void justr_render_terminate(void);
EGLBoolean justr_render_swap_buffers(void);

void justr_set_fsr_mode(justr_fsr_mode_t mode);
justr_fsr_mode_t justr_get_fsr_mode(void);
const char *justr_get_fsr_mode_name(void);
void justr_set_fsr_sharpening(float amount);
float justr_get_fsr_sharpening(void);
void justr_set_custom_scale(float scale);
float justr_get_custom_scale(void);
void justr_get_render_resolution(int *width, int *height);

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

const char *justr_get_renderer_string(void);
const char *justr_get_version_string(void);
const char *justr_get_vendor_string(void);

bool justr_check_egl_error(const char *operation);
bool justr_check_gl_error(const char *operation);
void justr_get_surface_size(int *width, int *height);
void justr_set_vsync(bool enabled);
bool justr_get_vsync(void);

#ifdef __cplusplus
}
#endif

#endif
