# JustrRender - NDK Application Configuration
# Target architectures
APP_ABI := arm64-v8a armeabi-v7a x86_64
# Minimum Android API level (Vulkan requires API 24+, matches app minSdk 26)
APP_PLATFORM := android-26
# Use C++ static runtime (we're pure C, but set for compatibility)
APP_STL := none
# Enable optimization for release
APP_CFLAGS += -O2 -fvisibility=hidden
# Disable debug symbols in release
APP_CFLAGS += -DNDEBUG
# Enable Android WSI surface support in Vulkan headers.
# Critical: without this, VkAndroidSurfaceCreateInfoKHR /
# vkCreateAndroidSurfaceKHR / VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
# are not declared, causing compile errors on NDK r25/r26.
APP_CFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
