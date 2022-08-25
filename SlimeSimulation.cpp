#include <SlimeSimulation.h>
#include <VulkanUtils.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define ENABLE_VALIDATION true

SlimeSimulation::SlimeSimulation() : VulkanCore(ENABLE_VALIDATION)
{ }

void SlimeSimulation::Render()
{
    // Rendering ..
}

void SlimeSimulation::Prepare()
{
    VulkanCore::Prepare();
    GenerateTriangle();
    SetupVertexDescriptions();
    SetupDescriptorSetLayout();
    PreparePipelines();
    SetupDescriptorPool();
    SetupDescriptorSet();
    PrepareGraphics();
    BuildCommandBuffers();
}


void SlimeSimulation::GenerateTriangle()
{
    // Setup only pos vertices for now
    std::vector<glm::vec2>vertices = {
        glm::vec2(0.0, -0.5),
        glm::vec2(0.5, 0.5),
        glm::vec2(-0.5, 0.5)
    };

    // Setup indices
    std::vector<uint32_t> indices = { 0,1,2 };
    m_indexCount = static_cast<uint32_t>(indices.size());

    // Create buffers
    // For the sake of simplicity we won't stage the vertex data to the gpu memory
    // TODO: stage memory for faster access device mem only

    // Vertex buffer
    VK_CHECK_RESULT(vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_vertexBuffer.buffer,
        &m_vertexBuffer.memory,
        vertices.size() * sizeof(Vertex),
        vertices.data()));
    // Index buffer
    VK_CHECK_RESULT(vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_indiceBuffer.buffer,
        &m_indiceBuffer.memory,
        indices.size() * sizeof(uint32_t),
        indices.data()));
}

void SlimeSimulation::SetupVertexDescriptions()
{
}

void SlimeSimulation::SetupDescriptorSetLayout()
{
}

void SlimeSimulation::PreparePipelines()
{
}

void SlimeSimulation::SetupDescriptorPool()
{
}

void SlimeSimulation::SetupDescriptorSet()
{
}

void SlimeSimulation::PrepareGraphics()
{
}

void SlimeSimulation::BuildCommandBuffers()
{
}