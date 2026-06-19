cmake_minimum_required(VERSION 3.15)
project(cpugpurender VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add include directories
include_directories(include)

# Standalone Executable target
add_executable(cpugpurender
    src/main.cpp
    src/framebuffer.cpp
    src/rasterizer.cpp
    src/texture.cpp
    src/tile_renderer.cpp
    src/model.cpp
    src/hybrid_detector.cpp
    src/shadow_map.cpp
    src/jit_compiler.cpp
)

# Shared Library target: vulkan_softgpu
add_library(vulkan_softgpu SHARED
    src/vulkan_entry.cpp
    src/framebuffer.cpp
    src/rasterizer.cpp
    src/texture.cpp
    src/tile_renderer.cpp
    src/model.cpp
    src/hybrid_detector.cpp
    src/shadow_map.cpp
    src/jit_compiler.cpp
)

# Test Client target
add_executable(vk_test
    src/vk_test.cpp
)

if(MSVC)
    target_compile_options(cpugpurender PRIVATE /arch:AVX2)
    target_compile_options(vulkan_softgpu PRIVATE /arch:AVX2)
else()
    target_compile_options(cpugpurender PRIVATE -mavx2 -mfma)
    target_compile_options(vulkan_softgpu PRIVATE -mavx2 -mfma)
endif()
