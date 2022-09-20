#include <SlimeSimulation.h>
#include <VulkanCamera.h>
#include <VulkanUtils.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/gtc/matrix_transform.hpp>

#define ENABLE_VALIDATION true

#include <array>
#include <ctime>
#include <fstream>
#include <random>

SlimeSimulation::SlimeSimulation() : VulkanCore(ENABLE_VALIDATION)
{
    float aspect = (float)m_width / (float)m_height;
    m_camera = VulkanCamera(glm::vec3(0, 0, -5), glm::vec3(0), 45.0, aspect, 0.0, 100.);
    m_camera.SetRotationSpeed(0.3f);
    m_camera.SetMovementSpeed(10.f);
    m_attractorMouse = false;
}

SlimeSimulation::~SlimeSimulation()
{
    // Delete buffers
    vkFreeMemory(m_logicalDevice, m_vertexBuffer.memory, nullptr);
    vkDestroyBuffer(m_logicalDevice, m_vertexBuffer.buffer, nullptr);
    vkFreeMemory(m_logicalDevice, m_indexBuffer.memory, nullptr);
    vkDestroyBuffer(m_logicalDevice, m_indexBuffer.buffer, nullptr);

    // Destroy fences
    for (auto& fence : m_queueCompleteFences) {
        vkDestroyFence(m_logicalDevice, fence, nullptr);
    }

    // Destroy textures
    m_textures.particle.Destroy();

    // Destroy compute
    vkFreeMemory(m_logicalDevice, m_compute.storageBuffer.memory, nullptr);
    vkDestroyBuffer(m_logicalDevice, m_compute.storageBuffer.buffer, nullptr);
    vkFreeMemory(m_logicalDevice, m_compute.uniformBuffer.memory, nullptr);
    vkDestroyBuffer(m_logicalDevice, m_compute.uniformBuffer.buffer, nullptr);

    vkDestroyDescriptorSetLayout(m_logicalDevice, m_compute.descriptorSetLayout, nullptr);
    vkDestroySemaphore(m_logicalDevice, m_compute.semaphore, nullptr);

    vkDestroyPipelineLayout(m_logicalDevice, m_compute.pipelineLayout, nullptr);
    vkDestroyPipeline(m_logicalDevice, m_compute.pipeline, nullptr);

    vkFreeCommandBuffers(m_logicalDevice, m_compute.commandPool, 1, &m_compute.commandBuffer);
    vkDestroyCommandPool(m_logicalDevice, m_compute.commandPool, nullptr);

    // Destroy graphics
    vkDestroyDescriptorSetLayout(m_logicalDevice, m_graphics.particle.descriptorSetLayout, nullptr);
    vkDestroyPipelineLayout(m_logicalDevice, m_graphics.particle.pipelineLayout, nullptr);
    vkDestroyPipeline(m_logicalDevice, m_graphics.particle.pipeline, nullptr);
    vkDestroySemaphore(m_logicalDevice, m_graphics.semaphore, nullptr);
}


void SlimeSimulation::Render()
{
    if (!m_prepared)
    {
        return;
    }

    Draw();
    UpdateUniformBuffers();
}

void SlimeSimulation::Draw()
{
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    // Submit graphics commands
    VkSubmitInfo computeSubmitInfo{};
    computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    computeSubmitInfo.commandBufferCount = 1;
    computeSubmitInfo.pCommandBuffers = &m_compute.commandBuffer;
    computeSubmitInfo.waitSemaphoreCount = 1;
    computeSubmitInfo.pWaitSemaphores = &m_graphics.semaphore;
    computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
    computeSubmitInfo.signalSemaphoreCount = 1;
    computeSubmitInfo.pSignalSemaphores = &m_compute.semaphore;
    VK_CHECK_RESULT(vkQueueSubmit(m_compute.queue, 1, &computeSubmitInfo, nullptr));
    // Acquire the next image
    // Note the cpu wait if there is image ready to be rendered in. However with 3 frames in flights and using a mail box presenting more
    // we should always have at least one image ready
    VulkanCore::PrepareFrame();

    VkPipelineStageFlags graphicsWaitStageMasks[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSemaphore graphicsWaitSemaphores[] = { m_compute.semaphore, m_semaphores.presentComplete };
    VkSemaphore graphicsSignalSemaphores[] = { m_graphics.semaphore, m_semaphores.renderComplete };

    // Submit graphics commands
    m_submitInfo.commandBufferCount = 1;
    m_submitInfo.pCommandBuffers = &m_drawCmdBuffers[m_currentBuffer];
    m_submitInfo.waitSemaphoreCount = 2;
    m_submitInfo.pWaitSemaphores = graphicsWaitSemaphores;
    m_submitInfo.pWaitDstStageMask = graphicsWaitStageMasks;
    m_submitInfo.signalSemaphoreCount = 2;
    m_submitInfo.pSignalSemaphores = graphicsSignalSemaphores;
    VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &m_submitInfo, nullptr));

    // Present the Frame to the queue
    // NOTE: Submit Frame does an vkQueueWaitIdle on the graphics Queue
    // So parallelism is actually enable only because it does not wait for present operation
    VulkanCore::SubmitFrame();
}

