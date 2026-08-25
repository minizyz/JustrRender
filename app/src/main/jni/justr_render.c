/*
 * JustrRender - Fold Craft Launcher Renderer Plugin
 * Main renderer implementation with dual-backend support.
 *
 * Priority: Vulkan -> OpenGL ES 3.0 -> OpenGL ES 2.0 (fallback)
 *
 * Implements automatic backend selection, EGL context management,
 * and Vulkan WSI swapchain for Minecraft: Java Edition on Android.
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

/* Global renderer context */
justr_render_context_t g_justr_ctx = {
    .display = EGL_NO_DISPLAY,
    .config = NULL,
    .context = EGL_NO_CONTEXT,
    .surface = EGL_NO_SURFACE,
    .window = NULL,
    .width = 0,
    .height = 0,
    .red_size = 8,
    .green_size = 8,
    .blue_size = 8,
    .alpha_size = 8,
    .depth_size = 24,
    .stencil_size = 8,
    .sample_buffers = 0,
    .samples = 0,
    .requested_backend = JUSTR_BACKEND_AUTO,
    .active_backend = JUSTR_BACKEND_NONE,
    .initialized = false,
    .vsync = true,
    .vulkan_available = false,
};

/* === Backend Selection === */

void justr_set_backend(justr_backend_t backend) {
    g_justr_ctx.requested_backend = backend;
    const char *name;
    switch (backend) {
        case JUSTR_BACKEND_AUTO:     name = "AUTO"; break;
        case JUSTR_BACKEND_VULKAN:   name = "VULKAN"; break;
        case JUSTR_BACKEND_OPENGLES: name = "OPENGLES"; break;
        default:                     name = "UNKNOWN"; break;
    }
    LOGI("Requested backend set to: %s", name);
}

justr_active_backend_t justr_get_active_backend(void) {
    return g_justr_ctx.active_backend;
}

const char *justr_get_backend_name(void) {
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN:   return "Vulkan";
        case JUSTR_BACKEND_ACTIVE_OPENGLES: return "OpenGL ES";
        default:                            return "None";
    }
}

bool justr_is_vulkan_available(void) {
    return g_justr_ctx.vulkan_available;
}

/* === EGL Error Checking === */

bool justr_check_egl_error(const char *operation) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        const char *error_str;
        switch (error) {
            case EGL_NOT_INITIALIZED:    error_str = "EGL_NOT_INITIALIZED"; break;
            case EGL_BAD_ACCESS:         error_str = "EGL_BAD_ACCESS"; break;
            case EGL_BAD_ALLOC:          error_str = "EGL_BAD_ALLOC"; break;
            case EGL_BAD_ATTRIBUTE:      error_str = "EGL_BAD_ATTRIBUTE"; break;
            case EGL_BAD_CONFIG:         error_str = "EGL_BAD_CONFIG"; break;
            case EGL_BAD_CONTEXT:        error_str = "EGL_BAD_CONTEXT"; break;
            case EGL_BAD_CURRENT_SURFACE:error_str = "EGL_BAD_CURRENT_SURFACE"; break;
            case EGL_BAD_DISPLAY:        error_str = "EGL_BAD_DISPLAY"; break;
            case EGL_BAD_MATCH:          error_str = "EGL_BAD_MATCH"; break;
            case EGL_BAD_NATIVE_PIXMAP:  error_str = "EGL_BAD_NATIVE_PIXMAP"; break;
            case EGL_BAD_NATIVE_WINDOW:  error_str = "EGL_BAD_NATIVE_WINDOW"; break;
            case EGL_BAD_PARAMETER:      error_str = "EGL_BAD_PARAMETER"; break;
            case EGL_BAD_SURFACE:        error_str = "EGL_BAD_SURFACE"; break;
            case EGL_CONTEXT_LOST:       error_str = "EGL_CONTEXT_LOST"; break;
            default:                     error_str = "UNKNOWN"; break;
        }
        LOGE("EGL error 0x%x (%s) during: %s", error, error_str, operation);
        return false;
    }
    return true;
}

bool justr_check_gl_error(const char *operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        const char *error_str;
        switch (error) {
            case GL_INVALID_ENUM:      error_str = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE:     error_str = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: error_str = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY:     error_str = "GL_OUT_OF_MEMORY"; break;
            default:                   error_str = "UNKNOWN"; break;
        }
        LOGE("GL error 0x%x (%s) during: %s", error, error_str, operation);
        return false;
    }
    return true;
}

