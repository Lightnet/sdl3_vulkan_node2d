#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include "module_vulkan.h"

int main(int argc, char *argv[]) {
    printf("Hello Vulkan\n");

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }

    // Create window
    SDL_Window *window = SDL_CreateWindow("SDL 3.x Vulkan Node 2D", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Window creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    // Initialize Vulkan
    VulkanContext vulkanContext = {0};
    if (!vulkan_init(window, &vulkanContext)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Main loop
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        if (!vulkan_render(&vulkanContext)) {
            running = false;
        }
    }

    // Cleanup
    vulkan_cleanup(&vulkanContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}