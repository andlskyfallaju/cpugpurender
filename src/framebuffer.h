#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <atomic>
#include <limits>
#include <memory>

struct Color {
    uint8_t r, g, b, a;
    Color() : r(0), g(0), b(0), a(255) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}
};

class Framebuffer {
private:
    int width;
    int height;
    std::vector<Color> colorBuffer;
    std::vector<float> depthBuffer;

    // Hierarchical Z-Buffer (Hi-Z) — one max-depth entry per tile
    int hiZTileSize = 0;
    int hiZNumTilesX = 0;
    int hiZNumTilesY = 0;
    std::unique_ptr<std::atomic<float>[]> hiZBuffer; // max depth written per tile
    bool hiZActive = false;

    // MSAA support
    bool msaaActive = false;
    std::vector<Color> msaaColorBuffer;
    std::vector<float> msaaDepthBuffer;

public:
    Framebuffer(int w, int h);
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    void clear(const Color& clearColor = Color(0, 0, 0, 255), float clearDepth = -1e5f);
    void setPixel(int x, int y, const Color& color);
    Color getPixel(int x, int y) const;
    void setDepth(int x, int y, float depth);
    float getDepth(int x, int y) const;
    bool saveToPng(const std::string& filepath) const;
    const Color* getColorData() const { return colorBuffer.data(); }

    // MSAA interface
    bool isMsaaActive() const { return msaaActive; }
    void setMsaaActive(bool active);
    void setMsaaPixel(int x, int y, int sampleIdx, const Color& color);
    void setMsaaDepth(int x, int y, int sampleIdx, float depth);
    float getMsaaDepth(int x, int y, int sampleIdx) const;
    void resolveMSAA();

    // Hi-Z interface
    void initHiZ(int tileSize, float clearDepth = -1e5f);
    void clearHiZ(float clearDepth = -1e5f);
    // Atomically update tile's max depth (thread-safe)
    void updateTileMaxDepth(int tileIdx, float depth);
    float getTileMaxDepth(int tileIdx) const;
    bool isHiZActive() const { return hiZActive; }
    int getHiZTileSize() const { return hiZTileSize; }
    int getHiZNumTilesX() const { return hiZNumTilesX; }
};