/* === EGL Configuration (GLES Backend) === */

static EGLConfig justr_choose_config(EGLDisplay display) {
    EGLint config_attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        g_justr_ctx.red_size,
        EGL_GREEN_SIZE,      g_justr_ctx.green_size,
        EGL_BLUE_SIZE,       g_justr_ctx.blue_size,
        EGL_ALPHA_SIZE,      g_justr_ctx.alpha_size,
        EGL_DEPTH_SIZE,      g_justr_ctx.depth_size,
        EGL_STENCIL_SIZE,    g_justr_ctx.stencil_size,
        EGL_NONE
    };

    EGLConfig configs[JUSTR_MAX_CONFIGS];
    EGLint num_configs = 0;

    if (!eglChooseConfig(display, config_attribs, configs,
                         JUSTR_MAX_CONFIGS, &num_configs)) {
        LOGE("eglChooseConfig failed");
        justr_check_egl_error("eglChooseConfig");
        return NULL;
    }

    if (num_configs <= 0) {
        LOGW("No config matching requested attributes, trying fallback");
        EGLint fallback_attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
            EGL_RED_SIZE,        5,
            EGL_GREEN_SIZE,      6,
            EGL_BLUE_SIZE,       5,
            EGL_DEPTH_SIZE,      16,
            EGL_STENCIL_SIZE,    0,
            EGL_NONE
        };
        if (!eglChooseConfig(display, fallback_attribs, configs,
                             JUSTR_MAX_CONFIGS, &num_configs)) {
            LOGE("Fallback eglChooseConfig also failed");
            justr_check_egl_error("eglChooseConfig(fallback)");
            return NULL;
        }
        if (num_configs <= 0) {
            LOGE("No EGL config available at all");
            return NULL;
        }
    }

    LOGI("Found %d EGL configs, using first", num_configs);

    EGLint val;
    eglGetConfigAttrib(display, configs[0], EGL_RED_SIZE, &val);
    LOGI("  RED_SIZE: %d", val);
    eglGetConfigAttrib(display, configs[0], EGL_GREEN_SIZE, &val);
    LOGI("  GREEN_SIZE: %d", val);
    eglGetConfigAttrib(display, configs[0], EGL_BLUE_SIZE, &val);
    LOGI("  BLUE_SIZE: %d", val);
    eglGetConfigAttrib(display, configs[0], EGL_ALPHA_SIZE, &val);
    LOGI("  ALPHA_SIZE: %d", val);
    eglGetConfigAttrib(display, configs[0], EGL_DEPTH_SIZE, &val);
    LOGI("  DEPTH_SIZE: %d", val);
    eglGetConfigAttrib(display, configs[0], EGL_STENCIL_SIZE, &val);
    LOGI("  STENCIL_SIZE: %d", val);

    return configs[0];
}

/* === GLES Backend Functions === */

EGLBoolean justr_egl_init(ANativeWindow *window) {
    if (g_justr_ctx.initialized && g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        LOGW("GLES renderer already initialized, reinitializing");
        justr_egl_terminate();
    }

    if (window == NULL) {
        LOGE("Cannot initialize with NULL window");
        return EGL_FALSE;
    }

    g_justr_ctx.window = window;
    g_justr_ctx.width = ANativeWindow_getWidth(window);
    g_justr_ctx.height = ANativeWindow_getHeight(window);
    LOGI("Initializing GLES backend on window %dx%d",
         g_justr_ctx.width, g_justr_ctx.height);

    g_justr_ctx.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_justr_ctx.display == EGL_NO_DISPLAY) {
        LOGE("eglGetDisplay failed");
        justr_check_egl_error("eglGetDisplay");
        return EGL_FALSE;
    }

    EGLint major, minor;
    if (!eglInitialize(g_justr_ctx.display, &major, &minor)) {
        LOGE("eglInitialize failed");
        justr_check_egl_error("eglInitialize");
        return EGL_FALSE;
    }
    LOGI("EGL initialized: version %d.%d", major, minor);

    g_justr_ctx.config = justr_choose_config(g_justr_ctx.display);
    if (g_justr_ctx.config == NULL) {
        LOGE("Failed to choose EGL config");
        eglTerminate(g_justr_ctx.display);
        g_justr_ctx.display = EGL_NO_DISPLAY;
        return EGL_FALSE;
    }

    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        LOGE("eglBindAPI failed");
        justr_check_egl_error("eglBindAPI");
        eglTerminate(g_justr_ctx.display);
        g_justr_ctx.display = EGL_NO_DISPLAY;
        return EGL_FALSE;
    }

    g_justr_ctx.active_backend = JUSTR_BACKEND_ACTIVE_OPENGLES;
    g_justr_ctx.initialized = true;
    LOGI("GLES backend initialized successfully");
    return EGL_TRUE;
}

