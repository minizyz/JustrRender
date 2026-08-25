/*
 * JustrRender - Fold Craft Launcher Renderer Plugin
 * Main renderer implementation with dual-backend + FSR 1.0 support.
 */

#include "justr_render.h"
#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "JustrRender"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

justr_render_context_t g_justr_ctx = {
    .display = EGL_NO_DISPLAY,
    .config = NULL,
    .context = EGL_NO_CONTEXT,
    .surface = EGL_NO_SURFACE,
    .window = NULL,
    .width = 0, .height = 0,
    .red_size = 8, .green_size = 8, .blue_size = 8, .alpha_size = 8,
    .depth_size = 24, .stencil_size = 8,
    .sample_buffers = 0, .samples = 0,
    .requested_backend = JUSTR_BACKEND_AUTO,
    .active_backend = JUSTR_BACKEND_NONE,
    .initialized = false,
    .vsync = true,
    .vulkan_available = false,
    .fsr_enabled = false,
    .fsr_mode = JUSTR_FSR_OFF,
    .fsr_sharpening = 0.5f,
    .custom_scale = 1.0f,
    .debug_log = false,
};

void justr_set_backend(justr_backend_t backend) {
    g_justr_ctx.requested_backend = backend;
    const char *name = "UNKNOWN";
    switch (backend) {
        case JUSTR_BACKEND_AUTO: name = "AUTO"; break;
        case JUSTR_BACKEND_VULKAN: name = "VULKAN"; break;
        case JUSTR_BACKEND_OPENGLES: name = "OPENGLES"; break;
    }
    LOGI("Requested backend: %s", name);
}

justr_active_backend_t justr_get_active_backend(void) { return g_justr_ctx.active_backend; }

const char *justr_get_backend_name(void) {
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN: return "Vulkan";
        case JUSTR_BACKEND_ACTIVE_OPENGLES: return "OpenGL ES";
        default: return "None";
    }
}

bool justr_is_vulkan_available(void) { return g_justr_ctx.vulkan_available; }

/* === FSR 1.0 API === */

void justr_set_fsr_mode(justr_fsr_mode_t mode) {
    g_justr_ctx.fsr_mode = mode;
    g_justr_ctx.fsr_enabled = (mode != JUSTR_FSR_OFF);
    if (g_justr_ctx.initialized && g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        if (g_justr_ctx.fsr_enabled) {
            if (!g_justr_ctx.fsr.initialized)
                justr_fsr_init(&g_justr_ctx.fsr, g_justr_ctx.width, g_justr_ctx.height);
            justr_fsr_set_mode(&g_justr_ctx.fsr, mode);
            justr_fsr_set_sharpening(&g_justr_ctx.fsr, g_justr_ctx.fsr_sharpening);
        } else if (g_justr_ctx.fsr.initialized) {
            justr_fsr_terminate(&g_justr_ctx.fsr);
        }
    }
    LOGI("FSR mode: %s", justr_fsr_get_mode_name(mode));
}

justr_fsr_mode_t justr_get_fsr_mode(void) { return g_justr_ctx.fsr_mode; }
const char *justr_get_fsr_mode_name(void) { return justr_fsr_get_mode_name(g_justr_ctx.fsr_mode); }
void justr_set_fsr_sharpening(float amount) {
    g_justr_ctx.fsr_sharpening = amount;
    if (g_justr_ctx.fsr.initialized) justr_fsr_set_sharpening(&g_justr_ctx.fsr, amount);
}
float justr_get_fsr_sharpening(void) { return g_justr_ctx.fsr_sharpening; }
void justr_set_custom_scale(float scale) { g_justr_ctx.custom_scale = scale; }
float justr_get_custom_scale(void) { return g_justr_ctx.custom_scale; }

void justr_get_render_resolution(int *width, int *height) {
    if (g_justr_ctx.fsr_enabled && g_justr_ctx.fsr.initialized) {
        justr_fsr_get_input_size(&g_justr_ctx.fsr, width, height);
    } else if (g_justr_ctx.custom_scale != 1.0f) {
        if (width) *width = (int)(g_justr_ctx.width * g_justr_ctx.custom_scale);
        if (height) *height = (int)(g_justr_ctx.height * g_justr_ctx.custom_scale);
    } else {
        if (width) *width = g_justr_ctx.width;
        if (height) *height = g_justr_ctx.height;
    }
}

/* === Error Checking === */

bool justr_check_egl_error(const char *op) {
    EGLint e = eglGetError();
    if (e != EGL_SUCCESS) { LOGE("EGL error 0x%x in %s", e, op); return false; }
    return true;
}

bool justr_check_gl_error(const char *op) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) { LOGE("GL error 0x%x in %s", e, op); return false; }
    return true;
}

/* === EGL Config === */

