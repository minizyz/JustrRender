# JustrRender - NDK Application Configuration

# Target architectures
APP_ABI := arm64-v8a armeabi-v7a x86_64

# Minimum Android API level (matches app minSdk 26)
APP_PLATFORM := android-26

# Use C++ static runtime (we're pure C, but set for compatibility)
APP_STL := none

# Enable optimization for release
APP_CFLAGS += -O2 -fvisibility=hidden

# Disable debug symbols in release
APP_CFLAGS += -DNDEBUG
