#pragma once

#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

#include <VulkanDevice.h>
#include <VulkanSwapChain.h>

class VulkanCore
{
public:
    VulkanCore(bool enableValidation = false);
    virtual ~VulkanCore();

    // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
    virtual void InitVulkan();
    virtual void CreateCommandBuffers();
    virtual void CreateSynchronizationPrimitives();
    virtual void Prepare();
    virtual void RenderLoop();
    virtual void SetupRenderPass();
    virtual void SetupWindow();
    virtual void SetupFrameBuffer();
    virtual void NextFrame();
    virtual void Render() = 0;

    // Called when the window has been resized, can be used by the sample application to recreate resources
    virtual void WindowResize();

protected:
    virtual VkResult CreateInstance(bool enableValidation);

protected:
    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    VkInstance m_instance;
    std::vector<std::string> m_supportedInstanceExtensions;

    // Set of device extensions to be enabled for the application, to be set in the derived object
    VkPhysicalDeviceFeatures m_enabledFeatures{};
    std::vector<const char*> m_enabledDeviceExtensions;
    std::vector<const char*> m_enabledInstanceExtensions;

    // Wrapper class around device related functionality
    VulkanDevice* vulkanDevice;

    // Physical device
    VkPhysicalDevice m_physicalDevice;
    VkPhysicalDeviceProperties m_deviceProperties;
    VkPhysicalDeviceFeatures m_deviceFeatures;
    VkPhysicalDeviceMemoryProperties m_deviceMemoryProperties;

    // Wraps the swap chain to present images (framebuffers) to the windowing system
    VulkanSwapChain m_swapChain;

    // Logical device
    VkDevice m_logicalDevice;

    // Handle to the device graphics queue that command buffers are submitted to
    VkQueue m_graphicsQueue;

    // Command buffer pool on swapchain graphics queue
    VkCommandPool m_cmdPool;

    // Command buffers used for rendering
    std::vector<VkCommandBuffer> m_drawCmdBuffers;

    // Global render pass for frame buffer writes
    VkRenderPass m_renderPass = VK_NULL_HANDLE;

    // List of available frame buffers (same as number of swap chain images)
    std::vector<VkFramebuffer> m_frameBuffers;

    std::vector<VkFence> m_waitFences;

    // Descriptor set pool
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    // Default clear color
    VkClearColorValue m_defaultClearColor = { { 0.0f, 0.0f, 0.0f, 1.0f } };

    // Synchronization semaphores
    struct {
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    } m_semaphores;

    bool m_prepared = false;

    // Settings
    bool m_validation;
    std::string m_applicationName;

    GLFWwindow* m_pWindow;
};