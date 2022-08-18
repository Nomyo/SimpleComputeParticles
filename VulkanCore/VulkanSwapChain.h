#pragma once

#include <vulkan/vulkan.h>

#include <vector>

typedef struct SwapChainBuffers_Rec {
    VkImage image;
    VkImageView view;
} SwapChainBuffer;

struct GLFWwindow;

class VulkanSwapChain
{
public:
    VulkanSwapChain();
    ~VulkanSwapChain();
    void Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice logicalDevice);
    void InitSurface(GLFWwindow* pWindow);

    uint32_t GetQueueIndex();

private:
    VkSurfaceKHR m_surface;
    VkFormat m_colorFormat;
    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    uint32_t m_imageCount;
    std::vector<VkImage> m_images;
    std::vector<SwapChainBuffer> m_buffers;
    uint32_t m_queueNodeIndex = UINT32_MAX;

    VkInstance m_instance;
    VkDevice m_logicalDevice;
    VkPhysicalDevice m_physicalDevice;
};