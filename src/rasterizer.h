#pragma once
#include "framebuffer.h"
#include "math_utils.h"
#include "texture.h"
#include "profiler.h"
#include "shadow_map.h"

namespace Rasterizer {
    void drawTriangle(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                      const Color& c0, const Color& c1, const Color& c2,
                      Framebuffer& fb);

    void drawTriangleTextured(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                              const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                              const Texture& texture, Framebuffer& fb);

    void drawTriangleTexturedShaded(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                    const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                    const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                    const Vec3f& lightDir, float lightIntensity,
                                    const Texture& texture, Framebuffer& fb);


    void drawTriangleTexturedShadedTile(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                        const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                        const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                        const Vec3f& lightDir, float lightIntensity,
                                        const Texture& texture, Framebuffer& fb,
                                        int tileMinX, int tileMaxX, int tileMinY, int tileMaxY,
                                        Telemetry* telemetry = nullptr,
                                        const Vec3f& w0 = Vec3f(), const Vec3f& w1 = Vec3f(), const Vec3f& w2 = Vec3f(),
                                        const ShadowMap* shadowMap = nullptr,
                                        const Vec3f& t0 = Vec3f(), const Vec3f& t1 = Vec3f(), const Vec3f& t2 = Vec3f(),
                                        const Texture* normalMap = nullptr,
                                        void(*jittedShader)(const void*, void*) = nullptr);

    void drawTriangleTexturedShadedSIMD(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2,
                                        const Vec2f& uv0, const Vec2f& uv1, const Vec2f& uv2,
                                        const Vec3f& n0, const Vec3f& n1, const Vec3f& n2,
                                        const Vec3f& lightDir, float lightIntensity,
                                        const Texture& texture, Framebuffer& fb,
                                        int tileMinX, int tileMaxX, int tileMinY, int tileMaxY,
                                        Telemetry* telemetry = nullptr,
                                        const Vec3f& w0 = Vec3f(), const Vec3f& w1 = Vec3f(), const Vec3f& w2 = Vec3f(),
                                        const ShadowMap* shadowMap = nullptr,
                                        const Vec3f& t0 = Vec3f(), const Vec3f& t1 = Vec3f(), const Vec3f& t2 = Vec3f(),
                                        const Texture* normalMap = nullptr,
                                        void(*jittedShader)(const void*, void*) = nullptr);
}
