# JustrRender - Native Renderer Build Configuration
# Build with: ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./Android.mk

LOCAL_PATH := $(call my-dir)

# === libjustr_render.so ===
# Main renderer library providing EGL bridge and OpenGL ES 3.0 backend
include $(CLEAR_VARS)

LOCAL_MODULE    := justr_render
LOCAL_SRC_FILES := \
    justr_render.c \
    egl_bridge.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)

LOCAL_CFLAGS    := -Wall -Wextra -Wno-unused-parameter -O2 -fvisibility=hidden
LOCAL_LDFLAGS   := -Wl,--gc-sections

# Link against Android EGL and OpenGL ES 3.0
LOCAL_LDLIBS    := -lEGL -lGLESv3 -landroid -llog

# Export headers
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)
