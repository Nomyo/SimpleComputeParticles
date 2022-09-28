add_executable(Slime_simulation
    Main.cpp
    SlimeSimulation.cpp
    VulkanCore/VulkanCamera.cpp
    VulkanCore/VulkanCore.cpp
    VulkanCore/VulkanImguiWrapper.cpp
    VulkanCore/VulkanSwapChain.cpp
    VulkanCore/VulkanTexture.cpp
    VulkanCore/VulkanDevice.cpp)


set_property(TARGET Slime_simulation PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Slime_simulation>")

target_include_directories(Slime_simulation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" VulkanCore)
target_link_libraries(Slime_simulation glm ktx imgui glfw)

target_link_libraries(Slime_simulation Vulkan::Vulkan)

add_dependencies(Slime_simulation Shaders)