EGLContext justr_egl_create_context(EGLDisplay display, EGLConfig config,
                                    EGLContext share_context,
                                    const EGLint *attrib_list) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;
    if (config == NULL) config = g_justr_ctx.config;

    EGLint default_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    if (attrib_list == NULL) attrib_list = default_attribs;

    EGLContext context = eglCreateContext(display, config, share_context, attrib_list);
    if (context == EGL_NO_CONTEXT) {
        LOGE("eglCreateContext failed (trying ES 2.0 fallback)");
        justr_check_egl_error("eglCreateContext(ES3)");

        EGLint fallback_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        context = eglCreateContext(display, config, share_context, fallback_attribs);
        if (context == EGL_NO_CONTEXT) {
            LOGE("eglCreateContext ES 2.0 fallback also failed");
            justr_check_egl_error("eglCreateContext(ES2)");
            return EGL_NO_CONTEXT;
        }
        LOGI("Created OpenGL ES 2.0 context (fallback)");
    } else {
        LOGI("Created OpenGL ES 3.0 context");
    }

    g_justr_ctx.context = context;
    return context;
}

EGLSurface justr_egl_create_window_surface(EGLDisplay display, EGLConfig config,
                                           ANativeWindow *window,
                                           const EGLint *attrib_list) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;
    if (config == NULL) config = g_justr_ctx.config;
    if (window == NULL) window = g_justr_ctx.window;

    EGLSurface surface = eglCreateWindowSurface(display, config, window, attrib_list);
    if (surface == EGL_NO_SURFACE) {
        LOGE("eglCreateWindowSurface failed");
        justr_check_egl_error("eglCreateWindowSurface");
        return EGL_NO_SURFACE;
    }

    g_justr_ctx.surface = surface;
    LOGI("Created EGL window surface");
    return surface;
}

EGLBoolean justr_egl_make_current(EGLDisplay display, EGLSurface draw,
                                  EGLSurface read, EGLContext context) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;

    EGLBoolean result = eglMakeCurrent(display, draw, read, context);
    if (!result) {
        LOGE("eglMakeCurrent failed");
        justr_check_egl_error("eglMakeCurrent");
        return EGL_FALSE;
    }

    if (context != EGL_NO_CONTEXT && draw != EGL_NO_SURFACE) {
        eglQuerySurface(display, draw, EGL_WIDTH, &g_justr_ctx.width);
        eglQuerySurface(display, draw, EGL_HEIGHT, &g_justr_ctx.height);
        LOGD("Context current, surface size: %dx%d",
             g_justr_ctx.width, g_justr_ctx.height);
    }

    return EGL_TRUE;
}

EGLBoolean justr_egl_swap_buffers(EGLDisplay display, EGLSurface surface) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;
    if (surface == EGL_NO_SURFACE) surface = g_justr_ctx.surface;

    EGLBoolean result = eglSwapBuffers(display, surface);
    if (!result) {
        EGLint error = eglGetError();
        if (error == EGL_CONTEXT_LOST) {
            LOGE("EGL context lost during swapBuffers!");
        } else if (error != EGL_SUCCESS) {
            LOGW("eglSwapBuffers failed: 0x%x", error);
        }
        return EGL_FALSE;
    }
    return EGL_TRUE;
}

EGLBoolean justr_egl_swap_interval(EGLDisplay display, EGLint interval) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;

    EGLBoolean result = eglSwapInterval(display, interval);
    if (result) {
        g_justr_ctx.vsync = (interval > 0);
        LOGI("Swap interval set to %d (vsync=%s)", interval,
             g_justr_ctx.vsync ? "on" : "off");
    } else {
        LOGW("eglSwapInterval(%d) failed", interval);
        justr_check_egl_error("eglSwapInterval");
    }
    return result;
}

