#include <iostream>
#include <windows.h>
#include <chrono>

// Define function pointer types matching our mock Vulkan API
typedef unsigned int (__stdcall *PFN_vkCreateInstance)(const void*, const void*, void**);
typedef void (__stdcall *PFN_vkDestroyInstance)(void*, const void*);
typedef unsigned int (__stdcall *PFN_vkEnumeratePhysicalDevices)(void*, unsigned int*, void**);
typedef unsigned int (__stdcall *PFN_vkCreateDevice)(void*, const void*, const void*, void**);
typedef void (__stdcall *PFN_vkDestroyDevice)(void*, const void*);
typedef void (__stdcall *PFN_vkGetDeviceQueue)(void*, unsigned int, unsigned int, void**);
typedef unsigned int (__stdcall *PFN_vkQueueSubmit)(void*, unsigned int, const void*, void*);

int main() {
    std::cout << "[Client] Launching Vulkan Linkage Verification Client..." << std::endl;

    // Load Vulkan shared library (DLL)
#ifdef NDEBUG
    const char* dllPath = "vulkan_softgpu.dll";
#else
    const char* dllPath = "vulkan_softgpu.dll"; // Fallback to path or build directory path
#endif

    HMODULE hLib = LoadLibraryA(dllPath);
    if (!hLib) {
        // Try absolute/relative paths under build dirs
        hLib = LoadLibraryA("Release\\vulkan_softgpu.dll");
        if (!hLib) {
            hLib = LoadLibraryA("Debug\\vulkan_softgpu.dll");
        }
    }

    if (!hLib) {
        std::cerr << "[Client] Error: Failed to load driver shared library (vulkan_softgpu.dll). Error code: " << GetLastError() << std::endl;
        return 1;
    }
    std::cout << "[Client] Driver loaded successfully." << std::endl;

    // Resolve exports
    auto vkCreateInstance = (PFN_vkCreateInstance)GetProcAddress(hLib, "vkCreateInstance");
    auto vkDestroyInstance = (PFN_vkDestroyInstance)GetProcAddress(hLib, "vkDestroyInstance");
    auto vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)GetProcAddress(hLib, "vkEnumeratePhysicalDevices");
    auto vkCreateDevice = (PFN_vkCreateDevice)GetProcAddress(hLib, "vkCreateDevice");
    auto vkDestroyDevice = (PFN_vkDestroyDevice)GetProcAddress(hLib, "vkDestroyDevice");
    auto vkGetDeviceQueue = (PFN_vkGetDeviceQueue)GetProcAddress(hLib, "vkGetDeviceQueue");
    auto vkQueueSubmit = (PFN_vkQueueSubmit)GetProcAddress(hLib, "vkQueueSubmit");

    if (!vkCreateInstance || !vkDestroyInstance || !vkEnumeratePhysicalDevices || 
        !vkCreateDevice || !vkDestroyDevice || !vkGetDeviceQueue || !vkQueueSubmit) {
        std::cerr << "[Client] Error: One or more Vulkan entry points could not be resolved!" << std::endl;
        FreeLibrary(hLib);
        return 1;
    }
    std::cout << "[Client] All Vulkan entry point function pointers resolved." << std::endl;

    // Execute mock sequence
    void* instance = nullptr;
    unsigned int res = vkCreateInstance(nullptr, nullptr, &instance);
    if (res != 0) {
        std::cerr << "[Client] vkCreateInstance failed!" << std::endl;
        FreeLibrary(hLib);
        return 1;
    }

    unsigned int deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    std::cout << "[Client] Physical Device count: " << deviceCount << std::endl;

    void* physicalDevice = nullptr;
    vkEnumeratePhysicalDevices(instance, &deviceCount, &physicalDevice);

    void* device = nullptr;
    res = vkCreateDevice(physicalDevice, nullptr, nullptr, &device);
    if (res != 0) {
        std::cerr << "[Client] vkCreateDevice failed!" << std::endl;
        vkDestroyInstance(instance, nullptr);
        FreeLibrary(hLib);
        return 1;
    }

    void* queue = nullptr;
    vkGetDeviceQueue(device, 0, 0, &queue);

    // Call submit and benchmark end-to-end execution
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::cout << "[Client] Dispatching frame submission..." << std::endl;
    res = vkQueueSubmit(queue, 0, nullptr, nullptr);

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> duration = endTime - startTime;

    if (res == 0) {
        std::cout << "[Client] Vulkan submission returned success!" << std::endl;
        std::cout << "[Client] Driver execution took: " << duration.count() << " ms" << std::endl;
        std::cout << "[Client] Pipeline Throughput: " << (800 * 800) / (duration.count() / 1000.0f) / 1000000.0f << " MegaPixels/sec" << std::endl;
    } else {
        std::cerr << "[Client] Error: Vulkan submission failed!" << std::endl;
    }

    // Clean up
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    FreeLibrary(hLib);
    std::cout << "[Client] Client completed execution successfully." << std::endl;

    return 0;
}
