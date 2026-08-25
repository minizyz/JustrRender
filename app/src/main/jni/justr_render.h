/*
 * JustrRender - Fold Craft Launcher Renderer Plugin
 * Native renderer implementation based on OpenGL ES 3.0
 *
 * This library provides EGL context management and OpenGL ES 3.0
 * rendering backend for Minecraft: Java Edition on Android.
 */

#ifndef JUSTR_RENDER_H
#define JUSTR_RENDER_H

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Renderer version */
#define JUSTR_RENDER_VERSION "1.0.0"
#define JUSTR_RENDERER_ID "opengles3"

/* Maximum number of EGL configs to enumerate */
#define JUSTR_MAX_CONFIGS 64

/* Renderer context state */
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
    bool initialized;
    bool vsync;
} justr_render_context_t;

/* Global renderer instance */
extern justr_render_context_t g_justr_ctx;

/* === EGL Bridge Functions === */

/* Initialize the renderer with a native window */
EGLBoolean justr_egl_init(ANativeWindow *window);

/* Create EGL context */
EGLContext justr_egl_create_context(EGLDisplay display, EGLConfig config,
                                    EGLContext share_context,
                                    const EGLint *attrib_list);

/* Create window surface */
EGLSurface justr_egl_create_window_surface(EGLDisplay display, EGLConfig config,
                                           ANativeWindow *window,
                                           const EGLint *attrib_list);

/* Make context current */
EGLBoolean justr_egl_make_current(EGLDisplay display, EGLSurface draw,
                                  EGLSurface read, EGLContext context);

/* Swap buffers */
EGLBoolean justr_egl_swap_buffers(EGLDisplay display, EGLSurface surface);

/* Set swap interval (vsync) */
EGLBoolean justr_egl_swap_interval(EGLDisplay display, EGLint interval);

/* Destroy context */
EGLBoolean justr_egl_destroy_context(EGLDisplay display, EGLContext context);

/* Destroy surface */
EGLBoolean justr_egl_destroy_surface(EGLDisplay display, EGLSurface surface);

/* Terminate renderer */
void justr_egl_terminate(void);

/* Query renderer info */
const char *justr_get_renderer_string(void);
const char *justr_get_version_string(void);
const char *justr_get_vendor_string(void);

/* === Utility Functions === */

/* Check for EGL errors */
bool justr_check_egl_error(const char *operation);

/* Check for GL errors */
bool justr_check_gl_error(const char *operation);

/* Get current framebuffer dimensions */
void justr_get_surface_size(int *width, int *height);

/* Set vsync enabled/disabled */
void justr_set_vsync(bool enabled);

/* Get vsync state */
bool justr_get_vsync(void);

#ifdef __cplusplus
}
#endif

#endif /* JUSTR_RENDER_H */
