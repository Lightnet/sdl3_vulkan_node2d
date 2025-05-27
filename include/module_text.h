#ifndef MODULE_TEXT_H
#define MODULE_TEXT_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include "module_vulkan.h"

typedef struct {
 float x, y; // Position
 float u, v; // Texture coordinates
} TextVertex;

typedef struct TextContext {
 VkBuffer vertexBuffer;
 VkDeviceMemory vertexBufferMemory;
 VkBuffer indexBuffer;
 VkDeviceMemory indexBufferMemory;
 VkImage textureImage;
 VkDeviceMemory textureImageMemory;
 VkImageView textureImageView;
 VkSampler textureSampler;
 VkDescriptorSetLayout descriptorSetLayout;
 VkDescriptorPool descriptorPool;
 VkDescriptorSet descriptorSet;
 VkPipelineLayout pipelineLayout;
 VkPipeline graphicsPipeline;
} TextContext;

bool text_init(VulkanContext *vulkanContext, TextContext *textContext);
void text_render(VulkanContext *vulkanContext, TextContext *textContext, VkCommandBuffer commandBuffer);
void text_cleanup(VulkanContext *vulkanContext, TextContext *textContext);

#endif // MODULE_TEXT_H