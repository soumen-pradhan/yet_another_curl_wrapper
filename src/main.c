#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

static inline const char* curr_time(void)
{
    static char buffer[64];

    struct timespec ts;
    struct tm tm_info;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_info);

    snprintf(buffer, sizeof(buffer),
        "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
        tm_info.tm_year + 1900,
        tm_info.tm_mon + 1,
        tm_info.tm_mday,
        tm_info.tm_hour,
        tm_info.tm_min,
        tm_info.tm_sec,
        ts.tv_nsec / 1000000);

    return buffer;
}

static inline uint32_t clamp_u32(uint32_t value, uint32_t min, uint32_t max)
{
    if (value < min) {
        return min;
    }

    if (value > max) {
        return max;
    }

    return value;
}

#define LOG_INFO(format, ...) \
    fprintf(stdout, "[%23s] [%5s] %15s:%4d: " format "\n", curr_time(), "INFO", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

#define LOG_ERROR(format, ...) \
    fprintf(stderr, "[%23s] [%5s] %15s:%4d: " format "\n", curr_time(), "ERROR", __FILE__ + YACW_BASE_DIR_LEN, __LINE__, ##__VA_ARGS__)

void error_callback(int error, const char* description)
{
    LOG_ERROR("[%d] %s", error, description);
}

char* readFile(const char* filename, size_t* fileSize)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        LOG_ERROR("Could not open file %s", filename);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    *fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char* buffer = (char*)malloc(*fileSize);

    if (fread(buffer, 1, *fileSize, fp) != *fileSize) {
        LOG_ERROR("Could not read file %s completely", filename);
        free(buffer);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    return buffer;
}

int main(void)
{
    GLFWwindow* window;

    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;

    VkSwapchainKHR swapchain;
    uint32_t swapChainImageCount = 0;
    VkImage* swapchainImages = NULL;
    VkImageView* swapchainImageViews = NULL;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkFramebuffer* swapchainFramebuffers = NULL;
    VkCommandPool commandPool;
    VkCommandBuffer* commandBuffers = NULL;

    VkSemaphore imageAvailableSemaphore;
    VkSemaphore* renderFinishedSemaphore = NULL;
    VkFence inFlightFence;

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

    // SwapChain
    VkSurfaceFormatKHR surfaceFormat = { 0 };
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR; // V-Sync
    VkExtent2D swapchainExtent = { 0, 0 };
    VkSurfaceTransformFlagBitsKHR swapChainTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; // No transformation
    {
        {
            uint32_t surfaceFormatCount = 0;
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, NULL);
            if (result != VK_SUCCESS || surfaceFormatCount == 0) {
                LOG_ERROR("No surface formats available for the physical device");
                goto cleanup_device;
            }

            VkSurfaceFormatKHR* surfaceFormats = malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR));
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to get surface formats: %d", result);
                free(surfaceFormats);
                goto cleanup_device;
            }

            for (uint32_t i = 0; i < surfaceFormatCount; i++) {
                if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB
                    && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                    LOG_INFO("Preferred surface format found: %d, %d", surfaceFormats[i].format, surfaceFormats[i].colorSpace);
                    surfaceFormat = surfaceFormats[i];
                    break;
                }
            }

            if (surfaceFormat.format == 0) {
                LOG_ERROR("No preferred surface format found, using the first available format");
                surfaceFormat = surfaceFormats[0];
            }

            free(surfaceFormats);
        }

        {
            uint32_t surfacePresentModeCount = 0;
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, NULL);
            if (result != VK_SUCCESS || surfacePresentModeCount == 0) {
                LOG_ERROR("No surface present modes available for the physical device");
                goto cleanup_device;
            }

            VkPresentModeKHR* surfacePresentModes = malloc(surfacePresentModeCount * sizeof(VkPresentModeKHR));
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to get surface present modes: %d", result);
                free(surfacePresentModes);
                goto cleanup_device;
            }

            for (uint32_t i = 0; i < surfacePresentModeCount; i++) {
                if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                    LOG_INFO("Preferred present mode found: %d", surfacePresentModes[i]);
                    presentMode = surfacePresentModes[i]; // Triple buffering
                    break;
                }
            }

            free(surfacePresentModes);
        }

        {
            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to get surface capabilities: %d", result);
                goto cleanup_device;
            }

            if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
                swapchainExtent = surfaceCapabilities.currentExtent;
            } else {
                int width, height;
                glfwGetFramebufferSize(window, &width, &height);

                swapchainExtent.width = (uint32_t)width;
                swapchainExtent.height = (uint32_t)height;

                swapchainExtent.width = clamp_u32(swapchainExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
                swapchainExtent.height = clamp_u32(swapchainExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);
            }

            swapChainImageCount = surfaceCapabilities.minImageCount + 1;
            if (surfaceCapabilities.maxImageCount > 0 && swapChainImageCount > surfaceCapabilities.maxImageCount) {
                swapChainImageCount = surfaceCapabilities.maxImageCount;
            }

            swapChainTransform = surfaceCapabilities.currentTransform;
        }
    }

    // Create Swapchain
    {
        VkSwapchainCreateInfoKHR swapchainCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = surface,
            .minImageCount = swapChainImageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = swapchainExtent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform = swapChainTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // Opaque composite
            .presentMode = presentMode,
            .clipped = VK_TRUE, // Discard pixels outside the visible area
        };

        result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create swapchain: %d", result);
            goto cleanup_device;
        }
        LOG_INFO("Swapchain created successfully");

        result = vkGetSwapchainImagesKHR(device, swapchain, &swapChainImageCount, NULL);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to get swapchain images count: %d", result);
            goto cleanup_swapchain;
        }
        LOG_INFO("Number of swapchain images: %u", swapChainImageCount);

        swapchainImages = malloc(swapChainImageCount * sizeof(VkImage));
        result = vkGetSwapchainImagesKHR(device, swapchain, &swapChainImageCount, swapchainImages);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to get swapchain images: %d", result);
            goto cleanup_swapchain;
        }
    }

    // Image views creation
    {
        swapchainImageViews = malloc(swapChainImageCount * sizeof(VkImageView));
        if (swapchainImageViews == NULL) {
            LOG_ERROR("Failed to allocate memory for swapchain image views");
            goto cleanup_swapchain;
        }

        for (uint32_t i = 0; i < swapChainImageCount; i++) {
            VkImageViewCreateInfo createInfo = {
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = swapchainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = surfaceFormat.format,
                .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY },
                .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 }
            };

            result = vkCreateImageView(device, &createInfo, NULL, &swapchainImageViews[i]);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to create image view %u: %d", i, result);
                goto cleanup_swapchain;
            }
        }
        LOG_INFO("Swapchain image views created successfully");
    }

    // Render pass
    {
        VkAttachmentDescription colorAttachment = {
            .format = surfaceFormat.format,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear the attachment before rendering
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        VkAttachmentReference colorAttachmentRef = {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        };

        VkSubpassDescription subpass = {
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef
        };

        VkSubpassDependency dependency = {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        };

        VkRenderPassCreateInfo renderPassInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &colorAttachment,
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency
        };

        result = vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create render pass: %d", result);
            goto cleanup_swapchain;
        }
        LOG_INFO("Render pass created successfully");
    }

    // Pipeline
    {
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
        {
            size_t vertShaderSize;
            char* vertShaderCode = readFile(YACW_VERT_SHADER_PATH, &vertShaderSize);
            if (vertShaderCode == NULL) {
                LOG_ERROR("Failed to read vertex shader SPIR-V: %s", YACW_VERT_SHADER_PATH);
                goto cleanup_renderpass;
            }

            VkShaderModuleCreateInfo vertShaderModuleCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = vertShaderSize,
                .pCode = (const uint32_t*)vertShaderCode
            };

            result = vkCreateShaderModule(device, &vertShaderModuleCreateInfo, NULL, &vertShaderModule);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to create vertex shader module: %d", result);
                free(vertShaderCode);
                goto cleanup_renderpass;
            }
            LOG_INFO("Vertex shader module created successfully. Shader size: %zu bytes", vertShaderSize);
            free(vertShaderCode);

            size_t fragShaderSize;
            char* fragShaderCode = readFile(YACW_FRAG_SHADER_PATH, &fragShaderSize);
            if (fragShaderCode == NULL) {
                LOG_ERROR("Failed to read fragment shader SPIR-V: %s", YACW_FRAG_SHADER_PATH);
                goto cleanup_renderpass;
            }

            VkShaderModuleCreateInfo fragShaderModuleCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = fragShaderSize,
                .pCode = (const uint32_t*)fragShaderCode
            };

            result = vkCreateShaderModule(device, &fragShaderModuleCreateInfo, NULL, &fragShaderModule);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to create fragment shader module: %d", result);
                vkDestroyShaderModule(device, vertShaderModule, NULL);
                free(fragShaderCode);
                goto cleanup_renderpass;
            }
            LOG_INFO("Fragment shader module created successfully. Shader size: %zu bytes", fragShaderSize);
            free(fragShaderCode);
        }

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main"
        };

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
        uint32_t shaderStageCount = sizeof(shaderStages) / sizeof(shaderStages[0]);

        VkVertexInputBindingDescription bindingDescription = {
            .binding = 0,
            .stride = sizeof(float) * 2, // size of vec2
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
        };

        VkVertexInputAttributeDescription attributeDescription = {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R32G32_SFLOAT, // vec2 format
            .offset = 0
        };

        // Vertex Input
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            // .vertexBindingDescriptionCount = 1,
            // .pVertexBindingDescriptions = &bindingDescription, // Dummy vertex to satisfy the vertex shader
            // .vertexAttributeDescriptionCount = 1,
            // .pVertexAttributeDescriptions = &attributeDescription
        };

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE
        };

        // Viewport and Scissor
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)swapchainExtent.width,
            .height = (float)swapchainExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };

        VkRect2D scissor = {
            .offset = { 0, 0 },
            .extent = swapchainExtent
        };

        VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor
        };

        // Rasterization
        VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .depthBiasEnable = VK_FALSE
        };

        // Multisampling (disabled for now)
        VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT // No multisampling
        };

        // Color Blending (disabled for now)
        VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .blendEnable = VK_FALSE,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        };

        // Pipeline layout (empty for now, for uniform buffers or push constants)
        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 0, // No descriptor sets for now
            .pushConstantRangeCount = 0 // No push constants for now
        };

        result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &pipelineLayout);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create pipeline layout: %d", result);
            vkDestroyShaderModule(device, fragShaderModule, NULL);
            vkDestroyShaderModule(device, vertShaderModule, NULL);
            goto cleanup_renderpass;
        }
        LOG_INFO("Pipeline layout created successfully");

        VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = shaderStageCount,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .logicOpEnable = VK_FALSE,
                .attachmentCount = 1,
                .pAttachments = &colorBlendAttachment },
            .layout = pipelineLayout,
            .renderPass = renderPass,
            .subpass = 0
        };

        result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &pipeline);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create graphics pipeline: %d", result);
            vkDestroyShaderModule(device, fragShaderModule, NULL);
            vkDestroyShaderModule(device, vertShaderModule, NULL);
            goto cleanup_pipeline_layout;
        }
        LOG_INFO("Graphics pipeline created successfully");

        vkDestroyShaderModule(device, fragShaderModule, NULL);
        vkDestroyShaderModule(device, vertShaderModule, NULL);
    }

    // Framebuffers
    {
        swapchainFramebuffers = malloc(swapChainImageCount * sizeof(VkFramebuffer));
        if (swapchainFramebuffers == NULL) {
            LOG_ERROR("Failed to allocate memory for swapchain framebuffers");
            goto cleanup_pipeline;
        }

        for (uint32_t i = 0; i < swapChainImageCount; i++) {
            VkImageView attachments[] = { swapchainImageViews[i] };

            VkFramebufferCreateInfo framebufferInfo = {
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = renderPass,
                .attachmentCount = 1,
                .pAttachments = attachments,
                .width = swapchainExtent.width,
                .height = swapchainExtent.height,
                .layers = 1
            };

            result = vkCreateFramebuffer(device, &framebufferInfo, NULL, &swapchainFramebuffers[i]);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to create framebuffer %u: %d", i, result);
                goto cleanup_framebuffers;
            }
            LOG_INFO("Framebuffer %u created successfully", i);
        }
    }

    // Command pool
    {
        VkCommandPoolCreateInfo commandPoolInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT // Allow command buffers to be reset
        };

        result = vkCreateCommandPool(device, &commandPoolInfo, NULL, &commandPool);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create command pool: %d", result);
            goto cleanup_framebuffers;
        }
        LOG_INFO("Command pool created successfully");

        commandBuffers = malloc(swapChainImageCount * sizeof(VkCommandBuffer));
        if (commandBuffers == NULL) {
            LOG_ERROR("Failed to allocate memory for command buffers");
            vkDestroyCommandPool(device, commandPool, NULL);
            goto cleanup_command_pool;
        }

        VkCommandBufferAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = swapChainImageCount
        };

        result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to allocate command buffers: %d", result);
            free(commandBuffers);
            goto cleanup_command_pool;
        }
        LOG_INFO("Command buffers allocated successfully");

        for (uint32_t i = 0; i < swapChainImageCount; i++) {
            VkCommandBufferBeginInfo beginInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT // Allow simultaneous use
            };

            result = vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to begin command buffer %u: %d", i, result);
                goto cleanup_command_buffers;
            }

            VkRenderPassBeginInfo renderPassInfo = {
                .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                .renderPass = renderPass,
                .framebuffer = swapchainFramebuffers[i],
                .renderArea = { .offset = { 0, 0 }, .extent = swapchainExtent },
                .clearValueCount = 1,
                .pClearValues = &(VkClearValue) { .color = { { 0.0f, 0.0f, 1.0f, 1.0f } } }
            };

            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0); // Draw a triangle (3 vertices)

            vkCmdEndRenderPass(commandBuffers[i]);

            result = vkEndCommandBuffer(commandBuffers[i]);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to end command buffer %u: %d", i, result);
                goto cleanup_command_buffers;
            }
        }
        LOG_INFO("Command buffers recorded successfully");
    }

    // Sync objects
    {
        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };

        result = vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailableSemaphore);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create image available semaphore: %d", result);
            goto cleanup_command_buffers;
        }
        LOG_INFO("Image available semaphore created successfully");

        renderFinishedSemaphore = malloc(swapChainImageCount * sizeof(VkSemaphore));
        if (renderFinishedSemaphore == NULL) {
            LOG_ERROR("Failed to allocate memory for render finished semaphores");
            vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
            goto cleanup_command_buffers;
        }

        for (uint32_t i = 0; i < swapChainImageCount; i++) {
            result = vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinishedSemaphore[i]);
            if (result != VK_SUCCESS) {
                LOG_ERROR("Failed to create render finished semaphore %u: %d", i, result);

                for (uint32_t j = 0; j < i; ++j) {
                    vkDestroySemaphore(device, renderFinishedSemaphore[j], NULL);
                }
                free(renderFinishedSemaphore);

                vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
                goto cleanup_command_buffers;
            }
        }
        LOG_INFO("Render finished semaphore created successfully");

        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT // Start in signaled state
        };

        result = vkCreateFence(device, &fenceInfo, NULL, &inFlightFence);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create in-flight fence: %d", result);

            for (uint32_t i = 0; i < swapChainImageCount; ++i) {
                vkDestroySemaphore(device, renderFinishedSemaphore[i], NULL);
            }
            free(renderFinishedSemaphore);

            vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
            goto cleanup_command_buffers;
        }
        LOG_INFO("In-flight fence created successfully");

        LOG_INFO("Synchronization objects created successfully");
    }

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);
    LOG_INFO("Graphics queue obtained");

    // Main render loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        uint32_t imageIndex;
        result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("Swapchain out of date, not recreating...");
            continue;
        } else if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to acquire next swapchain image: %d", result);
            break;
        }

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &imageAvailableSemaphore,
            .pWaitDstStageMask = (VkPipelineStageFlags[]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffers[imageIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &renderFinishedSemaphore[imageIndex]
        };

        result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer: %d", result);
            break;
        }

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &renderFinishedSemaphore[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &imageIndex
        };

        result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("Swapchain out of date, not recreating...");
            continue;
        } else if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to present swapchain image: %d", result);
            break;
        }
    }

    vkDeviceWaitIdle(device);

cleanup_sync_objects:
    vkDestroyFence(device, inFlightFence, NULL);
    for (uint32_t i = 0; i < swapChainImageCount; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphore[i], NULL);
    }
    free(renderFinishedSemaphore);
    vkDestroySemaphore(device, imageAvailableSemaphore, NULL);
cleanup_command_buffers:
    vkFreeCommandBuffers(device, commandPool, swapChainImageCount, commandBuffers);
    free(commandBuffers);
cleanup_command_pool:
    vkDestroyCommandPool(device, commandPool, NULL);
cleanup_framebuffers:
    for (uint32_t i = 0; i < swapChainImageCount; i++) {
        vkDestroyFramebuffer(device, swapchainFramebuffers[i], NULL);
    }
    free(swapchainFramebuffers);
cleanup_pipeline:
    vkDestroyPipeline(device, pipeline, NULL);
cleanup_pipeline_layout:
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
cleanup_renderpass:
    vkDestroyRenderPass(device, renderPass, NULL);
cleanup_swapchain:
    for (uint32_t i = 0; i < swapChainImageCount; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }
    free(swapchainImageViews);
    free(swapchainImages);
    vkDestroySwapchainKHR(device, swapchain, NULL);
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
