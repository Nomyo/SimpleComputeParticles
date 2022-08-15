#pragma once

#include <vulkan/vulkan.h>
#include <vector>

struct VulkanDevice
{
    explicit VulkanDevice(VkPhysicalDevice physicalDevice);
    ~VulkanDevice();

    VkPhysicalDevice physicalDevice;
    VkDevice logicalDevice;

    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;
    VkPhysicalDeviceFeatures enabledFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;

    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    std::vector<std::string> supportedExtensions;

    // Default command pool for the graphics queue family index
    VkCommandPool commandPool = VK_NULL_HANDLE;

    struct
    {
        uint32_t graphics;
        uint32_t compute;
        uint32_t transfer;
    } queueFamilyIndices;

    VkResult        CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char *> enabledExtensions, bool useSwapChain = true, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    VkCommandPool   CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    uint32_t        GetQueueFamilyIndex(VkQueueFlags queueFlags) const;
};