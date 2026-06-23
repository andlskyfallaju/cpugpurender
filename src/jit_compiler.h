#pragma once
#include <vector>
#include <cstdint>
#include <cstring>
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
    // Helper function to sample texture from JIT assembly
    static uint32_t sampleTextureHelper(const Texture* tex, float u, float v) {
        Color c;
        if (tex) c = tex->sample(u, v);
        else c = Color(180, 180, 180, 255);

        uint32_t packed;
        std::memcpy(&packed, &c, 4);
        return packed;
    }

    // Helper function to calculate perturbed normal and return diffuse dot product
    static float dotProductHelper(const PixelContext* ctx) {
        if (!ctx) return 0.0f;
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
        return std::max(0.0f, normal * ctx->lightDir);
    }

    // Helper function to write final shading results to output Color
    static void writeColorHelper(uint32_t texColorPacked, float diffuseFactor, const PixelContext* ctx, Color* outColor) {
        if (!ctx || !outColor) return;
        Color texColor;
        std::memcpy(&texColor, &texColorPacked, 4);

        float intensity = (0.15f + 0.85f * diffuseFactor * ctx->shadowFactor) * ctx->lightIntensity;
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

        bufferSize = 4096;
        codeBuffer = (uint8_t*)VirtualAlloc(NULL, bufferSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (!codeBuffer) return nullptr;

        std::vector<uint8_t> code;

        // --- Prologue ---
        code.push_back(0x55); // push rbp
        code.push_back(0x48); code.push_back(0x89); code.push_back(0xE5); // mov rbp, rsp
        code.push_back(0x48); code.push_back(0x83); code.push_back(0xEC); code.push_back(0x60); // sub rsp, 96
        code.push_back(0x48); code.push_back(0x89); code.push_back(0x4D); code.push_back(0xF8); // mov [rbp - 8], rcx
        code.push_back(0x48); code.push_back(0x89); code.push_back(0x55); code.push_back(0xF0); // mov [rbp - 16], rdx

        // --- ShaderIR Translation Loop ---
        for (const auto& instr : program) {
            if (instr.op == Op::SampleTex) {
                // mov rax, [rbp - 8]
                code.push_back(0x48); code.push_back(0x8B); code.push_back(0x45); code.push_back(0xF8);

                // mov rcx, [rax + 32]
                code.push_back(0x48); code.push_back(0x8B); code.push_back(0x48); code.push_back(0x20);
                // movss xmm1, [rax]
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x08);
                // movss xmm2, [rax + 4]
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x50); code.push_back(0x04);
                
                // mov rax, sampleTextureHelper
                uintptr_t addr = (uintptr_t)&JITCompiler::sampleTextureHelper;
                code.push_back(0x48); code.push_back(0xB8);
                for (int i = 0; i < 8; ++i) code.push_back(static_cast<uint8_t>((addr >> (i * 8)) & 0xFF));
                
                // call rax
                code.push_back(0xFF); code.push_back(0xD0);
                // mov [rbp - 36], eax
                code.push_back(0x89); code.push_back(0x45); code.push_back(0xDC);
            }
            else if (instr.op == Op::DotV3) {
                // mov rcx, [rbp - 8]
                code.push_back(0x48); code.push_back(0x8B); code.push_back(0x4D); code.push_back(0xF8);
                
                // mov rax, dotProductHelper
                uintptr_t addr = (uintptr_t)&JITCompiler::dotProductHelper;
                code.push_back(0x48); code.push_back(0xB8);
                for (int i = 0; i < 8; ++i) code.push_back(static_cast<uint8_t>((addr >> (i * 8)) & 0xFF));
                
                // call rax
                code.push_back(0xFF); code.push_back(0xD0);
                // movss [rbp - 32 + dst * 4], xmm0
                int offset = -32 + instr.dst * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x11); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset & 0xFF));
            }
            else if (instr.op == Op::MulF) {
                // movss xmm0, [rbp - 32 + src0 * 4]
                int offset0 = -32 + instr.src0 * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset0 & 0xFF));
                // mulss xmm0, [rbp - 32 + src1 * 4]
                int offset1 = -32 + instr.src1 * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x59); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset1 & 0xFF));
                // movss [rbp - 32 + dst * 4], xmm0
                int offsetDst = -32 + instr.dst * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x11); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offsetDst & 0xFF));
            }
            else if (instr.op == Op::AddF) {
                // movss xmm0, [rbp - 32 + src0 * 4]
                int offset0 = -32 + instr.src0 * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset0 & 0xFF));
                // addss xmm0, [rbp - 32 + src1 * 4]
                int offset1 = -32 + instr.src1 * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x58); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset1 & 0xFF));
                // movss [rbp - 32 + dst * 4], xmm0
                int offsetDst = -32 + instr.dst * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x11); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offsetDst & 0xFF));
            }
            else if (instr.op == Op::ClampF) {
                // movss xmm0, [rbp - 32 + src0 * 4]
                int offset0 = -32 + instr.src0 * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offset0 & 0xFF));
                
                // mov eax, dword ptr [imm]
                uint32_t immVal;
                std::memcpy(&immVal, &instr.imm, 4);
                code.push_back(0xB8);
                for (int i = 0; i < 4; ++i) code.push_back(static_cast<uint8_t>((immVal >> (i * 8)) & 0xFF));
                // movd xmm1, eax
                code.push_back(0x66); code.push_back(0x0F); code.push_back(0x6E); code.push_back(0xC8);
                // maxss xmm0, xmm1
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x5F); code.push_back(0xC1);

                // mov eax, 0x3f800000 (1.0f)
                code.push_back(0xB8); code.push_back(0x00); code.push_back(0x00); code.push_back(0x80); code.push_back(0x3F);
                // movd xmm1, eax
                code.push_back(0x66); code.push_back(0x0F); code.push_back(0x6E); code.push_back(0xC8);
                // minss xmm0, xmm1
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x5D); code.push_back(0xC1);

                // movss [rbp - 32 + dst * 4], xmm0
                int offsetDst = -32 + instr.dst * 4;
                code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x11); code.push_back(0x45);
                code.push_back(static_cast<uint8_t>(offsetDst & 0xFF));
            }
        }

        // --- Epilogue & Write Back ---
        // mov ecx, [rbp - 36] (texColor)
        code.push_back(0x8B); code.push_back(0x4D); code.push_back(0xDC);
        // movss xmm1, [rbp - 20] (regs[3] which contains clamped diffuseFactor)
        code.push_back(0xF3); code.push_back(0x0F); code.push_back(0x10); code.push_back(0x4D); code.push_back(0xEC);
        // mov r8, [rbp - 8] (ctx)
        code.push_back(0x4C); code.push_back(0x8B); code.push_back(0x45); code.push_back(0xF8);
        // mov r9, [rbp - 16] (outColor)
        code.push_back(0x4C); code.push_back(0x8B); code.push_back(0x4D); code.push_back(0xF0);

        // mov rax, writeColorHelper
        uintptr_t writeAddr = (uintptr_t)&JITCompiler::writeColorHelper;
        code.push_back(0x48); code.push_back(0xB8);
        for (int i = 0; i < 8; ++i) code.push_back(static_cast<uint8_t>((writeAddr >> (i * 8)) & 0xFF));
        // call rax
        code.push_back(0xFF); code.push_back(0xD0);

        // add rsp, 96
        code.push_back(0x48); code.push_back(0x83); code.push_back(0xC4); code.push_back(0x60);
        // pop rbp
        code.push_back(0x5D);
        // ret
        code.push_back(0xC3);

        std::memcpy(codeBuffer, code.data(), code.size());

        DWORD oldProtect;
        VirtualProtect(codeBuffer, bufferSize, PAGE_EXECUTE_READWRITE, &oldProtect);
        FlushInstructionCache(GetCurrentProcess(), codeBuffer, bufferSize);

        return reinterpret_cast<JittedShaderFunc>(codeBuffer);
    }
};
