wanted to create base on blender node 2d editor. For now let work on get vulkan working. Which required fetch content from vulkan repo without need dll vulkan sdk as well default driver.

will still use C:\VulkanSDK\1.4.313.0 compiler shader.
```
// Get SDL Vulkan extensions
    uint32_t extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&extensionCount, NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get Vulkan extensions count: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const char **extensions = malloc(extensionCount * sizeof(const char *));
    if (!SDL_Vulkan_GetInstanceExtensions(&extensionCount, extensions)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get Vulkan extensions: %s", SDL_GetError());
        free(extensions);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
```
incorrect that SDL 2.x

https://wiki.libsdl.org/SDL3/SDL_Vulkan_GetInstanceExtensions
this is SDL 3.x



