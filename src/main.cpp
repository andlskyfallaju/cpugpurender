#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#define NOMINMAX
#include <windows.h>
#include "framebuffer.h"
#include "rasterizer.h"
#include "math_utils.h"
#include "tile_renderer.h"
#include "model.h"
#include "profiler.h"
#include "font8x8.h"

#include "hybrid_detector.h"
#include "jit_compiler.h"

// Transform a model-space vertex to screen space
Vec3f worldToScreen(const Vec3f& v, int width, int height) {
    float x = (v.x + 1.0f) * 0.5f * (float)(width  - 1);
    float y = (1.0f - (v.y + 1.0f) * 0.5f) * (float)(height - 1);
    return Vec3f(x, y, v.z);
}

// Rotation around Y (yaw)
Vec3f rotateY(const Vec3f& v, float angle) {
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    return Vec3f(
        v.x * cosA + v.z * sinA,
        v.y,
        -v.x * sinA + v.z * cosA
    );
}

// Rotation around X (pitch)
Vec3f rotateX(const Vec3f& v, float angle) {
    float cosA = std::cos(angle);
    float sinA = std::sin(angle);
    return Vec3f(
        v.x,
        v.y * cosA - v.z * sinA,
        v.y * sinA + v.z * cosA
    );
}

// Helpers for drawing OSD text and rectangles directly onto the Framebuffer
void drawChar(Framebuffer& fb, int x, int y, char c, const Color& color) {
    if (c < 32 || c > 127) c = '?';
    int index = c - 32;
    for (int row = 0; row < 8; ++row) {
        unsigned char rowBits = font8x8[index][row];
        for (int col = 0; col < 8; ++col) {
            if (rowBits & (0x80 >> col)) {
                fb.setPixel(x + col, y + row, color);
            }
        }
    }
}

void drawText(Framebuffer& fb, int x, int y, const std::string& text, const Color& color) {
    int curX = x;
    for (char c : text) {
        if (c == '\n') {
            y += 10;
            curX = x;
        } else {
            drawChar(fb, curX, y, c, color);
            curX += 8;
        }
    }
}

void drawRect(Framebuffer& fb, int x0, int y0, int x1, int y1, const Color& color) {
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (color.a == 255) {
                fb.setPixel(x, y, color);
            } else {
                Color dest = fb.getPixel(x, y);
                float alpha = color.a / 255.0f;
                uint8_t r = static_cast<uint8_t>(color.r * alpha + dest.r * (1.0f - alpha));
                uint8_t g = static_cast<uint8_t>(color.g * alpha + dest.g * (1.0f - alpha));
                uint8_t b = static_cast<uint8_t>(color.b * alpha + dest.b * (1.0f - alpha));
                fb.setPixel(x, y, Color(r, g, b, 255));
            }
        }
    }
}