static EGLConfig justr_choose_config(EGLDisplay dpy) {
    EGLint attrs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, g_justr_ctx.red_size,
        EGL_GREEN_SIZE, g_justr_ctx.green_size,
        EGL_BLUE_SIZE, g_justr_ctx.blue_size,
        EGL_ALPHA_SIZE, g_justr_ctx.alpha_size,
        EGL_DEPTH_SIZE, g_justr_ctx.depth_size,
        EGL_STENCIL_SIZE, g_justr_ctx.stencil_size,
        EGL_NONE
    };
    EGLConfig cfgs[JUSTR_MAX_CONFIGS];
    EGLint n = 0;
    if (!eglChooseConfig(dpy, attrs, cfgs, JUSTR_MAX_CONFIGS, &n) || n <= 0) {
        EGLint fb[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5,
            EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 0, EGL_NONE
        };
        if (!eglChooseConfig(dpy, fb, cfgs, JUSTR_MAX_CONFIGS, &n) || n <= 0)
            return NULL;
    }
    return cfgs[0];
}

/* === Environment === */

static void read_env_config(void) {
    const char *e;
    e = getenv("JUSTR_BACKEND");
    if (e) {
        if (!strcmp(e, "vulkan")) justr_set_backend(JUSTR_BACKEND_VULKAN);
        else if (!strcmp(e, "opengles")) justr_set_backend(JUSTR_BACKEND_OPENGLES);
        else justr_set_backend(JUSTR_BACKEND_AUTO);
    }
    e = getenv("JUSTR_FSR_MODE");
    if (e) {
        if (!strcmp(e, "performance")) justr_set_fsr_mode(JUSTR_FSR_PERFORMANCE);
        else if (!strcmp(e, "balanced")) justr_set_fsr_mode(JUSTR_FSR_BALANCED);
        else if (!strcmp(e, "quality")) justr_set_fsr_mode(JUSTR_FSR_QUALITY);
        else if (!strcmp(e, "ultra_quality")) justr_set_fsr_mode(JUSTR_FSR_ULTRA_QUALITY);
        else justr_set_fsr_mode(JUSTR_FSR_OFF);
    }
    e = getenv("JUSTR_FSR_SHARPENING");
    if (e) justr_set_fsr_sharpening((float)atof(e));
    e = getenv("JUSTR_VSYNC");
    if (e) g_justr_ctx.vsync = (atoi(e) != 0);
    e = getenv("JUSTR_MSAA");
    if (e) {
        int m = atoi(e);
        if (m == 2 || m == 4 || m == 8) { g_justr_ctx.samples = m; g_justr_ctx.sample_buffers = 1; }
    }
    e = getenv("JUSTR_CUSTOM_SCALE");
    if (e) justr_set_custom_scale((float)atof(e));
    e = getenv("JUSTR_DEBUG");
    if (e) g_justr_ctx.debug_log = (atoi(e) != 0);
}

/* === GLES Backend === */

EGLBoolean justr_egl_init(ANativeWindow *window) {
    if (g_justr_ctx.initialized && g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_OPENGLES)
        justr_egl_terminate();
    if (!window) return EGL_FALSE;
    g_justr_ctx.window = window;
    g_justr_ctx.width = ANativeWindow_getWidth(window);
    g_justr_ctx.height = ANativeWindow_getHeight(window);
    LOGI("GLES init %dx%d", g_justr_ctx.width, g_justr_ctx.height);

    g_justr_ctx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_justr_ctx.display == EGL_NO_DISPLAY) return EGL_FALSE;
    EGLint major, minor;
    if (!eglInitialize(g_justr_ctx.display, &major, &minor)) return EGL_FALSE;
    g_justr_ctx.config = justr_choose_config(g_justr_ctx.display);
    if (!g_justr_ctx.config) { eglTerminate(g_justr_ctx.display); g_justr_ctx.display = EGL_NO_DISPLAY; return EGL_FALSE; }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) { eglTerminate(g_justr_ctx.display); g_justr_ctx.display = EGL_NO_DISPLAY; return EGL_FALSE; }

    g_justr_ctx.active_backend = JUSTR_BACKEND_ACTIVE_OPENGLES;
    g_justr_ctx.initialized = true;

    if (g_justr_ctx.fsr_enabled) {
        if (justr_fsr_init(&g_justr_ctx.fsr, g_justr_ctx.width, g_justr_ctx.height)) {
            justr_fsr_set_mode(&g_justr_ctx.fsr, g_justr_ctx.fsr_mode);
            justr_fsr_set_sharpening(&g_justr_ctx.fsr, g_justr_ctx.fsr_sharpening);
        } else { g_justr_ctx.fsr_enabled = false; }
    }
    LOGI("GLES ready (FSR: %s)", g_justr_ctx.fsr_enabled ? justr_fsr_get_mode_name(g_justr_ctx.fsr_mode) : "off");
    return EGL_TRUE;
}

