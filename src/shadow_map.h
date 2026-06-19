#pragma once
#include <vector>
#include <memory>
#include "math_matrix.h"

class ShadowMap {
private:
    int width;
    int height;
    std::vector<float> depthBuffer;
    Matrix4 lightSpaceMatrix;

public:
    ShadowMap(int w, int h) : width(w), height(h) {
        // Initialize to +1e5f (far sentinel) so min-depth recording works correctly
        depthBuffer.resize(width * height, 1e5f);
    }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    const float* getDepthData() const { return depthBuffer.data(); }

    void clear() {
        // Reset to far sentinel so closest-surface-wins logic works
        std::fill(depthBuffer.begin(), depthBuffer.end(), 1e5f);
    }

    // Records depth: shadow map keeps the MINIMUM depth (closest surface to the light)
    void setDepth(int x, int y, float depth) {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            int idx = x + y * width;
            if (depth < depthBuffer[idx]) {
                depthBuffer[idx] = depth;
            }
        }
    }

    float getDepth(int x, int y) const {
        if (x >= 0 && x < width && y >= 0 && y < height) {
            return depthBuffer[x + y * width];
        }
        return -1e5f;
    }

    void setLightSpaceMatrix(const Matrix4& mat) {
        lightSpaceMatrix = mat;
    }

    const Matrix4& getLightSpaceMatrix() const {
        return lightSpaceMatrix;
    }

    // Samples depth map with Percentage-Closer Filtering (PCF) 3x3 kernel
    // Returns shadow factor between 0.0 (in shadow) and 1.0 (fully lit)
    float sampleShadow(const Vec3f& worldPos, const Vec3f& normal, const Vec3f& lightDir) const {
        Vec3f projCoords = lightSpaceMatrix.transformPoint(worldPos);
        // Transform NDC [-1,1] → shadow map pixel coords.
        // X: NDC X+ = right  → texture X+ = right  (no flip needed)
        // Y: NDC Y+ = up     → texture Y+ = down    (flip Y to match fill pass)
        float scX = (projCoords.x + 1.0f) * 0.5f * (float)(width - 1);
        float scY = (1.0f - (projCoords.y + 1.0f) * 0.5f) * (float)(height - 1);
        float currentDepth = projCoords.z;

        if (scX < 0.0f || scX >= (float)width || scY < 0.0f || scY >= (float)height) {
            return 1.0f; // Outside shadow map bounds is lit
        }

        // Slope-scaled bias: larger bias for steeper angles
        Vec3f n = normal;
        Vec3f l = lightDir;
        n.normalize();
        l.normalize();
        float cosTheta = std::max(0.0f, n * l);
        float bias = std::max(0.015f * (1.0f - cosTheta), 0.003f);

        float litSum = 0.0f;
        int texX = static_cast<int>(scX);
        int texY = static_cast<int>(scY);

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                float shadowMapDepth = getDepth(texX + dx, texY + dy);
                // Shadow map stores the MINIMUM depth (closest surface to light).
                // A fragment is IN SHADOW when its currentDepth > shadowMapDepth + bias
                // (i.e. the light cannot see it — something closer blocks it).
                // litSum accumulates 1.0 for lit samples, 0.0 for shadowed samples.
                litSum += (currentDepth > shadowMapDepth + bias) ? 0.0f : 1.0f;
            }
        }
        // Returns lit fraction: 1.0 = fully lit, 0.0 = fully in shadow
        return litSum / 9.0f;
    }
};
