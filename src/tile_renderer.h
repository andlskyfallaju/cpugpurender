#pragma once
#include <vector>
#include <atomic>
#include "framebuffer.h"
#include "rasterizer.h"
#include "math_utils.h"

#define NOMINMAX
#include <windows.h>
#include "profiler.h"
#include "shadow_map.h"

struct RenderTask {
    Vec3f v0, v1, v2;
    Vec2f uv0, uv1, uv2;
    Vec3f n0, n1, n2;
    const Texture* texture = nullptr; // Per-triangle texture pointer
    Vec3f worldPos0, worldPos1, worldPos2; // World space positions for shadow mapping
    const ShadowMap* shadowMap = nullptr; // Pointer to shadow map
    Vec3f t0, t1, t2; // Tangent vectors
    const Texture* normalMap = nullptr; // Normal/bump map pointer
    void(*jittedShader)(const void*, void*) = nullptr; // JIT-compiled shader pointer
};

class TileRenderer {
public:
    static void renderParallel(Framebuffer& fb,
                               const std::vector<RenderTask>& tasks,
                               const Vec3f& lightDir, float lightIntensity,
                               bool useSIMD = false,
                               Telemetry* telemetry = nullptr,
                               int tileSize = 32,
                               DWORD_PTR workerAffinityMask = 0);
};