void SlimeSimulation::Prepare()
{
    VulkanCore::Prepare();
    LoadAssets();
    SetupParticleDescriptorPool();

    // create storagee buffer shared betweem compute / vertex shader
    PrepareStorageBuffers();

    PrepareCubeVextexBuffers();

    // create compute UBO and get host accessible mapping
    PrepareUniformBuffers();

    PrepareGraphics();
    PrepareCompute();

    BuildCommandBuffers();
    m_prepared = true;
}

void SlimeSimulation::LoadAssets()
{
    m_textures.particle.LoadFromFile("textures/particle_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, m_vulkanDevice, m_graphicsQueue);
}

void SlimeSimulation::SetupParticleDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.binding = 1;
    uboBinding.descriptorCount = 1;

    VkDescriptorSetLayoutBinding particleSamplerBinding{};
    particleSamplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    particleSamplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    particleSamplerBinding.binding = 0;
    particleSamplerBinding.descriptorCount = 1;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        uboBinding,
        particleSamplerBinding
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_graphics.particle.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pSetLayouts = &m_graphics.particle.descriptorSetLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_graphics.particle.pipelineLayout));
}

void SlimeSimulation::PrepareGraphicsPipelines()
{
    // Cube Pipeline
    PrepareCubePipeline();

    // Particle Pipeline
    PrepareParticlePipeline();
}

void SlimeSimulation::SetupParticleDescriptorPool()
{
    VkDescriptorPoolSize descriptorPoolUniformSize{};
    descriptorPoolUniformSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorPoolUniformSize.descriptorCount = 1;

    VkDescriptorPoolSize descriptorPoolStorageBufferSize{};
    descriptorPoolStorageBufferSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorPoolStorageBufferSize.descriptorCount = 1;

    VkDescriptorPoolSize descriptorPoolImageSampler{};
    descriptorPoolImageSampler.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolImageSampler.descriptorCount = 1;

    std::vector<VkDescriptorPoolSize> poolSizes =
    {
        descriptorPoolUniformSize,
        descriptorPoolStorageBufferSize,
        descriptorPoolImageSampler
    };

    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPoolInfo.maxSets = 3;

    VK_CHECK_RESULT(vkCreateDescriptorPool(m_logicalDevice, &descriptorPoolInfo, nullptr, &m_descriptorPool));
}

