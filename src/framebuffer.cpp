#include "framebuffer.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <algorithm>

Framebuffer::Framebuffer(int w, int h) : width(w), height(h) {
    colorBuffer.resize(width * height);
    depthBuffer.resize(width * height);
    clear();
}

void Framebuffer::clear(const Color& clearColor, float clearDepth) {
    std::fill(colorBuffer.begin(), colorBuffer.end(), clearColor);
    std::fill(depthBuffer.begin(), depthBuffer.end(), clearDepth);
}

void Framebuffer::setPixel(int x, int y, const Color& color) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        // Flip Y vertically so that (0,0) is bottom-left (OpenGL style)
        colorBuffer[x + (height - 1 - y) * width] = color;
    }
}

Color Framebuffer::getPixel(int x, int y) const {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return colorBuffer[x + (height - 1 - y) * width];
    }
    return Color(0, 0, 0, 0);
}

void Framebuffer::setDepth(int x, int y, float depth) {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        depthBuffer[x + (height - 1 - y) * width] = depth;
    }
}

float Framebuffer::getDepth(int x, int y) const {
    if (x >= 0 && x < width && y >= 0 && y < height) {
        return depthBuffer[x + (height - 1 - y) * width];
    }
    return -1e5f;
}

bool Framebuffer::saveToPng(const std::string& filepath) const {
    stbi_flip_vertically_on_write(0); // Our setPixel already flips Y, no extra flip needed
    int success = stbi_write_png(filepath.c_str(), width, height, 4, colorBuffer.data(), width * 4);
    return success != 0;
}

// ─── Hierarchical Z-Buffer ───────────────────────────────────────────────────

void Framebuffer::initHiZ(int tileSize, float clearDepth) {
    hiZTileSize   = tileSize;
    hiZNumTilesX  = (width  + tileSize - 1) / tileSize;
    hiZNumTilesY  = (height + tileSize - 1) / tileSize;
    int total     = hiZNumTilesX * hiZNumTilesY;

    hiZBuffer = std::make_unique<std::atomic<float>[]>(total);
    for (int i = 0; i < total; ++i) {
        hiZBuffer[i].store(clearDepth, std::memory_order_relaxed);
    }
    hiZActive = true;
}

void Framebuffer::clearHiZ(float clearDepth) {
    if (!hiZActive) return;
    int total = hiZNumTilesX * hiZNumTilesY;
    for (int i = 0; i < total; ++i) {
        hiZBuffer[i].store(clearDepth, std::memory_order_relaxed);
    }
}

// Thread-safe atomic max: loop until we've stored our value or found a larger one
void Framebuffer::updateTileMaxDepth(int tileIdx, float depth) {
    auto& atom = hiZBuffer[tileIdx];
    float current = atom.load(std::memory_order_relaxed);
    while (depth > current) {
        if (atom.compare_exchange_weak(current, depth,
                                       std::memory_order_release,
                                       std::memory_order_relaxed)) {
            break;
        }
        // current was reloaded by compare_exchange_weak — loop again
    }
}

float Framebuffer::getTileMaxDepth(int tileIdx) const {
    return hiZBuffer[tileIdx].load(std::memory_order_relaxed);
}

