#include <VulkanCore.h>
#include <VulkanUtils.h>

#include <algorithm>
#include <functional>
#include <iostream>


namespace {

    void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto app = reinterpret_cast<VulkanCore*>(glfwGetWindowUserPointer(window));
        app->WindowResize();
    }

} // anomymous

VulkanCore::VulkanCore(bool enableValidation)
    : m_validation(enableValidation)
    , m_applicationName("Vulkan Application")
{
}

VulkanCore::~VulkanCore()
{

}

VkResult VulkanCore::CreateInstance(bool enableValidation)
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = m_applicationName.c_str();
    appInfo.pEngineName = m_applicationName.c_str();
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Get extensions supported by the instance and store for later use
    uint32_t extCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
    if (extCount > 0)
    {
        std::vector<VkExtensionProperties> extensions(extCount);
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
        {
            for (VkExtensionProperties extension : extensions)
            {
                m_supportedInstanceExtensions.push_back(extension.extensionName);
            }
        }
    }

    VkInstanceCreateInfo instanceCreateInfo = {};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.pApplicationInfo = &appInfo;

    // Surface extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (auto i = 0u; i < glfwExtensionCount; ++i)
    {
        auto str = glfwExtensions[i];
        m_enabledInstanceExtensions.push_back(str);
    }

    instanceCreateInfo.enabledExtensionCount = glfwExtensionCount;
    instanceCreateInfo.ppEnabledExtensionNames = glfwExtensions;

    // Enabled requested instance extensions
    if (m_enabledInstanceExtensions.size() > 0)
    {
        for (const char* enabledExtension : m_enabledInstanceExtensions)
        {
            // Output message if requested extension is not available
            if (std::find(m_supportedInstanceExtensions.begin(), m_supportedInstanceExtensions.end(), enabledExtension) == m_supportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.push_back(enabledExtension);
        }
    }

    if (instanceExtensions.size() > 0)
    {
        if (enableValidation)
        {
            instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
            instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    if (enableValidation)
    {
        // The VK_LAYER_KHRONOS_validation contains all current validation functionality.
        const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

        // Check if this layer is available at instance level
        uint32_t instanceLayerCount;
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
        std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
        vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());

        bool validationLayerPresent = false;
        for (VkLayerProperties layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }
        if (validationLayerPresent) {
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
            instanceCreateInfo.enabledLayerCount = 1;
        }
        else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }
    return vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);
}

void VulkanCore::InitVulkan()
{
    // Vulkan instance
    VkResult err;
    err = CreateInstance(m_validation);
    if (err) {
        throw("Could not create Vulkan instance : \n" + Utils::errorString(err), err);
    }

    // Physical device
    uint32_t gpuCount = 0;
    // Get number of available physical devices
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(m_instance, &gpuCount, nullptr));
    if (gpuCount == 0) {
        throw("No device with Vulkan support found", -1);
    }
    // Enumerate devices
    std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
    err = vkEnumeratePhysicalDevices(m_instance, &gpuCount, physicalDevices.data());
    if (err) {
        throw("Could not enumerate physical devices : \n" + Utils::errorString(err), err);
    }

    // GPU selection: defaulted to the first device for now
    m_physicalDevice = physicalDevices[0];

    // Store properties (including limits), features and memory properties of the physical device
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_deviceProperties);
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &m_deviceFeatures);
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_deviceMemoryProperties);

#if 0
    // Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
    getEnabledFeatures();
#endif

    // Vulkan device creation that encapsulate functions related to a device
    vulkanDevice = new VulkanDevice(m_physicalDevice);
    VkResult res = vulkanDevice->CreateLogicalDevice(m_enabledFeatures, m_enabledDeviceExtensions);
    if (res != VK_SUCCESS) {
        throw("Could not create Vulkan device: \n" + Utils::errorString(res), res);
    }

    m_logicalDevice = vulkanDevice->logicalDevice;

    // Pass the necessary handle to the swapChain wrapper
    m_swapChain.Init(m_instance, m_physicalDevice, m_logicalDevice);

    // Get a graphics queue from the device
    vkGetDeviceQueue(m_logicalDevice, vulkanDevice->queueFamilyIndices.graphics, 0, &m_graphicsQueue);

    // Create synchronisation objects
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Create a semaphore used to synchronize image presentation
    // Ensures that the image is displayed before we start submitting new commands to the queue
    VK_CHECK_RESULT(vkCreateSemaphore(m_logicalDevice, &semaphoreCreateInfo, nullptr, &m_semaphores.presentComplete));
    // Create a semaphore used to synchronize command submission
    // Ensures that the image is not presented until all commands have been submitted and executed
    VK_CHECK_RESULT(vkCreateSemaphore(m_logicalDevice, &semaphoreCreateInfo, nullptr, &m_semaphores.renderComplete));
}

void VulkanCore::SetupWindow()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_pWindow = glfwCreateWindow(m_width, m_height, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(m_pWindow, this);

    glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
}

void VulkanCore::Prepare()
{
    // Init swapchain surface
    m_swapChain.InitSurface(m_pWindow);

    // Create command buffer pool on Graphics Queue
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdPoolInfo.queueFamilyIndex = m_swapChain.GetQueueIndex();
    cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK_RESULT(vkCreateCommandPool(m_logicalDevice, &cmdPoolInfo, nullptr, &m_cmdPool));

    // Setup the swapchain
    m_swapChain.Create(&m_width, &m_width);

    SetupRenderPass();

}

void VulkanCore::RenderLoop()
{
    while (!glfwWindowShouldClose(m_pWindow)) {
        glfwPollEvents();
    }

    // Flush device to make sure all resources can be freed
    if (m_logicalDevice != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_logicalDevice);
    }
}

void VulkanCore::SetupRenderPass()
{
    VkAttachmentDescription attachments = {};
    // Color attachment
    attachments.format = m_swapChain.GetColorFormat();
    attachments.samples = VK_SAMPLE_COUNT_1_BIT;
    attachments.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorReference = {};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorReference;
    subpassDescription.inputAttachmentCount = 0;
    subpassDescription.pInputAttachments = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments = nullptr;
    subpassDescription.pResolveAttachments = nullptr;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK_RESULT(vkCreateRenderPass(m_logicalDevice, &renderPassInfo, nullptr, &m_renderPass));
}

void VulkanCore::CreateCommandBuffers()
{
    // Create one command buffer for each swap chain image and reuse for rendering
    m_drawCmdBuffers.resize(m_swapChain.GetImageCount());

    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = m_cmdPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(m_drawCmdBuffers.size());

    VK_CHECK_RESULT(vkAllocateCommandBuffers(m_logicalDevice, &commandBufferAllocateInfo, m_drawCmdBuffers.data()));
}

void VulkanCore::CreateSynchronizationPrimitives()
{
    // Wait fences to sync command buffer access
    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    m_waitFences.resize(m_drawCmdBuffers.size());
    for (auto& fence : m_waitFences) {
        VK_CHECK_RESULT(vkCreateFence(m_logicalDevice, &fenceCreateInfo, nullptr, &fence));
    }
}

void VulkanCore::WindowResize()
{
    // TODO
}
