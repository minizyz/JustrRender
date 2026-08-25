/*
 * JustrRender - EGL Bridge for Fold Craft Launcher
 *
 * Provides the EGL function interface expected by FCL's native runtime.
 * Supports dual-backend: Vulkan (preferred) with automatic GLES fallback.
 * FSR 1.0 super resolution is available on the GLES backend.
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

static ANativeWindow *g_native_window = NULL;

void pojav_set_native_window(ANativeWindow *window) {
    LOGI("pojav_set_native_window: %p", window);
    g_native_window = window;
    if (window != NULL) {
        justr_render_init(window);
    }
}

ANativeWindow *pojav_get_native_window(void) {
    return g_native_window;
}

EGLBoolean pojav_egl_make_current(EGLSurface surface, EGLContext context) {
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return EGL_TRUE;
    }
    return justr_egl_make_current(g_justr_ctx.display, surface, surface, context);
}

EGLBoolean pojav_egl_swap_buffers(EGLSurface surface) {
    return justr_render_swap_buffers();
}

EGLBoolean pojav_egl_swap_interval(EGLint interval) {
    justr_set_vsync(interval > 0);
    return EGL_TRUE;
}

EGLContext pojav_egl_create_context(EGLContext shared_context) {
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return (EGLContext)0x1;
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
        return (EGLSurface)0x1;
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

const char *pojav_get_renderer_name(void) {
    static char name[256];
    const char *backend = justr_get_backend_name();
    const char *fsr = justr_get_fsr_mode_name();
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        snprintf(name, sizeof(name), "JustrRender (Vulkan - %s)",
                 g_justr_vk.device_name[0] ? g_justr_vk.device_name : "Device");
    } else if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        snprintf(name, sizeof(name), "JustrRender (OpenGL ES 3.0, FSR: %s)", fsr);
    } else {
        snprintf(name, sizeof(name), "JustrRender (Vulkan+GLES, FSR: %s)", fsr);
    }
    return name;
}

const char *pojav_get_renderer_version(void) {
    return JUSTR_RENDER_VERSION;
}

void *pojav_get_proc_address(const char *name) {
    if (name == NULL) return NULL;
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        static void *gles_handle = NULL;
        if (gles_handle == NULL) {
            gles_handle = dlopen("libGLESv3.so", RTLD_NOW | RTLD_GLOBAL);
            if (gles_handle == NULL) {
                gles_handle = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
            }
        }
        if (gles_handle != NULL) return dlsym(gles_handle, name);
        return NULL;
    }
    void *proc = (void *)eglGetProcAddress(name);
    if (proc != NULL) return proc;
    static void *gles_handle = NULL;
    if (gles_handle == NULL) {
        gles_handle = dlopen("libGLESv3.so", RTLD_NOW | RTLD_GLOBAL);
        if (gles_handle == NULL) {
            gles_handle = dlopen("libGLESv2.so", RTLD_NOW | RTLD_GLOBAL);
        }
    }
    if (gles_handle != NULL) proc = dlsym(gles_handle, name);
    return proc;
}

void pojav_renderer_start(void) {
    LOGI("pojav_renderer_start");
    if (g_native_window != NULL && !g_justr_ctx.initialized) {
        justr_render_init(g_native_window);
    }
    LOGI("Active backend: %s", justr_get_backend_name());
    LOGI("FSR mode: %s", justr_get_fsr_mode_name());
    LOGI("Vulkan available: %s", justr_is_vulkan_available() ? "yes" : "no");
}

void pojav_renderer_stop(void) {
    LOGI("pojav_renderer_stop");
    justr_render_terminate();
}

void pojav_surface_changed(int width, int height) {
    LOGI("pojav_surface_changed: %dx%d", width, height);
    g_justr_ctx.width = width;
    g_justr_ctx.height = height;
    if (justr_get_active_backend() == JUSTR_BACKEND_ACTIVE_VULKAN) {
        justr_vk_destroy_swapchain();
        justr_vk_create_swapchain();
    } else if (g_justr_ctx.display != EGL_NO_DISPLAY &&
               g_justr_ctx.surface != EGL_NO_SURFACE) {
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_WIDTH, &g_justr_ctx.width);
        eglQuerySurface(g_justr_ctx.display, g_justr_ctx.surface,
                        EGL_HEIGHT, &g_justr_ctx.height);
        if (g_justr_ctx.fsr.initialized) {
            justr_fsr_set_output_size(&g_justr_ctx.fsr, width, height);
        }
    }
}

void justr_bridge_set_fsr_mode(int mode) {
    justr_set_fsr_mode((justr_fsr_mode_t)mode);
}

int justr_bridge_get_fsr_mode(void) {
    return (int)justr_get_fsr_mode();
}

void justr_bridge_set_fsr_sharpening(float amount) {
    justr_set_fsr_sharpening(amount);
}

float justr_bridge_get_fsr_sharpening(void) {
    return justr_get_fsr_sharpening();
}

void justr_bridge_set_vsync(int enabled) {
    justr_set_vsync(enabled != 0);
}

int justr_bridge_get_vsync(void) {
    return justr_get_vsync() ? 1 : 0;
}
