#pragma once

#include <vulkan/vulkan.h>
#include <VulkanDevice.h>
#include <glm/glm.hpp>

struct VulkanIamGuiWrapper
{
    VulkanDevice *device;

    VkImage fontImage = VK_NULL_HANDLE;
    VkImageView fontImageView = VK_NULL_HANDLE;
    VkDeviceMemory fontMemory = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    std::vector<VkPipelineShaderStageCreateInfo> shaders;

    int32_t vertexCount = 0;
    int32_t indexCount = 0;

    struct PushConstBlock {
        glm::vec2 scale;
        glm::vec2 translate;
    } pushConstBlock;

    // Pipeline related
    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet descriptorSet;
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    // Buffers
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexMemory;
    void *vertexMapped;
    VkBuffer indexBuffer;
    VkDeviceMemory indexMemory;
    void *indexMapped;

    VkQueue queue;

    bool updated;
    bool visible;

    VulkanIamGuiWrapper();
    ~VulkanIamGuiWrapper();

    void CleanUp();
    void PrepareResources();
    void PreparePipeline(const VkRenderPass renderPass);
    bool UpdateBuffers();
    void Draw(VkCommandBuffer commandBuffer);

    bool CheckBox(const std::string& caption, bool* value);
};
