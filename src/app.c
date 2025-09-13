#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "log.h"

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

static const char* enabledLayers[] = { "VK_LAYER_KHRONOS_validation" };
static uint32_t enabledLayerCount = sizeof(enabledLayers) / sizeof(enabledLayers[0]);

VkResult init_instance(VkInstance* instance)
{
    VkResult result;

    VkApplicationInfo appInfo
        = { .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_2 };

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = NULL;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == NULL) {
        LOG_ERROR("Failed to get required Vulkan instance extensions from GLFW");
        return VK_RESULT_MAX_ENUM;
    }

    LOG_INFO("Number of required Vulkan instance extensions from GLFW: %u", glfwExtensionCount);
    for (unsigned int i = 0; i < glfwExtensionCount; i++) {
        LOG_INFO("  - %s", glfwExtensions[i]);
    }

    VkInstanceCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = glfwExtensionCount,
        .ppEnabledExtensionNames = glfwExtensions,
        .enabledLayerCount = enabledLayerCount,
        .ppEnabledLayerNames = enabledLayers };

    result = vkCreateInstance(&createInfo, NULL, instance);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance: %d", result);
        return VK_RESULT_MAX_ENUM;
    }
    LOG_INFO("Vulkan instance created successfully");

    return result;
}

VkResult init_surface(VkInstance instance, GLFWwindow* window, VkSurfaceKHR* surface)
{
    VkResult result;
    result = glfwCreateWindowSurface(instance, window, NULL, surface);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan surface: %d", result);
        return result;
    }
    LOG_INFO("Vulkan surface created successfully");
    return result;
}

VkResult init_physical_device(VkInstance instance, VkPhysicalDevice* physicalDevice)
{
    VkResult result;

    uint32_t deviceCount = 0;
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, NULL);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to enumerate physical devices: %d", result);
        return result;
    }
    if (deviceCount == 0) {
        LOG_ERROR("No physical devices found");
        return VK_RESULT_MAX_ENUM;
    }
    LOG_INFO("Number of physical devices available: %u", deviceCount);

    VkPhysicalDevice* physicalDevices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    result = vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to enumerate physical devices: %d", result);
        free(physicalDevices);
        return result;
    }

    VkPhysicalDeviceType preferredTypeOrder[] = { VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU,
        VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU,
        VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU,
        VK_PHYSICAL_DEVICE_TYPE_CPU };
    uint32_t preferredTypeOrderCount = sizeof(preferredTypeOrder) / sizeof(preferredTypeOrder[0]);

    for (uint32_t i = 0; i < preferredTypeOrderCount; i++) {
        for (uint32_t j = 0; j < deviceCount; j++) {
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(physicalDevices[j], &properties);

            if (properties.deviceType == preferredTypeOrder[i]) {
                *physicalDevice = physicalDevices[j];
                LOG_INFO("Selected physical device: %s", properties.deviceName);
                break;
            }
        }

        if (*physicalDevice != VK_NULL_HANDLE) {
            break;
        }
    }

    if (*physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("No suitable physical device found");
        free(physicalDevices);
        return VK_RESULT_MAX_ENUM;
    }

    free(physicalDevices);
    return result;
}

VkResult init_queue_family_index(
    VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, int32_t* queueFamilyIndex)
{
    VkResult result;
    *queueFamilyIndex = -1;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    if (queueFamilyCount == 0) {
        LOG_ERROR("No queue families found for the physical device");
        return VK_RESULT_MAX_ENUM;
    }
    LOG_INFO("Number of queue families available: %u", queueFamilyCount);

    VkQueueFamilyProperties* queueFamilies
        = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    VkBool32 presentSupport = VK_FALSE;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {

        // Check graphics bit
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *queueFamilyIndex = i;
        }

        // Check presentation bit
        result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);
        if (result == VK_SUCCESS && presentSupport == VK_TRUE) {
            LOG_INFO("Queue family %u supports presentation", i);
            break;
        }
    }

    if (*queueFamilyIndex == -1 || presentSupport == VK_FALSE) {
        LOG_ERROR("No suitable queue family found for graphics operations");
        free(queueFamilies);
        return VK_RESULT_MAX_ENUM;
    }

    free(queueFamilies);
    return result;
}

