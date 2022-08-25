#pragma once

#include <VulkanCore.h>

#define ENABLE_VALIDATION true

struct Vertex {
    float pos[3];
    float uv[2]; // Texture coord
};

struct BufferWrapper {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

class SlimeSimulation : public VulkanCore
{
public:
    SlimeSimulation();

    virtual void Render();
    virtual void Prepare();
private:
    struct {
        VkPipelineVertexInputStateCreateInfo inputState;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    } vertices;

    void GenerateTriangle();
    void SetupVertexDescriptions();
    void SetupDescriptorSetLayout();
    void PreparePipelines();
    void SetupDescriptorPool();
    void SetupDescriptorSet();
    void PrepareGraphics();
    void BuildCommandBuffers();

    BufferWrapper m_vertexBuffer;
    BufferWrapper m_indiceBuffer;

    uint32_t m_indexCount;
};