EGLBoolean justr_egl_destroy_context(EGLDisplay display, EGLContext context) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;
    if (context == EGL_NO_CONTEXT) return EGL_TRUE;

    EGLContext current = eglGetCurrentContext();
    if (current == context) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    EGLBoolean result = eglDestroyContext(display, context);
    if (!result) {
        LOGW("eglDestroyContext failed");
        justr_check_egl_error("eglDestroyContext");
    }
    if (context == g_justr_ctx.context) {
        g_justr_ctx.context = EGL_NO_CONTEXT;
    }
    return result;
}

EGLBoolean justr_egl_destroy_surface(EGLDisplay display, EGLSurface surface) {
    if (display == EGL_NO_DISPLAY) display = g_justr_ctx.display;
    if (surface == EGL_NO_SURFACE) return EGL_TRUE;

    EGLBoolean result = eglDestroySurface(display, surface);
    if (!result) {
        LOGW("eglDestroySurface failed");
        justr_check_egl_error("eglDestroySurface");
    }
    if (surface == g_justr_ctx.surface) {
        g_justr_ctx.surface = EGL_NO_SURFACE;
    }
    return result;
}

void justr_egl_terminate(void) {
    LOGI("Terminating GLES backend");

    if (g_justr_ctx.display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_justr_ctx.display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                       EGL_NO_CONTEXT);
        if (g_justr_ctx.surface != EGL_NO_SURFACE) {
            eglDestroySurface(g_justr_ctx.display, g_justr_ctx.surface);
            g_justr_ctx.surface = EGL_NO_SURFACE;
        }
        if (g_justr_ctx.context != EGL_NO_CONTEXT) {
            eglDestroyContext(g_justr_ctx.display, g_justr_ctx.context);
            g_justr_ctx.context = EGL_NO_CONTEXT;
        }
        eglTerminate(g_justr_ctx.display);
        g_justr_ctx.display = EGL_NO_DISPLAY;
    }

    g_justr_ctx.config = NULL;
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_OPENGLES) {
        g_justr_ctx.active_backend = JUSTR_BACKEND_NONE;
        g_justr_ctx.initialized = false;
    }
    g_justr_ctx.width = 0;
    g_justr_ctx.height = 0;

    LOGI("GLES backend terminated");
}

/* === Unified Backend Management === */

EGLBoolean justr_render_init(ANativeWindow *window) {
    if (g_justr_ctx.initialized) {
        LOGW("Renderer already initialized, reinitializing");
        justr_render_terminate();
    }

    if (window == NULL) {
        LOGE("Cannot initialize with NULL window");
        return EGL_FALSE;
    }

    g_justr_ctx.window = window;
    g_justr_ctx.width = ANativeWindow_getWidth(window);
    g_justr_ctx.height = ANativeWindow_getHeight(window);

    LOGI("=== JustrRender v%s init (window %dx%d) ===",
         JUSTR_RENDER_VERSION, g_justr_ctx.width, g_justr_ctx.height);

    /* Probe Vulkan availability (cached) */
    g_justr_ctx.vulkan_available = justr_vk_probe();

    bool try_vulkan = false;
    switch (g_justr_ctx.requested_backend) {
        case JUSTR_BACKEND_AUTO:
            try_vulkan = g_justr_ctx.vulkan_available;
            LOGI("AUTO mode: Vulkan %s, will %s",
                 g_justr_ctx.vulkan_available ? "available" : "unavailable",
                 try_vulkan ? "try Vulkan first" : "use GLES directly");
            break;
        case JUSTR_BACKEND_VULKAN:
            try_vulkan = true;
            LOGI("VULKAN mode: forcing Vulkan backend");
            break;
        case JUSTR_BACKEND_OPENGLES:
            try_vulkan = false;
            LOGI("OPENGLES mode: forcing OpenGL ES backend");
            break;
    }

    /* Try Vulkan first */
    if (try_vulkan) {
        LOGI("Attempting Vulkan backend initialization...");
        if (justr_vk_init(window)) {
            g_justr_ctx.active_backend = JUSTR_BACKEND_ACTIVE_VULKAN;
            g_justr_ctx.initialized = true;
            justr_vk_get_extent(&g_justr_ctx.width, &g_justr_ctx.height);
            LOGI("=== Vulkan backend ACTIVE (device: %s) ===",
                 g_justr_vk.device_name);
            return EGL_TRUE;
        }

        LOGW("Vulkan backend failed to initialize");
        if (g_justr_ctx.requested_backend == JUSTR_BACKEND_VULKAN) {
            LOGE("Vulkan forced but unavailable, initialization failed");
            return EGL_FALSE;
        }
        LOGI("Falling back to OpenGL ES backend...");
    }

    /* Fallback to OpenGL ES */
    if (justr_egl_init(window)) {
        LOGI("=== OpenGL ES backend ACTIVE ===");
        return EGL_TRUE;
    }

    LOGE("=== All backends failed to initialize ===");
    return EGL_FALSE;
}

