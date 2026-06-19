#pragma once

#ifdef _WIN32
  #define VKAPI_ATTR __declspec(dllexport)
  #define VKAPI_CALL __stdcall
#else
  #define VKAPI_ATTR
  #define VKAPI_CALL
#endif

// Mock standard Vulkan type definitions for compatibility
typedef unsigned int VkResult;
typedef void* VkInstance;
typedef void* VkPhysicalDevice;
typedef void* VkDevice;
typedef void* VkQueue;

#define VK_SUCCESS 0
#define VK_ERROR_INITIALIZATION_FAILED 1

struct VkInstanceCreateInfo {
    int sType;
    const void* pNext;
    unsigned int flags;
};

struct VkDeviceCreateInfo {
    int sType;
    const void* pNext;
    unsigned int flags;
};

extern "C" {
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
        const VkInstanceCreateInfo* pCreateInfo,
        const void* pAllocator,
        VkInstance* pInstance
    );

    VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(
        VkInstance instance,
        const void* pAllocator
    );

    VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(
        VkInstance instance,
        unsigned int* pPhysicalDeviceCount,
        VkPhysicalDevice* pPhysicalDevices
    );

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* pCreateInfo,
        const void* pAllocator,
        VkDevice* pDevice
    );

    VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
        VkDevice device,
        const void* pAllocator
    );

    VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(
        VkDevice device,
        unsigned int queueFamilyIndex,
        unsigned int queueIndex,
        VkQueue* pQueue
    );

    // Dynamic rendering dispatcher hook
    VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(
        VkQueue queue,
        unsigned int submitCount,
        const void* pSubmits,
        void* fence
    );
}
