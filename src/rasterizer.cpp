#include "rasterizer.h"
#include <algorithm>
#include <cmath>
#include <immintrin.h>

namespace Rasterizer {

inline float edgeFunction(const Vec3f& a, const Vec3f& b, const Vec3f& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

void drawTriangle(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                  const Color& c0, const Color& c1, const Color& c2,
                  Framebuffer& fb) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    // Compute bounding box
    int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));

    // Area of the triangle (parallelogram area)
    float area = edgeFunction(v0, v1, v2);
    if (std::abs(area) < 1e-5f) return; // Degenerate triangle

    // Loop through the bounding box
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec3f p(x + 0.5f, y + 0.5f, 0.0f);

            // Compute edge weights
            float w0 = edgeFunction(v1, v2, p);
            float w1 = edgeFunction(v2, v0, p);
            float w2 = edgeFunction(v0, v1, p);

            // Check if inside based on winding order
            bool inside = false;
            if (area > 0) {
                inside = (w0 >= 0 && w1 >= 0 && w2 >= 0);
            } else {
                inside = (w0 <= 0 && w1 <= 0 && w2 <= 0);
            }

            if (inside) {
                // Normalize weights to get barycentric coordinates
                float u = w0 / area;
                float v = w1 / area;
                float w = w2 / area;

                // Interpolate Z depth
                float z = u * v0.z + v * v1.z + w * v2.z;

                // Z-buffer check
                if (z > fb.getDepth(x, y)) {
                    fb.setDepth(x, y, z);

                    // Interpolate vertex colors
                    uint8_t r = static_cast<uint8_t>(std::clamp(u * c0.r + v * c1.r + w * c2.r, 0.0f, 255.0f));
                    uint8_t g = static_cast<uint8_t>(std::clamp(u * c0.g + v * c1.g + w * c2.g, 0.0f, 255.0f));
                    uint8_t b = static_cast<uint8_t>(std::clamp(u * c0.b + v * c1.b + w * c2.b, 0.0f, 255.0f));
                    uint8_t a = static_cast<uint8_t>(std::clamp(u * c0.a + v * c1.a + w * c2.a, 0.0f, 255.0f));

                    fb.setPixel(x, y, Color(r, g, b, a));
                }
            }
        }
    }
}

void drawTriangleTextured(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                          const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                          const Texture& texture, Framebuffer& fb) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));

    float area = edgeFunction(v0, v1, v2);
    if (std::abs(area) < 1e-5f) return;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec3f p(x + 0.5f, y + 0.5f, 0.0f);

            float w0 = edgeFunction(v1, v2, p);
            float w1 = edgeFunction(v2, v0, p);
            float w2 = edgeFunction(v0, v1, p);

            bool inside = false;
            if (area > 0) {
                inside = (w0 >= 0 && w1 >= 0 && w2 >= 0);
            } else {
                inside = (w0 <= 0 && w1 <= 0 && w2 <= 0);
            }

            if (inside) {
                float u = w0 / area;
                float v = w1 / area;
                float w = w2 / area;

                float z = u * v0.z + v * v1.z + w * v2.z;

                if (z > fb.getDepth(x, y)) {
                    fb.setDepth(x, y, z);

                    // Interpolate texture coordinates
                    float texU = u * uv0.x + v * uv1.x + w * uv2.x;
                    float texV = u * uv0.y + v * uv1.y + w * uv2.y;

                    Color texColor = texture.sample(texU, texV);
                    fb.setPixel(x, y, texColor);
                }
            }
        }
    }
}