void justr_render_terminate(void) {
    LOGI("Terminating JustrRender (active backend: %s)",
         justr_get_backend_name());

    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN:
            justr_vk_terminate();
            break;
        case JUSTR_BACKEND_ACTIVE_OPENGLES:
            justr_egl_terminate();
            break;
        default:
            break;
    }

    g_justr_ctx.active_backend = JUSTR_BACKEND_NONE;
    g_justr_ctx.initialized = false;
    g_justr_ctx.window = NULL;
    g_justr_ctx.width = 0;
    g_justr_ctx.height = 0;

    LOGI("JustrRender fully terminated");
}

EGLBoolean justr_render_swap_buffers(void) {
    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN: {
            uint32_t image_index;
            if (!justr_vk_acquire_next_image(&image_index)) {
                LOGE("Vulkan: failed to acquire next image");
                return EGL_FALSE;
            }
            if (!justr_vk_present()) {
                LOGE("Vulkan: failed to present");
                return EGL_FALSE;
            }
            return EGL_TRUE;
        }
        case JUSTR_BACKEND_ACTIVE_OPENGLES:
            return justr_egl_swap_buffers(g_justr_ctx.display, g_justr_ctx.surface);
        default:
            LOGE("No active backend for swap_buffers");
            return EGL_FALSE;
    }
}

/* === Info Queries === */

const char *justr_get_renderer_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return g_justr_vk.device_name[0] ? g_justr_vk.device_name : "Vulkan Device";
    }
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    return renderer ? renderer : "Unknown";
}

const char *justr_get_version_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return justr_vk_get_api_version_string();
    }
    const char *version = (const char *)glGetString(GL_VERSION);
    return version ? version : "Unknown";
}

const char *justr_get_vendor_string(void) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) {
        return "Vulkan";
    }
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    return vendor ? vendor : "Unknown";
}

/* === Utilities === */

void justr_get_surface_size(int *width, int *height) {
    if (g_justr_ctx.active_backend == JUSTR_BACKEND_ACTIVE_VULKAN) {
        justr_vk_get_extent(width, height);
        return;
    }
    if (width) *width = g_justr_ctx.width;
    if (height) *height = g_justr_ctx.height;
}

void justr_set_vsync(bool enabled) {
    g_justr_ctx.vsync = enabled;

    switch (g_justr_ctx.active_backend) {
        case JUSTR_BACKEND_ACTIVE_VULKAN:
            justr_vk_set_vsync(enabled);
            break;
        case JUSTR_BACKEND_ACTIVE_OPENGLES:
            if (g_justr_ctx.display != EGL_NO_DISPLAY) {
                justr_egl_swap_interval(g_justr_ctx.display, enabled ? 1 : 0);
            }
            break;
        default:
            break;
    }
}

bool justr_get_vsync(void) {
    return g_justr_ctx.vsync;
}

/* === Library Constructor / Destructor === */

__attribute__((constructor))
static void justr_render_init(void) {
    LOGI("JustrRender v%s loaded", JUSTR_RENDER_VERSION);
    LOGI("Renderer ID: %s", JUSTR_RENDERER_ID);
    LOGI("Dual-backend: Vulkan (preferred) + OpenGL ES (fallback)");

    /* Read backend preference from environment */
    const char *backend_env = getenv("JUSTR_BACKEND");
    if (backend_env) {
        if (strcmp(backend_env, "vulkan") == 0) {
            justr_set_backend(JUSTR_BACKEND_VULKAN);
        } else if (strcmp(backend_env, "opengles") == 0) {
            justr_set_backend(JUSTR_BACKEND_OPENGLES);
        } else {
            justr_set_backend(JUSTR_BACKEND_AUTO);
        }
    }
}

__attribute__((destructor))
static void justr_render_fini(void) {
    if (g_justr_ctx.initialized) {
        justr_render_terminate();
    }
    LOGI("JustrRender unloaded");
}
