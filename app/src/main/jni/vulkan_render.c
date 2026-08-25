/*
 * JustrRender - Vulkan Rendering Backend Implementation
 *
 * Implements Vulkan instance/device/swapchain management with
 * Android WSI surface. Probes for Vulkan availability and
 * provides automatic fallback coordination with the GLES backend.
 */

#include "vulkan_render.h"
#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "JustrRender-VK"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

#define VK_CHECK(result, op) \
    do { \
        VkResult _r = (result); \
        if (_r != VK_SUCCESS) { \
            LOGE("%s failed: %d", op, _r); \
            return false; \
        } \
    } while(0)

/* Global Vulkan context */
justr_vk_context_t g_justr_vk = {
    .instance = VK_NULL_HANDLE,
    .physical_device = VK_NULL_HANDLE,
    .device = VK_NULL_HANDLE,
    .graphics_queue = VK_NULL_HANDLE,
    .present_queue = VK_NULL_HANDLE,
    .surface = VK_NULL_HANDLE,
    .swapchain = VK_NULL_HANDLE,
    .surface_format = VK_FORMAT_UNDEFINED,
    .color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    .present_mode = VK_PRESENT_MODE_FIFO_KHR,
    .extent = {0, 0},
    .image_count = 0,
    .graphics_family = UINT32_MAX,
    .present_family = UINT32_MAX,
    .current_image = 0,
    .initialized = false,
    .vsync = true,
    .api_version = 0,
};

/* Synchronization primitives */
static VkSemaphore g_image_available_semaphore = VK_NULL_HANDLE;
static VkSemaphore g_render_finished_semaphore = VK_NULL_HANDLE;
static VkFence g_in_flight_fence = VK_NULL_HANDLE;

/* Required instance extensions for Android */
static const char *g_required_instance_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
};
#define REQUIRED_INSTANCE_EXT_COUNT \
    (sizeof(g_required_instance_extensions) / sizeof(g_required_instance_extensions[0]))

/* Required device extensions */
static const char *g_required_device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
#define REQUIRED_DEVICE_EXT_COUNT \
    (sizeof(g_required_device_extensions) / sizeof(g_required_device_extensions[0]))

/* === Vulkan Availability Probe === */

bool justr_vk_probe(void) {
    LOGI("Probing Vulkan availability...");

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "JustrRender",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "JustrRender",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
    };

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&create_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        LOGW("Vulkan not available: vkCreateInstance returned %d", result);
        return false;
    }

    uint32_t device_count = 0;
    result = vkEnumeratePhysicalDevices(instance, &device_count, NULL);
    vkDestroyInstance(instance, NULL);

    if (result != VK_SUCCESS || device_count == 0) {
        LOGW("Vulkan available but no physical devices found");
        return false;
    }

    LOGI("Vulkan available with %d physical device(s)", device_count);
    return true;
}

/* === Extension Support Checks === */

static bool check_instance_extensions(void) {
    uint32_t extension_count = 0;
    if (vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL) != VK_SUCCESS) {
        return false;
    }

    VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    if (!extensions) return false;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, extensions);

    bool all_found = true;
    for (uint32_t i = 0; i < REQUIRED_INSTANCE_EXT_COUNT; i++) {
        bool found = false;
        for (uint32_t j = 0; j < extension_count; j++) {
            if (strcmp(g_required_instance_extensions[i], extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOGE("Required instance extension not found: %s",
                 g_required_instance_extensions[i]);
            all_found = false;
        }
    }

    free(extensions);
    return all_found;
}

static bool check_device_extensions(VkPhysicalDevice device) {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    if (!extensions) return false;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, extensions);

    bool all_found = true;
    for (uint32_t i = 0; i < REQUIRED_DEVICE_EXT_COUNT; i++) {
        bool found = false;
        for (uint32_t j = 0; j < extension_count; j++) {
            if (strcmp(g_required_device_extensions[i], extensions[j].extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            LOGE("Required device extension not found: %s",
                 g_required_device_extensions[i]);
            all_found = false;
        }
    }

    free(extensions);
    return all_found;
}

/* === Queue Family Selection === */

static bool find_queue_families(VkPhysicalDevice device) {
    uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, NULL);

    VkQueueFamilyProperties *families = malloc(family_count * sizeof(VkQueueFamilyProperties));
    if (!families) return false;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &family_count, families);

    g_justr_vk.graphics_family = UINT32_MAX;
    g_justr_vk.present_family = UINT32_MAX;

    for (uint32_t i = 0; i < family_count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            if (g_justr_vk.graphics_family == UINT32_MAX) {
                g_justr_vk.graphics_family = i;
            }
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_justr_vk.surface, &present_support);
        if (present_support) {
            g_justr_vk.present_family = i;
        }
    }

    free(families);

    if (g_justr_vk.graphics_family == UINT32_MAX) {
        LOGE("No graphics queue family found");
        return false;
    }
    if (g_justr_vk.present_family == UINT32_MAX) {
        LOGE("No present queue family found");
        return false;
    }

    LOGI("Queue families: graphics=%u present=%u",
         g_justr_vk.graphics_family, g_justr_vk.present_family);
    return true;
}

