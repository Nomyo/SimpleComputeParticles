#include <VulkanCore.h>

#include <algorithm>
#include <functional>
#include <iostream>

namespace {

    void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<VulkanCore*>(glfwGetWindowUserPointer(window));
        app->WindowResize();
    }

} // anomymous

VulkanCore::VulkanCore(bool enableValidation)
    : m_validation(enableValidation)
    , m_applicationName("Vulkan Application")
{

}
VulkanCore::~VulkanCore()
{

}

VkResult VulkanCore::CreateInstance(bool enableValidation)
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_applicationName.c_str();
    appInfo.pEngineName = m_applicationName.c_str();
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Get extensions supported by the instance and store for later use
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties extension : extensions)
            {
                m_supportedInstanceExtensions.push_back(extension.extensionName);
            }
        }
    }

    // Enabled requested instance extensions
    if (m_enabledInstanceExtensions.size() > 0)
    {
        for (const char* enabledExtension : m_enabledInstanceExtensions)
        {
            // Output message if requested extension is not available
            if (std::find(m_supportedInstanceExtensions.begin(), m_supportedInstanceExtensions.end(), enabledExtension) == m_supportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.push_back(enabledExtension);
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    if (instanceExtensions.size() > 0)
    {
        if (enableValidation)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }
    
    if (enableValidation)
    {
        // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        }
        else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    return vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
}

void VulkanCore::InitVulkan()
{
    CreateInstance(m_validation);
}

void VulkanCore::SetupWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_pWindow = glfwCreateWindow(m_width, m_height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(m_pWindow, this);

    glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
}

void VulkanCore::Prepare()
{
    // TODO
}

void VulkanCore::RenderLoop()
{
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
    }
    //vkDeviceWaitIdle(m_device);
}

void VulkanCore::WindowResize()
{
    // TODO
}
