// module_vulkan.c
// #include "module_vulkan.h"
// #include "vulkan_utils.h"
// #include "module_text.h"
// #include <stdio.h>
// #include <stdlib.h>
// #include "shader2d_vert_spv.h"
// #include "shader2d_frag_spv.h"
// #include <string.h>

#include <vulkan/vulkan.h>
#include "module_vulkan.h"
#include "vulkan_utils.h"
#include "module_text.h"
#include <stdio.h>
#include <stdlib.h>
#include "shader2d_vert_spv.h"
#include "shader2d_frag_spv.h"
#include <string.h>


static const Vertex triangleVertices[] = {
    {0.0f, -0.5f, 1.0f, 0.0f, 0.0f},
    {0.5f, 0.5f, 0.0f, 1.0f, 0.0f},
    {-0.5f, 0.5f, 0.0f, 0.0f, 1.0f}
};

static const Vertex squareVertices[] = {
    {-0.25f, -0.25f, 0.0f, 1.0f, 1.0f},
    {0.25f, -0.25f, 0.0f, 1.0f, 1.0f},
    {-0.25f, 0.25f, 0.0f, 1.0f, 1.0f},
    {0.25f, 0.25f, 0.0f, 1.0f, 1.0f}
};

static const uint32_t squareIndices[] = {0, 1, 2, 2, 1, 3};