/* === Physical Device Selection === */

static bool select_physical_device(void) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(g_justr_vk.instance, &device_count, NULL);
    if (device_count == 0) {
        LOGE("No Vulkan physical devices found");
        return false;
    }

    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    if (!devices) return false;
    vkEnumeratePhysicalDevices(g_justr_vk.instance, &device_count, devices);

    g_justr_vk.physical_device = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);

        LOGI("Device %u: %s (type=%d, API=%d.%d.%d)",
             i, props.deviceName, props.deviceType,
             VK_VERSION_MAJOR(props.apiVersion),
             VK_VERSION_MINOR(props.apiVersion),
             VK_VERSION_PATCH(props.apiVersion));

        if (!check_device_extensions(devices[i])) {
            LOGW("Device %s missing required extensions, skipping", props.deviceName);
            continue;
        }

        if (!find_queue_families(devices[i])) {
            LOGW("Device %s missing required queue families, skipping", props.deviceName);
            continue;
        }

        g_justr_vk.physical_device = devices[i];
        g_justr_vk.api_version = props.apiVersion;
        strncpy(g_justr_vk.device_name, props.deviceName,
                sizeof(g_justr_vk.device_name) - 1);

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            break;
        }
    }

    free(devices);

    if (g_justr_vk.physical_device == VK_NULL_HANDLE) {
        LOGE("No suitable Vulkan physical device found");
        return false;
    }

    LOGI("Selected physical device: %s", g_justr_vk.device_name);
    return true;
}

/* === Logical Device Creation === */

static bool create_logical_device(void) {
    float queue_priority = 1.0f;

    uint32_t queue_families[2];
    uint32_t queue_count = 0;
    queue_families[queue_count++] = g_justr_vk.graphics_family;
    if (g_justr_vk.present_family != g_justr_vk.graphics_family) {
        queue_families[queue_count++] = g_justr_vk.present_family;
    }

    VkDeviceQueueCreateInfo queue_infos[2];
    memset(queue_infos, 0, sizeof(queue_infos));
    for (uint32_t i = 0; i < queue_count; i++) {
        queue_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[i].queueFamilyIndex = queue_families[i];
        queue_infos[i].queueCount = 1;
        queue_infos[i].pQueuePriorities = &queue_priority;
    }

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queue_count,
        .pQueueCreateInfos = queue_infos,
        .enabledExtensionCount = REQUIRED_DEVICE_EXT_COUNT,
        .ppEnabledExtensionNames = g_required_device_extensions,
    };

    VK_CHECK(vkCreateDevice(g_justr_vk.physical_device, &create_info, NULL,
                            &g_justr_vk.device), "vkCreateDevice");

    vkGetDeviceQueue(g_justr_vk.device, g_justr_vk.graphics_family, 0,
                     &g_justr_vk.graphics_queue);
    vkGetDeviceQueue(g_justr_vk.device, g_justr_vk.present_family, 0,
                     &g_justr_vk.present_queue);

    LOGI("Logical device created");
    return true;
}

/* === Surface Creation === */

static bool create_surface(ANativeWindow *window) {
    VkAndroidSurfaceCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = window,
    };

    VK_CHECK(vkCreateAndroidSurfaceKHR(g_justr_vk.instance, &create_info, NULL,
                                       &g_justr_vk.surface),
             "vkCreateAndroidSurfaceKHR");

    LOGI("Android surface created");
    return true;
}

/* === Swapchain === */

