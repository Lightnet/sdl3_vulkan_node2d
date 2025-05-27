cmake_minimum_required(VERSION 3.22)
project(sdl3_vulkan_node2d C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(APP_NAME sdl_terminal)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Find Vulkan
find_package(Vulkan REQUIRED)

# Find SDL3
include(FetchContent)
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

# Shader compilation
# do list need to compile first header which before application.
set(VULKAN_SDK "C:/VulkanSDK/1.4.313.0" CACHE PATH "Path to Vulkan SDK")
find_program(GLSLANG_VALIDATOR_EXECUTABLE NAMES glslangValidator HINTS ENV VULKAN_SDK PATH_SUFFIXES Bin)
if (NOT GLSLANG_VALIDATOR_EXECUTABLE)
    message(FATAL_ERROR "glslangValidator not found. Please install the Vulkan SDK.")
endif()

set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_FILES
    ${SHADER_DIR}/shader2d.vert
    ${SHADER_DIR}/shader2d.frag
    ${SHADER_DIR}/shader_text.vert
    ${SHADER_DIR}/shader_text.frag
)
set(SHADER_OUTPUT_DIR ${CMAKE_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(SHADER ${SHADER_FILES})
    get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
    set(SHADER_OUTPUT ${SHADER_OUTPUT_DIR}/${SHADER_NAME}_spv.h)
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT}
        COMMAND ${GLSLANG_VALIDATOR_EXECUTABLE} -V --vn ${SHADER_NAME}_spv ${SHADER} -o ${SHADER_OUTPUT}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER_NAME} to ${SHADER_OUTPUT}"
    )
    list(APPEND SHADER_OUTPUT_FILES ${SHADER_OUTPUT})
endforeach()


# Source files
set(SOURCE_FILES
    src/main.c
    src/module_vulkan.c
    src/module_text.c
    src/vulkan_utils.c
)

# Add executable
add_executable(${APP_NAME} ${SOURCE_FILES} ${SHADER_OUTPUT_FILES})

# Include directories
target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${SDL3_SOURCE_DIR}/include
    ${Vulkan_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${APP_NAME} PRIVATE
    SDL3::SDL3
    Vulkan::Vulkan
    SDL3_ttf::SDL3_ttf
    freetype
)

# Ensure shader headers are generated
add_custom_target(shaders DEPENDS ${SHADER_OUTPUT_FILES})
add_dependencies(${APP_NAME} shaders)