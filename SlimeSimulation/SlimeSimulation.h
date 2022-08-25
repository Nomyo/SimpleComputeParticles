#pragma once

#include <VulkanCore.h>
#include <glm/glm.hpp>

#define VERTEX_BUFFER_BIND_ID 0

#define ENABLE_VALIDATION true

struct Vertex {
    glm::vec3 pos;
    //float uv[2]; // Texture coord
};

struct BufferWrapper {
    VkBuffer buffer;
    VkDeviceMemory memory;
};

class SlimeSimulation : public VulkanCore
{
public:
    struct {
        VkPipelineVertexInputStateCreateInfo inputState;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    } m_vertices;

    // Resources for the graphics part
    struct {
        VkDescriptorSetLayout descriptorSetLayout;  // Image display shader binding layout
        VkDescriptorSet descriptorSetPreCompute;    // Image display shader bindings before compute shader image manipulation
        VkDescriptorSet descriptorSetPostCompute;   // Image display shader bindings after compute shader image manipulation
        VkPipeline pipeline;                        // Image display pipeline
        VkPipelineLayout pipelineLayout;            // Layout of the graphics pipeline
        VkSemaphore semaphore;                      // Execution dependency between compute & graphic submission
    } m_graphics;

    SlimeSimulation();

    virtual void Render();
    virtual void Prepare();
private:

    void GenerateTriangle();
    void SetupVertexDescriptions();
    void SetupDescriptorSetLayout();
    void PreparePipelines();
    void SetupDescriptorPool();
    void SetupDescriptorSet();
    void PrepareGraphics();
    void BuildCommandBuffers();

    VkPipelineShaderStageCreateInfo LoadShader(const std::string& filepath, VkShaderStageFlagBits stage);

    BufferWrapper m_vertexBuffer;
    BufferWrapper m_indexBuffer;

    uint32_t m_indexCount;
};

