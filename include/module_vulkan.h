#ifndef MODULE_VULKAN_H
#define MODULE_VULKAN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>

// Forward declaration
struct TextContext;

typedef struct {
 float x, y; // Position
 float r, g, b; // Color
} Vertex;

typedef struct {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsFamily; // Add queue family indices
    uint32_t presentFamily;
    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkCommandPool commandPool;
    VkCommandBuffer *commandBuffers;
    VkSemaphore *imageAvailableSemaphores;
    VkSemaphore *renderFinishedSemaphores;
    VkFence *inFlightFences;
    VkFence *fencesInUse;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkFramebuffer *framebuffers;
    VkImageView *imageViews;
    uint32_t imageCount;
    VkExtent2D swapchainExtent;
    struct TextContext *textContext;
} VulkanContext;

bool vulkan_init(SDL_Window *window, VulkanContext *context);
bool vulkan_render(VulkanContext *context);
void vulkan_cleanup(VulkanContext *context);
bool recreate_swapchain(VulkanContext *context, SDL_Window *window);

#endif // MODULE_VULKAN_H