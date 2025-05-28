#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <cglm/cglm.h>
#include "module_vulkan.h"
#include "module_text.h"

int main(int argc, char *argv[]) {
    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("SDL Vulkan Node 2D", 600, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Initialize Vulkan
    VulkanContext context = {0};
    if (!vulkan_init(window, &context)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Vulkan");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Main loop
    bool running = true;
    SDL_Event event;
    bool dragging = false;
    vec2 dragStart = {0.0f, 0.0f};
    int selectedObject = -1; // -1: none, 0: triangle, 1: square, 2: text

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_WINDOW_RESIZED:
                    SDL_Log("Window resized to %dx%d", event.window.data1, event.window.data2);
                    if (!recreate_swapchain(&context, window)) {
                        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain, retrying");
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        dragging = true;
                        dragStart[0] = event.button.x;
                        dragStart[1] = event.button.y;
                        // Convert screen to world coordinates
                        float wx = (event.button.x - context.swapchainExtent.width / 2.0f) / context.camera.scale + context.camera.position[0];
                        float wy = (event.button.y - context.swapchainExtent.height / 2.0f) / context.camera.scale + context.camera.position[1];
                        // Check if clicking on square (simplified bounding box)
                        float sx = context.objects[1].position[0];
                        float sy = context.objects[1].position[1];
                        if (wx >= sx - 0.25f && wx <= sx + 0.25f && wy >= sy - 0.25f && wy <= sy + 0.25f) {
                            selectedObject = 1; // Square
                        }
                        // Check if clicking on text
                        float tx = context.textContext->position[0];
                        float ty = context.textContext->position[1];
                        float tw = 100.0f / context.camera.scale; // Approximate text width
                        float th = 24.0f / context.camera.scale;  // Approximate text height
                        if (wx >= tx && wx <= tx + tw && wy >= ty && wy <= ty + th) {
                            selectedObject = 2; // Text
                        }
                    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                        dragging = true;
                        dragStart[0] = event.button.x;
                        dragStart[1] = event.button.y;
                        selectedObject = -1; // Panning mode
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_MIDDLE) {
                        dragging = false;
                        selectedObject = -1;
                    }
                    break;
                case SDL_EVENT_MOUSE_MOTION:
                    if (dragging) {
                        float dx = (event.motion.x - dragStart[0]) / context.camera.scale;
                        float dy = (event.motion.y - dragStart[1]) / context.camera.scale;
                        if (selectedObject == 1) { // Dragging square
                            context.objects[1].position[0] += dx;
                            context.objects[1].position[1] += dy;
                            glm_translate_make(context.objects[1].modelMatrix, (vec3){context.objects[1].position[0], context.objects[1].position[1], 0.0f});
                        } else if (selectedObject == 2) { // Dragging text
                            context.textContext->position[0] += dx;
                            context.textContext->position[1] += dy;
                            glm_translate_make(context.textContext->modelMatrix, (vec3){context.textContext->position[0], context.textContext->position[1], 0.0f});
                        } else if (selectedObject == -1) { // Panning
                            context.camera.position[0] -= dx;
                            context.camera.position[1] -= dy;
                        }
                        dragStart[0] = event.motion.x;
                        dragStart[1] = event.motion.y;
                    }
                    break;
                case SDL_EVENT_MOUSE_WHEEL:
                    context.camera.scale += event.wheel.y * 0.1f;
                    if (context.camera.scale < 0.1f) context.camera.scale = 0.1f; // Minimum zoom
                    if (context.camera.scale > 10.0f) context.camera.scale = 10.0f; // Maximum zoom
                    break;
            }
        }

        if (!vulkan_render(&context)) {
            if (!recreate_swapchain(&context, window)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain, retrying");
            }
        }
    }

    // Cleanup
    vulkan_cleanup(&context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}