EGLContext justr_egl_create_context(EGLDisplay dpy, EGLConfig cfg, EGLContext share, const EGLint *attrs) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (!cfg) cfg = g_justr_ctx.config;
    EGLint def[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    if (!attrs) attrs = def;
    EGLContext ctx = eglCreateContext(dpy, cfg, share, attrs);
    if (ctx == EGL_NO_CONTEXT) {
        EGLint fb[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        ctx = eglCreateContext(dpy, cfg, share, fb);
        if (ctx == EGL_NO_CONTEXT) return EGL_NO_CONTEXT;
        LOGI("ES 2.0 fallback");
    }
    g_justr_ctx.context = ctx;
    return ctx;
}

EGLSurface justr_egl_create_window_surface(EGLDisplay dpy, EGLConfig cfg, ANativeWindow *win, const EGLint *attrs) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (!cfg) cfg = g_justr_ctx.config;
    if (!win) win = g_justr_ctx.window;
    EGLSurface s = eglCreateWindowSurface(dpy, cfg, win, attrs);
    if (s == EGL_NO_SURFACE) { justr_check_egl_error("eglCreateWindowSurface"); return EGL_NO_SURFACE; }
    g_justr_ctx.surface = s;
    return s;
}

EGLBoolean justr_egl_make_current(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (!eglMakeCurrent(dpy, draw, read, ctx)) { justr_check_egl_error("eglMakeCurrent"); return EGL_FALSE; }
    if (ctx != EGL_NO_CONTEXT && draw != EGL_NO_SURFACE) {
        eglQuerySurface(dpy, draw, EGL_WIDTH, &g_justr_ctx.width);
        eglQuerySurface(dpy, draw, EGL_HEIGHT, &g_justr_ctx.height);
        if (g_justr_ctx.fsr.initialized)
            justr_fsr_set_output_size(&g_justr_ctx.fsr, g_justr_ctx.width, g_justr_ctx.height);
    }
    return EGL_TRUE;
}

EGLBoolean justr_egl_swap_buffers(EGLDisplay dpy, EGLSurface s) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (s == EGL_NO_SURFACE) s = g_justr_ctx.surface;
    if (g_justr_ctx.fsr_enabled && g_justr_ctx.fsr.initialized) justr_fsr_end(&g_justr_ctx.fsr);
    EGLBoolean r = eglSwapBuffers(dpy, s);
    if (!r) { EGLint e = eglGetError(); if (e != EGL_SUCCESS) LOGW("swap failed 0x%x", e); return EGL_FALSE; }
    if (g_justr_ctx.fsr_enabled && g_justr_ctx.fsr.initialized) justr_fsr_begin(&g_justr_ctx.fsr);
    return EGL_TRUE;
}

EGLBoolean justr_egl_swap_interval(EGLDisplay dpy, EGLint i) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    EGLBoolean r = eglSwapInterval(dpy, i);
    if (r) g_justr_ctx.vsync = (i > 0);
    return r;
}

EGLBoolean justr_egl_destroy_context(EGLDisplay dpy, EGLContext ctx) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (ctx == EGL_NO_CONTEXT) return EGL_TRUE;
    if (eglGetCurrentContext() == ctx) eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    EGLBoolean r = eglDestroyContext(dpy, ctx);
    if (ctx == g_justr_ctx.context) g_justr_ctx.context = EGL_NO_CONTEXT;
    return r;
}

EGLBoolean justr_egl_destroy_surface(EGLDisplay dpy, EGLSurface s) {
    if (dpy == EGL_NO_DISPLAY) dpy = g_justr_ctx.display;
    if (s == EGL_NO_SURFACE) return EGL_TRUE;
    EGLBoolean r = eglDestroySurface(dpy, s);
    if (s == g_justr_ctx.surface) g_justr_ctx.surface = EGL_NO_SURFACE;
    return r;
}

void justr_egl_terminate(void) {
    LOGI("GLES terminate");
    if (g_justr_ctx.fsr.initialized) justr_fsr_terminate(&g_justr_ctx.fsr);
    if (g_justr_ctx.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_justr_ctx.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_justr_ctx.surface != EGL_NO_SURFACE) eglDestroySurface(g_justr_ctx.display, g_justr_ctx.surface);
        if (g_justr_ctx.context != EGL_NO_CONTEXT) eglDestroyContext(g_justr_ctx.display, g_justr_ctx.context);
        eglTerminate(g_justr_ctx.display);
        g_justr_ctx.display = EGL_NO_DISPLAY;
    }
    g_justr_ctx.config = NULL;
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        g_justr_ctx.active_backend = JUSTR_BACKEND_NONE;
        g_justr_ctx.initialized = false;
    }
    g_justr_ctx.width = 0; g_justr_ctx.height = 0;
}