VkResult init_device(int32_t queueFamilyIndex, VkPhysicalDevice physicalDevice, VkDevice* device)
{
    VkResult result;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority };

    const char* const enabledExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    uint32_t enabledExtensionCount = sizeof(enabledExtensions) / sizeof(enabledExtensions[0]);

    VkDeviceCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = enabledLayerCount,
        .ppEnabledLayerNames = enabledLayers,
        .enabledExtensionCount = enabledExtensionCount,
        .ppEnabledExtensionNames = enabledExtensions };

    result = vkCreateDevice(physicalDevice, &createInfo, NULL, device);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan device: %d", result);
        return result;
    }
    LOG_INFO("Vulkan logical device created successfully");

    return result;
}

VkResult init_swapchain_metadata(VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    GLFWwindow* window,
    SwapchainMetadata* swapChainMetadata)
{
    VkResult result;

    *swapChainMetadata = (SwapchainMetadata) {
        .presentMode = VK_PRESENT_MODE_FIFO_KHR, // V-Sync
        .swapChainTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR // No transformation
    };

    // surface format
    {
        uint32_t surfaceFormatCount = 0;
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &surfaceFormatCount, NULL);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Could not fetch surface formats: %d", result);
            return result;
        }
        if (surfaceFormatCount == 0) {
            LOG_ERROR("No surface formats available for the physical device");
            return VK_RESULT_MAX_ENUM;
        }

        VkSurfaceFormatKHR* surfaceFormats
            = malloc(surfaceFormatCount * sizeof(VkSurfaceFormatKHR));
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            physicalDevice, surface, &surfaceFormatCount, surfaceFormats);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to get surface formats: %d", result);
            free(surfaceFormats);
            return result;
        }

        for (uint32_t i = 0; i < surfaceFormatCount; i++) {
            if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB
                && surfaceFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                LOG_INFO("Preferred surface format found: %d, %d",
                    surfaceFormats[i].format,
                    surfaceFormats[i].colorSpace);
                swapChainMetadata->surfaceFormat = surfaceFormats[i];
                break;
            }
        }

        if (swapChainMetadata->surfaceFormat.format == 0) {
            LOG_ERROR("No preferred surface format found, using the first available format");
            swapChainMetadata->surfaceFormat = surfaceFormats[0];
        }

        free(surfaceFormats);
    }

    // presentation mode
    {
        uint32_t surfacePresentModeCount = 0;
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &surfacePresentModeCount, NULL);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Could not fetch surface present modes: %d", result);
            return result;
        }
        if (surfacePresentModeCount == 0) {
            LOG_ERROR("No surface present modes available for the physical device");
            return VK_RESULT_MAX_ENUM;
        }

        VkPresentModeKHR* surfacePresentModes
            = malloc(surfacePresentModeCount * sizeof(VkPresentModeKHR));
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to get surface present modes: %d", result);
            free(surfacePresentModes);
            return result;
        }

        for (uint32_t i = 0; i < surfacePresentModeCount; i++) {
            if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
                LOG_INFO("Preferred present mode found: %d", surfacePresentModes[i]);
                swapChainMetadata->presentMode = surfacePresentModes[i]; // Triple buffering
                break;
            }
        }

        free(surfacePresentModes);
    }

    // swapchain count
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities;
        result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            physicalDevice, surface, &surfaceCapabilities);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to get surface capabilities: %d", result);
            return result;
        }

        if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
            swapChainMetadata->swapchainExtent = surfaceCapabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            swapChainMetadata->swapchainExtent.width = (uint32_t)width;
            swapChainMetadata->swapchainExtent.height = (uint32_t)height;

            swapChainMetadata->swapchainExtent.width
                = clamp_u32(swapChainMetadata->swapchainExtent.width,
                    surfaceCapabilities.minImageExtent.width,
                    surfaceCapabilities.maxImageExtent.width);
            swapChainMetadata->swapchainExtent.height
                = clamp_u32(swapChainMetadata->swapchainExtent.height,
                    surfaceCapabilities.minImageExtent.height,
                    surfaceCapabilities.maxImageExtent.height);
        }

        swapChainMetadata->swapChainImageCount = surfaceCapabilities.minImageCount + 1;
        if (surfaceCapabilities.maxImageCount > 0
            && swapChainMetadata->swapChainImageCount > surfaceCapabilities.maxImageCount) {
            swapChainMetadata->swapChainImageCount = surfaceCapabilities.maxImageCount;
        }

        swapChainMetadata->swapChainTransform = surfaceCapabilities.currentTransform;
    }

    return result;
}