void SlimeSimulation::SetupParticleDescriptorSet()
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
    descriptorSetAllocateInfo.pSetLayouts = &m_graphics.particle.descriptorSetLayout;
    descriptorSetAllocateInfo.descriptorSetCount = 1;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(m_logicalDevice, &descriptorSetAllocateInfo, &m_graphics.particle.descriptorSet));

    VkWriteDescriptorSet writeSamplerDescriptorSet{};
    writeSamplerDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSamplerDescriptorSet.dstSet = m_graphics.particle.descriptorSet;
    writeSamplerDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSamplerDescriptorSet.dstBinding = 0;
    writeSamplerDescriptorSet.pImageInfo = &m_textures.particle.m_descriptor;
    writeSamplerDescriptorSet.descriptorCount = 1;

    VkWriteDescriptorSet writeUboDescriptorSet{};
    writeUboDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeUboDescriptorSet.dstSet = m_graphics.particle.descriptorSet;
    writeUboDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeUboDescriptorSet.dstBinding = 1;
    writeUboDescriptorSet.pBufferInfo = &m_graphics.uniformBuffer.descriptor;
    writeUboDescriptorSet.descriptorCount = 1;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets
    {
        writeUboDescriptorSet,
        writeSamplerDescriptorSet
    };

    vkUpdateDescriptorSets(m_logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void SlimeSimulation::PrepareCubeVextexBuffers()
{
    std::vector<CubeVertex> vertexBuffer{
        {glm::vec3{1, 1, 1},    glm::vec3{1}},
        {glm::vec3{-1, 1, 1},   glm::vec3{1}},
        {glm::vec3{-1, 1, -1},  glm::vec3{1}},
        {glm::vec3{1, 1, -1},   glm::vec3{1}},
        {glm::vec3{1, 1, 1},   glm::vec3{1}},
        {glm::vec3{1, -1, 1},    glm::vec3{1}},
        {glm::vec3{1, -1, -1},  glm::vec3{1}},
        {glm::vec3{1, 1, -1},   glm::vec3{1}},
        {glm::vec3{1, -1, -1},   glm::vec3{1}},
        {glm::vec3{-1, -1, -1},    glm::vec3{1}},
        {glm::vec3{-1, 1, -1},    glm::vec3{1}},
        {glm::vec3{-1, -1, -1},    glm::vec3{1}},
        {glm::vec3{-1, -1, 1},    glm::vec3{1}},
        {glm::vec3{-1, 1, 1},    glm::vec3{1}},
        {glm::vec3{-1, -1, 1},    glm::vec3{1}},
        {glm::vec3{1, -1, 1},    glm::vec3{1}},
        {glm::vec3{1, 1, 1},    glm::vec3{1}},
        {glm::vec3{1, -1, 1},   glm::vec3{1}},
        {glm::vec3{1, -1, -1},  glm::vec3{1}},
        {glm::vec3{1, 1, -1},   glm::vec3{1}},
        {glm::vec3{1, -1, -1},   glm::vec3{1}},
    };

    VkDeviceSize storageBufferSize = vertexBuffer.size() * sizeof(CubeVertex);
    BufferWrapper stagingBuffer;

    m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer.buffer,
        &stagingBuffer.memory,
        storageBufferSize,
        vertexBuffer.data());

    m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &m_graphics.cubeVertexBuffer.buffer,
        &m_graphics.cubeVertexBuffer.memory,
        storageBufferSize);

    // Copy from staging buffer to storage buffer
    VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};
    copyRegion.size = storageBufferSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_graphics.cubeVertexBuffer.buffer, 1, &copyRegion);

    m_vulkanDevice->FlushCommandBuffer(copyCmd, m_graphicsQueue, true);

    // Set descriptor
    m_graphics.cubeVertexBuffer.descriptor.buffer = m_graphics.cubeVertexBuffer.buffer;
    m_graphics.cubeVertexBuffer.descriptor.offset = 0;
    m_graphics.cubeVertexBuffer.descriptor.range = VK_WHOLE_SIZE;

    // Cleanup
    vkDestroyBuffer(m_logicalDevice, stagingBuffer.buffer, nullptr);
    vkFreeMemory(m_logicalDevice, stagingBuffer.memory, nullptr);

    // Binding description
    VkVertexInputBindingDescription vInputBindDescription{};
    vInputBindDescription.binding = VERTEX_BUFFER_BIND_ID;
    vInputBindDescription.stride = sizeof(CubeVertex);
    vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_cubeVertices.bindingDescriptions.resize(1);
    m_cubeVertices.bindingDescriptions[0] = vInputBindDescription;

    // Attribute descriptions
    // Describes memory layout and shader positions
    VkVertexInputAttributeDescription vInputPositionAttribDescriptionPosition{};
    vInputPositionAttribDescriptionPosition.location = 0;
    vInputPositionAttribDescriptionPosition.binding = VERTEX_BUFFER_BIND_ID;
    vInputPositionAttribDescriptionPosition.format = VK_FORMAT_R32G32B32_SFLOAT;
    vInputPositionAttribDescriptionPosition.offset = offsetof(CubeVertex, pos);

    VkVertexInputAttributeDescription vInputPositionAttribDescriptionColor{};
    vInputPositionAttribDescriptionColor.location = 1;
    vInputPositionAttribDescriptionColor.binding = VERTEX_BUFFER_BIND_ID;
    vInputPositionAttribDescriptionColor.format = VK_FORMAT_R32G32B32_SFLOAT;
    vInputPositionAttribDescriptionColor.offset = offsetof(CubeVertex, color);

    m_cubeVertices.attributeDescriptions = {
        // Location 0: Position
        vInputPositionAttribDescriptionPosition,

        // Location 1: Color
        vInputPositionAttribDescriptionColor
    };

    // Assign to vertex buffer
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_cubeVertices.inputState = pipelineVertexInputStateCreateInfo;
    m_cubeVertices.inputState.vertexBindingDescriptionCount = (uint32_t)m_cubeVertices.bindingDescriptions.size();
    m_cubeVertices.inputState.pVertexBindingDescriptions = m_cubeVertices.bindingDescriptions.data();
    m_cubeVertices.inputState.vertexAttributeDescriptionCount = (uint32_t)m_cubeVertices.attributeDescriptions.size();
    m_cubeVertices.inputState.pVertexAttributeDescriptions = m_cubeVertices.attributeDescriptions.data();
}

