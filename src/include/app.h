#ifndef APP_H
#define APP_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

typedef struct SwapChainMetadata {
    VkSurfaceFormatKHR surfaceFormat;
    VkPresentModeKHR presentMode;
    VkExtent2D swapchainExtent;
    VkSurfaceTransformFlagBitsKHR swapChainTransform;
    uint32_t swapChainImageCount;
} SwapchainMetadata;

typedef struct AppCtx {
    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    int32_t queueFamilyIndex;
    VkDevice device;
    SwapchainMetadata swapchainMetadata;
    VkSwapchainKHR swapchain;
    VkImage* swapchainImages;
    VkImageView* swapchainImageViews;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    VkFramebuffer* swapchainFramebuffers;
    VkCommandPool commandPool;
    VkCommandBuffer* commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore* renderFinishedSemaphore;
    VkFence inFlightFence;
} AppCtx;

VkResult appCtx_init(AppCtx* appCtx);
void appCtx_deinit(AppCtx* appCtx);

#endif // APP_H
