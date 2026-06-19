#include "texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <algorithm>
#include <string>

// Windows-specific: find actual file on disk matching path case-insensitively
// Returns the corrected path if found, otherwise returns original.
#define NOMINMAX
#include <windows.h>
static std::string findFileCaseInsensitive(const std::string& filepath) {
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(filepath.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        FindClose(h);
        return filepath; // Exact match exists
    }

    // Extract directory and filename
    size_t lastSlash = filepath.find_last_of("/\\");
    if (lastSlash == std::string::npos) return filepath;
    std::string dir  = filepath.substr(0, lastSlash);
    std::string name = filepath.substr(lastSlash + 1);

    // Search directory for case-insensitive match
    std::string pattern = dir + "\\*";
    h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return filepath;

    std::string nameLower = name;
    std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

    do {
        std::string candidate = fd.cFileName;
        std::string candidateLower = candidate;
        std::transform(candidateLower.begin(), candidateLower.end(), candidateLower.begin(), ::tolower);

        // Match on lowercase name, or on prefix (for typos like missing "apha" suffix)
        if (candidateLower == nameLower ||
            (candidateLower.find(nameLower.substr(0, nameLower.find_last_of('.'))) != std::string::npos &&
             candidateLower.find(nameLower.substr(nameLower.find_last_of('.'))) != std::string::npos)) {
            FindClose(h);
            return dir + "/" + candidate;
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return filepath; // No match found
}

Texture::Texture() : width(0), height(0), channels(0) {}

Texture::~Texture() {}

bool Texture::load(const std::string& filepath) {
    // First try exact path, then try case-insensitive search
    std::string resolvedPath = findFileCaseInsensitive(filepath);

    int w, h, c;
    unsigned char* rawData = stbi_load(resolvedPath.c_str(), &w, &h, &c, 4);
    if (!rawData && resolvedPath != filepath) {
        // Fallback to original path just in case
        rawData = stbi_load(filepath.c_str(), &w, &h, &c, 4);
        resolvedPath = filepath;
    }
    if (!rawData) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return false;
    }
    if (resolvedPath != filepath) {
        std::cout << "  [Resolved] " << filepath << " -> " << resolvedPath << std::endl;
    }
    width = w;
    height = h;
    channels = 4;
    data.assign(rawData, rawData + (width * height * 4));
    stbi_image_free(rawData);
    return true;
}

void Texture::createSolid(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    width = 1;
    height = 1;
    channels = 4;
    data = { r, g, b, a };
}

Color Texture::sample(float u, float v) const {
    if (data.empty()) return Color(255, 0, 255, 255); // Magenta fallback

    // Clamp coordinates to [0.0, 1.0] using min/max (MSVC compatible)
    u = std::min(1.0f, std::max(0.0f, u));
    v = std::min(1.0f, std::max(0.0f, v));

    // Map normalized coordinates to pixel indices (flip V vertically for standard UV space)
    int x = std::min(width  - 1, std::max(0, static_cast<int>(u * (width  - 1))));
    int y = std::min(height - 1, std::max(0, static_cast<int>((1.0f - v) * (height - 1))));

    int idx = (x + y * width) * 4;
    return Color(data[idx], data[idx + 1], data[idx + 2], data[idx + 3]);
}
