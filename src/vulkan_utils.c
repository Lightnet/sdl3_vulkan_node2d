// vulkan_utils.c
#include "vulkan_utils.h"
#include <string.h>

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
 VkPhysicalDeviceMemoryProperties memProperties;
 vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
 for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
 if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
 return i;
 }
 }
 SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find suitable memory type");
 return UINT32_MAX;
}


bool createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties, VkBuffer *buffer, VkDeviceMemory *bufferMemory) {
    *buffer = VK_NULL_HANDLE;
    *bufferMemory = VK_NULL_HANDLE;

    // Create buffer
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    if (vkCreateBuffer(device, &bufferInfo, NULL, buffer) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create buffer");
        return false;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

    // Find suitable memory type
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find suitable memory type");
        vkDestroyBuffer(device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    // Allocate memory
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    if (vkAllocateMemory(device, &allocInfo, NULL, bufferMemory) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to allocate buffer memory");
        vkDestroyBuffer(device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        return false;
    }

    // Bind memory to buffer
    if (vkBindBufferMemory(device, *buffer, *bufferMemory, 0) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to bind buffer memory");
        vkFreeMemory(device, *bufferMemory, NULL);
        vkDestroyBuffer(device, *buffer, NULL);
        *buffer = VK_NULL_HANDLE;
        *bufferMemory = VK_NULL_HANDLE;
        return false;
    }

    return true;
}


VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
 VkCommandBufferAllocateInfo allocInfo = {
 .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
 .commandPool = commandPool,
 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
 .commandBufferCount = 1
 };
 VkCommandBuffer commandBuffer;
 vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
 VkCommandBufferBeginInfo beginInfo = {
 .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
 };
 vkBeginCommandBuffer(commandBuffer, &beginInfo);
 return commandBuffer;
}

void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer) {
 vkEndCommandBuffer(commandBuffer);
 VkSubmitInfo submitInfo = {
 .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
 .commandBufferCount = 1,
 .pCommandBuffers = &commandBuffer
 };
 vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
 vkQueueWaitIdle(graphicsQueue);
 vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
 VkImageMemoryBarrier barrier = {
 .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
 .oldLayout = oldLayout,
 .newLayout = newLayout,
 .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
 .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
 .image = image,
 .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
 .subresourceRange.baseMipLevel = 0,
 .subresourceRange.levelCount = 1,
 .subresourceRange.baseArrayLayer = 0,
 .subresourceRange.layerCount = 1
 };
 VkPipelineStageFlags srcStage, dstStage;
 if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
 barrier.srcAccessMask = 0;
 barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
 srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
 dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
 } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
 barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
 barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
 srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
 dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
 }
 vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &barrier); 
}

void copyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
 VkBufferImageCopy region = {
 .bufferOffset = 0,
 .bufferRowLength = 0,
 .bufferImageHeight = 0,
 .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
 .imageSubresource.mipLevel = 0,
 .imageSubresource.baseArrayLayer = 0,
 .imageSubresource.layerCount = 1,
 .imageOffset = {0, 0, 0},
 .imageExtent = {width, height, 1}
 };
 vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}