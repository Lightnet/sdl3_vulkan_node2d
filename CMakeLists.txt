cmake_minimum_required(VERSION 3.22)
project(TerminalEmulator C)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(FetchContent)

# Fetch SDL3
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.14
    GIT_SHALLOW TRUE
)
set(SDL_STATIC ON CACHE BOOL "Build SDL3 static library" FORCE)
set(SDL_SHARED OFF CACHE BOOL "Build SDL3 shared library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Build SDL3 tests" FORCE)
FetchContent_MakeAvailable(SDL3)

# Fetch FreeType
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-3
    GIT_SHALLOW TRUE
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable HarfBuzz" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "Disable PNG" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable BZip2" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable Brotli" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable ZLIB" FORCE)
FetchContent_MakeAvailable(freetype)
set(FREETYPE_LIBRARY ${CMAKE_BINARY_DIR}/libfreetyped.a CACHE PATH "Path to FreeType library" FORCE)
set(FREETYPE_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/_deps/freetype-src/include CACHE PATH "FreeType include directories" FORCE)

# Fetch SDL_ttf
FetchContent_Declare(
    sdl_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(sdl_ttf)

# Fetch Vulkan Headers
FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG vulkan-sdk-1.4.313.0 # Match your Vulkan SDK version
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(vulkan_headers)

# Find Vulkan SDK for shader compiler (glslc)
set(VULKAN_SDK "C:/VulkanSDK/1.4.313.0" CACHE PATH "Path to Vulkan SDK")
find_program(GLSLC_EXECUTABLE NAMES glslc HINTS "${VULKAN_SDK}/Bin")

# Define the application
set(APP_NAME sdl_terminal)

add_executable(${APP_NAME} src/main.c)

# Link libraries
target_link_libraries(${APP_NAME} PRIVATE 
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    freetype
    vulkan-1 # Vulkan loader (assumes system provides libvulkan.a or vulkan-1.lib)
)

# Include directories
target_include_directories(${APP_NAME} PRIVATE 
    ${freetype_SOURCE_DIR}/include
    ${SDL3_SOURCE_DIR}/include
    ${sdl_ttf_SOURCE_DIR}
    ${vulkan_headers_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

set_property(TARGET ${APP_NAME} PROPERTY C_STANDARD 11)

# Copy font file
configure_file("Kenney Pixel.ttf" "${CMAKE_BINARY_DIR}/Kenney Pixel.ttf" COPYONLY)

#================================================
#================================================
# Custom command for shader compilation (example)
# set(SHADER_SRC "${CMAKE_SOURCE_DIR}/src/shader.vert")
# set(SHADER_OUT "${CMAKE_BINARY_DIR}/shader.vert.spv")
# add_custom_command(
#     OUTPUT ${SHADER_OUT}
#     COMMAND ${GLSLC_EXECUTABLE} ${SHADER_SRC} -o ${SHADER_OUT}
#     DEPENDS ${SHADER_SRC}
#     COMMENT "Compiling vertex shader"
# )
# add_custom_target(shaders ALL DEPENDS ${SHADER_OUT})
# add_dependencies(${APP_NAME} shaders)
#================================================
#================================================

# Find Vulkan SDK for glslangValidator
set(VULKAN_SDK "C:/VulkanSDK/1.4.313.0" CACHE PATH "Path to Vulkan SDK")
find_program(GLSLANG_VALIDATOR_EXECUTABLE NAMES glslangValidator HINTS "${VULKAN_SDK}/Bin")

# # Shader compilation
# set(SHADER_VERT_SRC "${CMAKE_SOURCE_DIR}/shaders/shader2d.vert")
# set(SHADER_FRAG_SRC "${CMAKE_SOURCE_DIR}/shaders/shader2d.frag")
# set(SHADER_VERT_OUT "${CMAKE_BINARY_DIR}/include/shader2d_vert_spv.h")
# set(SHADER_FRAG_OUT "${CMAKE_BINARY_DIR}/include/shader2d_frag_spv.h")

# add_custom_command(
#     OUTPUT ${SHADER_VERT_OUT}
#     COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V --vn shader2d_vert_spv ${SHADER_VERT_SRC} -o ${SHADER_VERT_OUT}
#     DEPENDS ${SHADER_VERT_SRC}
#     COMMENT "Compiling vertex shader to SPIR-V header"
# )
# add_custom_command(
#     OUTPUT ${SHADER_FRAG_OUT}
#     COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V --vn shader2d_frag_spv ${SHADER_FRAG_SRC} -o ${SHADER_FRAG_OUT}
#     DEPENDS ${SHADER_FRAG_SRC}
#     COMMENT "Compiling fragment shader to SPIR-V header"
# )
# add_custom_target(shaders DEPENDS ${SHADER_VERT_OUT} ${SHADER_FRAG_OUT})
# add_dependencies(${APP_NAME} shaders)