void SlimeSimulation::PrepareStorageBuffers()
{
    std::default_random_engine rndEngine((unsigned)time(nullptr));
    std::uniform_real_distribution<float> rndDist(-1.f, 1.f);

    // Initial particle positions
    std::vector<Particle> particleBuffer(PARTICLE_COUNT);
    for (auto& particle : particleBuffer) {
        particle.pos = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0);
        particle.vel = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0);
    }
    auto sizeParticule = sizeof(Particle);
    VkDeviceSize storageBufferSize = particleBuffer.size() * sizeof(Particle);

    // Staging
    // SSBO won't be changed on the host after upload so copy to device local memory
    BufferWrapper stagingBuffer;

    m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &stagingBuffer.buffer,
        &stagingBuffer.memory,
        storageBufferSize,
        particleBuffer.data());

    m_vulkanDevice->CreateBuffer(
        // The SSBO will be used as a storage buffer for the compute pipeline and as a vertex buffer in the graphics pipeline
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        &m_compute.storageBuffer.buffer,
        &m_compute.storageBuffer.memory,
        storageBufferSize);

    // Copy from staging buffer to storage buffer
    VkCommandBuffer copyCmd = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
    VkBufferCopy copyRegion = {};
    copyRegion.size = storageBufferSize;
    vkCmdCopyBuffer(copyCmd, stagingBuffer.buffer, m_compute.storageBuffer.buffer, 1, &copyRegion);

    m_vulkanDevice->FlushCommandBuffer(copyCmd, m_graphicsQueue, true);

    // Set descriptor
    m_compute.storageBuffer.descriptor.buffer = m_compute.storageBuffer.buffer;
    m_compute.storageBuffer.descriptor.offset = 0;
    m_compute.storageBuffer.descriptor.range = VK_WHOLE_SIZE;

    // Cleanup
    vkDestroyBuffer(m_logicalDevice, stagingBuffer.buffer, nullptr);
    vkFreeMemory(m_logicalDevice, stagingBuffer.memory, nullptr);

    // Binding description
    VkVertexInputBindingDescription vInputBindDescription{};
    vInputBindDescription.binding = VERTEX_BUFFER_BIND_ID;
    vInputBindDescription.stride = sizeof(Particle);
    vInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    m_particleVertices.bindingDescriptions.resize(1);
    m_particleVertices.bindingDescriptions[0] = vInputBindDescription;

    // Attribute descriptions
    // Describes memory layout and shader positions
    VkVertexInputAttributeDescription vInputPositionAttribDescriptionPosition{};
    vInputPositionAttribDescriptionPosition.location = 0;
    vInputPositionAttribDescriptionPosition.binding = VERTEX_BUFFER_BIND_ID;
    vInputPositionAttribDescriptionPosition.format = VK_FORMAT_R32G32B32_SFLOAT;
    vInputPositionAttribDescriptionPosition.offset = offsetof(Particle, pos);

    VkVertexInputAttributeDescription vInputPositionAttribDescriptionVelocity{};
    vInputPositionAttribDescriptionVelocity.location = 1;
    vInputPositionAttribDescriptionVelocity.binding = VERTEX_BUFFER_BIND_ID;
    vInputPositionAttribDescriptionVelocity.format = VK_FORMAT_R32G32B32_SFLOAT;
    vInputPositionAttribDescriptionVelocity.offset = offsetof(Particle, vel);

    m_particleVertices.attributeDescriptions = {
        // Location 0: Position
        vInputPositionAttribDescriptionPosition,

        // Location 1: Velocity
        vInputPositionAttribDescriptionVelocity
    };

    // Assign to vertex buffer
    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
    pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    m_particleVertices.inputState = pipelineVertexInputStateCreateInfo;
    m_particleVertices.inputState.vertexBindingDescriptionCount = (uint32_t)m_particleVertices.bindingDescriptions.size();
    m_particleVertices.inputState.pVertexBindingDescriptions = m_particleVertices.bindingDescriptions.data();
    m_particleVertices.inputState.vertexAttributeDescriptionCount = (uint32_t)m_particleVertices.attributeDescriptions.size();
    m_particleVertices.inputState.pVertexAttributeDescriptions = m_particleVertices.attributeDescriptions.data();
}