VkResult init_swapchain(VkSurfaceKHR surface,
    VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkSwapchainKHR* swapchain,
    VkImage** swapchainImages)
{
    VkResult result;

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = swapchainMetadata.swapChainImageCount,
        .imageFormat = swapchainMetadata.surfaceFormat.format,
        .imageColorSpace = swapchainMetadata.surfaceFormat.colorSpace,
        .imageExtent = swapchainMetadata.swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = swapchainMetadata.swapChainTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // Opaque composite
        .presentMode = swapchainMetadata.presentMode,
        .clipped = VK_TRUE, // Discard pixels outside the visible area
    };

    result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, swapchain);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain: %d", result);
        return result;
    }
    LOG_INFO("Swapchain created successfully");

    result
        = vkGetSwapchainImagesKHR(device, *swapchain, &swapchainMetadata.swapChainImageCount, NULL);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to get swapchain images count: %d", result);
        return result;
    }
    LOG_INFO("Number of swapchain images: %u", swapchainMetadata.swapChainImageCount);

    *swapchainImages = malloc(swapchainMetadata.swapChainImageCount * sizeof(VkImage));
    result = vkGetSwapchainImagesKHR(
        device, *swapchain, &swapchainMetadata.swapChainImageCount, *swapchainImages);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to get swapchain images: %d", result);
        return result;
    }

    return result;
}

VkResult init_image_views(VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkImage* swapchainImages,
    VkImageView** swapchainImageViews)
{
    VkResult result;

    *swapchainImageViews = malloc(swapchainMetadata.swapChainImageCount * sizeof(VkImageView));
    if (*swapchainImageViews == NULL) {
        LOG_ERROR("Failed to allocate memory for swapchain image views");
        return VK_RESULT_MAX_ENUM;
    }

    VkImageView* views = *swapchainImageViews;

    for (uint32_t i = 0; i < swapchainMetadata.swapChainImageCount; i++) {
        VkImageViewCreateInfo createInfo = { .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchainMetadata.surfaceFormat.format,
            .components = { .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1 } };

        result = vkCreateImageView(device, &createInfo, NULL, &views[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create image view %u: %d", i, result);
            return result;
        }
    }
    LOG_INFO("Swapchain image views created successfully");

    return result;
}

VkResult init_render_pass(
    VkDevice device, SwapchainMetadata swapchainMetadata, VkRenderPass* renderPass)
{
    VkResult result;

    VkAttachmentDescription colorAttachment = { .format = swapchainMetadata.surfaceFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // Clear the attachment before rendering
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR };

    VkAttachmentReference colorAttachmentRef
        = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = { .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef };

    VkSubpassDependency dependency = { .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT };

    VkRenderPassCreateInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency };

    result = vkCreateRenderPass(device, &renderPassInfo, NULL, renderPass);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create render pass: %d", result);
        return result;
    }
    LOG_INFO("Render pass created successfully");

    return result;
}

