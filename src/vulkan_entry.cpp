#include "vulkan_entry.h"
#include "framebuffer.h"
#include "tile_renderer.h"
#include "model.h"
#include "math_utils.h"
#include <iostream>
#include <chrono>

// Transform a model-space vertex to screen space
static Vec3f vkWorldToScreen(const Vec3f& v, int width, int height) {
    float x = (v.x + 1.0f) * 0.5f * (float)(width  - 1);
    float y = (1.0f - (v.y + 1.0f) * 0.5f) * (float)(height - 1);
    return Vec3f(x, y, v.z);
}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
    const VkInstanceCreateInfo* pCreateInfo,
    const void* pAllocator,
    VkInstance* pInstance
) {
    std::cout << "[Vulkan ICD] vkCreateInstance called successfully." << std::endl;
    if (pInstance) {
        *pInstance = (VkInstance)0xDEADC0DE; // Return mock handle
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
    VkInstance instance,
    const void* pAllocator
) {
    std::cout << "[Vulkan ICD] vkDestroyInstance called." << std::endl;
}

VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
    VkInstance instance,
    unsigned int* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices
) {
    std::cout << "[Vulkan ICD] vkEnumeratePhysicalDevices called." << std::endl;
    if (pPhysicalDeviceCount) {
        *pPhysicalDeviceCount = 1;
        if (pPhysicalDevices) {
            pPhysicalDevices[0] = (VkPhysicalDevice)0xFEEDFACE; // Mock physical device
        }
    }
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const void* pAllocator,
    VkDevice* pDevice
) {
    std::cout << "[Vulkan ICD] vkCreateDevice called successfully." << std::endl;
    if (pDevice) {
        *pDevice = (VkDevice)0xBEEFCAFE; // Return mock handle
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice device,
    const void* pAllocator
) {
    std::cout << "[Vulkan ICD] vkDestroyDevice called." << std::endl;
}

VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
    VkDevice device,
    unsigned int queueFamilyIndex,
    unsigned int queueIndex,
    VkQueue* pQueue
) {
    std::cout << "[Vulkan ICD] vkGetDeviceQueue called." << std::endl;
    if (pQueue) {
        *pQueue = (VkQueue)0xCAFEBABE; // Mock queue handle
    }
}


VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
    VkQueue queue,
    unsigned int submitCount,
    const void* pSubmits,
    void* fence
) {
    std::cout << "[Vulkan ICD] vkQueueSubmit received render submission. Dispatched to Parallel SIMD backend..." << std::endl;
    
    const int width = 800;
    const int height = 800;
    Framebuffer fb(width, height);
    Color backgroundColor(18, 20, 24, 255);
    fb.clear(backgroundColor, -1e5f);

    std::cout << "[Vulkan ICD] Loading 3D model..." << std::endl;
    Model model("assets/african_head.obj");

    std::cout << "[Vulkan ICD] Loading texture..." << std::endl;
    Texture texture;
    if (!texture.load("assets/african_head_diffuse.tga")) {
        std::cerr << "[Vulkan ICD] Error: Failed to load texture in submission." << std::endl;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    Vec3f lightDir(0.0f, 0.0f, 1.0f);
    float lightIntensity = 1.0f;

    std::cout << "[Vulkan ICD] Building render tasks for " << model.getNumFaces() << " faces..." << std::endl;
    std::vector<RenderTask> tasks;
    tasks.reserve(model.getNumFaces());

    for (int i = 0; i < model.getNumFaces(); ++i) {
        Vec3f mv0 = model.getVertex(i, 0);
        Vec3f mv1 = model.getVertex(i, 1);
        Vec3f mv2 = model.getVertex(i, 2);

        Vec3f sv0 = vkWorldToScreen(mv0, width, height);
        Vec3f sv1 = vkWorldToScreen(mv1, width, height);
        Vec3f sv2 = vkWorldToScreen(mv2, width, height);

        Vec2f uv0 = model.getUV(i, 0);
        Vec2f uv1 = model.getUV(i, 1);
        Vec2f uv2 = model.getUV(i, 2);

        Vec3f n0 = model.getNormal(i, 0);
        Vec3f n1 = model.getNormal(i, 1);
        Vec3f n2 = model.getNormal(i, 2);

        tasks.push_back({ sv0, sv1, sv2, uv0, uv1, uv2, n0, n1, n2, &texture });
    }

    std::cout << "[Vulkan ICD] Invoking Parallel SIMD pipeline..." << std::endl;
    auto t0 = std::chrono::high_resolution_clock::now();
    TileRenderer::renderParallel(fb, tasks, lightDir, lightIntensity, true);
    auto t1 = std::chrono::high_resolution_clock::now();
    
    std::chrono::duration<float, std::milli> duration = t1 - t0;
    std::cout << "[Vulkan ICD] Parallel Render complete in " << duration.count() << " ms." << std::endl;

    if (fb.saveToPng("output_vulkan.png")) {
        std::cout << "[Vulkan ICD] Frame submitted successfully. Saved to output_vulkan.png" << std::endl;
    } else {
        std::cerr << "[Vulkan ICD] Error: Failed to save output_vulkan.png" << std::endl;
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

}