// Global state for interaction
float yaw = 0.0f;
float pitch = 0.0f;
bool isDragging = false;
POINT lastMousePos;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_LBUTTONDOWN:
            isDragging = true;
            lastMousePos.x = (short)LOWORD(lParam);
            lastMousePos.y = (short)HIWORD(lParam);
            SetCapture(hwnd);
            break;
        case WM_LBUTTONUP:
            isDragging = false;
            ReleaseCapture();
            break;
        case WM_MOUSEMOVE:
            if (isDragging) {
                int x = (short)LOWORD(lParam);
                int y = (short)HIWORD(lParam);
                int dx = x - lastMousePos.x;
                int dy = y - lastMousePos.y;
                yaw += (float)dx * 0.005f;
                pitch += (float)dy * 0.005f;
                lastMousePos.x = x;
                lastMousePos.y = y;
            }
            break;
        case WM_KEYDOWN:
            if (wParam == VK_LEFT) yaw -= 0.05f;
            else if (wParam == VK_RIGHT) yaw += 0.05f;
            else if (wParam == VK_UP) pitch -= 0.05f;
            else if (wParam == VK_DOWN) pitch += 0.05f;
            else if (wParam == VK_ESCAPE) PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    bool headless = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--headless") {
            headless = true;
        }
    }

    std::cout << "Initializing CPU Software GPU Renderer (cpugpurender)..." << std::endl;

    // Detect CPU topology and apply thread affinity
    HybridDetector detector;
    std::cout << "Processor topology detected:\n";
    if (detector.isHybridArchitecture()) {
        std::cout << "  [Topology] Intel Hybrid CPU detected.\n";
        std::cout << "  [Topology] P-Core Affinity Mask: 0x" << std::hex << detector.getPCoreMask() << std::dec << "\n";
        std::cout << "  [Topology] E-Core Affinity Mask: 0x" << std::hex << detector.getECoreMask() << std::dec << "\n";
        
        // Pin main thread (executing geometry, binning, blit) to P-Cores
        SetThreadAffinityMask(GetCurrentThread(), detector.getPCoreMask());
        std::cout << "  [Topology] Main thread successfully pinned to P-Cores.\n";
    } else {
        std::cout << "  [Topology] Standard CPU architecture (No hybrid split applied).\n";
    }

    const int width  = 800;
    const int height = 800;
    Framebuffer fb(width, height);

    // Load 3D model
    std::cout << "Loading 3D model..." << std::endl;
    Model model("assets/girl/girl OBJ.obj");

    // Load all material textures
    std::cout << "Loading material textures..." << std::endl;
    const auto& materials = model.getMaterials();
    std::vector<Texture> materialTextures(materials.size());
    std::vector<Texture> normalTextures(materials.size());
    Texture fallbackTexture;
    fallbackTexture.createSolid(180, 180, 180); // Light gray fallback

    for (int i = 0; i < (int)materials.size(); ++i) {
        const auto& mat = materials[i];
        // 1. Diffuse texture map
        if (!mat.map_Kd.empty()) {
            std::string texPath = model.getBasePath() + mat.map_Kd;
            if (materialTextures[i].load(texPath)) {
                std::cout << "  [OK] " << mat.name << " -> " << texPath << std::endl;
            } else {
                std::cout << "  [FALLBACK] " << mat.name << " -> solid Kd color" << std::endl;
                uint8_t r = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[0])) * 255.0f);
                uint8_t g = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[1])) * 255.0f);
                uint8_t b = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[2])) * 255.0f);
                materialTextures[i].createSolid(r, g, b);
            }
        } else {
            // No texture map — use material's diffuse color as a solid texture
            uint8_t r = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[0])) * 255.0f);
            uint8_t g = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[1])) * 255.0f);
            uint8_t b = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, mat.Kd[2])) * 255.0f);
            materialTextures[i].createSolid(r, g, b);
            std::cout << "  [SOLID] " << mat.name << " -> Kd(" << (int)r << "," << (int)g << "," << (int)b << ")" << std::endl;
        }

        // 2. Normal/bump map
        if (!mat.map_Bump.empty()) {
            std::string nmPath = model.getBasePath() + mat.map_Bump;
            if (normalTextures[i].load(nmPath)) {
                std::cout << "  [OK NORMAL] " << mat.name << " -> " << nmPath << std::endl;
            } else {
                normalTextures[i].createSolid(128, 128, 255); // Default flat normal map
            }
        } else {
            normalTextures[i].createSolid(128, 128, 255); // Default flat normal map
        }
    }

    // Compute bounding box for automatic scale and center
    float minX = 1e9f, maxX = -1e9f;
    float minY = 1e9f, maxY = -1e9f;
    float minZ = 1e9f, maxZ = -1e9f;
    for (int i = 0; i < model.getNumFaces(); ++i) {
        for (int j = 0; j < 3; ++j) {
            Vec3f v = model.getVertex(i, j);
            if (v.x < minX) minX = v.x; if (v.x > maxX) maxX = v.x;
            if (v.y < minY) minY = v.y; if (v.y > maxY) maxY = v.y;
            if (v.z < minZ) minZ = v.z; if (v.z > maxZ) maxZ = v.z;
        }
    }

    float centerX = (minX + maxX) * 0.5f;
    float centerY = (minY + maxY) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;

    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    float maxDim = std::max({sizeX, sizeY, sizeZ});
    float scale = (maxDim > 1e-5f) ? (1.7f / maxDim) : 1.0f; // Scale to fit nicely in [-0.85, 0.85]

    // Light direction and intensity
    Vec3f lightDir(0.5f, 1.0f, 0.5f); // Angle light slightly from top-right-front
    float lightIntensity = 1.0f;

    // Allocate a ShadowMap (e.g. 1024x1024 depth buffer)
    ShadowMap shadowMap(1024, 1024);

    // Initialize JIT Compiler and compile our shader program skeleton
    JITCompiler jitCompiler;
    ShaderProgram program = {
        { Op::SampleTex, 0, 0, 0 },
        { Op::DotV3, 1, 0, 0 },
        { Op::MulF, 2, 1, 1 },
        { Op::ClampF, 3, 2, 2, 0.0f }
    };
    std::cout << "[JIT Compiler] Compiling shader program skeleton..." << std::endl;
    auto tJitStart = std::chrono::high_resolution_clock::now();
    JittedShaderFunc jittedShader = jitCompiler.compile(program);
    auto tJitEnd = std::chrono::high_resolution_clock::now();
    if (jittedShader) {
        std::cout << "  [OK] JIT compilation successful! Emitted assembly jump to execution block. (took " 
                  << std::chrono::duration<float, std::milli>(tJitEnd - tJitStart).count() << " ms)" << std::endl;
    } else {
        std::cerr << "  [ERROR] JIT compilation failed!" << std::endl;
    }

    // Initialize Hi-Z tile buffer once (tile size is default to 32)
    fb.initHiZ(32);

    auto renderFrame = [&](float currentYaw, float currentPitch, Profiler& prof, int tileSize = 32) {
        prof.telemetry.reset();

        auto tGeoStart = std::chrono::high_resolution_clock::now();

        // ── PASS 1: SHADOW PASS (Compute Light Space Depth Buffer) ───────────
        shadowMap.clear();

        // Set up Light Space View & Projection matrices
        // Eye position looks at the center from lightDir direction
        Vec3f lightEye = lightDir * 3.0f;
        Vec3f lightCenter(0.0f, 0.0f, 0.0f);
        Vec3f lightUp(0.0f, 1.0f, 0.0f);
        if (std::abs(lightDir.x) < 1e-4f && std::abs(lightDir.z) < 1e-4f) {
            lightUp = Vec3f(1.0f, 0.0f, 0.0f);
        }

        Matrix4 lightView = Matrix4::lookAt(lightEye, lightCenter, lightUp);
        Matrix4 lightProj = Matrix4::ortho(-1.5f, 1.5f, -1.5f, 1.5f, 0.1f, 10.0f);
        Matrix4 lightSpaceMatrix = lightProj * lightView;
        shadowMap.setLightSpaceMatrix(lightSpaceMatrix);

        // Render OBJ geometry into the shadow map
        for (int i = 0; i < model.getNumFaces(); ++i) {
            Vec3f mv0 = model.getVertex(i, 0);
            Vec3f mv1 = model.getVertex(i, 1);
            Vec3f mv2 = model.getVertex(i, 2);

            // Center and scale to normalized model space [-0.85, 0.85]
            Vec3f nv0((mv0.x - centerX) * scale, (mv0.y - centerY) * scale, (mv0.z - centerZ) * scale);
            Vec3f nv1((mv1.x - centerX) * scale, (mv1.y - centerY) * scale, (mv1.z - centerZ) * scale);
            Vec3f nv2((mv2.x - centerX) * scale, (mv2.y - centerY) * scale, (mv2.z - centerZ) * scale);

            // Rotate normalized model space coordinates (matches main camera rotation)
            Vec3f rv0 = rotateX(rotateY(nv0, currentYaw), currentPitch);
            Vec3f rv1 = rotateX(rotateY(nv1, currentYaw), currentPitch);
            Vec3f rv2 = rotateX(rotateY(nv2, currentYaw), currentPitch);

            // Project vertices to Light NDC
            Vec3f lv0 = lightSpaceMatrix.transformPoint(rv0);
            Vec3f lv1 = lightSpaceMatrix.transformPoint(rv1);
            Vec3f lv2 = lightSpaceMatrix.transformPoint(rv2);

            // Transform Light NDC coordinates to Shadow Map screen coordinates
            // NDC Y is +up but texture Y is +down, so flip Y.
            float lscX0 = (lv0.x + 1.0f) * 0.5f * (float)(shadowMap.getWidth() - 1);
            float lscY0 = (1.0f - (lv0.y + 1.0f) * 0.5f) * (float)(shadowMap.getHeight() - 1);
            float lscX1 = (lv1.x + 1.0f) * 0.5f * (float)(shadowMap.getWidth() - 1);
            float lscY1 = (1.0f - (lv1.y + 1.0f) * 0.5f) * (float)(shadowMap.getHeight() - 1);
            float lscX2 = (lv2.x + 1.0f) * 0.5f * (float)(shadowMap.getWidth() - 1);
            float lscY2 = (1.0f - (lv2.y + 1.0f) * 0.5f) * (float)(shadowMap.getHeight() - 1);

            int minX = std::max(0, static_cast<int>(std::floor(std::min({lscX0, lscX1, lscX2}))));
            int maxX = std::min(shadowMap.getWidth() - 1, static_cast<int>(std::ceil(std::max({lscX0, lscX1, lscX2}))));
            int minY = std::max(0, static_cast<int>(std::floor(std::min({lscY0, lscY1, lscY2}))));
            int maxY = std::min(shadowMap.getHeight() - 1, static_cast<int>(std::ceil(std::max({lscY0, lscY1, lscY2}))));

            float area = (lscX1 - lscX0) * (lscY2 - lscY0) - (lscY1 - lscY0) * (lscX2 - lscX0);
            if (std::abs(area) < 1e-5f) continue;

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    float w0 = ((x + 0.5f - lscX1) * (lscY2 - lscY1) - (y + 0.5f - lscY1) * (lscX2 - lscX1)) / area;
                    float w1 = ((x + 0.5f - lscX2) * (lscY0 - lscY2) - (y + 0.5f - lscY2) * (lscX0 - lscX2)) / area;
                    float w2 = 1.0f - w0 - w1;

                    if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                        float depth = w0 * lv0.z + w1 * lv1.z + w2 * lv2.z;
                        // setDepth records the minimum (closest-to-light) depth automatically
                        shadowMap.setDepth(x, y, depth);
                    }
                }
            }
        }

        // ── PASS 2: CAMERA SHADING PASS ─────────────────────────────────────
        Color backgroundColor(18, 20, 24, 255);
        fb.clear(backgroundColor, -1e5f);
        fb.clearHiZ(-1e5f);

        std::vector<RenderTask> tasks;
        tasks.reserve(model.getNumFaces());

        for (int i = 0; i < model.getNumFaces(); ++i) {
            Vec3f mv0 = model.getVertex(i, 0);
            Vec3f mv1 = model.getVertex(i, 1);
            Vec3f mv2 = model.getVertex(i, 2);

            // Center and scale to normalized model space [-0.85, 0.85]
            Vec3f nv0((mv0.x - centerX) * scale, (mv0.y - centerY) * scale, (mv0.z - centerZ) * scale);
            Vec3f nv1((mv1.x - centerX) * scale, (mv1.y - centerY) * scale, (mv1.z - centerZ) * scale);
            Vec3f nv2((mv2.x - centerX) * scale, (mv2.y - centerY) * scale, (mv2.z - centerZ) * scale);

            // Rotate normalized model space coordinates
            Vec3f rv0 = rotateX(rotateY(nv0, currentYaw), currentPitch);
            Vec3f rv1 = rotateX(rotateY(nv1, currentYaw), currentPitch);
            Vec3f rv2 = rotateX(rotateY(nv2, currentYaw), currentPitch);

            Vec3f sv0 = worldToScreen(rv0, width, height);
            Vec3f sv1 = worldToScreen(rv1, width, height);
            Vec3f sv2 = worldToScreen(rv2, width, height);

            Vec2f uv0 = model.getUV(i, 0);
            Vec2f uv1 = model.getUV(i, 1);
            Vec2f uv2 = model.getUV(i, 2);

            Vec3f n0 = model.getNormal(i, 0);
            Vec3f n1 = model.getNormal(i, 1);
            Vec3f n2 = model.getNormal(i, 2);

            // Rotate normals as well
            Vec3f rn0 = rotateX(rotateY(n0, currentYaw), currentPitch);
            Vec3f rn1 = rotateX(rotateY(n1, currentYaw), currentPitch);
            Vec3f rn2 = rotateX(rotateY(n2, currentYaw), currentPitch);

            // Calculate tangents for normal mapping
            Vec3f edge1 = rv1 - rv0;
            Vec3f edge2 = rv2 - rv0;
            Vec2f deltaUV1 = uv1 - uv0;
            Vec2f deltaUV2 = uv2 - uv0;

            float r_det = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
            Vec3f tangent(0.0f, 0.0f, 0.0f);
            if (std::abs(r_det) > 1e-5f) {
                float f = 1.0f / r_det;
                tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                tangent.normalize();
            } else {
                // Degenerate UVs, construct default tangent orthogonal to vertex normal
                tangent = (edge1.normalize() - rn0 * (edge1 * rn0)).normalize();
            }

            // Assign per-face textures from material
            int matIdx = model.getMaterialIndex(i);
            const Texture* tex = (matIdx >= 0 && matIdx < (int)materialTextures.size())
                                 ? &materialTextures[matIdx]
                                 : &fallbackTexture;

            const Texture* normMap = (matIdx >= 0 && matIdx < (int)normalTextures.size() && !materials[matIdx].map_Bump.empty())
                                     ? &normalTextures[matIdx]
                                     : nullptr;

            // Pass rotated model space coordinate as world space coordinates for shadows
            tasks.push_back({
                sv0, sv1, sv2,
                uv0, uv1, uv2,
                rn0, rn1, rn2,
                tex,
                rv0, rv1, rv2,
                &shadowMap,
                tangent, tangent, tangent, // Per-vertex tangents (simplified to face-tangent for flat/Gouraud normal maps)
                normMap,
                reinterpret_cast<void(*)(const void*, void*)>(jittedShader)
            });
        }

        auto tGeoEnd = std::chrono::high_resolution_clock::now();
        prof.geometryTimeMs = std::chrono::duration<float, std::milli>(tGeoEnd - tGeoStart).count();

        prof.binningTimeMs = 0.0f; // Placeholder for future binning optimization

        auto tRasStart = std::chrono::high_resolution_clock::now();
        DWORD_PTR workerAffinity = detector.isHybridArchitecture() ? detector.getECoreMask() : 0;
        TileRenderer::renderParallel(fb, tasks, lightDir, lightIntensity, true, &prof.telemetry, tileSize, workerAffinity);
        auto tRasEnd = std::chrono::high_resolution_clock::now();
        prof.rasterizationTimeMs = std::chrono::duration<float, std::milli>(tRasEnd - tRasStart).count();
    };

    bool benchTiles = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--bench-tiles") {
            benchTiles = true;
        }
    }

    if (benchTiles) {
        std::cout << "\n=== Running Tile-Size Tuning Benchmark ===" << std::endl;
        std::vector<int> sizes = { 8, 16, 32, 64 };
        Profiler prof;

        for (int size : sizes) {
            std::cout << "Benchmarking tile size: " << size << "x" << size << "..." << std::endl;
            // Initialize/reinitialize Hi-Z for the benchmark tile size
            fb.initHiZ(size);
            // Warm-up frame
            renderFrame(0.0f, 0.0f, prof, size);

            // 10 runs
            float totalRasTime = 0.0f;
            for (int r = 0; r < 10; ++r) {
                renderFrame(0.0f, 0.0f, prof, size);
                totalRasTime += prof.rasterizationTimeMs;
            }
            float avgRasTime = totalRasTime / 10.0f;
            std::cout << "  -> Avg Rasterization: " << avgRasTime << " ms | Tested Tris: " << prof.telemetry.trianglesTested.load() << std::endl;
        }
        std::cout << "==========================================\n" << std::endl;
        return 0;
    }

    if (headless) {
        std::cout << "Running in Headless Mode..." << std::endl;
        std::cout << "Rendering model with parallel SIMD backend..." << std::endl;

        Profiler prof;
        renderFrame(0.0f, 0.0f, prof);

        // Draw OSD
        float totalTime = prof.geometryTimeMs + prof.binningTimeMs + prof.rasterizationTimeMs;
        float fps = 1000.0f / totalTime;
        float mps = (width * height) / (prof.rasterizationTimeMs / 1000.0f) / 1000000.0f;

        char osdBuffer[512];
        sprintf_s(osdBuffer, 
            "PERFORMANCE RUNTIME OSD\n"
            "-----------------------\n"
            "FPS:        %.1f\n"
            "Frame Time: %.2f ms\n"
            "  Geometry: %.2f ms\n"
            "  Raster:   %.2f ms\n"
            "Throughput: %.2f MP/s\n"
            "Tris Count: %llu\n"
            "Hi-Z Culls: %llu\n"
            "Shaded Pix: %llu",
            fps, totalTime, prof.geometryTimeMs, prof.rasterizationTimeMs, mps, 
            prof.telemetry.trianglesTested.load(), prof.telemetry.hiZTileCulls.load(), prof.telemetry.pixelsShaded.load()
        );
        drawRect(fb, 10, 10, 210, 120, Color(10, 12, 16, 220));
        drawText(fb, 15, 15, osdBuffer, Color(0, 255, 128, 255));

        auto tPostStart = std::chrono::high_resolution_clock::now();
        std::cout << "Saving frame to output_model.png..." << std::endl;
        bool saved = fb.saveToPng("output_model.png");
        auto tPostEnd = std::chrono::high_resolution_clock::now();
        prof.postProcessTimeMs = std::chrono::duration<float, std::milli>(tPostEnd - tPostStart).count();

        float finalTotal = totalTime + prof.postProcessTimeMs;

        std::cout << "\n================= PROFILER BREAKDOWN REPORT =================\n";
        std::cout << "1. Geometry Processing:  " << prof.geometryTimeMs << " ms (" << (prof.geometryTimeMs / finalTotal * 100.0f) << "%)\n";
        std::cout << "2. Tile Binning:         " << prof.binningTimeMs << " ms (0.00%)\n";
        std::cout << "3. Rasterization & Shade: " << prof.rasterizationTimeMs << " ms (" << (prof.rasterizationTimeMs / finalTotal * 100.0f) << "%)\n";
        std::cout << "4. Post-processing/Save: " << prof.postProcessTimeMs << " ms (" << (prof.postProcessTimeMs / finalTotal * 100.0f) << "%)\n";
        std::cout << "-------------------------------------------------------------\n";
        std::cout << "Total Frame Time:        " << finalTotal << " ms\n";
        std::cout << "Throughput:              " << mps << " MegaPixels/sec (rasterization only)\n";
        std::cout << "===================== TELEMETRY COUNTERS ====================\n";
        std::cout << "Triangles Tested:        " << prof.telemetry.trianglesTested.load() << "\n";
        std::cout << "Early-Z Rejections:      " << prof.telemetry.earlyZRejections.load() << "\n";
        std::cout << "Hi-Z Tile Culls:         " << prof.telemetry.hiZTileCulls.load() << "\n";
        std::cout << "Pixels Fully Shaded:     " << prof.telemetry.pixelsShaded.load() << "\n";
        std::cout << "=============================================================\n\n";

        if (saved) {
            std::cout << "Render successful! Saved to output_model.png" << std::endl;
        } else {
            std::cerr << "Error: Failed to save output_model.png" << std::endl;
            return 1;
        }
        return 0;
    }

    // Win32 Interactive GUI Mode
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "CpuSoftGpuViewport";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClassEx(&wc)) {
        std::cerr << "Failed to register window class!" << std::endl;
        return 1;
    }

    RECT wr = { 0, 0, width, height };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        "CPU Software GPU Renderer - Interactive Viewport",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        std::cerr << "Failed to create window!" << std::endl;
        return 1;
    }

    HDC hdc = GetDC(hwnd);
    std::vector<uint32_t> displayBuffer(width * height);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Top-down representation to match flipped Y correctly
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    MSG msg = {};
    bool running = true;

    std::cout << "Entering Interactive Event Loop. Control rotation with Arrow Keys or Mouse Drag." << std::endl;

    Profiler prof;

    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                running = false;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (!running) break;

        auto tFrameStart = std::chrono::high_resolution_clock::now();

        // Render scene with current rotation
        renderFrame(yaw, pitch, prof);

        auto tPostStart = std::chrono::high_resolution_clock::now();

        // Calculate statistics for OSD
        float totalTime = prof.geometryTimeMs + prof.binningTimeMs + prof.rasterizationTimeMs;
        float fps = 1000.0f / totalTime;
        float mps = (width * height) / (prof.rasterizationTimeMs / 1000.0f) / 1000000.0f;

        // Draw OSD directly on the framebuffer
        char osdBuffer[512];
        sprintf_s(osdBuffer, 
            "PERFORMANCE RUNTIME OSD\n"
            "-----------------------\n"
            "FPS:        %.1f\n"
            "Frame Time: %.2f ms\n"
            "  Geometry: %.2f ms\n"
            "  Raster:   %.2f ms\n"
            "Throughput: %.2f MP/s\n"
            "Tris Count: %llu\n"
            "Hi-Z Culls: %llu\n"
            "Shaded Pix: %llu",
            fps, totalTime, prof.geometryTimeMs, prof.rasterizationTimeMs, mps, 
            prof.telemetry.trianglesTested.load(), prof.telemetry.hiZTileCulls.load(), prof.telemetry.pixelsShaded.load()
        );
        drawRect(fb, 10, 10, 210, 120, Color(10, 12, 16, 220));
        drawText(fb, 15, 15, osdBuffer, Color(0, 255, 128, 255));

        // Perform fast conversion from RGBA to BGRA for GDI blitting, flipping only Y to match GDI's viewport orientation
        const Color* colors = fb.getColorData();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int srcX = x;
                int srcY = height - 1 - y;
                int srcIdx = srcX + srcY * width;
                int destIdx = x + y * width;

                uint8_t r = colors[srcIdx].r;
                uint8_t g = colors[srcIdx].g;
                uint8_t b = colors[srcIdx].b;
                uint8_t a = colors[srcIdx].a;
                displayBuffer[destIdx] = b | (g << 8) | (r << 16) | (a << 24);
            }
        }

        // Blit to window
        StretchDIBits(
            hdc,
            0, 0, width, height,
            0, 0, width, height,
            displayBuffer.data(),
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY
        );

        auto tPostEnd = std::chrono::high_resolution_clock::now();
        prof.postProcessTimeMs = std::chrono::duration<float, std::milli>(tPostEnd - tPostStart).count();

        float finalTotal = totalTime + prof.postProcessTimeMs;
        float finalFps = 1000.0f / finalTotal;

        char titleBuffer[512];
        sprintf_s(titleBuffer, "CPU Software GPU Renderer - Viewport [G: %.1fms | R: %.1fms | P: %.1fms | Total: %.1fms | %.1f FPS | %.1f MP/s | Tested Tris: %llu | Hi-Z Culls: %llu]", 
                  prof.geometryTimeMs, prof.rasterizationTimeMs, prof.postProcessTimeMs, finalTotal, finalFps, mps, prof.telemetry.trianglesTested.load(), prof.telemetry.hiZTileCulls.load());
        SetWindowText(hwnd, titleBuffer);

        // Frame rate limiter: Sleep to target 60 FPS (16.67 ms per frame)
        auto tFrameEnd = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float, std::milli> frameDuration = tFrameEnd - tFrameStart;
        const float targetFrameTimeMs = 16.67f;
        if (frameDuration.count() < targetFrameTimeMs) {
            float sleepTimeMs = targetFrameTimeMs - frameDuration.count();
            Sleep(static_cast<DWORD>(sleepTimeMs));
        }
    }

    ReleaseDC(hwnd, hdc);
    return 0;
}