VkResult init_pipeline(VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkRenderPass renderPass,
    VkPipelineLayout* pipelineLayout,
    VkPipeline* pipeline)
{
    VkResult result;

    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    {
        size_t vertShaderSize;
        char* vertShaderCode = readFile(YACW_VERT_SHADER_PATH, &vertShaderSize);
        if (vertShaderCode == NULL) {
            LOG_ERROR("Failed to read vertex shader SPIR-V: %s", YACW_VERT_SHADER_PATH);
            return VK_RESULT_MAX_ENUM;
        }

        VkShaderModuleCreateInfo vertShaderModuleCreateInfo
            = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                  .codeSize = vertShaderSize,
                  .pCode = (const uint32_t*)vertShaderCode };

        result = vkCreateShaderModule(device, &vertShaderModuleCreateInfo, NULL, &vertShaderModule);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create vertex shader module: %d", result);
            free(vertShaderCode);
            return result;
        }
        LOG_INFO(
            "Vertex shader module created successfully. Shader size: %zu bytes", vertShaderSize);
        free(vertShaderCode);

        size_t fragShaderSize;
        char* fragShaderCode = readFile(YACW_FRAG_SHADER_PATH, &fragShaderSize);
        if (fragShaderCode == NULL) {
            LOG_ERROR("Failed to read fragment shader SPIR-V: %s", YACW_FRAG_SHADER_PATH);
            return VK_RESULT_MAX_ENUM;
        }

        VkShaderModuleCreateInfo fragShaderModuleCreateInfo
            = { .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                  .codeSize = fragShaderSize,
                  .pCode = (const uint32_t*)fragShaderCode };

        result = vkCreateShaderModule(device, &fragShaderModuleCreateInfo, NULL, &fragShaderModule);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create fragment shader module: %d", result);
            vkDestroyShaderModule(device, vertShaderModule, NULL);
            free(fragShaderCode);
            return result;
        }
        LOG_INFO(
            "Fragment shader module created successfully. Shader size: %zu bytes", fragShaderSize);
        free(fragShaderCode);
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_VERTEX_BIT,
              .module = vertShaderModule,
              .pName = "main" };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
              .module = fragShaderModule,
              .pName = "main" };

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };
    uint32_t shaderStageCount = sizeof(shaderStages) / sizeof(shaderStages[0]);

    VkVertexInputBindingDescription bindingDescription = { .binding = 0,
        .stride = sizeof(float) * 2, // size of vec2
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };

    VkVertexInputAttributeDescription attributeDescription = { .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT, // vec2 format
        .offset = 0 };

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        // .vertexBindingDescriptionCount = 1,
        // .pVertexBindingDescriptions = &bindingDescription, // Dummy vertex to satisfy
        // the vertex shader .vertexAttributeDescriptionCount = 1,
        // .pVertexAttributeDescriptions = &attributeDescription
    };

    // Input Assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
              .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
              .primitiveRestartEnable = VK_FALSE };

    // Viewport and Scissor
    VkViewport viewport = { .x = 0.0f,
        .y = 0.0f,
        .width = (float)swapchainMetadata.swapchainExtent.width,
        .height = (float)swapchainMetadata.swapchainExtent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f };

    VkRect2D scissor = { .offset = { 0, 0 }, .extent = swapchainMetadata.swapchainExtent };

    VkPipelineViewportStateCreateInfo viewportState
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
              .viewportCount = 1,
              .pViewports = &viewport,
              .scissorCount = 1,
              .pScissors = &scissor };

    // Rasterization
    VkPipelineRasterizationStateCreateInfo rasterizer
        = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
              .depthClampEnable = VK_FALSE,
              .rasterizerDiscardEnable = VK_FALSE,
              .polygonMode = VK_POLYGON_MODE_FILL,
              .lineWidth = 1.0f,
              .cullMode = VK_CULL_MODE_BACK_BIT,
              .frontFace = VK_FRONT_FACE_CLOCKWISE,
              .depthBiasEnable = VK_FALSE };

    // Multisampling (disabled for now)
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT // No multisampling
    };

    // Color Blending (disabled for now)
    VkPipelineColorBlendAttachmentState colorBlendAttachment = { .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };

    // Pipeline layout (empty for now, for uniform buffers or push constants)
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0, // No descriptor sets for now
        .pushConstantRangeCount = 0 // No push constants for now
    };

    result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create pipeline layout: %d", result);
        vkDestroyShaderModule(device, fragShaderModule, NULL);
        vkDestroyShaderModule(device, vertShaderModule, NULL);
        return result;
    }
    LOG_INFO("Pipeline layout created successfully");

    VkGraphicsPipelineCreateInfo pipelineInfo
        = { .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
              .stageCount = shaderStageCount,
              .pStages = shaderStages,
              .pVertexInputState = &vertexInputInfo,
              .pInputAssemblyState = &inputAssembly,
              .pViewportState = &viewportState,
              .pRasterizationState = &rasterizer,
              .pMultisampleState = &multisampling,
              .pColorBlendState = &(VkPipelineColorBlendStateCreateInfo) { .sType
                  = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                  .logicOpEnable = VK_FALSE,
                  .attachmentCount = 1,
                  .pAttachments = &colorBlendAttachment },
              .layout = *pipelineLayout,
              .renderPass = renderPass,
              .subpass = 0 };

    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, pipeline);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create graphics pipeline: %d", result);
        vkDestroyShaderModule(device, fragShaderModule, NULL);
        vkDestroyShaderModule(device, vertShaderModule, NULL);
        return result;
    }
    LOG_INFO("Graphics pipeline created successfully");

    vkDestroyShaderModule(device, fragShaderModule, NULL);
    vkDestroyShaderModule(device, vertShaderModule, NULL);

    return result;
}

