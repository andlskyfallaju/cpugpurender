#pragma once
#include <memory>
#include <atomic>
#include <algorithm>
#include "math_matrix.h"

class ShadowMap {
private:
    int width;
    int height;
    // Atomic floats for lock-free parallel shadow fill (min-depth semantics via CAS loop)
    std::unique_ptr<std::atomic<float>[]> depthBuffer;
    Matrix4 lightSpaceMatrix;

public:
    ShadowMap(int w, int h) : width(w), height(h) {
        depthBuffer = std::make_unique<std::atomic<float>[]>(w * h);
        clear();
    }

    int getWidth()  const { return width; }
    int getHeight() const { return height; }

    // Reset all texels to +1e5f (far sentinel) — single-threaded, before parallel fill
    void clear() {
        int total = width * height;
        for (int i = 0; i < total; ++i) {
            depthBuffer[i].store(1e5f, std::memory_order_relaxed);
        }
    }

    // Lock-free minimum-depth write: thread-safe for parallel fill.
    // Only stores 'depth' if it is less than the current stored value (closest to light wins).
    void setDepth(int x, int y, float depth) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            auto& slot = depthBuffer[x + y * width];
            float current = slot.load(std::memory_order_relaxed);
            // CAS loop: keep trying until we succeed or find a smaller existing value
            while (depth < current &&
                   !slot.compare_exchange_weak(current, depth,
                                               std::memory_order_relaxed,
                                               std::memory_order_relaxed));
        }
    }

    float getDepth(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return depthBuffer[x + y * width].load(std::memory_order_relaxed);
        }
        return 1e5f; // Out-of-bounds: treat as fully lit (far depth)
    }

    void setLightSpaceMatrix(const Matrix4& mat) { lightSpaceMatrix = mat; }
    const Matrix4& getLightSpaceMatrix() const { return lightSpaceMatrix; }

    // PCF 3x3 soft shadow sampling — returns lit fraction [0,1]
    // 1.0 = fully lit, 0.0 = fully in shadow
    float sampleShadow(const Vec3f& worldPos, const Vec3f& normal, const Vec3f& lightDir) const {
        Vec3f projCoords = lightSpaceMatrix.transformPoint(worldPos);
        // Transform NDC [-1,1] to shadow map pixel coords.
        // X: NDC X+ = right → texture X+ = right (no flip)
        // Y: NDC Y+ = up   → texture Y+ = down  (flip Y to match fill pass)
        float scX = (projCoords.x + 1.0f) * 0.5f * (float)(width  - 1);
        float scY = (1.0f - (projCoords.y + 1.0f) * 0.5f) * (float)(height - 1);
        float currentDepth = projCoords.z;

        if (scX < 0.0f || scX >= (float)width || scY < 0.0f || scY >= (float)height) {
            return 1.0f; // Outside shadow map bounds → fully lit
        }

        // Slope-scaled bias to reduce shadow acne on steep surfaces
        // Assume normal and lightDir are pre-normalized
        float cosTheta = std::max(0.0f, normal * lightDir);
        float bias = std::max(0.03f * (1.0f - cosTheta), 0.012f);

        float litSum = 0.0f;
        int texX = static_cast<int>(scX + 0.5f);
        int texY = static_cast<int>(scY + 0.5f);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                float shadowMapDepth = getDepth(texX + dx, texY + dy);
                // Fragment is in shadow if its depth > closest-to-light stored depth + bias
                litSum += (currentDepth > shadowMapDepth + bias) ? 0.0f : 1.0f;
            }
        }
        return litSum / 9.0f;
    }
};
