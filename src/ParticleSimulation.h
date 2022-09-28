#pragma once

#include <VulkanCore.h>
#include <VulkanTexture.h>
#include <glm/glm.hpp>

#define VERTEX_BUFFER_BIND_ID 0

#define ENABLE_VALIDATION true

#define PARTICLE_COUNT 25600 * 4

// SSBO particle declaration
struct Particle {
    glm::vec4 pos; // Particle position
    glm::vec4 vel; // Particle velocity
};

struct CubeVertex {
    glm::vec3 pos;
    glm::vec3 color;
};

struct BufferWrapper {
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDescriptorBufferInfo descriptor;
    void *mapped = nullptr;
};

class ParticleSimulation : public VulkanCore
{
public:
    struct VerticesDescription {
        VkPipelineVertexInputStateCreateInfo inputState;
        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    };

    VerticesDescription m_particleVertices;
    VerticesDescription m_cubeVertices;

    // Resources for the graphics part
    struct {
        uint32_t queueFamilyIndex;

        struct pipelineWrapper {
            VkDescriptorSetLayout descriptorSetLayout;  // Image display shader binding layout
            VkDescriptorSet descriptorSet;              // Particle system rendering shader bindings
            VkPipeline pipeline;                        // Image display pipeline
            VkPipelineLayout pipelineLayout;            // Layout of the graphics pipeline
        };

        // Particle pipeline
        pipelineWrapper particle;

        // Simple Cube pipeline
        BufferWrapper cubeVertexBuffer;
        pipelineWrapper cube;

        VkSemaphore semaphore;                      // Execution dependency between compute & graphic submission
        BufferWrapper uniformBuffer;
        struct graphicsUbo {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 projection;
        } ubo;
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
            float destZ;
            uint32_t particleCount = PARTICLE_COUNT;
        } ubo;
    } m_compute;

    struct {
        Texture2D particle;
    } m_textures;

    ParticleSimulation();
    virtual ~ParticleSimulation();

    virtual void Render();
    virtual void Prepare();
private:

    void PrepareGraphics();
    void PrepareCompute();

    void PrepareGraphicsPipelines();
    void PrepareParticlePipeline();
    void PrepareCubePipeline();

    void PrepareStorageBuffers();
    void PrepareCubeVextexBuffers();
    void PrepareUniformBuffers();

    void Draw();
    void LoadAssets();

    void SetupParticleDescriptorSetLayout();
    void SetupParticleDescriptorPool();
    void SetupParticleDescriptorSet();

    virtual void BuildCommandBuffers();
    void BuildComputeCommandBuffer();
    void UpdateUniformBuffers();
    void UpdateViewUniformBuffers();

    virtual void OnUpdateUIOverlay(VulkanIamGuiWrapper* ui);
    virtual void OnViewChanged();

    std::vector<VkFence> m_queueCompleteFences;

    bool m_attractorMouse;

    uint32_t m_indexCount;
};