VkResult init_framebuffers(VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkImageView* swapchainImageViews,
    VkRenderPass renderPass,
    VkFramebuffer** swapchainFramebuffers)
{
    VkResult result;

    *swapchainFramebuffers = malloc(swapchainMetadata.swapChainImageCount * sizeof(VkFramebuffer));
    if (swapchainFramebuffers == NULL) {
        LOG_ERROR("Failed to allocate memory for swapchain framebuffers");
        return VK_RESULT_MAX_ENUM;
    }

    VkFramebuffer* buffers = *swapchainFramebuffers;

    for (uint32_t i = 0; i < swapchainMetadata.swapChainImageCount; i++) {
        VkImageView attachments[] = { swapchainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo
            = { .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                  .renderPass = renderPass,
                  .attachmentCount = 1,
                  .pAttachments = attachments,
                  .width = swapchainMetadata.swapchainExtent.width,
                  .height = swapchainMetadata.swapchainExtent.height,
                  .layers = 1 };

        result = vkCreateFramebuffer(device, &framebufferInfo, NULL, &buffers[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create framebuffer %u: %d", i, result);
            return result;
        }
        LOG_INFO("Framebuffer %u created successfully", i);
    }

    return result;
}

VkResult init_command_pool(int32_t queueFamilyIndex,
    VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkRenderPass renderPass,
    VkFramebuffer* swapchainFramebuffers,
    VkPipeline pipeline,
    VkCommandPool* commandPool,
    VkCommandBuffer** commandBuffers)
{
    VkResult result;

    VkCommandPoolCreateInfo commandPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = queueFamilyIndex,
        .flags
        = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT // Allow command buffers to be reset
    };

    result = vkCreateCommandPool(device, &commandPoolInfo, NULL, commandPool);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create command pool: %d", result);
        return result;
    }
    LOG_INFO("Command pool created successfully");

    *commandBuffers = malloc(swapchainMetadata.swapChainImageCount * sizeof(VkCommandBuffer));
    if (*commandBuffers == NULL) {
        LOG_ERROR("Failed to allocate memory for command buffers");
        return VK_RESULT_MAX_ENUM;
    }

    VkCommandBufferAllocateInfo allocInfo
        = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
              .commandPool = *commandPool,
              .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
              .commandBufferCount = swapchainMetadata.swapChainImageCount };

    result = vkAllocateCommandBuffers(device, &allocInfo, *commandBuffers);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate command buffers: %d", result);
        free(commandBuffers);
        return result;
    }
    LOG_INFO("Command buffers allocated successfully");

    VkCommandBuffer* buffers = *commandBuffers;

    for (uint32_t i = 0; i < swapchainMetadata.swapChainImageCount; i++) {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT // Allow simultaneous use
        };

        result = vkBeginCommandBuffer(buffers[i], &beginInfo);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to begin command buffer %u: %d", i, result);
            return result;
        }

        VkRenderPassBeginInfo renderPassInfo = { .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = renderPass,
            .framebuffer = swapchainFramebuffers[i],
            .renderArea = { .offset = { 0, 0 }, .extent = swapchainMetadata.swapchainExtent },
            .clearValueCount = 1,
            .pClearValues = &(VkClearValue) { .color = { { 0.0f, 0.0f, 1.0f, 1.0f } } } };

        vkCmdBeginRenderPass(buffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        vkCmdDraw(buffers[i], 3, 1, 0, 0); // Draw a triangle (3 vertices)

        vkCmdEndRenderPass(buffers[i]);

        result = vkEndCommandBuffer(buffers[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to end command buffer %u: %d", i, result);
            return result;
        }
    }
    LOG_INFO("Command buffers recorded successfully");

    return result;
}