static bool choose_swapchain_format(void) {
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_justr_vk.physical_device,
                                         g_justr_vk.surface, &format_count, NULL);
    if (format_count == 0) return false;

    VkSurfaceFormatKHR *formats = malloc(format_count * sizeof(VkSurfaceFormatKHR));
    if (!formats) return false;
    vkGetPhysicalDeviceSurfaceFormatsKHR(g_justr_vk.physical_device,
                                         g_justr_vk.surface, &format_count, formats);

    g_justr_vk.surface_format = VK_FORMAT_B8G8R8A8_SRGB;
    g_justr_vk.color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    for (uint32_t i = 0; i < format_count; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            g_justr_vk.surface_format = formats[i].format;
            g_justr_vk.color_space = formats[i].colorSpace;
            break;
        }
    }

    if (g_justr_vk.surface_format == VK_FORMAT_B8G8R8A8_SRGB && format_count > 0) {
        bool found = false;
        for (uint32_t i = 0; i < format_count; i++) {
            if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB) {
                found = true;
                break;
            }
        }
        if (!found) {
            g_justr_vk.surface_format = formats[0].format;
            g_justr_vk.color_space = formats[0].colorSpace;
        }
    }

    LOGI("Surface format: %d, color space: %d",
         g_justr_vk.surface_format, g_justr_vk.color_space);
    free(formats);
    return true;
}

static bool choose_present_mode(void) {
    uint32_t mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_justr_vk.physical_device,
                                              g_justr_vk.surface, &mode_count, NULL);
    if (mode_count == 0) return false;

    VkPresentModeKHR *modes = malloc(mode_count * sizeof(VkPresentModeKHR));
    if (!modes) return false;
    vkGetPhysicalDeviceSurfacePresentModesKHR(g_justr_vk.physical_device,
                                              g_justr_vk.surface, &mode_count, modes);

    g_justr_vk.present_mode = VK_PRESENT_MODE_FIFO_KHR;

    if (!g_justr_vk.vsync) {
        for (uint32_t i = 0; i < mode_count; i++) {
            if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                g_justr_vk.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
                break;
            }
        }
        if (g_justr_vk.present_mode == VK_PRESENT_MODE_FIFO_KHR) {
            for (uint32_t i = 0; i < mode_count; i++) {
                if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                    g_justr_vk.present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                    break;
                }
            }
        }
    }

    LOGI("Present mode: %d (vsync=%s)", g_justr_vk.present_mode,
         g_justr_vk.vsync ? "on" : "off");
    free(modes);
    return true;
}

bool justr_vk_create_swapchain(void) {
    if (g_justr_vk.surface == VK_NULL_HANDLE) {
        LOGE("Cannot create swapchain: no surface");
        return false;
    }

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_justr_vk.physical_device,
                                              g_justr_vk.surface, &caps);

    if (caps.currentExtent.width != UINT32_MAX) {
        g_justr_vk.extent = caps.currentExtent;
    } else {
        g_justr_vk.extent.width = caps.minImageExtent.width;
        g_justr_vk.extent.height = caps.minImageExtent.height;
    }

    LOGI("Swapchain extent: %ux%u", g_justr_vk.extent.width, g_justr_vk.extent.height);

    choose_swapchain_format();
    choose_present_mode();

    uint32_t image_count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
        image_count = caps.maxImageCount;
    }
    if (image_count > JUSTR_VK_MAX_IMAGES) {
        image_count = JUSTR_VK_MAX_IMAGES;
    }

    uint32_t queue_family_indices[] = {
        g_justr_vk.graphics_family,
        g_justr_vk.present_family
    };

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = g_justr_vk.surface,
        .minImageCount = image_count,
        .imageFormat = g_justr_vk.surface_format,
        .imageColorSpace = g_justr_vk.color_space,
        .imageExtent = g_justr_vk.extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .imageSharingMode = (g_justr_vk.graphics_family != g_justr_vk.present_family)
                            ? VK_SHARING_MODE_CONCURRENT
                            : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = (g_justr_vk.graphics_family != g_justr_vk.present_family) ? 2 : 0,
        .pQueueFamilyIndices = queue_family_indices,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = g_justr_vk.present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    VK_CHECK(vkCreateSwapchainKHR(g_justr_vk.device, &create_info, NULL,
                                  &g_justr_vk.swapchain),
             "vkCreateSwapchainKHR");

    vkGetSwapchainImagesKHR(g_justr_vk.device, g_justr_vk.swapchain,
                            &g_justr_vk.image_count, NULL);
    if (g_justr_vk.image_count > JUSTR_VK_MAX_IMAGES) {
        g_justr_vk.image_count = JUSTR_VK_MAX_IMAGES;
    }
    vkGetSwapchainImagesKHR(g_justr_vk.device, g_justr_vk.swapchain,
                            &g_justr_vk.image_count, g_justr_vk.images);

    LOGI("Swapchain created with %u images", g_justr_vk.image_count);

    for (uint32_t i = 0; i < g_justr_vk.image_count; i++) {
        VkImageViewCreateInfo view_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = g_justr_vk.images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = g_justr_vk.surface_format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        if (vkCreateImageView(g_justr_vk.device, &view_info, NULL,
                              &g_justr_vk.image_views[i]) != VK_SUCCESS) {
            LOGE("Failed to create image view %u", i);
            return false;
        }
    }

    return true;
}

