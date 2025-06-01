#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

static inline const char* curr_time(void)
{
    static char time_str[20];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    return time_str;
}

#define LOG_INFO(format, ...) \
    fprintf(stdout, "[%s] [%5s] %15s:%4d: " format "\n", curr_time(), "INFO", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    fprintf(stderr, "[%s] [%5s] %15s:%4d: " format "\n", curr_time(), "ERROR", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

void error_callback(int error, const char* description)
{
    LOG_ERROR("[%d] %s", error, description);
}

int main(void)
{
    GLFWwindow* window;

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    VkResult result;

    glfwSetErrorCallback(error_callback);

    if (glfwInit() != GLFW_TRUE) {
        LOG_ERROR("Failed to initialize GLFW");
    }
    LOG_INFO("GLFW initialized successfully");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    // Add this hint for Wayland compatibility (if you want to force XWayland or native Wayland)
    // For Wayland, GLFW will typically try to use Wayland natively if available.
    // If you explicitly want X11 fallback on Wayland, you might set this:
    // glfwWindowHint(GLFW_X11_PLATFORM, GLFW_PLATFORM_WAYLAND); // This is not standard; GLFW handles Wayland automatically.

    window = glfwCreateWindow(640, 480, "Hello Vulkan", NULL, NULL);
    if (window == NULL) {
        LOG_ERROR("Failed to create GLFW window");
        goto cleanup_glfw;
    }
    LOG_INFO("GLFW window created successfully");

    // Vulkan specific setup (minimal example, you'll expand this for a real Vulkan app)
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == NULL) {
        LOG_ERROR("Failed to get required Vulkan instance extensions from GLFW");
        goto cleanup_window;
    }

    LOG_INFO("Number of required Vulkan instance extensions from GLFW: %u", glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++) {
        LOG_INFO("  - %s", glfwExtensions[i]);
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_2
    };

    const char* enabledLayers[] = { "VK_LAYER_KHRONOS_validation" };
    uint32_t enabledLayerCount = sizeof(enabledLayers) / sizeof(enabledLayers[0]);

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions,
        .enabledLayerCount = enabledLayerCount,
        .ppEnabledLayerNames = enabledLayers
    };

    result = vkCreateInstance(&createInfo, NULL, &instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance: %d", result);
        goto cleanup_window;
    }
    LOG_INFO("Vulkan instance created successfully");

    // Surface
    result = glfwCreateWindowSurface(instance, window, NULL, &surface);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan surface: %d", result);
        goto cleanup_instance;
    }
    LOG_INFO("Vulkan surface created successfully");

    // Physical devices
    {
        uint32_t deviceCount = 0;
        result = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
        if (result != VK_SUCCESS || deviceCount == 0) {
            LOG_ERROR("Failed to enumerate physical devices: %d", result);
            goto cleanup_surface;
        }
        LOG_INFO("Number of physical devices available: %u", deviceCount);

        VkPhysicalDevice* physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));
        result = vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to enumerate physical devices: %d", result);
            free(physicalDevices);
            goto cleanup_surface;
        }

        physicalDevice = VK_NULL_HANDLE;

        VkPhysicalDeviceType preferredTypeOrder[] = {
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
            VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
            VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
            VK_PHYSICAL_DEVICE_TYPE_CPU
        };
        uint32_t preferredTypeOrderCount = sizeof(preferredTypeOrder) / sizeof(preferredTypeOrder[0]);

        for (uint32_t i = 0; i < preferredTypeOrderCount; i++) {
            for (uint32_t j = 0; j < deviceCount; j++) {
                VkPhysicalDeviceProperties properties;
                vkGetPhysicalDeviceProperties(physicalDevices[j], &properties);

                if (properties.deviceType == preferredTypeOrder[i]) {
                    physicalDevice = physicalDevices[j];
                    LOG_INFO("Selected physical device: %s", properties.deviceName);
                    break;
                }
            }

            if (physicalDevice != VK_NULL_HANDLE) {
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE) {
            LOG_ERROR("No suitable physical device found");
            free(physicalDevices);
            goto cleanup_surface;
        }

        free(physicalDevices);
    }

    // Queue Families
    int32_t queueFamilyIndex = -1;
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
        if (queueFamilyCount == 0) {
            LOG_ERROR("No queue families found for the physical device");
            goto cleanup_instance;
        }
        LOG_INFO("Number of queue families available: %u", queueFamilyCount);

        VkQueueFamilyProperties* queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

        VkBool32 presentSupport = VK_FALSE;
        for (uint32_t i = 0; i < queueFamilyCount; i++) {

            // Check graphics bit
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queueFamilyIndex = i;
            }

            // Check presentation bit
            result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
            if (result == VK_SUCCESS && presentSupport == VK_TRUE) {
                LOG_INFO("Queue family %u supports presentation", i);
                break;
            }
        }

        if (queueFamilyIndex == -1 || presentSupport == VK_FALSE) {
            LOG_ERROR("No suitable queue family found for graphics operations");
            free(queueFamilies);
            goto cleanup_instance;
        }

        free(queueFamilies);
    }

    // Logical device creation
    {

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        const char* const enabledExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        uint32_t enabledExtensionCount = sizeof(enabledExtensions) / sizeof(enabledExtensions[0]);

        VkDeviceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledLayerCount = enabledLayerCount,
            .ppEnabledLayerNames = enabledLayers,
            .enabledExtensionCount = enabledExtensionCount,
            .ppEnabledExtensionNames = enabledExtensions
        };

        result = vkCreateDevice(physicalDevice, &createInfo, NULL, &device);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create Vulkan device: %d", result);
            goto cleanup_surface;
        }
        LOG_INFO("Vulkan logical device created successfully");
    }

    // Loop until the user closes the window
    while (!glfwWindowShouldClose(window)) {
        // In a real Vulkan application, you would render your Vulkan scene here
        // and present your swapchain images.
        // For this minimal example, we just poll events.

        // Poll for and process events
        glfwPollEvents();
    }

cleanup_device:
    vkDestroyDevice(device, NULL);
cleanup_surface:
    vkDestroySurfaceKHR(instance, surface, NULL);
cleanup_instance:
    vkDestroyInstance(instance, NULL);
cleanup_window:
    glfwDestroyWindow(window);
cleanup_glfw:
    glfwTerminate();

    return 0;
}
