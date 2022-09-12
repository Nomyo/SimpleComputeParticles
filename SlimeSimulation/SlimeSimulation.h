#pragma once

#include <VulkanCore.h>
#include <VulkanTexture.h>
#include <glm/glm.hpp>

#define VERTEX_BUFFER_BIND_ID 0

#define ENABLE_VALIDATION true

#define PARTICLE_COUNT 256000 * 4  

// SSBO particle declaration
struct Particle {
    glm::vec2 pos; // Particle position
    glm::vec2 vel; // Particle velocity
};

struct BufferWrapper {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorBufferInfo descriptor;
    void *mapped = nullptr;
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
        uint32_t queueFamilyIndex;
        VkDescriptorSetLayout descriptorSetLayout;  // Image display shader binding layout
        VkDescriptorSet descriptorSet;              // Particle system rendering shader bindings
        VkPipeline pipeline;                        // Image display pipeline
        VkPipelineLayout pipelineLayout;            // Layout of the graphics pipeline
        VkSemaphore semaphore;                      // Execution dependency between compute & graphic submission
    } m_graphics;

    struct {
        uint32_t queueFamilyIndex;
        VkQueue queue;
        VkCommandPool commandPool;
        VkCommandBuffer commandBuffer;
        VkDescriptorSetLayout descriptorSetLayout;
        VkDescriptorSet descriptorSet;
        VkPipeline pipeline;
        VkPipelineLayout pipelineLayout;
        BufferWrapper storageBuffer;
        BufferWrapper uniformBuffer;
        VkSemaphore semaphore;                      // Execution dependency between compute & graphic submission
        struct computeUbo {
            float elapsedTime;
            float destX;
            float destY;
            uint32_t particleCount = PARTICLE_COUNT;
        } ubo;
    } m_compute;

    struct {
        Texture2D particle;
    } m_textures;

    SlimeSimulation();
    virtual ~SlimeSimulation();

    virtual void Render();
    virtual void Prepare();
private:

    void PrepareGraphics();
    void PrepareCompute();
    void PreparePipelines();
    void PrepareStorageBuffers();
    void PrepareUniformBuffers();

    void Draw();
    void LoadAssets();

    void SetupDescriptorSetLayout();
    void SetupDescriptorPool();
    void SetupDescriptorSet();

    virtual void BuildCommandBuffers();
    void BuildComputeCommandBuffer();
    void UpdateUniformBuffers();

    virtual void OnUpdateUIOverlay(VulkanIamGuiWrapper* ui);

    std::vector<VkFence> m_queueCompleteFences;

    BufferWrapper m_vertexBuffer;
    BufferWrapper m_indexBuffer;

    bool m_attractorMouse;

    uint32_t m_indexCount;
};

