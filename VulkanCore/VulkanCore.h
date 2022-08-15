#pragma once

#include <windows.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>

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
    std::vector<const char*> m_enabledDeviceExtensions;
    std::vector<const char*> m_enabledInstanceExtensions;

    // Settings
    bool m_validation;
    std::string m_applicationName;

    GLFWwindow* m_pWindow;
};