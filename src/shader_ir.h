#pragma once
#include <vector>
#include <string>

enum class Op {
    MulF,      // Multiply float: dst = src0 * src1
    AddF,      // Add float: dst = src0 + src1
    SampleTex, // Sample texture: dst = sample(texture, u, v)
    DotV3,     // Dot product of Vec3: dst = dot(v0, v1)
    ClampF     // Clamp float: dst = clamp(src, minVal, maxVal)
};

struct ShaderInstr {
    Op op;
    int dst;
    int src0;
    int src1;
    float imm = 0.0f;
};

using ShaderProgram = std::vector<ShaderInstr>;
