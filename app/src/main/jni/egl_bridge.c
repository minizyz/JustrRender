/*
 * JustrRender - EGL Bridge for Fold Craft Launcher
 *
 * Provides the EGL function interface expected by FCL's native runtime.
 * Supports dual-backend: Vulkan (preferred) with automatic GLES fallback.
 *
 * FCL loads this library and calls these functions to manage rendering.
 * The bridge dispatches to the active backend (Vulkan or GLES).
 */

#include "justr_render.h"
#include <android/log.h>
#include <android/native_window.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "JustrRender-Bridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/*
 * FCL/Pojav renderer bridge interface.
 *
 * The launcher expects the renderer library to provide EGL functionality.
 * JustrRender supports two backends:
 *   - Vulkan (preferred, via WSI swapchain)
 *   - OpenGL ES 3.0 (fallback, via standard EGL)
 *
 * Backend selection is automatic unless JUSTR_BACKEND env var is set.
 */

/* === Window Management === */

static ANativeWindow *g_native_window = NULL;

/* Called by FCL to set the native window for rendering */
void pojav_set_native_window(ANativeWindow *window) {
    LOGI("pojav_set_native_window: %p", window);
    g_native_window = window;
    if (window != NULL) {
        /* Use unified init with automatic backend selection */
        justr_render_init(window);
    }
}

/* Called by FCL to get the native window */
ANativeWindow *pojav_get_native_window(void) {
    return g_native_window;
}

/* === EGL Context Management (FCL-compatible wrappers) === */

EGLBoolean pojav_egl_make_current(EGLSurface surface, EGLContext context) {
    /* For Vulkan backend, context is managed internally */
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        LOGD("Vulkan backend: make_current is no-op (context managed internally)");
        return EGL_TRUE;
    }
    return justr_egl_make_current(g_justr_ctx.display, surface, surface, context);
}

EGLBoolean pojav_egl_swap_buffers(EGLSurface surface) {
    /* Use unified swap that dispatches to active backend */
    return justr_render_swap_buffers();
}

EGLBoolean pojav_egl_swap_interval(EGLint interval) {
    justr_set_vsync(interval > 0);
    return EGL_TRUE;
}

EGLContext pojav_egl_create_context(EGLContext shared_context) {
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        /* For Vulkan, return a non-null dummy handle since FCL expects one */
        LOGI("Vulkan backend: returning dummy EGLContext handle");
        return (EGLContext)0x1; /* Non-null sentinel */
    }
    return justr_egl_create_context(g_justr_ctx.display, g_justr_ctx.config,
                                    shared_context, NULL);
}

EGLSurface pojav_egl_create_window_surface(void) {
    if (g_native_window == NULL) {
        LOGE("Cannot create surface: no native window set");
        return EGL_NO_SURFACE;
    }

    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        /* Vulkan swapchain already created during init */
        LOGI("Vulkan backend: window surface already created (swapchain)");
        return (EGLSurface)0x1; /* Non-null sentinel */
    }

    return justr_egl_create_window_surface(g_justr_ctx.display,
                                           g_justr_ctx.config,
                                           g_native_window, NULL);
}

EGLBoolean pojav_egl_destroy_context(EGLContext context) {
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return EGL_TRUE;
    }
    return justr_egl_destroy_context(g_justr_ctx.display, context);
}

EGLBoolean pojav_egl_destroy_surface(EGLSurface surface) {
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return EGL_TRUE;
    }
    return justr_egl_destroy_surface(g_justr_ctx.display, surface);
}

/* === Renderer Info === */

/* Returns the renderer name string for FCL's renderer selection */
const char *pojav_get_renderer_name(void) {
    static char name[128];
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        snprintf(name, sizeof(name), "JustrRender (Vulkan - %s)",
                 g_justr_vk.device_name[0] ? g_justr_vk.device_name : "Device");
    } else if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        snprintf(name, sizeof(name), "JustrRender (OpenGL ES 3.0)");
    } else {
        snprintf(name, sizeof(name), "JustrRender (Vulkan+GLES Auto)");
    }
    return name;
}

/* Returns the renderer version */
const char *pojav_get_renderer_version(void) {
    return JUSTR_RENDER_VERSION;
}

/* === GL Function Loader === */

/*
 * FCL uses this to load OpenGL function pointers.
 * For Vulkan backend, this would typically route through a GL-on-Vulkan
 * translation layer (e.g. Zink). For GLES, use standard EGL loader.
 */
void *pojav_get_proc_address(const char *name) {
    if (name == NULL) return NULL;

    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        /*
         * In Vulkan mode, OpenGL function pointers should be provided by
         * the GL translation layer (Zink/Mesa). We attempt to load from
         * the system GLES libraries as a fallback for basic functions.
         */
        static void *gles_handle = NULL;
        if (gles_handle == NULL) {
            gles_handle = dlopen("libGLESv3.so", RTLD_NOW | RTLD_GLOBAL);
            if (gles_handle == NULL) {
                gles_handle = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
            }
        }
        if (gles_handle != NULL) {
            return dlsym(gles_handle, name);
        }
        return NULL;
    }

    /* GLES backend: use eglGetProcAddress with dlsym fallback */
    void *proc = (void *)eglGetProcAddress(name);
    if (proc != NULL) {
        return proc;
    }

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
        justr_render_init(g_native_window);
    }
    LOGI("Active backend: %s", justr_get_backend_name());
    LOGI("Vulkan available: %s", justr_is_vulkan_available() ? "yes" : "no");
}

/* Called when FCL stops rendering */
void pojav_renderer_stop(void) {
    LOGI("pojav_renderer_stop");
    justr_render_terminate();
}

/* Called on surface size change */
void pojav_surface_changed(int width, int height) {
    LOGI("pojav_surface_changed: %dx%d", width, height);
    g_justr_ctx.width = width;
    g_justr_ctx.height = height;

    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        /* Recreate Vulkan swapchain on resize */
        LOGI("Vulkan: recreating swapchain for new size %dx%d", width, height);
        justr_vk_destroy_swapchain();
        justr_vk_create_swapchain();
    } else if (g_justr_ctx.display != EGL_NO_DISPLAY &&
               g_justr_ctx.surface != EGL_NO_SURFACE) {
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_WIDTH, &g_justr_ctx.width);
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_HEIGHT, &g_justr_ctx.height);
    }
}
