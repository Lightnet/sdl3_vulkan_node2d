#include "module_vulkan.h"
#include "module_text.h"
#include "vulkan_utils.h"
#include <string.h>
#include <stdlib.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "shader_text_vert_spv.h"
#include "shader_text_frag_spv.h"


static const TextVertex vertices[] = {
    {-0.5f, -0.1f, 0.0f, 0.0f}, // Bottom-left: v=0 (texture bottom)
    { 0.5f, -0.1f, 1.0f, 0.0f}, // Bottom-right: v=0
    {-0.5f,  0.1f, 0.0f, 1.0f}, // Top-left: v=1 (texture top)
    { 0.5f,  0.1f, 1.0f, 1.0f}  // Top-right: v=1
};

static const uint32_t indices[] = {0, 1, 2, 2, 1, 3};


static bool createTextureImage(VulkanContext *vulkanContext, SDL_Surface *surface, VkImage *image, VkDeviceMemory *imageMemory) {
    SDL_Surface *convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
    if (!convertedSurface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to convert surface to RGBA8888: %s", SDL_GetError());
        return false;
    }
    SDL_Log("Converted surface: %dx%d, format=%s", convertedSurface->w, convertedSurface->h, SDL_GetPixelFormatName(convertedSurface->format));

    VkDeviceSize imageSize = convertedSurface->w * convertedSurface->h * 4;
    SDL_Log("Creating texture image: %dx%d, size=%zu bytes", convertedSurface->w, convertedSurface->h, imageSize);

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(vulkanContext->device, vulkanContext->physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create staging buffer");
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    void *data;
    if (vkMapMemory(vulkanContext->device, stagingBufferMemory, 0, imageSize, 0, &data) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to map staging buffer memory");
        vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
        vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
        SDL_DestroySurface(convertedSurface);
        return false;
    }
    memcpy(data, convertedSurface->pixels, imageSize);
    vkUnmapMemory(vulkanContext->device, stagingBufferMemory);

    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .extent = { (uint32_t)convertedSurface->w, (uint32_t)convertedSurface->h, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    *image = VK_NULL_HANDLE;
    if (vkCreateImage(vulkanContext->device, &imageInfo, NULL, image) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create image");
        vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
        vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vulkanContext->device, *image, &memRequirements);
    SDL_Log("Image memory requirements: size=%zu, alignment=%u, memoryTypeBits=0x%x", memRequirements.size, memRequirements.alignment, memRequirements.memoryTypeBits);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(vulkanContext->physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = findMemoryType(vulkanContext->physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryTypeIndex == UINT32_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find suitable memory type for image");
        vkDestroyImage(vulkanContext->device, *image, NULL);
        vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
        vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    *imageMemory = VK_NULL_HANDLE;
    SDL_Log("Allocating image memory: size=%zu, memoryTypeIndex=%u", memRequirements.size, memoryTypeIndex);
    if (vkAllocateMemory(vulkanContext->device, &allocInfo, NULL, imageMemory) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate image memory");
        vkDestroyImage(vulkanContext->device, *image, NULL);
        vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
        vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    if (vkBindImageMemory(vulkanContext->device, *image, *imageMemory, 0) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to bind image memory");
        vkFreeMemory(vulkanContext->device, *imageMemory, NULL);
        vkDestroyImage(vulkanContext->device, *image, NULL);
        vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
        vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
        SDL_DestroySurface(convertedSurface);
        return false;
    }

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(vulkanContext->device, vulkanContext->commandPool);
    transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(commandBuffer, stagingBuffer, *image, convertedSurface->w, convertedSurface->h);
    transitionImageLayout(commandBuffer, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    endSingleTimeCommands(vulkanContext->device, vulkanContext->commandPool, vulkanContext->graphicsQueue, commandBuffer);

    vkDestroyBuffer(vulkanContext->device, stagingBuffer, NULL);
    vkFreeMemory(vulkanContext->device, stagingBufferMemory, NULL);
    SDL_DestroySurface(convertedSurface);
    return true;
}




bool text_init(VulkanContext *vulkanContext, TextContext *textContext) {
    SDL_Log("Initializing text module");
    if (!TTF_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL_ttf: %s", SDL_GetError());
        return false;
    }

    TTF_Font *font = TTF_OpenFont("assets/fonts/Kenney Pixel.ttf", 24);
    if (!font) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load font: %s", SDL_GetError());
        TTF_Quit();
        return false;
    }

    // SDL_Color color = {255, 255, 255, 255}; // White text
    SDL_Color color = {255, 255, 255, 255}; // White text
    const char *text = "Hello World";
    size_t text_len = strlen(text);
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, text_len, color);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to render text: %s", SDL_GetError());
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    // Initialize text position
    glm_vec2_zero(textContext->position);
    glm_mat4_identity(textContext->modelMatrix);

    SDL_Log("Text surface created: %dx%d, format=%s", surface->w, surface->h, SDL_GetPixelFormatName(surface->format));
    SDL_SaveBMP(surface, "text_surface.bmp"); // Debug: Save surface to inspect

    if (!createTextureImage(vulkanContext, surface, &textContext->textureImage, &textContext->textureImageMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture image");
        if (textContext->textureImage != VK_NULL_HANDLE) {
            vkDestroyImage(vulkanContext->device, textContext->textureImage, NULL);
            textContext->textureImage = VK_NULL_HANDLE;
        }
        if (textContext->textureImageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanContext->device, textContext->textureImageMemory, NULL);
            textContext->textureImageMemory = VK_NULL_HANDLE;
        }
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = textContext->textureImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1
    };
    if (vkCreateImageView(vulkanContext->device, &viewInfo, NULL, &textContext->textureImageView) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture image view");
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    if (vkCreateSampler(vulkanContext->device, &samplerInfo, NULL, &textContext->textureSampler) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture sampler");
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkDeviceSize bufferSize = sizeof(vertices);
    textContext->vertexBuffer = VK_NULL_HANDLE;
    textContext->vertexBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(vulkanContext->device, vulkanContext->physicalDevice, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &textContext->vertexBuffer, &textContext->vertexBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create vertex buffer");
        if (textContext->vertexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        }
        if (textContext->vertexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        }
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        vkDestroyImage(vulkanContext->device, textContext->textureImage, NULL);
        vkFreeMemory(vulkanContext->device, textContext->textureImageMemory, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }
    void *data;
    vkMapMemory(vulkanContext->device, textContext->vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices, bufferSize);
    vkUnmapMemory(vulkanContext->device, textContext->vertexBufferMemory);

    bufferSize = sizeof(indices);
    textContext->indexBuffer = VK_NULL_HANDLE;
    textContext->indexBufferMemory = VK_NULL_HANDLE;
    if (!createBuffer(vulkanContext->device, vulkanContext->physicalDevice, bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &textContext->indexBuffer, &textContext->indexBufferMemory)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create index buffer");
        if (textContext->indexBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        }
        if (textContext->indexBufferMemory != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        }
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        vkDestroyImage(vulkanContext->device, textContext->textureImage, NULL);
        vkFreeMemory(vulkanContext->device, textContext->textureImageMemory, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }
    vkMapMemory(vulkanContext->device, textContext->indexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, indices, bufferSize);
    vkUnmapMemory(vulkanContext->device, textContext->indexBufferMemory);

    VkDescriptorSetLayoutBinding bindings[2] = {
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL
    },
    {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL
        }
    };
    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = bindings
    };
    if (vkCreateDescriptorSetLayout(vulkanContext->device, &layoutInfo, NULL, &textContext->descriptorSetLayout) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor set layout");
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkDescriptorPoolSize poolSizes[2] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1 },
        { .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 }
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };
    if (vkCreateDescriptorPool(vulkanContext->device, &poolInfo, NULL, &textContext->descriptorPool) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create descriptor pool");
        vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = textContext->descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &textContext->descriptorSetLayout
    };
    if (vkAllocateDescriptorSets(vulkanContext->device, &allocInfo, &textContext->descriptorSet) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate descriptor set");
        vkDestroyDescriptorPool(vulkanContext->device, textContext->descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo = {
        .buffer = vulkanContext->uniformBuffer,
        .offset = 0,
        .range = sizeof(mat4)
    };

    VkDescriptorImageInfo imageInfo = {
        .sampler = textContext->textureSampler,
        .imageView = textContext->textureImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet descriptorWrites[2] = {
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = textContext->descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &imageInfo
    },
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = textContext->descriptorSet,
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &bufferInfo
    }
    };
    vkUpdateDescriptorSets(vulkanContext->device, 2, descriptorWrites, 0, NULL);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &textContext->descriptorSetLayout
    };
    if (vkCreatePipelineLayout(vulkanContext->device, &pipelineLayoutInfo, NULL, &textContext->pipelineLayout) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create pipeline layout");
        vkDestroyDescriptorPool(vulkanContext->device, textContext->descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }

    VkShaderModule vertShaderModule, fragShaderModule;
    VkShaderModuleCreateInfo vertShaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(shader_text_vert_spv),
        .pCode = shader_text_vert_spv
    };
    VkShaderModuleCreateInfo fragShaderInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(shader_text_frag_spv),
        .pCode = shader_text_frag_spv
    };
    if (vkCreateShaderModule(vulkanContext->device, &vertShaderInfo, NULL, &vertShaderModule) != VK_SUCCESS ||
        vkCreateShaderModule(vulkanContext->device, &fragShaderInfo, NULL, &fragShaderModule) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader modules");
        vkDestroyPipelineLayout(vulkanContext->device, textContext->pipelineLayout, NULL);
        vkDestroyDescriptorPool(vulkanContext->device, textContext->descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
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
    VkVertexInputBindingDescription bindingDesc = {
        .binding = 0,
        .stride = sizeof(TextVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };
    VkVertexInputAttributeDescription attributeDescs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TextVertex, x) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(TextVertex, u) }
    };
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDesc,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributeDescs
    };
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };
    VkViewport viewport = { 0.0f, 0.0f, (float)vulkanContext->swapchainExtent.width, (float)vulkanContext->swapchainExtent.height, 0.0f, 1.0f };
    VkRect2D scissor = { {0, 0}, vulkanContext->swapchainExtent };
    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor
    };
    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f
    };
    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
    };
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };
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
        .layout = textContext->pipelineLayout,
        .renderPass = vulkanContext->renderPass,
        .subpass = 0
    };
    if (vkCreateGraphicsPipelines(vulkanContext->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &textContext->graphicsPipeline) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create graphics pipeline");
        vkDestroyShaderModule(vulkanContext->device, fragShaderModule, NULL);
        vkDestroyShaderModule(vulkanContext->device, vertShaderModule, NULL);
        vkDestroyPipelineLayout(vulkanContext->device, textContext->pipelineLayout, NULL);
        vkDestroyDescriptorPool(vulkanContext->device, textContext->descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
        vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
        vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
        vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
        vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
        SDL_DestroySurface(surface);
        TTF_CloseFont(font);
        TTF_Quit();
        return false;
    }
    vkDestroyShaderModule(vulkanContext->device, fragShaderModule, NULL);
    vkDestroyShaderModule(vulkanContext->device, vertShaderModule, NULL);
    SDL_DestroySurface(surface);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Log("Text module initialized successfully");
    return true;
}


