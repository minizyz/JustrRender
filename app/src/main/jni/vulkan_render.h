/*
 * JustrRender - Vulkan Rendering Backend
 *
 * Provides Vulkan-based rendering with Android WSI surface support.
 * Used as the preferred backend when available, with automatic
 * fallback to OpenGL ES when Vulkan is not supported.
 */
#ifndef JUSTR_VULKAN_H
#define JUSTR_VULKAN_H

/* Enable Android WSI surface support in Vulkan headers.
 * Must be defined BEFORE including vulkan/vulkan.h, otherwise
 * VkAndroidSurfaceCreateInfoKHR / vkCreateAndroidSurfaceKHR /
 * VK_KHR_ANDROID_SURFACE_EXTENSION_NAME will not be declared. */
#ifndef VK_USE_PLATFORM_ANDROID_KHR
#define VK_USE_PLATFORM_ANDROID_KHR
#endif

#include <android/native_window.h>
#include <stdbool.h>
#include <stdint.h>
#include <vulkan/vulkan.h>
#ifdef __cplusplus
extern "C" {
#endif
#define JUSTR_VK_MAX_IMAGES 4
#define JUSTR_VK_MAX_EXTENSIONS 16
/* Vulkan context state */
typedef struct {
    VkInstance          instance;
    VkPhysicalDevice    physical_device;
    VkDevice            device;
    VkQueue             graphics_queue;
    VkQueue             present_queue;
    VkSurfaceKHR        surface;
    VkSwapchainKHR      swapchain;
    VkFormat            surface_format;
    VkColorSpaceKHR     color_space;
    VkPresentModeKHR    present_mode;
    VkExtent2D          extent;
    VkImage             images[JUSTR_VK_MAX_IMAGES];
    VkImageView         image_views[JUSTR_VK_MAX_IMAGES];
    uint32_t            image_count;
    uint32_t            graphics_family;
    uint32_t            present_family;
    uint32_t            current_image;
    bool                initialized;
    bool                vsync;
    uint32_t            api_version;
    char                device_name[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
    char                driver_name[256];
} justr_vk_context_t;
/* Global Vulkan context */
extern justr_vk_context_t g_justr_vk;
/* === Lifecycle === */
/* Probe if Vulkan is available on this device */
bool justr_vk_probe(void);
/* Initialize Vulkan backend with a native window */
bool justr_vk_init(ANativeWindow *window);
/* Create swapchain (call after window is ready) */
bool justr_vk_create_swapchain(void);
/* Acquire next swapchain image for rendering */
bool justr_vk_acquire_next_image(uint32_t *image_index);
/* Present the current image to the screen */
bool justr_vk_present(void);
/* Wait for device idle */
void justr_vk_wait_idle(void);
/* Destroy swapchain (for recreation on resize) */
void justr_vk_destroy_swapchain(void);
/* Full teardown */
void justr_vk_terminate(void);
/* === Queries === */
/* Get physical device properties */
void justr_vk_get_device_name(char *buf, size_t len);
/* Get Vulkan API version as string */
const char *justr_vk_get_api_version_string(void);
/* Get current swapchain dimensions */
void justr_vk_get_extent(int *width, int *height);
/* Check if a specific Vulkan extension is available */
bool justr_vk_has_extension(const char *extension_name);
/* Set present mode (vsync on/off) */
bool justr_vk_set_vsync(bool enabled);
/* === Fence/Semaphore helpers (for external rendering interop) === */
VkSemaphore justr_vk_get_present_semaphore(void);
VkFence justr_vk_get_in_flight_fence(void);
#ifdef __cplusplus
}
#endif
#endif /* JUSTR_VULKAN_H */
