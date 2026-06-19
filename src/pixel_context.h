#pragma once
#include "math_utils.h"
#include "texture.h"

struct PixelContext {
    Vec2f uv;
    Vec3f normal;
    Vec3f worldPos;
    const Texture* texture;
    const Texture* normalMap;
    float shadowFactor;
    float lightIntensity;
    Vec3f lightDir;
    Vec3f tangent;
    Vec3f bitangent;
};
