# JustrRender - Native Renderer Build Configuration
# Build with: ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk
#
# Dual-backend renderer: Vulkan (preferred) + OpenGL ES (fallback)
# Features: FSR 1.0 super resolution, VSync, MSAA
LOCAL_PATH := $(call my-dir)

# === libjustr_render.so ===
# Main renderer library with Vulkan + GLES dual-backend + FSR 1.0
include $(CLEAR_VARS)
LOCAL_MODULE    := justr_render
LOCAL_SRC_FILES := \
    justr_render.c \
    egl_bridge.c \
    vulkan_render.c \
    fsr_render.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)
LOCAL_CFLAGS    := -Wall -Wextra -Wno-unused-parameter -O2 -fvisibility=hidden
LOCAL_LDFLAGS   := -Wl,--gc-sections
# Link against Android EGL, OpenGL ES 3.0, and Vulkan
LOCAL_LDLIBS    := -lEGL -lGLESv3 -lvulkan -landroid -llog
# Export headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
include $(BUILD_SHARED_LIBRARY)