void drawTriangleTexturedShaded(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                const Vec3f& lightDir, float lightIntensity,
                                const Texture& texture, Framebuffer& fb) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    int minX = std::max(0, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x}))));
    int maxX = std::min(width - 1, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x}))));
    int minY = std::max(0, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y}))));
    int maxY = std::min(height - 1, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y}))));

    float area = edgeFunction(v0, v1, v2);
    if (std::abs(area) < 1e-5f) return;

    Vec3f lDir = lightDir;
    lDir.normalize();

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec3f p(x + 0.5f, y + 0.5f, 0.0f);

            float w0 = edgeFunction(v1, v2, p);
            float w1 = edgeFunction(v2, v0, p);
            float w2 = edgeFunction(v0, v1, p);

            bool inside = false;
            if (area > 0) {
                inside = (w0 >= 0 && w1 >= 0 && w2 >= 0);
            } else {
                inside = (w0 <= 0 && w1 <= 0 && w2 <= 0);
            }

            if (inside) {
                float u = w0 / area;
                float v = w1 / area;
                float w = w2 / area;

                float z = u * v0.z + v * v1.z + w * v2.z;

                if (z > fb.getDepth(x, y)) {
                    fb.setDepth(x, y, z);

                    float texU = u * uv0.x + v * uv1.x + w * uv2.x;
                    float texV = u * uv0.y + v * uv1.y + w * uv2.y;

                    Color texColor = texture.sample(texU, texV);

                    Vec3f normal = n0 * u + n1 * v + n2 * w;
                    normal.normalize();

                    float diffuse = std::max(0.0f, normal * lDir);
                    float intensity = (0.15f + 0.85f * diffuse) * lightIntensity;
                    intensity = std::clamp(intensity, 0.0f, 1.0f);

                    uint8_t r = static_cast<uint8_t>(std::clamp(texColor.r * intensity, 0.0f, 255.0f));
                    uint8_t g = static_cast<uint8_t>(std::clamp(texColor.g * intensity, 0.0f, 255.0f));
                    uint8_t b = static_cast<uint8_t>(std::clamp(texColor.b * intensity, 0.0f, 255.0f));

                    fb.setPixel(x, y, Color(r, g, b, texColor.a));
                }
            }
        }
    }
}

#include "pixel_context.h"