/* === Synchronization === */

static bool create_sync_objects(void) {
    VkSemaphoreCreateInfo sem_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    if (vkCreateSemaphore(g_justr_vk.device, &sem_info, NULL,
                          &g_image_available_semaphore) != VK_SUCCESS ||
        vkCreateSemaphore(g_justr_vk.device, &sem_info, NULL,
                          &g_render_finished_semaphore) != VK_SUCCESS ||
        vkCreateFence(g_justr_vk.device, &fence_info, NULL,
                      &g_in_flight_fence) != VK_SUCCESS) {
        LOGE("Failed to create synchronization objects");
        return false;
    }

    return true;
}

/* === Public API === */

bool justr_vk_init(ANativeWindow *window) {
    if (g_justr_vk.initialized) {
        LOGW("Vulkan already initialized, reinitializing");
        justr_vk_terminate();
    }

    if (window == NULL) {
        LOGE("Cannot init Vulkan with NULL window");
        return false;
    }

    LOGI("Initializing Vulkan backend...");

    if (!check_instance_extensions()) {
        LOGE("Required Vulkan instance extensions not available");
        return false;
    }

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "JustrRender",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "JustrRender",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = REQUIRED_INSTANCE_EXT_COUNT,
        .ppEnabledExtensionNames = g_required_instance_extensions,
    };

    VK_CHECK(vkCreateInstance(&instance_info, NULL, &g_justr_vk.instance),
             "vkCreateInstance");

    if (!create_surface(window)) {
        justr_vk_terminate();
        return false;
    }

    if (!select_physical_device()) {
        justr_vk_terminate();
        return false;
    }

    if (!create_logical_device()) {
        justr_vk_terminate();
        return false;
    }

    if (!justr_vk_create_swapchain()) {
        justr_vk_terminate();
        return false;
    }

    if (!create_sync_objects()) {
        justr_vk_terminate();
        return false;
    }

    g_justr_vk.initialized = true;
    LOGI("Vulkan backend initialized successfully on %s", g_justr_vk.device_name);
    return true;
}

bool justr_vk_acquire_next_image(uint32_t *image_index) {
    if (!g_justr_vk.initialized) return false;

    vkWaitForFences(g_justr_vk.device, 1, &g_in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(g_justr_vk.device, 1, &g_in_flight_fence);

    VkResult result = vkAcquireNextImageKHR(
        g_justr_vk.device, g_justr_vk.swapchain, UINT64_MAX,
        g_image_available_semaphore, VK_NULL_HANDLE,
        &g_justr_vk.current_image);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGW("Swapchain out of date, recreating");
        justr_vk_destroy_swapchain();
        if (!justr_vk_create_swapchain()) {
            return false;
        }
        result = vkAcquireNextImageKHR(
            g_justr_vk.device, g_justr_vk.swapchain, UINT64_MAX,
            g_image_available_semaphore, VK_NULL_HANDLE,
            &g_justr_vk.current_image);
    }

    if (result != VK_SUCCESS) {
        LOGE("vkAcquireNextImageKHR failed: %d", result);
        return false;
    }

    if (image_index) *image_index = g_justr_vk.current_image;
    return true;
}

bool justr_vk_present(void) {
    if (!g_justr_vk.initialized) return false;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &g_render_finished_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &g_justr_vk.swapchain,
        .pImageIndices = &g_justr_vk.current_image,
    };

    VkResult result = vkQueuePresentKHR(g_justr_vk.present_queue, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOGW("Present: swapchain out of date");
        return true;
    }
    if (result != VK_SUCCESS) {
        LOGE("vkQueuePresentKHR failed: %d", result);
        return false;
    }

    return true;
}

void justr_vk_wait_idle(void) {
    if (g_justr_vk.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_justr_vk.device);
    }
}

