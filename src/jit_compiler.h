#pragma once
#include <vector>
#include <cstdint>
#define NOMINMAX
#include <windows.h>
#include "shader_ir.h"
#include "pixel_context.h"
#include "framebuffer.h"

// JIT compiled shader signature: takes pixel context, executes compiled instructions, returns color
using JittedShaderFunc = void(*)(const PixelContext*, Color*);

class JITCompiler {
private:
    uint8_t* codeBuffer = nullptr;
    size_t bufferSize = 0;

public:
    static void executeShading(const PixelContext* ctx, Color* outColor) {
        // Default fallback texture sample
        Color texColor(180, 180, 180, 255);
        if (ctx->texture) {
            texColor = ctx->texture->sample(ctx->uv.x, ctx->uv.y);
        }

        Vec3f normal = ctx->normal;
        if (ctx->normalMap) {
            Vec3f tangent = ctx->tangent;
            tangent = (tangent - normal * (tangent * normal)).normalize();
            Vec3f bitangent = normal ^ tangent;
            bitangent.normalize();
            Color nmColor = ctx->normalMap->sample(ctx->uv.x, ctx->uv.y);
            Vec3f tangentNormal(
                (nmColor.r / 255.0f) * 2.0f - 1.0f,
                (nmColor.g / 255.0f) * 2.0f - 1.0f,
                (nmColor.b / 255.0f) * 2.0f - 1.0f
            );
            tangentNormal.normalize();
            normal = (tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z).normalize();
        }

        float diffuse = std::max(0.0f, normal * ctx->lightDir);
        float intensity = (0.15f + 0.85f * diffuse * ctx->shadowFactor) * ctx->lightIntensity;
        intensity = std::min(1.0f, std::max(0.0f, intensity));

        outColor->r = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, texColor.r * intensity)));
        outColor->g = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, texColor.g * intensity)));
        outColor->b = static_cast<uint8_t>(std::min(255.0f, std::max(0.0f, texColor.b * intensity)));
        outColor->a = texColor.a;
    }

    JITCompiler() = default;
    ~JITCompiler() {
        if (codeBuffer) {
            VirtualFree(codeBuffer, 0, MEM_RELEASE);
        }
    }

    // Compile a ShaderProgram to dynamic x86-64 machine code
    JittedShaderFunc compile(const ShaderProgram& program) {
        if (codeBuffer) {
            VirtualFree(codeBuffer, 0, MEM_RELEASE);
            codeBuffer = nullptr;
        }

        // We allocate PAGE_EXECUTE_READWRITE memory to build the machine code.
        bufferSize = 4096;
        codeBuffer = (uint8_t*)VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!codeBuffer) return nullptr;

        std::vector<uint8_t> code;

        // The JIT stub is a simple trampoline: mov rax, <addr>; jmp rax.
        // Do NOT emit any prologue (push rbp / mov rbp,rsp) — that would push an
        // extra qword onto the stack, misaligning it by 8 bytes before jmp, which
        // causes an access violation when executeShading tries to make any call.
        // A direct jmp leaves the stack exactly as the original caller set it up.

        // mov rax, <64-bit immediate> — opcode: 48 B8 [8 bytes LE]
        uintptr_t helperAddr = (uintptr_t)&JITCompiler::executeShading;
        code.push_back(0x48); code.push_back(0xB8);
        for (int i = 0; i < 8; ++i) {
            code.push_back(static_cast<uint8_t>((helperAddr >> (i * 8)) & 0xFF));
        }
        // jmp rax — opcode: FF E0
        code.push_back(0xFF); code.push_back(0xE0);

        // Copy generated machine code bytes to virtual memory
        std::memcpy(codeBuffer, code.data(), code.size());

        // Change memory protection to execute-only (or read+execute) to secure target JIT pages
        DWORD oldProtect;
        VirtualProtect(codeBuffer, bufferSize, PAGE_EXECUTE_READWRITE, &oldProtect);

        return reinterpret_cast<JittedShaderFunc>(codeBuffer);
    }
};