bool vulkan_init(SDL_Window *window, VulkanContext *context) {
    // Initialize Vulkan instance
    uint32_t extensionCount = 0;
    const char *const *extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (!extensions) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get Vulkan extensions: %s", SDL_GetError());
        return false;
    }

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "SDL Vulkan Node 2D",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };

    const char *validationLayers[] = { "VK_LAYER_KHRONOS_validation" };
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = validationLayers
    };
    
    if (vkCreateInstance(&createInfo, NULL, &context->instance) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan instance");
        return false;
    }

    // Create surface
    if (!SDL_Vulkan_CreateSurface(window, context->instance, NULL, &context->surface)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan surface: %s", SDL_GetError());
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Select physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(context->instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No Vulkan physical devices found");
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    VkPhysicalDevice *devices = malloc(deviceCount * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(context->instance, &deviceCount, devices);
    context->physicalDevice = devices[0]; // Pick first device
    free(devices);

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(context->physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = malloc(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(context->physicalDevice, &queueFamilyCount, queueFamilies);
    context->graphicsFamily = UINT32_MAX;
    context->presentFamily = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            context->graphicsFamily = i;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(context->physicalDevice, i, context->surface, &presentSupport);
        if (presentSupport) {
            context->presentFamily = i;
        }
        if (context->graphicsFamily != UINT32_MAX && context->presentFamily != UINT32_MAX) {
            break;
        }
    }
    free(queueFamilies);
    if (context->graphicsFamily == UINT32_MAX || context->presentFamily == UINT32_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find required queue families");
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Create logical device
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfos[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = context->graphicsFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        },
        {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = context->presentFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        }
    };
    uint32_t queueCreateInfoCount = context->graphicsFamily == context->presentFamily ? 1 : 2;
    const char *deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .enabledExtensionCount = SDL_arraysize(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions
    };
    if (vkCreateDevice(context->physicalDevice, &deviceInfo, NULL, &context->device) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create Vulkan device");
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    vkGetDeviceQueue(context->device, context->graphicsFamily, 0, &context->graphicsQueue);
    vkGetDeviceQueue(context->device, context->presentFamily, 0, &context->presentQueue);

    // Create swapchain
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice, context->surface, &capabilities);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, NULL);
    VkSurfaceFormatKHR *formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, formats);
    VkSurfaceFormatKHR selectedFormat = formats[0]; // Pick first format
    free(formats);
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, NULL);
    VkPresentModeKHR *presentModes = malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, presentModes);
    VkPresentModeKHR selectedPresentMode = presentModes[0]; // Pick first present mode
    free(presentModes);
    context->swapchainExtent = capabilities.currentExtent;
    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = capabilities.minImageCount + 1,
        .imageFormat = selectedFormat.format,
        .imageColorSpace = selectedFormat.colorSpace,
        .imageExtent = context->swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = context->graphicsFamily == context->presentFamily ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = queueCreateInfoCount,
        .pQueueFamilyIndices = (uint32_t[]){context->graphicsFamily, context->presentFamily},
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = selectedPresentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    if (vkCreateSwapchainKHR(context->device, &swapchainInfo, NULL, &context->swapchain) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create swapchain");
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Get swapchain images
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->imageCount, NULL);
    VkImage *images = malloc(context->imageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->imageCount, images);
    context->imageViews = malloc(context->imageCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < context->imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = selectedFormat.format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };
        if (vkCreateImageView(context->device, &viewInfo, NULL, &context->imageViews[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create image view %u", i);
            for (uint32_t j = 0; j < i; j++) vkDestroyImageView(context->device, context->imageViews[j], NULL);
            free(context->imageViews);
            free(images);
            vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
            vkDestroyDevice(context->device, NULL);
            vkDestroySurfaceKHR(context->instance, context->surface, NULL);
            vkDestroyInstance(context->instance, NULL);
            return false;
        }
    }
    free(images);

    // Create render pass
    VkAttachmentDescription colorAttachment = {
        .format = selectedFormat.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference colorAttachmentRef = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentRef
    };
    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };
    if (vkCreateRenderPass(context->device, &renderPassInfo, NULL, &context->renderPass) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create render pass");
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Create descriptor set layout
    VkDescriptorSetLayoutBinding uboLayoutBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &uboLayoutBinding
    };
    if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, NULL, &context->descriptorSetLayout) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor set layout");
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Create graphics pipeline
    VkShaderModule vertShaderModule, fragShaderModule;
    VkShaderModuleCreateInfo vertShaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(shader2d_vert_spv),
        .pCode = shader2d_vert_spv
    };
    VkShaderModuleCreateInfo fragShaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(shader2d_frag_spv),
        .pCode = shader2d_frag_spv
    };
    if (vkCreateShaderModule(context->device, &vertShaderInfo, NULL, &vertShaderModule) != VK_SUCCESS ||
        vkCreateShaderModule(context->device, &fragShaderInfo, NULL, &fragShaderModule) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader modules");
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule,
            .pName = "main"
        }
    };
    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescriptions[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, x) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, r) }
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = SDL_arraysize(attributeDescriptions),
        .pVertexAttributeDescriptions = attributeDescriptions
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkViewport viewport = { 0.0f, 0.0f, (float)context->swapchainExtent.width, (float)context->swapchainExtent.height, 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, context->swapchainExtent };
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &context->descriptorSetLayout
    };
    if (vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, NULL, &context->pipelineLayout) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create pipeline layout");
        vkDestroyShaderModule(context->device, fragShaderModule, NULL);
        vkDestroyShaderModule(context->device, vertShaderModule, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = NULL,
        .layout = context->pipelineLayout,
        .renderPass = context->renderPass,
        .subpass = 0
    };
    if (vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &context->graphicsPipeline) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create graphics pipeline");
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyShaderModule(context->device, fragShaderModule, NULL);
        vkDestroyShaderModule(context->device, vertShaderModule, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    vkDestroyShaderModule(context->device, fragShaderModule, NULL);
    vkDestroyShaderModule(context->device, vertShaderModule, NULL);

    // Create framebuffers
    context->framebuffers = malloc(context->imageCount * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < context->imageCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = context->renderPass,
            .attachmentCount = 1,
            .pAttachments = &context->imageViews[i],
            .width = context->swapchainExtent.width,
            .height = context->swapchainExtent.height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context->device, &framebufferInfo, NULL, &context->framebuffers[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create framebuffer %u", i);
            for (uint32_t j = 0; j < i; j++) vkDestroyFramebuffer(context->device, context->framebuffers[j], NULL);
            free(context->framebuffers);
            vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
            vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
            vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
            vkDestroyRenderPass(context->device, context->renderPass, NULL);
            for (uint32_t j = 0; j < context->imageCount; j++) vkDestroyImageView(context->device, context->imageViews[j], NULL);
            free(context->imageViews);
            vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
            vkDestroyDevice(context->device, NULL);
            vkDestroySurfaceKHR(context->instance, context->surface, NULL);
            vkDestroyInstance(context->instance, NULL);
            return false;
        }
    }

    // Create command pool
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = context->graphicsFamily,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    if (vkCreateCommandPool(context->device, &poolInfo, NULL, &context->commandPool) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create command pool");
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Allocate command buffers
    context->commandBuffers = malloc(context->imageCount * sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = context->imageCount
    };
    if (vkAllocateCommandBuffers(context->device, &allocInfo, context->commandBuffers) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate command buffers");
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Create semaphores and fences
    context->imageAvailableSemaphores = malloc(context->imageCount * sizeof(VkSemaphore));
    context->renderFinishedSemaphores = malloc(context->imageCount * sizeof(VkSemaphore));
    context->inFlightFences = malloc(context->imageCount * sizeof(VkFence));
    context->fencesInUse = malloc(context->imageCount * sizeof(VkFence));
    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    for (uint32_t i = 0; i < context->imageCount; i++) {
        if (vkCreateSemaphore(context->device, &semaphoreInfo, NULL, &context->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context->device, &semaphoreInfo, NULL, &context->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context->device, &fenceInfo, NULL, &context->inFlightFences[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create synchronization objects for frame %u", i);
            for (uint32_t j = 0; j < i; j++) {
                vkDestroySemaphore(context->device, context->imageAvailableSemaphores[j], NULL);
                vkDestroySemaphore(context->device, context->renderFinishedSemaphores[j], NULL);
                vkDestroyFence(context->device, context->inFlightFences[j], NULL);
            }
            free(context->imageAvailableSemaphores);
            free(context->renderFinishedSemaphores);
            free(context->inFlightFences);
            free(context->fencesInUse);
            vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
            free(context->commandBuffers);
            vkDestroyCommandPool(context->device, context->commandPool, NULL);
            for (uint32_t j = 0; j < context->imageCount; j++) vkDestroyFramebuffer(context->device, context->framebuffers[j], NULL);
            free(context->framebuffers);
            vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
            vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
            vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
            vkDestroyRenderPass(context->device, context->renderPass, NULL);
            for (uint32_t j = 0; j < context->imageCount; j++) vkDestroyImageView(context->device, context->imageViews[j], NULL);
            free(context->imageViews);
            vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
            vkDestroyDevice(context->device, NULL);
            vkDestroySurfaceKHR(context->instance, context->surface, NULL);
            vkDestroyInstance(context->instance, NULL);
            return false;
        }
        context->fencesInUse[i] = VK_NULL_HANDLE;
    }

    // Initialize camera
    context->camera.position[0] = 0.0f;
    context->camera.position[1] = 0.0f;
    context->camera.scale = 1.0f;

    // Initialize object positions
    glm_vec2_zero(context->objects[0].position); // Triangle
    glm_vec2_zero(context->objects[1].position); // Square
    glm_mat4_identity(context->objects[0].modelMatrix);
    glm_mat4_identity(context->objects[1].modelMatrix);

    // Create triangle vertex buffer
    VkDeviceSize bufferSize = sizeof(triangleVertices);
    context->triangleVertexBuffer = VK_NULL_HANDLE;
    context->triangleVertexBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(context->device, context->physicalDevice, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &context->triangleVertexBuffer, &context->triangleVertexBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create triangle vertex buffer");
        // Clean up any partially created resources
        if (context->triangleVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        }
        if (context->triangleVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        }
        // Clean up other resources
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    void *data;
    vkMapMemory(context->device, context->triangleVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, triangleVertices, bufferSize);
    vkUnmapMemory(context->device, context->triangleVertexBufferMemory);

    // Create square vertex buffer
    static const Vertex squareVertices[] = {
        {-0.25f, -0.25f, 0.0f, 1.0f, 1.0f},
        {0.25f, -0.25f, 0.0f, 1.0f, 1.0f},
        {-0.25f, 0.25f, 0.0f, 1.0f, 1.0f},
        {0.25f, 0.25f, 0.0f, 1.0f, 1.0f}
    };
    // Create square vertex buffer
    bufferSize = sizeof(squareVertices);
    context->squareVertexBuffer = VK_NULL_HANDLE;
    context->squareVertexBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(context->device, context->physicalDevice, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &context->squareVertexBuffer, &context->squareVertexBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create square vertex buffer");
        // Clean up square buffer resources
        if (context->squareVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        }
        if (context->squareVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        }
        // Clean up triangle buffer resources
        if (context->triangleVertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        }
        if (context->triangleVertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        }
        // Clean up other resources
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    vkMapMemory(context->device, context->squareVertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, squareVertices, bufferSize);
    vkUnmapMemory(context->device, context->squareVertexBufferMemory);

    // Create square index buffer
    static const uint32_t squareIndices[] = {0, 1, 2, 2, 1, 3};
    bufferSize = sizeof(squareIndices);
    if (!createBuffer(context->device, context->physicalDevice, bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &context->squareIndexBuffer, &context->squareIndexBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create square index buffer");
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    vkMapMemory(context->device, context->squareIndexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, squareIndices, bufferSize);
    vkUnmapMemory(context->device, context->squareIndexBufferMemory);

    // Create uniform buffer
    bufferSize = sizeof(mat4);
    if (!createBuffer(context->device, context->physicalDevice, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     &context->uniformBuffer, &context->uniformBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create uniform buffer");
        vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
        vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Create descriptor pool
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = context->imageCount
    };
    VkDescriptorPoolCreateInfo descriptorPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = context->imageCount,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize
    };
    if (vkCreateDescriptorPool(context->device, &descriptorPoolInfo, NULL, &context->descriptorPool) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor pool");
        vkDestroyBuffer(context->device, context->uniformBuffer, NULL);
        vkFreeMemory(context->device, context->uniformBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
        vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Allocate descriptor sets
    context->descriptorSets = malloc(context->imageCount * sizeof(VkDescriptorSet));
    VkDescriptorSetLayout layouts[context->imageCount];
    for (uint32_t i = 0; i < context->imageCount; i++) {
        layouts[i] = context->descriptorSetLayout;
    }
    VkDescriptorSetAllocateInfo descriptorAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = context->descriptorPool,
        .descriptorSetCount = context->imageCount,
        .pSetLayouts = layouts
    };
    if (vkAllocateDescriptorSets(context->device, &descriptorAllocInfo, context->descriptorSets) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor sets");
        vkDestroyDescriptorPool(context->device, context->descriptorPool, NULL);
        vkDestroyBuffer(context->device, context->uniformBuffer, NULL);
        vkFreeMemory(context->device, context->uniformBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
        vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    // Update descriptor sets
    for (uint32_t i = 0; i < context->imageCount; i++) {
        VkDescriptorBufferInfo bufferInfo = {
            .buffer = context->uniformBuffer,
            .offset = 0,
            .range = sizeof(mat4)
        };
        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = context->descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &bufferInfo
        };
        vkUpdateDescriptorSets(context->device, 1, &descriptorWrite, 0, NULL);
    }

    // Initialize text module
    context->textContext = malloc(sizeof(TextContext));
    if (!context->textContext) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate TextContext");
        vkDestroyDescriptorPool(context->device, context->descriptorPool, NULL);
        vkDestroyBuffer(context->device, context->uniformBuffer, NULL);
        vkFreeMemory(context->device, context->uniformBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
        vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }
    memset(context->textContext, 0, sizeof(TextContext));
    if (!text_init(context, context->textContext)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize text module");
        free(context->textContext);
        vkDestroyDescriptorPool(context->device, context->descriptorPool, NULL);
        vkDestroyBuffer(context->device, context->uniformBuffer, NULL);
        vkFreeMemory(context->device, context->uniformBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
        vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
        vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
        vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
        vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) {
            vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
            vkDestroyFence(context->device, context->inFlightFences[i], NULL);
        }
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
        free(context->commandBuffers);
        vkDestroyCommandPool(context->device, context->commandPool, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        free(context->framebuffers);
        vkDestroyPipeline(context->device, context->graphicsPipeline, NULL);
        vkDestroyPipelineLayout(context->device, context->pipelineLayout, NULL);
        vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
        vkDestroyRenderPass(context->device, context->renderPass, NULL);
        for (uint32_t i = 0; i < context->imageCount; i++) vkDestroyImageView(context->device, context->imageViews[i], NULL);
        free(context->imageViews);
        vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
        vkDestroyDevice(context->device, NULL);
        vkDestroySurfaceKHR(context->instance, context->surface, NULL);
        vkDestroyInstance(context->instance, NULL);
        return false;
    }

    SDL_Log("Vulkan initialized successfully");
    return true;
}




bool vulkan_render(VulkanContext *context) {
    static uint32_t currentFrame = 0;

    // Wait for fence
    if (vkWaitForFences(context->device, 1, &context->inFlightFences[currentFrame], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to wait for fence");
        return false;
    }

    // Acquire image
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                                            context->imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    } else if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to acquire swapchain image");
        return false;
    }

    if (context->fencesInUse[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(context->device, 1, &context->fencesInUse[imageIndex], VK_TRUE, UINT64_MAX);
    }
    context->fencesInUse[imageIndex] = context->inFlightFences[currentFrame];

    // Reset fence and command buffer
    vkResetFences(context->device, 1, &context->inFlightFences[currentFrame]);
    vkResetCommandBuffer(context->commandBuffers[imageIndex], 0);

    // Calculate view-projection matrix
    mat4 projection, view, vp;
    glm_ortho(0.0f, context->swapchainExtent.width / context->camera.scale,
              context->swapchainExtent.height / context->camera.scale, 0.0f,
              -1.0f, 1.0f, projection);
    glm_translate_make(view, (vec3){context->camera.position[0], context->camera.position[1], 0.0f});
    glm_mat4_mul(projection, view, vp);

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(context->commandBuffers[imageIndex], &beginInfo);

    VkClearValue clearColor = { .color = { { 0.0f, 0.0f, 0.0f, 1.0f } } };
    VkRenderPassBeginInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context->renderPass,
        .framebuffer = context->framebuffers[imageIndex],
        .renderArea.offset = { 0, 0 },
        .renderArea.extent = context->swapchainExtent,
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(context->commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Render triangle and square
    vkCmdBindPipeline(context->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, context->graphicsPipeline);
    VkDeviceSize offsets[] = {0};
    for (int i = 0; i < 2; i++) {
        mat4 mvp;
        glm_mat4_mul(vp, context->objects[i].modelMatrix, mvp);
        void *data;
        vkMapMemory(context->device, context->uniformBufferMemory, 0, sizeof(mat4), 0, &data);
        memcpy(data, mvp, sizeof(mat4));
        vkUnmapMemory(context->device, context->uniformBufferMemory);

        vkCmdBindDescriptorSets(context->commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                                context->pipelineLayout, 0, 1, &context->descriptorSets[imageIndex], 0, NULL);
        if (i == 0) {
            vkCmdBindVertexBuffers(context->commandBuffers[imageIndex], 0, 1, &context->triangleVertexBuffer, offsets);
            vkCmdDraw(context->commandBuffers[imageIndex], 3, 1, 0, 0);
        } else {
            vkCmdBindVertexBuffers(context->commandBuffers[imageIndex], 0, 1, &context->squareVertexBuffer, offsets);
            vkCmdBindIndexBuffer(context->commandBuffers[imageIndex], context->squareIndexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(context->commandBuffers[imageIndex], 6, 1, 0, 0, 0);
        }
    }

    // Render text
    if (context->textContext) {
        text_render(context, context->textContext, context->commandBuffers[imageIndex]);
    }

    vkCmdEndRenderPass(context->commandBuffers[imageIndex]);
    if (vkEndCommandBuffer(context->commandBuffers[imageIndex]) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to end command buffer");
        return false;
    }

    // Submit
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context->imageAvailableSemaphores[currentFrame],
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &context->commandBuffers[imageIndex],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &context->renderFinishedSemaphores[imageIndex]
    };
    if (vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, context->inFlightFences[currentFrame]) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to submit draw command buffer");
        return false;
    }

    // Present
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &context->renderFinishedSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &context->swapchain,
        .pImageIndices = &imageIndex
    };
    result = vkQueuePresentKHR(context->presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    } else if (result != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to present queue");
        return false;
    }

    currentFrame = (currentFrame + 1) % context->imageCount;
    return true;
}






void vulkan_cleanup(VulkanContext *context) {
    vkDeviceWaitIdle(context->device);

    if (context->textContext) {
        text_cleanup(context, context->textContext);
        free(context->textContext);
    }

    vkDestroyBuffer(context->device, context->triangleVertexBuffer, NULL);
    vkFreeMemory(context->device, context->triangleVertexBufferMemory, NULL);
    vkDestroyBuffer(context->device, context->squareVertexBuffer, NULL);
    vkFreeMemory(context->device, context->squareVertexBufferMemory, NULL);
    vkDestroyBuffer(context->device, context->squareIndexBuffer, NULL);
    vkFreeMemory(context->device, context->squareIndexBufferMemory, NULL);
    vkDestroyBuffer(context->device, context->uniformBuffer, NULL);
    vkFreeMemory(context->device, context->uniformBufferMemory, NULL);
    vkDestroyDescriptorPool(context->device, context->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(context->device, context->descriptorSetLayout, NULL);
    free(context->descriptorSets);

    // ... rest of cleanup ...
}




bool recreate_swapchain(VulkanContext *context, SDL_Window *window) {
    SDL_Log("Recreating swapchain");

    // Wait for the device to be idle
    vkDeviceWaitIdle(context->device);

    // Destroy old resources
    for (uint32_t i = 0; i < context->imageCount; i++) {
        vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
        vkDestroyImageView(context->device, context->imageViews[i], NULL);
    }
    vkFreeCommandBuffers(context->device, context->commandPool, context->imageCount, context->commandBuffers);
    for (uint32_t i = 0; i < context->imageCount; i++) {
        vkDestroySemaphore(context->device, context->imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(context->device, context->renderFinishedSemaphores[i], NULL);
        vkDestroyFence(context->device, context->inFlightFences[i], NULL);
    }
    free(context->framebuffers);
    free(context->imageViews);
    free(context->commandBuffers);
    free(context->imageAvailableSemaphores);
    free(context->renderFinishedSemaphores);
    free(context->inFlightFences);
    free(context->fencesInUse);
    vkDestroySwapchainKHR(context->device, context->swapchain, NULL);

    // Get new surface capabilities
    VkSurfaceCapabilitiesKHR surfaceCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice, context->surface, &surfaceCaps);
    context->swapchainExtent = surfaceCaps.currentExtent;
    if (context->swapchainExtent.width == 0 || context->swapchainExtent.height == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Window minimized, skipping swapchain recreation");
        return false;
    }

    // Recreate swapchain
    uint32_t formatCount, presentModeCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, NULL);
    VkSurfaceFormatKHR *formats = malloc(formatCount * sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, formats);
    VkSurfaceFormatKHR surfaceFormat = formats[0]; // Simplified: pick first format
    free(formats);
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, NULL);
    VkPresentModeKHR *presentModes = malloc(presentModeCount * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, presentModes);
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    free(presentModes);

    uint32_t queueFamilyIndices[] = { context->graphicsFamily, context->presentFamily }; // From vulkan_init
    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = context->surface,
        .minImageCount = surfaceCaps.minImageCount + 1,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = context->swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = queueFamilyIndices[0] == queueFamilyIndices[1] ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
        .queueFamilyIndexCount = queueFamilyIndices[0] == queueFamilyIndices[1] ? 0 : 2,
        .pQueueFamilyIndices = queueFamilyIndices,
        .preTransform = surfaceCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    if (vkCreateSwapchainKHR(context->device, &swapchainInfo, NULL, &context->swapchain) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain");
        return false;
    }

    // Get new swapchain images
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->imageCount, NULL);
    VkImage *swapchainImages = malloc(context->imageCount * sizeof(VkImage));
    vkGetSwapchainImagesKHR(context->device, context->swapchain, &context->imageCount, swapchainImages);

    // Recreate image views
    context->imageViews = malloc(context->imageCount * sizeof(VkImageView));
    for (uint32_t i = 0; i < context->imageCount; i++) {
        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1
        };
        if (vkCreateImageView(context->device, &viewInfo, NULL, &context->imageViews[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate image view %u", i);
            free(swapchainImages);
            return false;
        }
    }
    free(swapchainImages);

    // Recreate framebuffers
    context->framebuffers = malloc(context->imageCount * sizeof(VkFramebuffer));
    for (uint32_t i = 0; i < context->imageCount; i++) {
        VkFramebufferCreateInfo framebufferInfo = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = context->renderPass,
            .attachmentCount = 1,
            .pAttachments = &context->imageViews[i],
            .width = context->swapchainExtent.width,
            .height = context->swapchainExtent.height,
            .layers = 1
        };
        if (vkCreateFramebuffer(context->device, &framebufferInfo, NULL, &context->framebuffers[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate framebuffer %u", i);
            return false;
        }
    }

    // Recreate command buffers
    context->commandBuffers = malloc(context->imageCount * sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = context->imageCount
    };
    if (vkAllocateCommandBuffers(context->device, &allocInfo, context->commandBuffers) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate new command buffers");
        free(context->commandBuffers);
        return false;
    }

    // Recreate synchronization objects
    context->imageAvailableSemaphores = malloc(context->imageCount * sizeof(VkSemaphore));
    context->renderFinishedSemaphores = malloc(context->imageCount * sizeof(VkSemaphore));
    context->inFlightFences = malloc(context->imageCount * sizeof(VkFence));
    context->fencesInUse = malloc(context->imageCount * sizeof(VkFence));
    if (!context->imageAvailableSemaphores || !context->renderFinishedSemaphores || 
        !context->inFlightFences || !context->fencesInUse) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate synchronization arrays");
        free(context->imageAvailableSemaphores);
        free(context->renderFinishedSemaphores);
        free(context->inFlightFences);
        free(context->fencesInUse);
        return false;
    }
    VkSemaphoreCreateInfo semaphoreInfo = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    for (uint32_t i = 0; i < context->imageCount; i++) {
        if (vkCreateSemaphore(context->device, &semaphoreInfo, NULL, &context->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(context->device, &semaphoreInfo, NULL, &context->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(context->device, &fenceInfo, NULL, &context->inFlightFences[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create synchronization objects for frame %u", i);
            for (uint32_t j = 0; j < i; j++) {
                vkDestroySemaphore(context->device, context->imageAvailableSemaphores[j], NULL);
                vkDestroySemaphore(context->device, context->renderFinishedSemaphores[j], NULL);
                vkDestroyFence(context->device, context->inFlightFences[j], NULL);
            }
            free(context->imageAvailableSemaphores);
            free(context->renderFinishedSemaphores);
            free(context->inFlightFences);
            free(context->fencesInUse);
            return false;
        }
        context->fencesInUse[i] = VK_NULL_HANDLE;
    }

    SDL_Log("Swapchain recreated successfully with %u images", context->imageCount);
    return true;
}


//