/* === Unified === */

EGLBoolean justr_render_init(ANativeWindow *window) {
    if (g_justr_ctx.initialized) justr_render_terminate();
    if (!window) return EGL_FALSE;
    read_env_config();
    g_justr_ctx.window = window;
    g_justr_ctx.width = ANativeWindow_getWidth(window);
    g_justr_ctx.height = ANativeWindow_getHeight(window);
    LOGI("=== JustrRender v%s init %dx%d ===", JUSTR_RENDER_VERSION, g_justr_ctx.width, g_justr_ctx.height);
    LOGI("backend=%s fsr=%s vsync=%d msaa=%d",
         justr_get_backend_name(),
         g_justr_ctx.fsr_enabled ? justr_fsr_get_mode_name(g_justr_ctx.fsr_mode) : "off",
         g_justr_ctx.vsync, g_justr_ctx.samples);

    g_justr_ctx.vulkan_available = justr_vk_probe();
    bool try_vk = (g_justr_ctx.requested_backend == JUSTR_BACKEND_AUTO && g_justr_ctx.vulkan_available)
                  || (g_justr_ctx.requested_backend == JUSTR_BACKEND_VULKAN);

    if (try_vk && justr_vk_init(window)) {
        g_justr_ctx.active_backend = JUSTR_BACKEND_ACTIVE_VULKAN;
        g_justr_ctx.initialized = true;
        justr_vk_get_extent(&g_justr_ctx.width, &g_justr_ctx.height);
        LOGI("=== Vulkan ACTIVE (%s) ===", g_justr_vk.device_name);
        return EGL_TRUE;
    }
    if (g_justr_ctx.requested_backend == JUSTR_BACKEND_VULKAN) return EGL_FALSE;
    if (try_vk) LOGI("Vulkan failed, fallback to GLES");

    if (justr_egl_init(window)) {
        LOGI("=== OpenGL ES ACTIVE ===");
        return EGL_TRUE;
    }
    return EGL_FALSE;
}

void justr_render_terminate(void) {
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN: justr_vk_terminate(); break;
        case JUSTR_BACKEND_ACTIVE_OPENGLES: justr_egl_terminate(); break;
        default: break;
    }
    g_justr_ctx.active_backend = JUSTR_BACKEND_NONE;
    g_justr_ctx.initialized = false;
    g_justr_ctx.window = NULL;
    g_justr_ctx.width = 0; g_justr_ctx.height = 0;
}

EGLBoolean justr_render_swap_buffers(void) {
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN: {
            uint32_t idx;
            if (!justr_vk_acquire_next_image(&idx)) return EGL_FALSE;
            if (!justr_vk_present()) return EGL_FALSE;
            return EGL_TRUE;
        }
        case JUSTR_BACKEND_ACTIVE_OPENGLES:
            return justr_egl_swap_buffers(g_justr_ctx.display, g_justr_ctx.surface);
        default: return EGL_FALSE;
    }
}

/* === Info === */

const char *justr_get_renderer_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN)
        return g_justr_vk.device_name[0] ? g_justr_vk.device_name : "Vulkan Device";
    const char *r = (const char *)glGetString(GL_RENDERER);
    return r ? r : "Unknown";
}
const char *justr_get_version_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) return justr_vk_get_api_version_string();
    const char *v = (const char *)glGetString(GL_VERSION);
    return v ? v : "Unknown";
}
const char *justr_get_vendor_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) return "Vulkan";
    const char *v = (const char *)glGetString(GL_VENDOR);
    return v ? v : "Unknown";
}

/* === Utils === */

void justr_get_surface_size(int *w, int *h) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) { justr_vk_get_extent(w, h); return; }
    if (w) *w = g_justr_ctx.width;
    if (h) *h = g_justr_ctx.height;
}

void justr_set_vsync(bool en) {
    g_justr_ctx.vsync = en;
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN: justr_vk_set_vsync(en); break;
        case JUSTR_BACKEND_ACTIVE_OPENGLES:
            if (g_justr_ctx.display != EGL_NO_DISPLAY) justr_egl_swap_interval(g_justr_ctx.display, en ? 1 : 0);
            break;
        default: break;
    }
}
bool justr_get_vsync(void) { return g_justr_ctx.vsync; }

__attribute__((constructor))
static void justr_render_ctor(void) {
    LOGI("JustrRender v%s loaded", JUSTR_RENDER_VERSION);
    LOGI("Features: Dual-backend + FSR 1.0 + VSync + MSAA");
}

__attribute__((destructor))
static void justr_render_dtor(void) {
    if (g_justr_ctx.initialized) justr_render_terminate();
}
