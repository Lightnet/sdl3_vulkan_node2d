#ifndef MODULE_VULKAN_H
#define MODULE_VULKAN_H

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <cglm/cglm.h>

struct TextContext;

typedef struct {
    float x, y; // Position
    float r, g, b; // Color
} Vertex;

typedef struct {
    vec2 position; // Object position
    mat4 modelMatrix; // Per-object transformation
} Object;

typedef struct {
    vec2 position; // Camera position (x, y)
    float scale;   // Zoom level
} Camera;

typedef struct {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    uint32_t graphicsFamily;
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
    VkBuffer triangleVertexBuffer;
    VkDeviceMemory triangleVertexBufferMemory;
    VkBuffer squareVertexBuffer;
    VkDeviceMemory squareVertexBufferMemory;
    VkBuffer squareIndexBuffer;
    VkDeviceMemory squareIndexBufferMemory;
    VkFramebuffer *framebuffers;
    VkImageView *imageViews;
    uint32_t imageCount;
    VkExtent2D swapchainExtent;
    struct TextContext *textContext;
    Camera camera;
    Object objects[2]; // 0: triangle, 1: square
    VkBuffer uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet *descriptorSets;
} VulkanContext;

bool vulkan_init(SDL_Window *window, VulkanContext *context);
bool vulkan_render(VulkanContext *context);
void vulkan_cleanup(VulkanContext *context);
bool recreate_swapchain(VulkanContext *context, SDL_Window *window);

#endif // MODULE_VULKAN_H