void SlimeSimulation::PrepareUniformBuffers()
{
    // Graphics UBO
    VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_graphics.uniformBuffer.buffer,
        &m_graphics.uniformBuffer.memory,
        sizeof(m_graphics.ubo)));

    // Compute UBO
    VK_CHECK_RESULT(m_vulkanDevice->CreateBuffer(
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &m_compute.uniformBuffer.buffer,
        &m_compute.uniformBuffer.memory,
        sizeof(m_compute.ubo)));

    // Map so that further update will just have to write into the buffer
    vkMapMemory(m_logicalDevice, m_graphics.uniformBuffer.memory, 0, sizeof(m_graphics.ubo), 0, &m_graphics.uniformBuffer.mapped);
    vkMapMemory(m_logicalDevice, m_compute.uniformBuffer.memory, 0, sizeof(m_compute.ubo), 0, &m_compute.uniformBuffer.mapped);

    // Set descriptor
    m_graphics.uniformBuffer.descriptor.buffer = m_graphics.uniformBuffer.buffer;
    m_graphics.uniformBuffer.descriptor.offset = 0;
    m_graphics.uniformBuffer.descriptor.range = sizeof(m_graphics.ubo);

    m_compute.uniformBuffer.descriptor.buffer = m_compute.uniformBuffer.buffer;
    m_compute.uniformBuffer.descriptor.offset = 0;
    m_compute.uniformBuffer.descriptor.range = sizeof(m_compute.ubo);

    UpdateUniformBuffers();
    UpdateViewUniformBuffers();
    m_graphics.ubo.model = glm::mat4(1.0f);
}


void SlimeSimulation::UpdateViewUniformBuffers()
{
    m_graphics.ubo.model = glm::rotate(m_graphics.ubo.model, glm::radians(m_frameTimer * 50.0f), glm::vec3(0, 1, 0));
    m_graphics.ubo.view = m_camera.GetViewMatrix();
    m_graphics.ubo.projection = m_camera.GetProjectionMatrix();
    memcpy(m_graphics.uniformBuffer.mapped, &m_graphics.ubo, sizeof(m_graphics.ubo));
}

void SlimeSimulation::UpdateUniformBuffers()
{
    UpdateViewUniformBuffers();
    static float timer = 0.0f;
    static float timerSpeed = .2f;
    timer += timerSpeed * m_frameTimer;
    if (timer > 1.0)
    {
        timer -= 1.0f;
    }

    m_compute.ubo.elapsedTime = m_frameTimer / 80;
    if (!m_attractorMouse)
    {
        m_compute.ubo.destX = sin(glm::radians(m_compute.ubo.elapsedTime * 360.0f));
        m_compute.ubo.destY = sin(glm::radians(m_compute.ubo.elapsedTime * 360.0f));
        m_compute.ubo.destZ = sin(glm::radians(m_compute.ubo.elapsedTime * 360.0f));
    }
    else
    {
        float normalizedMx = ((float)m_mousePosX - static_cast<float>(m_width / 2)) / static_cast<float>(m_width / 2);
        float normalizedMy = ((float)m_mousePosY - static_cast<float>(m_height / 2)) / static_cast<float>(m_height / 2);
        m_compute.ubo.destX = normalizedMx;
        m_compute.ubo.destY = normalizedMy;
    }

    memcpy(m_compute.uniformBuffer.mapped, &m_compute.ubo, sizeof(m_compute.ubo));

    // DEBUG
    /*
    void *data;
    vkMapMemory(m_logicalDevice, m_compute.storageBuffer.memory, 0, PARTICLE_COUNT * sizeof(Particle), 0, &data);

    std::vector<Particle> p;
    p.resize(PARTICLE_COUNT * sizeof(Particle));
    memcpy(p.data(), data, PARTICLE_COUNT * sizeof(Particle));

    for (auto e : p)
    {
        auto pos = e.pos;
        auto vel = e.vel;
        std::cout << pos.x << " " << pos.y << " " << pos.z << std::endl;
        std::cout << vel.x << " " << vel.y << " " << vel.z << std::endl;
    }*/
}

