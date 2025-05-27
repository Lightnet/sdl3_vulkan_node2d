#include <SDL3/SDL.h>
#include "module_vulkan.h"

int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Window *window = SDL_CreateWindow("SDL Vulkan Node 2D", 600, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create window: %s", SDL_GetError());
        return 1;
    }

    VulkanContext context = {0};
    if (!vulkan_init(window, &context)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Vulkan");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    SDL_Event event;
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
            }
        }

        if (!vulkan_render(&context)) {
            if (!recreate_swapchain(&context, window)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to recreate swapchain, retrying");
                continue;
            }
        }
    }

    vulkan_cleanup(&context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}