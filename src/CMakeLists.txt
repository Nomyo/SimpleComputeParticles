add_executable(ParticleSimulation
    Main.cpp
    ParticleSimulation.cpp
    VulkanCore/VulkanCamera.cpp
    VulkanCore/VulkanCore.cpp
    VulkanCore/VulkanImguiWrapper.cpp
    VulkanCore/VulkanSwapChain.cpp
    VulkanCore/VulkanTexture.cpp
    VulkanCore/VulkanDevice.cpp)


set_property(TARGET ParticleSimulation PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ParticleSimulation>")

target_include_directories(ParticleSimulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" VulkanCore)
target_link_libraries(ParticleSimulation glm ktx imgui glfw)

target_link_libraries(ParticleSimulation Vulkan::Vulkan)

add_dependencies(ParticleSimulation Shaders)