void SlimeSimulation::PrepareCubePipeline()
{
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboBinding.binding = 0;
    uboBinding.descriptorCount = 1;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        uboBinding
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_graphics.cube.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pSetLayouts = &m_graphics.cube.descriptorSetLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_graphics.cube.pipelineLayout));

    // ParticlePipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.flags = 0;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.flags = 0;

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
    dynamicState.flags = 0;

    // Rendering pipeline
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    shaderStages[0] = Utils::LoadShader(m_logicalDevice, "shaders/cube_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Utils::LoadShader(m_logicalDevice, "shaders/cube_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = m_graphics.cube.pipelineLayout;
    pipelineCreateInfo.renderPass = m_renderPass;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.basePipelineIndex = -1;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    pipelineCreateInfo.pVertexInputState = &m_cubeVertices.inputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = (uint32_t)shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphics.cube.pipeline));

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
    descriptorSetAllocateInfo.pSetLayouts = &m_graphics.cube.descriptorSetLayout;
    descriptorSetAllocateInfo.descriptorSetCount = 1;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(m_logicalDevice, &descriptorSetAllocateInfo, &m_graphics.cube.descriptorSet));

    // Matrix, model view projections
    VkWriteDescriptorSet writeUboDescriptorSet{};
    writeUboDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeUboDescriptorSet.dstSet = m_graphics.cube.descriptorSet;
    writeUboDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeUboDescriptorSet.dstBinding = 0;
    writeUboDescriptorSet.pBufferInfo = &m_graphics.uniformBuffer.descriptor;
    writeUboDescriptorSet.descriptorCount = 1;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets
    {
        writeUboDescriptorSet,
    };

    vkUpdateDescriptorSets(m_logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
}

void SlimeSimulation::PrepareParticlePipeline()
{
    SetupParticleDescriptorSetLayout();

    // ParticlePipeline
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo{};
    inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    inputAssemblyStateCreateInfo.flags = 0;
    inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentState.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.flags = 0;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.flags = 0;

    std::vector<VkDynamicState> dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = (uint32_t)dynamicStateEnables.size();
    dynamicState.flags = 0;

    // Rendering pipeline
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    shaderStages[0] = Utils::LoadShader(m_logicalDevice, "shaders/particle_vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = Utils::LoadShader(m_logicalDevice, "shaders/particle_frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = m_graphics.particle.pipelineLayout;
    pipelineCreateInfo.renderPass = m_renderPass;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.basePipelineIndex = -1;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

    pipelineCreateInfo.pVertexInputState = &m_particleVertices.inputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = nullptr;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = (uint32_t)shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(m_logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_graphics.particle.pipeline));

    SetupParticleDescriptorSet();
}

void SlimeSimulation::PrepareGraphics()
{
    PrepareGraphicsPipelines();

    // Semaphore for compute & graphics sync
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreateSemaphore(m_logicalDevice, &semaphoreCreateInfo, nullptr, &m_graphics.semaphore));

    // Signal the semaphore
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_graphics.semaphore;
    VK_CHECK_RESULT(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK_RESULT(vkQueueWaitIdle(m_graphicsQueue));

    // Fences (Used to check draw command buffer completion)
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // Create in signaled state so we don't wait on first render of each command buffer
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    m_queueCompleteFences.resize(m_drawCmdBuffers.size());
    for (auto& fence : m_queueCompleteFences)
    {
        VK_CHECK_RESULT(vkCreateFence(m_logicalDevice, &fenceCreateInfo, nullptr, &fence));
    }
}

