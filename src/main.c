#include <stdbool.h>
#include <stdint.h>

#define NK_IMPLEMENTATION
#include "nuklear.h"

#include "app.h"
#include "log.h"

void glfw_error_callback(int error, const char* description)
{
    LOG_ERROR("[%d] %s", error, description);
}

int main(void)
{
    VkResult result;
    AppCtx appCtx = { 0 };

    // Glfw setup
    {
        glfwSetErrorCallback(glfw_error_callback);

        if (glfwInit() != GLFW_TRUE) {
            LOG_ERROR("Failed to initialize GLFW");
            goto cleanup_glfw;
        }
        LOG_INFO("GLFW initialized successfully");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        appCtx.window = glfwCreateWindow(640, 480, "Hello Vulkan", NULL, NULL);
        if (appCtx.window == NULL) {
            LOG_ERROR("Failed to create GLFW window");
            goto cleanup_glfw;
        }
        LOG_INFO("GLFW window created successfully");

        glfwSetWindowUserPointer(appCtx.window, &appCtx);
    }

    result = appCtx_init(&appCtx);
    if (result != VK_SUCCESS) {
        goto cleanup_glfw;
    }

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(appCtx.device, appCtx.queueFamilyIndex, 0, &graphicsQueue);
    LOG_INFO("Graphics queue obtained");

    // Main render loop
    while (!glfwWindowShouldClose(appCtx.window)) {
        glfwPollEvents();

        vkWaitForFences(appCtx.device, 1, &appCtx.inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(appCtx.device, 1, &appCtx.inFlightFence);

        uint32_t imageIndex;
        result = vkAcquireNextImageKHR(appCtx.device,
            appCtx.swapchain,
            UINT64_MAX,
            appCtx.imageAvailableSemaphore,
            VK_NULL_HANDLE,
            &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("Swapchain out of date, not recreating...");
            continue;
        } else if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to acquire next swapchain image: %d", result);
            break;
        }

        VkSubmitInfo submitInfo = { .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &appCtx.imageAvailableSemaphore,
            .pWaitDstStageMask
            = (VkPipelineStageFlags[]) { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT },
            .commandBufferCount = 1,
            .pCommandBuffers = &appCtx.commandBuffers[imageIndex],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &appCtx.renderFinishedSemaphore[imageIndex] };

        result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, appCtx.inFlightFence);
        if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to submit draw command buffer: %d", result);
            break;
        }

        VkPresentInfoKHR presentInfo = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &appCtx.renderFinishedSemaphore[imageIndex],
            .swapchainCount = 1,
            .pSwapchains = &appCtx.swapchain,
            .pImageIndices = &imageIndex };

        result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            LOG_INFO("Swapchain out of date, not recreating...");
            continue;
        } else if (result != VK_SUCCESS) {
            LOG_ERROR("Failed to present swapchain image: %d", result);
            break;
        }
    }

    vkDeviceWaitIdle(appCtx.device);

cleanup_glfw:
    appCtx_deinit(&appCtx);
    glfwDestroyWindow(appCtx.window);
    glfwTerminate();

    return 0;
}