void justr_vk_destroy_swapchain(void) {
    if (g_justr_vk.device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(g_justr_vk.device);

    for (uint32_t i = 0; i < g_justr_vk.image_count; i++) {
        if (g_justr_vk.image_views[i] != VK_NULL_HANDLE) {
            vkDestroyImageView(g_justr_vk.device, g_justr_vk.image_views[i], NULL);
            g_justr_vk.image_views[i] = VK_NULL_HANDLE;
        }
    }

    if (g_justr_vk.swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(g_justr_vk.device, g_justr_vk.swapchain, NULL);
        g_justr_vk.swapchain = VK_NULL_HANDLE;
    }

    g_justr_vk.image_count = 0;
    LOGI("Swapchain destroyed");
}

void justr_vk_terminate(void) {
    LOGI("Terminating Vulkan backend");

    if (g_justr_vk.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(g_justr_vk.device);
    }

    if (g_image_available_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_justr_vk.device, g_image_available_semaphore, NULL);
        g_image_available_semaphore = VK_NULL_HANDLE;
    }
    if (g_render_finished_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(g_justr_vk.device, g_render_finished_semaphore, NULL);
        g_render_finished_semaphore = VK_NULL_HANDLE;
    }
    if (g_in_flight_fence != VK_NULL_HANDLE) {
        vkDestroyFence(g_justr_vk.device, g_in_flight_fence, NULL);
        g_in_flight_fence = VK_NULL_HANDLE;
    }

    justr_vk_destroy_swapchain();

    if (g_justr_vk.device != VK_NULL_HANDLE) {
        vkDestroyDevice(g_justr_vk.device, NULL);
        g_justr_vk.device = VK_NULL_HANDLE;
    }

    if (g_justr_vk.surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(g_justr_vk.instance, g_justr_vk.surface, NULL);
        g_justr_vk.surface = VK_NULL_HANDLE;
    }

    if (g_justr_vk.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(g_justr_vk.instance, NULL);
        g_justr_vk.instance = VK_NULL_HANDLE;
    }

    memset(&g_justr_vk, 0, sizeof(g_justr_vk));
    g_justr_vk.instance = VK_NULL_HANDLE;
    g_justr_vk.physical_device = VK_NULL_HANDLE;
    g_justr_vk.device = VK_NULL_HANDLE;
    g_justr_vk.surface = VK_NULL_HANDLE;
    g_justr_vk.swapchain = VK_NULL_HANDLE;
    g_justr_vk.graphics_family = UINT32_MAX;
    g_justr_vk.present_family = UINT32_MAX;
    g_justr_vk.vsync = true;

    LOGI("Vulkan backend terminated");
}

/* === Queries === */

void justr_vk_get_device_name(char *buf, size_t len) {
    if (buf && len > 0) {
        strncpy(buf, g_justr_vk.device_name, len - 1);
        buf[len - 1] = '\0';
    }
}

const char *justr_vk_get_api_version_string(void) {
    static char version_str[32];
    snprintf(version_str, sizeof(version_str), "Vulkan %d.%d.%d",
             VK_VERSION_MAJOR(g_justr_vk.api_version),
             VK_VERSION_MINOR(g_justr_vk.api_version),
             VK_VERSION_PATCH(g_justr_vk.api_version));
    return version_str;
}

void justr_vk_get_extent(int *width, int *height) {
    if (width) *width = (int)g_justr_vk.extent.width;
    if (height) *height = (int)g_justr_vk.extent.height;
}

bool justr_vk_has_extension(const char *extension_name) {
    if (!extension_name || g_justr_vk.physical_device == VK_NULL_HANDLE) {
        return false;
    }

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(g_justr_vk.physical_device, NULL,
                                         &extension_count, NULL);

    VkExtensionProperties *extensions = malloc(extension_count * sizeof(VkExtensionProperties));
    if (!extensions) return false;
    vkEnumerateDeviceExtensionProperties(g_justr_vk.physical_device, NULL,
                                         &extension_count, extensions);

    bool found = false;
    for (uint32_t i = 0; i < extension_count; i++) {
        if (strcmp(extension_name, extensions[i].extensionName) == 0) {
            found = true;
            break;
        }
    }

    free(extensions);
    return found;
}

bool justr_vk_set_vsync(bool enabled) {
    g_justr_vk.vsync = enabled;
    if (g_justr_vk.initialized && g_justr_vk.swapchain != VK_NULL_HANDLE) {
        justr_vk_destroy_swapchain();
        return justr_vk_create_swapchain();
    }
    return true;
}

VkSemaphore justr_vk_get_present_semaphore(void) {
    return g_render_finished_semaphore;
}

VkFence justr_vk_get_in_flight_fence(void) {
    return g_in_flight_fence;
}
