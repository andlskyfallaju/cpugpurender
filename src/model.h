#pragma once
#include <vector>
#include <string>
#include <map>
#include "math_utils.h"

struct Material {
    std::string name;
    float Kd[3] = {0.8f, 0.8f, 0.8f}; // Diffuse color (default light gray)
    std::string map_Kd;                 // Diffuse texture path (relative to MTL file)
    std::string map_Bump;               // Bump/Normal texture path
};

struct ModelFace {
    int v[3];   // Vertex indices (0-based)
    int uv[3];  // UV indices (0-based)
    int n[3];   // Normal indices (0-based)
    int materialIndex = -1; // Index into the materials vector (-1 = no material)
};

class Model {
private:
    std::vector<Vec3f> vertices;
    std::vector<Vec2f> uvs;
    std::vector<Vec3f> normals;
    std::vector<ModelFace> faces;
    std::vector<Material> materials;
    std::string basePath; // Directory containing the OBJ file

    bool loadMTL(const std::string& filepath);

public:
    Model(const std::string& filepath);
    ~Model() = default;

    bool load(const std::string& filepath);

    int getNumFaces() const { return static_cast<int>(faces.size()); }
    int getNumMaterials() const { return static_cast<int>(materials.size()); }
    const std::vector<Material>& getMaterials() const { return materials; }
    const std::string& getBasePath() const { return basePath; }
    
    Vec3f getVertex(int faceIdx, int vertexIdx) const {
        return vertices[faces[faceIdx].v[vertexIdx]];
    }

    Vec2f getUV(int faceIdx, int vertexIdx) const {
        int idx = faces[faceIdx].uv[vertexIdx];
        if (idx < 0 || idx >= (int)uvs.size()) return Vec2f(0.0f, 0.0f);
        return uvs[idx];
    }

    Vec3f getNormal(int faceIdx, int vertexIdx) const {
        int idx = faces[faceIdx].n[vertexIdx];
        if (idx < 0 || idx >= (int)normals.size()) return Vec3f(0.0f, 1.0f, 0.0f);
        return normals[idx];
    }

    int getMaterialIndex(int faceIdx) const {
        return faces[faceIdx].materialIndex;
    }
};
