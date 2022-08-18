#include <VulkanSwapChain.h>

#include <GLFW/glfw3.h>
#include <cassert>

VulkanSwapChain::VulkanSwapChain()
{
}

VulkanSwapChain::~VulkanSwapChain()
{

}

void VulkanSwapChain::Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice)
{
    m_instance = instance;
    m_logicalDevice = logicalDevice;
    m_physicalDevice = physicalDevice;
}

void VulkanSwapChain::InitSurface(GLFWwindow* pWindow)
{
    if (glfwCreateWindowSurface(m_instance, pWindow, nullptr, &m_surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    // Get available queue family properties
    uint32_t queueCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, NULL);
    assert(queueCount >= 1);

    std::vector<VkQueueFamilyProperties> queueProps(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueCount, queueProps.data());

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    std::vector<VkBool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; i++)
    {
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; i++)
    {
        if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
        {
            if (graphicsQueueNodeIndex == UINT32_MAX)
            {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == VK_TRUE)
            {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }
    if (presentQueueNodeIndex == UINT32_MAX)
    {
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queueCount; ++i)
        {
            if (supportsPresent[i] == VK_TRUE)
            {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
    {
        throw("Could not find a graphics and/or presenting queue!", -1);
    }

    if (graphicsQueueNodeIndex != presentQueueNodeIndex)
    {
        throw("Separate graphics and presenting queues are not supported yet!", -1);
    }

    m_queueNodeIndex = graphicsQueueNodeIndex;
}

uint32_t VulkanSwapChain::GetQueueIndex()
{
    return m_queueNodeIndex;
}