void text_render(VulkanContext *vulkanContext, TextContext *textContext, VkCommandBuffer commandBuffer) {
    if (!textContext->graphicsPipeline) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Text pipeline is invalid, skipping text render");
        return;
    }

    mat4 projection, view, vp, mvp;
    glm_ortho(0.0f, (float)vulkanContext->swapchainExtent.width,
              (float)vulkanContext->swapchainExtent.height, 0.0f,
              -1.0f, 1.0f, projection);
    glm_mat4_identity(view); // Reset view to identity for screen-space text
    glm_mat4_mul(projection, view, vp);

    // Scale and translate the text quad
    mat4 model;
    glm_mat4_identity(model);
    glm_scale(model, (vec3){100.0f, 100.0f, 1.0f}); // Scale quad to ~100x20 pixels
    glm_translate(model, (vec3){1.0f, 1.0f, 0.0f}); // Move to (100,100) pixels
    glm_mat4_mul(vp, model, mvp);

    void *data;
    vkMapMemory(vulkanContext->device, vulkanContext->uniformBufferMemory, 0, sizeof(mat4), 0, &data);
    memcpy(data, mvp, sizeof(mat4));
    vkUnmapMemory(vulkanContext->device, vulkanContext->uniformBufferMemory);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textContext->graphicsPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textContext->pipelineLayout,
                            0, 1, &textContext->descriptorSet, 0, NULL);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &textContext->vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, textContext->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, 6, 1, 0, 0, 0);
}




void text_cleanup(VulkanContext *vulkanContext, TextContext *textContext) {
    SDL_Log("Cleaning up text module");
    vkDestroyPipeline(vulkanContext->device, textContext->graphicsPipeline, NULL);
    vkDestroyPipelineLayout(vulkanContext->device, textContext->pipelineLayout, NULL);
    vkDestroyDescriptorPool(vulkanContext->device, textContext->descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(vulkanContext->device, textContext->descriptorSetLayout, NULL);
    vkDestroySampler(vulkanContext->device, textContext->textureSampler, NULL);
    vkDestroyImageView(vulkanContext->device, textContext->textureImageView, NULL);
    vkDestroyImage(vulkanContext->device, textContext->textureImage, NULL);
    vkFreeMemory(vulkanContext->device, textContext->textureImageMemory, NULL);
    vkDestroyBuffer(vulkanContext->device, textContext->vertexBuffer, NULL);
    vkFreeMemory(vulkanContext->device, textContext->vertexBufferMemory, NULL);
    vkDestroyBuffer(vulkanContext->device, textContext->indexBuffer, NULL);
    vkFreeMemory(vulkanContext->device, textContext->indexBufferMemory, NULL);
}