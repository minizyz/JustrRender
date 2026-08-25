/*
 * JustrRender - EGL Bridge for Fold Craft Launcher
 *
 * Provides the EGL function interface expected by FCL's native runtime.
 * FCL loads this library and calls these functions to manage rendering.
 *
 * This bridge translates FCL's renderer calls to standard EGL/GLES3.
 */

#include "justr_render.h"
#include <android/log.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "JustrRender-EGL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * FCL/Pojav renderer bridge interface.
 *
 * The launcher expects the renderer library to provide EGL functionality.
 * For opengles3 renderer type, we directly use the system EGL and GLES3.
 */

/* === Window Management === */

static ANativeWindow *g_native_window = NULL;

/* Called by FCL to set the native window for rendering */
void pojav_set_native_window(ANativeWindow *window) {
    LOGI("pojav_set_native_window: %p", window);
    g_native_window = window;
    if (window != NULL) {
        justr_egl_init(window);
    }
}

/* Called by FCL to get the native window */
ANativeWindow *pojav_get_native_window(void) {
    return g_native_window;
}

/* === EGL Context Management (FCL-compatible wrappers) === */

/*
 * FCL calls these through the EGL bridge.
 * We wrap them to provide JustrRender's context management.
 */

EGLBoolean pojav_egl_make_current(EGLSurface surface, EGLContext context) {
    return justr_egl_make_current(g_justr_ctx.display, surface, surface, context);
}

EGLBoolean pojav_egl_swap_buffers(EGLSurface surface) {
    return justr_egl_swap_buffers(g_justr_ctx.display, surface);
}

EGLBoolean pojav_egl_swap_interval(EGLint interval) {
    return justr_egl_swap_interval(g_justr_ctx.display, interval);
}

EGLContext pojav_egl_create_context(EGLContext shared_context) {
    return justr_egl_create_context(g_justr_ctx.display, g_justr_ctx.config,
                                    shared_context, NULL);
}

EGLSurface pojav_egl_create_window_surface(void) {
    if (g_native_window == NULL) {
        LOGE("Cannot create surface: no native window set");
        return EGL_NO_SURFACE;
    }
    return justr_egl_create_window_surface(g_justr_ctx.display,
                                           g_justr_ctx.config,
                                           g_native_window, NULL);
}

EGLBoolean pojav_egl_destroy_context(EGLContext context) {
    return justr_egl_destroy_context(g_justr_ctx.display, context);
}

EGLBoolean pojav_egl_destroy_surface(EGLSurface surface) {
    return justr_egl_destroy_surface(g_justr_ctx.display, surface);
}

/* === Renderer Info === */

/* Returns the renderer name string for FCL's renderer selection */
const char *pojav_get_renderer_name(void) {
    return "JustrRender (OpenGL ES 3.0)";
}

/* Returns the renderer version */
const char *pojav_get_renderer_version(void) {
    return JUSTR_RENDER_VERSION;
}

/* === GL Function Loader === */

/*
 * FCL uses this to load OpenGL function pointers.
 * For GLES3, we use eglGetProcAddress with fallback to dlsym.
 */
void *pojav_get_proc_address(const char *name) {
    if (name == NULL) return NULL;

    /* Try eglGetProcAddress first (for extension functions) */
    void *proc = (void *)eglGetProcAddress(name);
    if (proc != NULL) {
        return proc;
    }

    /* Fallback: dlsym on GLESv3 library */
    static void *gles_handle = NULL;
    if (gles_handle == NULL) {
        gles_handle = dlopen("libGLESv3.so", RTLD_NOW | RTLD_GLOBAL);
        if (gles_handle == NULL) {
            gles_handle = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
        }
    }
    if (gles_handle != NULL) {
        proc = dlsym(gles_handle, name);
    }

    return proc;
}

/* === Lifecycle === */

/* Called when FCL starts rendering */
void pojav_renderer_start(void) {
    LOGI("pojav_renderer_start");
    if (g_native_window != NULL && !g_justr_ctx.initialized) {
        justr_egl_init(g_native_window);
    }
}

/* Called when FCL stops rendering */
void pojav_renderer_stop(void) {
    LOGI("pojav_renderer_stop");
    justr_egl_terminate();
}

/* Called on surface size change */
void pojav_surface_changed(int width, int height) {
    LOGI("pojav_surface_changed: %dx%d", width, height);
    g_justr_ctx.width = width;
    g_justr_ctx.height = height;
    if (g_justr_ctx.display != EGL_NO_DISPLAY &&
        g_justr_ctx.surface != EGL_NO_SURFACE) {
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_WIDTH, &g_justr_ctx.width);
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_HEIGHT, &g_justr_ctx.height);
    }
}