void drawTriangleTexturedShadedTile(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                    const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                    const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                    const Vec3f& lightDir, float lightIntensity,
                                    const Texture& texture, Framebuffer& fb,
                                    int tileMinX, int tileMaxX, int tileMinY, int tileMaxY,
                                    Telemetry* telemetry,
                                    const Vec3f& w0, const Vec3f& w1, const Vec3f& w2,
                                    const ShadowMap* shadowMap,
                                    const Vec3f& t0, const Vec3f& t1, const Vec3f& t2,
                                    const Texture* normalMap,
                                    void(*jittedShader)(const void*, void*)) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    // Clamp bounding box to both screen limits and tile limits
    int minX = std::max({0, tileMinX, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x})))});
    int maxX = std::min({width - 1, tileMaxX, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x})))});
    int minY = std::max({0, tileMinY, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y})))});
    int maxY = std::min({height - 1, tileMaxY, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y})))});

    if (minX > maxX || minY > maxY) return;

    if (telemetry) {
        telemetry->trianglesTested.fetch_add(1, std::memory_order_relaxed);
    }

    float area = edgeFunction(v0, v1, v2);
    if (std::abs(area) < 1e-5f) return;

    Vec3f lDir = lightDir;
    lDir.normalize();

    uint64_t localShaded = 0;
    uint64_t localEarlyZ = 0;

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            Vec3f p(x + 0.5f, y + 0.5f, 0.0f);

            float w0_val = edgeFunction(v1, v2, p);
            float w1_val = edgeFunction(v2, v0, p);
            float w2_val = edgeFunction(v0, v1, p);

            bool inside = false;
            if (area > 0) {
                inside = (w0_val >= 0 && w1_val >= 0 && w2_val >= 0);
            } else {
                inside = (w0_val <= 0 && w1_val <= 0 && w2_val <= 0);
            }

            if (inside) {
                float u = w0_val / area;
                float v = w1_val / area;
                float w = w2_val / area;

                float z = u * v0.z + v * v1.z + w * v2.z;

                if (z > fb.getDepth(x, y)) {
                    fb.setDepth(x, y, z);

                    float texU = u * uv0.x + v * uv1.x + w * uv2.x;
                    float texV = u * uv0.y + v * uv1.y + w * uv2.y;

                    Color finalColor;

                    if (jittedShader) {
                        float shadowFactor = 1.0f;
                        Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                        Vec3f interpNormal = n0 * u + n1 * v + n2 * w;
                        interpNormal.normalize();
                        if (shadowMap) {
                            shadowFactor = shadowMap->sampleShadow(worldPos, interpNormal, lDir);
                        }
                        PixelContext ctx = {
                            Vec2f(texU, texV),
                            interpNormal,
                            worldPos,
                            &texture,
                            normalMap,
                            shadowFactor,
                            lightIntensity,
                            lDir,
                            t0 * u + t1 * v + t2 * w,
                            Vec3f(), // Bitangent is reconstructed in JIT when normal mapping
                        };
                        jittedShader(&ctx, &finalColor);
                    } else {
                        Color texColor = texture.sample(texU, texV);

                        Vec3f normal = n0 * u + n1 * v + n2 * w;
                        normal.normalize();

                        if (normalMap) {
                            // Reconstruct TBN matrix
                            Vec3f tangent = t0 * u + t1 * v + t2 * w;
                            tangent.normalize();
                            // Re-orthogonalize tangent with respect to normal (Gram-Schmidt)
                            tangent = (tangent - normal * (tangent * normal)).normalize();
                            Vec3f bitangent = normal ^ tangent;
                            bitangent.normalize();

                            Color nmColor = normalMap->sample(texU, texV);
                            // Map RGB [0, 255] to tangent space normal [-1, 1]
                            Vec3f tangentNormal(
                                (nmColor.r / 255.0f) * 2.0f - 1.0f,
                                (nmColor.g / 255.0f) * 2.0f - 1.0f,
                                (nmColor.b / 255.0f) * 2.0f - 1.0f
                            );
                            tangentNormal.normalize();
                            // Transform tangentNormal to worldNormal
                            normal = (tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z).normalize();
                        }

                        float shadowFactor = 1.0f;
                        if (shadowMap) {
                            Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                            shadowFactor = shadowMap->sampleShadow(worldPos, normal, lDir);
                        }

                        float diffuse = std::max(0.0f, normal * lDir);
                        // Shading intensity combines ambient (0.15) + diffuse * shadow factor
                        float intensity = (0.15f + 0.85f * diffuse * shadowFactor) * lightIntensity;
                        intensity = std::clamp(intensity, 0.0f, 1.0f);

                        finalColor.r = static_cast<uint8_t>(std::clamp(texColor.r * intensity, 0.0f, 255.0f));
                        finalColor.g = static_cast<uint8_t>(std::clamp(texColor.g * intensity, 0.0f, 255.0f));
                        finalColor.b = static_cast<uint8_t>(std::clamp(texColor.b * intensity, 0.0f, 255.0f));
                        finalColor.a = texColor.a;
                    }

                    fb.setPixel(x, y, finalColor);
                    localShaded++;
                } else {
                    localEarlyZ++;
                }
            }
        }
    }

    if (telemetry) {
        if (localShaded > 0) telemetry->pixelsShaded.fetch_add(localShaded, std::memory_order_relaxed);
        if (localEarlyZ > 0) telemetry->earlyZRejections.fetch_add(localEarlyZ, std::memory_order_relaxed);
    }
}