VkResult init_sync_objects(VkDevice device,
    SwapchainMetadata swapchainMetadata,
    VkSemaphore* imageAvailableSemaphore,
    VkSemaphore** renderFinishedSemaphore,
    VkFence* inFlightFence)
{
    VkResult result;

    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

    result = vkCreateSemaphore(device, &semaphoreInfo, NULL, imageAvailableSemaphore);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create image available semaphore: %d", result);
        return result;
    }
    LOG_INFO("Image available semaphore created successfully");

    *renderFinishedSemaphore = malloc(swapchainMetadata.swapChainImageCount * sizeof(VkSemaphore));
    if (renderFinishedSemaphore == NULL) {
        LOG_ERROR("Failed to allocate memory for render finished semaphores");
        return VK_RESULT_MAX_ENUM;
    }

    VkSemaphore* semaphores = *renderFinishedSemaphore;
    for (uint32_t i = 0; i < swapchainMetadata.swapChainImageCount; i++) {
        result = vkCreateSemaphore(device, &semaphoreInfo, NULL, &semaphores[i]);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to create render finished semaphore %u: %d", i, result);

            for (uint32_t j = 0; j < i; ++j) {
                vkDestroySemaphore(device, semaphores[j], NULL);
            }
            free(renderFinishedSemaphore);
            return VK_RESULT_MAX_ENUM;
        }
    }
    LOG_INFO("Render finished semaphore created successfully");

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT // Start in signaled state
    };

    result = vkCreateFence(device, &fenceInfo, NULL, inFlightFence);
    if (result != VK_SUCCESS) {
        LOG_ERROR("Failed to create in-flight fence: %d", result);
        return result;
    }
    LOG_INFO("In-flight fence created successfully");

    LOG_INFO("Synchronization objects created successfully");
    return result;
}

