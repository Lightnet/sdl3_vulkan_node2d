#ifndef MODULE_VULKAN_H
#define MODULE_VULKAN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>

typedef struct {
    float x, y; // Position
    float r, g, b; // Color
} Vertex;

// Vulkan context structure to hold all Vulkan objects
typedef struct {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkFramebuffer *framebuffers;
    VkImageView *imageViews;
    uint32_t imageCount;
    VkExtent2D swapchainExtent;
} VulkanContext;

// Initialize Vulkan context
bool vulkan_init(SDL_Window *window, VulkanContext *context);

// Render a frame
bool vulkan_render(VulkanContext *context);

// Cleanup Vulkan resources
void vulkan_cleanup(VulkanContext *context);

#endif // MODULE_VULKAN_H