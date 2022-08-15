#pragma once

#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

#include <VulkanDevice.h>

class VulkanCore
{
public:
    VulkanCore(bool enableValidation = false);
    virtual ~VulkanCore();

    // Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
    void InitVulkan();

    void SetupWindow();
    void Prepare();
    void RenderLoop();

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

    // Logical device
    VkDevice m_device;

    // Handle to the device graphics queue that command buffers are submitted to
    VkQueue m_graphicsQueue;

    // Synchronization semaphores
    struct {
        // Swap chain image presentation
        VkSemaphore presentComplete;
        // Command buffer submission and execution
        VkSemaphore renderComplete;
    } m_semaphores;

    // Settings
    bool m_validation;
    std::string m_applicationName;

    GLFWwindow* m_pWindow;
};