void drawTriangleTexturedShadedSIMD(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                    const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                    const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                    const Vec3f& lightDir, float lightIntensity,
                                    const Texture& texture, Framebuffer& fb,
                                    int tileMinX, int tileMaxX, int tileMinY, int tileMaxY,
                                    Telemetry* telemetry,
                                    const Vec3f& w0, const Vec3f& w1, const Vec3f& w2,
                                    const ShadowMap* shadowMap,
                                    const Vec3f& t0, const Vec3f& t1, const Vec3f& t2,
                                    const Texture* normalMap,
                                    void(*jittedShader)(const void*, void*)) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    int minX = std::max({0, tileMinX, static_cast<int>(std::floor(std::min({v0.x, v1.x, v2.x})))});
    int maxX = std::min({width - 1, tileMaxX, static_cast<int>(std::ceil(std::max({v0.x, v1.x, v2.x})))});
    int minY = std::max({0, tileMinY, static_cast<int>(std::floor(std::min({v0.y, v1.y, v2.y})))});
    int maxY = std::min({height - 1, tileMaxY, static_cast<int>(std::ceil(std::max({v0.y, v1.y, v2.y})))});

    if (minX > maxX || minY > maxY) return;

    if (telemetry) {
        telemetry->trianglesTested.fetch_add(1, std::memory_order_relaxed);
    }

    float area = edgeFunction(v0, v1, v2);
    if (std::abs(area) < 1e-5f) return;

    Vec3f lDir = lightDir;
    lDir.normalize();

    uint64_t localShaded = 0;
    uint64_t localEarlyZ = 0;

    // Prepare SIMD constants
    __m256 v_v0_x = _mm256_set1_ps(v0.x);
    __m256 v_v0_y = _mm256_set1_ps(v0.y);
    __m256 v_v1_x = _mm256_set1_ps(v1.x);
    __m256 v_v1_y = _mm256_set1_ps(v1.y);
    __m256 v_v2_x = _mm256_set1_ps(v2.x);
    __m256 v_v2_y = _mm256_set1_ps(v2.y);

    __m256 v_zero = _mm256_setzero_ps();

    for (int y = minY; y <= maxY; ++y) {
        __m256 v_py = _mm256_set1_ps(y + 0.5f);
        int x = minX;
        for (; x <= maxX - 7; x += 8) {
            __m256 v_px = _mm256_set_ps(
                x + 7.5f, x + 6.5f, x + 5.5f, x + 4.5f,
                x + 3.5f, x + 2.5f, x + 1.5f, x + 0.5f
            );

            __m256 term1_w0 = _mm256_mul_ps(_mm256_sub_ps(v_px, v_v1_x), _mm256_sub_ps(v_v2_y, v_v1_y));
            __m256 term2_w0 = _mm256_mul_ps(_mm256_sub_ps(v_py, v_v1_y), _mm256_sub_ps(v_v2_x, v_v1_x));
            __m256 v_w0 = _mm256_sub_ps(term1_w0, term2_w0);

            __m256 term1_w1 = _mm256_mul_ps(_mm256_sub_ps(v_px, v_v2_x), _mm256_sub_ps(v_v0_y, v_v2_y));
            __m256 term2_w1 = _mm256_mul_ps(_mm256_sub_ps(v_py, v_v2_y), _mm256_sub_ps(v_v0_x, v_v2_x));
            __m256 v_w1 = _mm256_sub_ps(term1_w1, term2_w1);

            __m256 term1_w2 = _mm256_mul_ps(_mm256_sub_ps(v_px, v_v0_x), _mm256_sub_ps(v_v1_y, v_v0_y));
            __m256 term2_w2 = _mm256_mul_ps(_mm256_sub_ps(v_py, v_v0_y), _mm256_sub_ps(v_v1_x, v_v0_x));
            __m256 v_w2 = _mm256_sub_ps(term1_w2, term2_w2);

            __m256 v_inside;
            if (area > 0) {
                v_inside = _mm256_and_ps(
                    _mm256_cmp_ps(v_w0, v_zero, _CMP_GE_OQ),
                    _mm256_and_ps(
                        _mm256_cmp_ps(v_w1, v_zero, _CMP_GE_OQ),
                        _mm256_cmp_ps(v_w2, v_zero, _CMP_GE_OQ)
                    )
                );
            } else {
                v_inside = _mm256_and_ps(
                    _mm256_cmp_ps(v_w0, v_zero, _CMP_LE_OQ),
                    _mm256_and_ps(
                        _mm256_cmp_ps(v_w1, v_zero, _CMP_LE_OQ),
                        _mm256_cmp_ps(v_w2, v_zero, _CMP_LE_OQ)
                    )
                );
            }

            int mask = _mm256_movemask_ps(v_inside);
            if (mask == 0) continue;

            float w0_arr[8], w1_arr[8], w2_arr[8];
            _mm256_storeu_ps(w0_arr, v_w0);
            _mm256_storeu_ps(w1_arr, v_w1);
            _mm256_storeu_ps(w2_arr, v_w2);

            for (int i = 0; i < 8; ++i) {
                if (mask & (1 << i)) {
                    int px_val = x + i;
                    float u = w0_arr[i] / area;
                    float v = w1_arr[i] / area;
                    float w = w2_arr[i] / area;

                    float z = u * v0.z + v * v1.z + w * v2.z;

                    if (z > fb.getDepth(px_val, y)) {
                        fb.setDepth(px_val, y, z);

                        float texU = u * uv0.x + v * uv1.x + w * uv2.x;
                        float texV = u * uv0.y + v * uv1.y + w * uv2.y;

                        Color finalColor;

                        if (jittedShader) {
                            float shadowFactor = 1.0f;
                            Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                            Vec3f interpNormal = n0 * u + n1 * v + n2 * w;
                            interpNormal.normalize();
                            if (shadowMap) {
                                shadowFactor = shadowMap->sampleShadow(worldPos, interpNormal, lDir);
                            }
                            PixelContext ctx = {
                                Vec2f(texU, texV),
                                interpNormal,
                                worldPos,
                                &texture,
                                normalMap,
                                shadowFactor,
                                lightIntensity,
                                lDir,
                                t0 * u + t1 * v + t2 * w,
                                Vec3f(),
                            };
                            jittedShader(&ctx, &finalColor);
                        } else {
                            Color texColor = texture.sample(texU, texV);

                            Vec3f normal = n0 * u + n1 * v + n2 * w;
                            normal.normalize();

                            if (normalMap) {
                                Vec3f tangent = t0 * u + t1 * v + t2 * w;
                                tangent.normalize();
                                tangent = (tangent - normal * (tangent * normal)).normalize();
                                Vec3f bitangent = normal ^ tangent;
                                bitangent.normalize();

                                Color nmColor = normalMap->sample(texU, texV);
                                Vec3f tangentNormal(
                                    (nmColor.r / 255.0f) * 2.0f - 1.0f,
                                    (nmColor.g / 255.0f) * 2.0f - 1.0f,
                                    (nmColor.b / 255.0f) * 2.0f - 1.0f
                                );
                                tangentNormal.normalize();
                                normal = (tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z).normalize();
                            }

                            float shadowFactor = 1.0f;
                            if (shadowMap) {
                                Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                                shadowFactor = shadowMap->sampleShadow(worldPos, normal, lDir);
                            }

                            float diffuse = std::max(0.0f, normal * lDir);
                            float intensity = (0.15f + 0.85f * diffuse * shadowFactor) * lightIntensity;
                            intensity = std::clamp(intensity, 0.0f, 1.0f);

                            finalColor.r = static_cast<uint8_t>(std::clamp(texColor.r * intensity, 0.0f, 255.0f));
                            finalColor.g = static_cast<uint8_t>(std::clamp(texColor.g * intensity, 0.0f, 255.0f));
                            finalColor.b = static_cast<uint8_t>(std::clamp(texColor.b * intensity, 0.0f, 255.0f));
                            finalColor.a = texColor.a;
                        }

                        fb.setPixel(px_val, y, finalColor);
                        localShaded++;
                    } else {
                        localEarlyZ++;
                    }
                }
            }
        }
        for (; x <= maxX; ++x) {
            Vec3f p(x + 0.5f, y + 0.5f, 0.0f);

            float w0_val = edgeFunction(v1, v2, p);
            float w1_val = edgeFunction(v2, v0, p);
            float w2_val = edgeFunction(v0, v1, p);

            bool inside = false;
            if (area > 0) {
                inside = (w0_val >= 0 && w1_val >= 0 && w2_val >= 0);
            } else {
                inside = (w0_val <= 0 && w1_val <= 0 && w2_val <= 0);
            }

            if (inside) {
                float u = w0_val / area;
                float v = w1_val / area;
                float w = w2_val / area;

                float z = u * v0.z + v * v1.z + w * v2.z;

                if (z > fb.getDepth(x, y)) {
                    fb.setDepth(x, y, z);

                    float texU = u * uv0.x + v * uv1.x + w * uv2.x;
                    float texV = u * uv0.y + v * uv1.y + w * uv2.y;

                    Color finalColor;

                    if (jittedShader) {
                        float shadowFactor = 1.0f;
                        Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                        Vec3f interpNormal = n0 * u + n1 * v + n2 * w;
                        interpNormal.normalize();
                        if (shadowMap) {
                            shadowFactor = shadowMap->sampleShadow(worldPos, interpNormal, lDir);
                        }
                        PixelContext ctx = {
                            Vec2f(texU, texV),
                            interpNormal,
                            worldPos,
                            &texture,
                            normalMap,
                            shadowFactor,
                            lightIntensity,
                            lDir,
                            t0 * u + t1 * v + t2 * w,
                            Vec3f(),
                        };
                        jittedShader(&ctx, &finalColor);
                    } else {
                        Color texColor = texture.sample(texU, texV);

                        Vec3f normal = n0 * u + n1 * v + n2 * w;
                        normal.normalize();

                        if (normalMap) {
                            Vec3f tangent = t0 * u + t1 * v + t2 * w;
                            tangent.normalize();
                            tangent = (tangent - normal * (tangent * normal)).normalize();
                            Vec3f bitangent = normal ^ tangent;
                            bitangent.normalize();

                            Color nmColor = normalMap->sample(texU, texV);
                            Vec3f tangentNormal(
                                (nmColor.r / 255.0f) * 2.0f - 1.0f,
                                (nmColor.g / 255.0f) * 2.0f - 1.0f,
                                (nmColor.b / 255.0f) * 2.0f - 1.0f
                            );
                            tangentNormal.normalize();
                            normal = (tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z).normalize();
                        }

                        float shadowFactor = 1.0f;
                        if (shadowMap) {
                            Vec3f worldPos = w0 * u + w1 * v + w2 * w;
                            shadowFactor = shadowMap->sampleShadow(worldPos, normal, lDir);
                        }

                        float diffuse = std::max(0.0f, normal * lDir);
                        float intensity = (0.15f + 0.85f * diffuse * shadowFactor) * lightIntensity;
                        intensity = std::clamp(intensity, 0.0f, 1.0f);

                        finalColor.r = static_cast<uint8_t>(std::clamp(texColor.r * intensity, 0.0f, 255.0f));
                        finalColor.g = static_cast<uint8_t>(std::clamp(texColor.g * intensity, 0.0f, 255.0f));
                        finalColor.b = static_cast<uint8_t>(std::clamp(texColor.b * intensity, 0.0f, 255.0f));
                        finalColor.a = texColor.a;
                    }

                    fb.setPixel(x, y, finalColor);
                    localShaded++;
                } else {
                    localEarlyZ++;
                }
            }
        }
    }

    if (telemetry) {
        if (localShaded > 0) telemetry->pixelsShaded.fetch_add(localShaded, std::memory_order_relaxed);
        if (localEarlyZ > 0) telemetry->earlyZRejections.fetch_add(localEarlyZ, std::memory_order_relaxed);
    }
}

} // namespace Rasterizer
