#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "framebuffer.h"

class Texture {
private:
    int width;
    int height;
    int channels;
    std::vector<uint8_t> data;

public:
    Texture();
    ~Texture();
    bool load(const std::string& filepath);
    void createSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    Color sample(float u, float v) const;
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    bool isLoaded() const { return !data.empty(); }
};