VkResult appCtx_init(AppCtx* appCtx)
{
    VkResult result = VK_SUCCESS;

    result = init_instance(&appCtx->instance);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_surface(appCtx->instance, appCtx->window, &appCtx->surface);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_physical_device(appCtx->instance, &appCtx->physicalDevice);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_queue_family_index(
        appCtx->physicalDevice, appCtx->surface, &appCtx->queueFamilyIndex);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_device(appCtx->queueFamilyIndex, appCtx->physicalDevice, &appCtx->device);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_swapchain_metadata(
        appCtx->physicalDevice, appCtx->surface, appCtx->window, &appCtx->swapchainMetadata);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_swapchain(appCtx->surface,
        appCtx->device,
        appCtx->swapchainMetadata,
        &appCtx->swapchain,
        &appCtx->swapchainImages);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_image_views(appCtx->device,
        appCtx->swapchainMetadata,
        appCtx->swapchainImages,
        &appCtx->swapchainImageViews);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_render_pass(appCtx->device, appCtx->swapchainMetadata, &appCtx->renderPass);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_pipeline(appCtx->device,
        appCtx->swapchainMetadata,
        appCtx->renderPass,
        &appCtx->pipelineLayout,
        &appCtx->pipeline);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_framebuffers(appCtx->device,
        appCtx->swapchainMetadata,
        appCtx->swapchainImageViews,
        appCtx->renderPass,
        &appCtx->swapchainFramebuffers);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_command_pool(appCtx->queueFamilyIndex,
        appCtx->device,
        appCtx->swapchainMetadata,
        appCtx->renderPass,
        appCtx->swapchainFramebuffers,
        appCtx->pipeline,
        &appCtx->commandPool,
        &appCtx->commandBuffers);
    if (result != VK_SUCCESS) {
        return result;
    }

    result = init_sync_objects(appCtx->device,
        appCtx->swapchainMetadata,
        &appCtx->imageAvailableSemaphore,
        &appCtx->renderFinishedSemaphore,
        &appCtx->inFlightFence);
    if (result != VK_SUCCESS) {
        return result;
    }

    return result;
}

void appCtx_deinit(AppCtx* appCtx)
{
    if (appCtx->inFlightFence != VK_NULL_HANDLE) {
        vkDestroyFence(appCtx->device, appCtx->inFlightFence, NULL);
    }

    if (appCtx->renderFinishedSemaphore != NULL) {
        for (uint32_t i = 0; i < appCtx->swapchainMetadata.swapChainImageCount; i++) {
            vkDestroySemaphore(appCtx->device, appCtx->renderFinishedSemaphore[i], NULL);
        }
        free(appCtx->renderFinishedSemaphore);
        appCtx->renderFinishedSemaphore = NULL;
    }

    if (appCtx->imageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(appCtx->device, appCtx->imageAvailableSemaphore, NULL);
    }

    if (appCtx->commandBuffers != NULL) {
        vkFreeCommandBuffers(appCtx->device,
            appCtx->commandPool,
            appCtx->swapchainMetadata.swapChainImageCount,
            appCtx->commandBuffers);
        free(appCtx->commandBuffers);
        appCtx->commandBuffers = NULL;
    }

    if (appCtx->commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(appCtx->device, appCtx->commandPool, NULL);
    }

    if (appCtx->swapchainFramebuffers != NULL) {
        for (uint32_t i = 0; i < appCtx->swapchainMetadata.swapChainImageCount; i++) {
            vkDestroyFramebuffer(appCtx->device, appCtx->swapchainFramebuffers[i], NULL);
        }
        free(appCtx->swapchainFramebuffers);
        appCtx->swapchainFramebuffers = NULL;
    }

    if (appCtx->pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(appCtx->device, appCtx->pipeline, NULL);
    }

    if (appCtx->pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(appCtx->device, appCtx->pipelineLayout, NULL);
    }

    if (appCtx->renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(appCtx->device, appCtx->renderPass, NULL);
    }

    if (appCtx->swapchainImageViews != NULL) {
        for (uint32_t i = 0; i < appCtx->swapchainMetadata.swapChainImageCount; i++) {
            vkDestroyImageView(appCtx->device, appCtx->swapchainImageViews[i], NULL);
        }
        free(appCtx->swapchainImageViews);
        appCtx->swapchainImageViews = NULL;
    }

    // No need to call vkDestroyImage on each of these, they are destroyed by vkDestroySwapchainKHR
    if (appCtx->swapchainImages != NULL) {
        free(appCtx->swapchainImages);
        appCtx->swapchainImages = NULL;
    }

    if (appCtx->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(appCtx->device, appCtx->swapchain, NULL);
    }

    if (appCtx->device != VK_NULL_HANDLE) {
        vkDestroyDevice(appCtx->device, NULL);
    }

    if (appCtx->surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(appCtx->instance, appCtx->surface, NULL);
    }

    if (appCtx->instance != VK_NULL_HANDLE) {
        vkDestroyInstance(appCtx->instance, NULL);
    }
}