void SlimeSimulation::PrepareCompute()
{
    // Create a compute capable device queue
    // The VulkanDevice::createLogicalDevice functions finds a compute capable queue and prefers queue families that only support compute
    // Depending on the implementation this may result in different queue family indices for graphics and computes,
    // requiring proper synchronization (see the memory and pipeline barriers)
    vkGetDeviceQueue(m_logicalDevice, m_compute.queueFamilyIndex, 0, &m_compute.queue);

    // Create compute pipeline
    // Compute pipelines are created separate from graphics pipelines even if they use the same queue (family index)
    VkDescriptorSetLayoutBinding particleSSBOBinding{};
    particleSSBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    particleSSBOBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    particleSSBOBinding.binding = 0;
    particleSSBOBinding.descriptorCount = 1;

    VkDescriptorSetLayoutBinding particleUBOBinding{};
    particleUBOBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    particleUBOBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    particleUBOBinding.binding = 1;
    particleUBOBinding.descriptorCount = 1;

    std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
        particleSSBOBinding,
        particleUBOBinding
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pBindings = setLayoutBindings.data();
    descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(m_logicalDevice, &descriptorSetLayoutCreateInfo, nullptr, &m_compute.descriptorSetLayout));

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.pSetLayouts = &m_compute.descriptorSetLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutCreateInfo, nullptr, &m_compute.pipelineLayout));


    // Write descriptor sets
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
    descriptorSetAllocateInfo.pSetLayouts = &m_compute.descriptorSetLayout;
    descriptorSetAllocateInfo.descriptorSetCount = 1;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(m_logicalDevice, &descriptorSetAllocateInfo, &m_compute.descriptorSet));

    VkWriteDescriptorSet particleSSBODescriptorSet{};
    particleSSBODescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    particleSSBODescriptorSet.dstSet = m_compute.descriptorSet;
    particleSSBODescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    particleSSBODescriptorSet.dstBinding = 0;
    particleSSBODescriptorSet.pBufferInfo = &m_compute.storageBuffer.descriptor;
    particleSSBODescriptorSet.descriptorCount = 1;

    VkWriteDescriptorSet particleUBODescriptorSet{};
    particleUBODescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    particleUBODescriptorSet.dstSet = m_compute.descriptorSet;
    particleUBODescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    particleUBODescriptorSet.dstBinding = 1;
    particleUBODescriptorSet.pBufferInfo = &m_compute.uniformBuffer.descriptor;
    particleUBODescriptorSet.descriptorCount = 1;

    std::vector<VkWriteDescriptorSet> writeDescriptorSets
    {
        particleSSBODescriptorSet,
        particleUBODescriptorSet
    };
    vkUpdateDescriptorSets(m_logicalDevice, (uint32_t)writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

    VkComputePipelineCreateInfo computePipelineCreateInfo{};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.layout = m_compute.pipelineLayout;
    computePipelineCreateInfo.flags = 0;
    computePipelineCreateInfo.stage = Utils::LoadShader(m_logicalDevice, "shaders/simulation_comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
    VK_CHECK_RESULT(vkCreateComputePipelines(m_logicalDevice, nullptr, 1, &computePipelineCreateInfo, nullptr, &m_compute.pipeline));

    VkCommandPoolCreateInfo computeCommandPoolCreateInfo{};
    computeCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computeCommandPoolCreateInfo.queueFamilyIndex = m_compute.queueFamilyIndex;
    computeCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(vkCreateCommandPool(m_logicalDevice, &computeCommandPoolCreateInfo, nullptr, &m_compute.commandPool));

    // Create a command buffer for compute operations
    m_compute.commandBuffer = m_vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, m_compute.commandPool);

    // Semaphore for compute & graphics sync
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK_RESULT(vkCreateSemaphore(m_logicalDevice, &semaphoreCreateInfo, nullptr, &m_compute.semaphore));

    // Build a single command buffer containing the compute dispatch commands
    BuildComputeCommandBuffer();
}

