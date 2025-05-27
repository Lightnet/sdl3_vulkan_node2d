# sdl3_vulkan_node2d

# License: MIT

# Information:

    Work in progress.

    Working simple test on SDL 3.x, Vulkan and Freetype. As well other libraries.

## Features:
- [x] Vulkan SDK 1.4.313.0
- [x] SDL 3.2.14
- [x] Freetype 2.13.3
- [x] sdl_ttf 3.2.2
- [ ] node
    - [ ] drag
    - [ ] input
    - [ ] output
    - [ ] tick
    - [ ] variable
    - [ ] function


## Required:

### Prerequisites
- CMake: Version 3.10 or higher.
- MinGW-w64: For Windows builds (e.g., via MSYS2).
- SDL3, SDL3_ttf, FreeType: Source or prebuilt libraries (statically linked).
- Font: "Kenney Pixel.ttf" in the executable’s directory (or update the path, e.g.).

### Software
- Operating System: Windows 10 or later (64-bit).
- MSYS2: A software distribution and building platform for Windows.
    - Download: [MSYS2 Installer](https://www.msys2.org/)
- CMake: Version 3.10 or later (recommended: latest version available via MSYS2).
- MinGW-w64 Toolchain: Provides GCC, G++, and other tools for compiling.
- Git: For cloning the project repository (optional, if using source control).

## Dependencies

The project uses the following libraries, which are automatically fetched and built via CMake’s FetchContent:

- SDL2: Version 2.32.6 (Simple DirectMedia Layer for window and graphics handling).
- SDL_ttf: Version 2.24.0 (Font rendering library for SDL2).
- FreeType: Version 2.13.3 (Font rendering engine used by SDL_ttf).
- Font: "Kenney Pixel.ttf" (included in the project directory).

## MSYS2 Packages


```
pacman -Syu
```
update

```
mingw-w64-x86_64-vulkan-headers
mingw-w64-x86_64-vulkan-loader 
mingw-w64-x86_64-vulkan-validation-layers
```
packages

## Directory Structure:

```
sdl3_vulkan_node2d/
├── CMakeLists.txt
├── include/
│   ├── module_vulkan.h
│   ├── module_text.h
│   ├── shader2d_vert_spv.h
│   ├── shader2d_frag_spv.h
│   ├── shader_text_frag_spv.h
│   ├── shader_text_vert_spv.h
├── shaders/
│   ├── shader2d.vert
│   ├── shader2d.frag
│   ├── shader_text.frag
├── src/
│   ├── main.c
│   ├── module_vulkan.c
│   ├── module_text.c
├── build/
```

# Setup Instructions

1. Install MSYS2
2. Download and install MSYS2 from [msys2.org](https://www.msys2.org/).
3. Open the MSYS2 MinGW 64-bit terminal (mingw64.exe) from the MSYS2 installation directory (e.g., C:\msys64).
4. Update the package database and install required tools:

bash
```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-g++ mingw-w64-x86_64-cmake mingw-w64-x86_64-make mingw-w64-x86_64-pkgconf
```
- Note might need to install vulkan-d library.


# Credits

- Kenney Fonts: The "Kenney Mini.ttf" font is provided by [Kenney](https://kenney.nl/assets/kenney-fonts).
- SDL2: [Simple DirectMedia Layer](https://www.libsdl.org/).
- SDL_ttf: [SDL_ttf Library](https://github.com/libsdl-org/SDL_ttf).
- FreeType: [FreeType Project](https://www.freetype.org/).
- [Grok](https://x.com/i/grok)