void SlimeSimulation::BuildCommandBuffers()
{
    VkCommandBufferBeginInfo cmdBufInfo{};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkClearValue clearValues = { {m_defaultClearColor} };

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = m_renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = m_width;
    renderPassBeginInfo.renderArea.extent.height = m_height;
    renderPassBeginInfo.clearValueCount = 1;
    renderPassBeginInfo.pClearValues = &clearValues;

    for (int32_t i = 0; i < m_drawCmdBuffers.size(); ++i)
    {
        // Set target frame buffer
        renderPassBeginInfo.framebuffer = m_frameBuffers[i];

        VK_CHECK_RESULT(vkBeginCommandBuffer(m_drawCmdBuffers[i], &cmdBufInfo));

        vkCmdBeginRenderPass(m_drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_width;
        viewport.height = (float)m_height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_drawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent.width = m_width;
        scissor.extent.height = m_height;
        scissor.offset.x = 0;
        scissor.offset.y = 0;

        vkCmdSetScissor(m_drawCmdBuffers[i], 0, 1, &scissor);

        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics.particle.pipeline);
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics.particle.pipelineLayout, 0, 1, &m_graphics.particle.descriptorSet, 0, NULL);
        vkCmdBindVertexBuffers(m_drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_compute.storageBuffer.buffer, offsets);
        vkCmdDraw(m_drawCmdBuffers[i], PARTICLE_COUNT, 1, 0, 0);

        vkCmdBindPipeline(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics.cube.pipeline);
        vkCmdBindDescriptorSets(m_drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics.cube.pipelineLayout, 0, 1, &m_graphics.cube.descriptorSet, 0, NULL);
        vkCmdBindVertexBuffers(m_drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &m_graphics.cubeVertexBuffer.buffer, offsets);
        vkCmdDraw(m_drawCmdBuffers[i], 21, 1, 0, 0); // 8 Vertices for a cube

        DrawUI(m_drawCmdBuffers[i]);

        vkCmdEndRenderPass(m_drawCmdBuffers[i]);

        VK_CHECK_RESULT(vkEndCommandBuffer(m_drawCmdBuffers[i]));
    }
}

void SlimeSimulation::BuildComputeCommandBuffer()
{
    VkCommandBufferBeginInfo cmdBufInfo{};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK_RESULT(vkBeginCommandBuffer(m_compute.commandBuffer, &cmdBufInfo));

    // make sure ssbo ownership is passed to compute queue
    if (m_graphics.queueFamilyIndex != m_compute.queueFamilyIndex)
    {
        VkBufferMemoryBarrier bufferMemoryBarrier{};
        bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferMemoryBarrier.srcAccessMask = 0;
        bufferMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferMemoryBarrier.srcQueueFamilyIndex = m_graphics.queueFamilyIndex;
        bufferMemoryBarrier.dstQueueFamilyIndex = m_compute.queueFamilyIndex;
        bufferMemoryBarrier.pNext = nullptr;
        bufferMemoryBarrier.size = m_compute.storageBuffer.descriptor.range; // range == size
        bufferMemoryBarrier.buffer = m_compute.storageBuffer.buffer;

        vkCmdPipelineBarrier(
            m_compute.commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, nullptr,
            1, &bufferMemoryBarrier,
            0, nullptr);
    }

    // Dispatch the compute job
    vkCmdBindPipeline(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute.pipeline);
    vkCmdBindDescriptorSets(m_compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_compute.pipelineLayout, 0, 1, &m_compute.descriptorSet, 0, 0);
    vkCmdDispatch(m_compute.commandBuffer, PARTICLE_COUNT / 1024, 1, 1);

    // Add barrier to ensure that compute shader has finished writing to the buffer
    // Without this the (rendering) vertex shader may display incomplete results (partial data from last frame)
    if (m_graphics.queueFamilyIndex != m_compute.queueFamilyIndex)
    {
        VkBufferMemoryBarrier bufferMemoryBarrier{};
        bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        bufferMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bufferMemoryBarrier.dstAccessMask = 0;
        bufferMemoryBarrier.srcQueueFamilyIndex = m_compute.queueFamilyIndex;
        bufferMemoryBarrier.dstQueueFamilyIndex = m_graphics.queueFamilyIndex;
        bufferMemoryBarrier.pNext = nullptr;
        bufferMemoryBarrier.size = m_compute.storageBuffer.descriptor.range; // range == size
        bufferMemoryBarrier.buffer = m_compute.storageBuffer.buffer;

        vkCmdPipelineBarrier(
            m_compute.commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            1, &bufferMemoryBarrier,
            0, nullptr);
    }

    vkEndCommandBuffer(m_compute.commandBuffer);
}

void SlimeSimulation::OnUpdateUIOverlay(VulkanIamGuiWrapper *uiWrapper)
{
    //uiWrapper->CheckBox("Attach attractor to cursor", &m_attractorMouse);
}

void SlimeSimulation::OnViewChanged()
{
    UpdateViewUniformBuffers();
}
