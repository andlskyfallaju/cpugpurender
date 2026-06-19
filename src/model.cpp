#include "model.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

Model::Model(const std::string& filepath) {
    load(filepath);
}

bool Model::loadMTL(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        std::cerr << "[Model] Warning: Could not open MTL file: " << filepath << std::endl;
        return false;
    }

    std::string line;
    Material* currentMat = nullptr;

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "newmtl") {
            materials.emplace_back();
            currentMat = &materials.back();
            iss >> currentMat->name;
        } else if (prefix == "Kd" && currentMat) {
            iss >> currentMat->Kd[0] >> currentMat->Kd[1] >> currentMat->Kd[2];
        } else if (prefix == "map_Kd" && currentMat) {
            // map_Kd may have options like -s x y z or -bm x before the filename.
            // The filename itself may contain spaces (e.g. "BOdy Skin Base Color.png").
            // Strategy: collect all tokens, skip known option flags and their numeric args,
            // then join remaining tokens (with spaces) as the filename.
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }

            // Known option → number of float args that follow it
            std::map<std::string, int> optArgs = {
                {"-s",3},{"-o",3},{"-t",3},{"-mm",2},{"-bm",1},{"-imfchan",1},
                {"-blendu",1},{"-blendv",1},{"-cc",1},{"-clamp",1},{"-texres",1}
            };
            auto isOpt = [](const std::string& s) {
                return s.size() > 1 && s[0] == '-' && std::isalpha((unsigned char)s[1]);
            };

            size_t idx = 0;
            while (idx < tokens.size()) {
                if (isOpt(tokens[idx])) {
                    auto it = optArgs.find(tokens[idx]);
                    int skip = (it != optArgs.end()) ? it->second : 0;
                    idx += 1 + (size_t)skip;
                } else {
                    // Everything from here to end of line is the filename (spaces allowed)
                    std::string filename;
                    for (size_t j = idx; j < tokens.size(); ++j) {
                        if (j > idx) filename += ' ';
                        filename += tokens[j];
                    }
                    // Normalize path separators
                    std::replace(filename.begin(), filename.end(), '\\', '/');
                    currentMat->map_Kd = filename;
                    break;
                }
            }  // end while (idx < tokens.size())
        } else if ((prefix == "map_Bump" || prefix == "bump" || prefix == "map_bump") && currentMat) {
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token) {
                tokens.push_back(token);
            }

            // Known option → number of float args that follow it
            std::map<std::string, int> optArgs = {
                {"-s",3},{"-o",3},{"-t",3},{"-mm",2},{"-bm",1},{"-imfchan",1},
                {"-blendu",1},{"-blendv",1},{"-cc",1},{"-clamp",1},{"-texres",1}
            };
            auto isOpt = [](const std::string& s) {
                return s.size() > 1 && s[0] == '-' && std::isalpha((unsigned char)s[1]);
            };

            size_t idx = 0;
            while (idx < tokens.size()) {
                if (isOpt(tokens[idx])) {
                    auto it = optArgs.find(tokens[idx]);
                    int skip = (it != optArgs.end()) ? it->second : 0;
                    idx += 1 + (size_t)skip;
                } else {
                    std::string filename;
                    for (size_t j = idx; j < tokens.size(); ++j) {
                        if (j > idx) filename += ' ';
                        filename += tokens[j];
                    }
                    std::replace(filename.begin(), filename.end(), '\\', '/');
                    currentMat->map_Bump = filename;
                    break;
                }
            }
        }  // end else if (prefix == "map_Bump")
    }  // end while (getline)

    std::cout << "[Model] Loaded MTL with " << materials.size() << " materials:" << std::endl;
    for (const auto& mat : materials) {
        std::cout << "  - " << mat.name;
        if (!mat.map_Kd.empty()) {
            std::cout << " (texture: " << mat.map_Kd << ")";
        } else {
            std::cout << " (solid Kd: " << mat.Kd[0] << ", " << mat.Kd[1] << ", " << mat.Kd[2] << ")";
        }
        std::cout << std::endl;
    }
    return true;
}

bool Model::load(const std::string& filepath) {
    // Extract base directory path
    size_t lastSlash = filepath.find_last_of("/\\");
    basePath = (lastSlash != std::string::npos) ? filepath.substr(0, lastSlash + 1) : "";

    std::ifstream in(filepath);
    if (!in.is_open()) {
        std::cerr << "Failed to open OBJ file: " << filepath << std::endl;
        return false;
    }

    // Build a name-to-index map for materials after MTL is loaded
    std::map<std::string, int> materialMap;
    int currentMaterialIndex = -1;

    std::string line;
    // First pass: find and load MTL file
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        if (prefix == "mtllib") {
            std::string mtlFilename;
            // Read rest of line (filename may contain spaces)
            std::getline(iss >> std::ws, mtlFilename);
            std::string mtlPath = basePath + mtlFilename;
            loadMTL(mtlPath);
            // Build name-to-index map
            for (int i = 0; i < (int)materials.size(); ++i) {
                materialMap[materials[i].name] = i;
            }
            break;
        }
    }

    // Reset to beginning for geometry parsing
    in.clear();
    in.seekg(0, std::ios::beg);

    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3f v;
            iss >> v.x >> v.y >> v.z;
            vertices.push_back(v);
        } else if (prefix == "vt") {
            Vec2f uv;
            iss >> uv.x >> uv.y;
            uvs.push_back(uv);
        } else if (prefix == "vn") {
            Vec3f n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "usemtl") {
            std::string matName;
            iss >> matName;
            auto it = materialMap.find(matName);
            currentMaterialIndex = (it != materialMap.end()) ? it->second : -1;
        } else if (prefix == "f") {
            // Collect all face vertex tokens (may be 3, 4, or more — triangulate as fan)
            std::vector<std::string> tokens;
            std::string tok;
            while (iss >> tok) {
                tokens.push_back(tok);
            }

            auto parseVertex = [](const std::string& str, int& vIdx, int& uvIdx, int& nIdx) {
                vIdx = 0; uvIdx = 0; nIdx = 0;
                int matched = std::sscanf(str.c_str(), "%d/%d/%d", &vIdx, &uvIdx, &nIdx);
                if (matched < 3) {
                    matched = std::sscanf(str.c_str(), "%d//%d", &vIdx, &nIdx);
                    uvIdx = 0;
                }
                if (matched < 2) {
                    std::sscanf(str.c_str(), "%d", &vIdx);
                    uvIdx = 0; nIdx = 0;
                }
                vIdx--;
                uvIdx--;
                nIdx--;
            };

            if (tokens.size() >= 3) {
                // Triangulate as a fan from vertex 0
                int v0Idx, uv0Idx, n0Idx;
                parseVertex(tokens[0], v0Idx, uv0Idx, n0Idx);

                for (size_t i = 1; i + 1 < tokens.size(); ++i) {
                    ModelFace face;
                    face.v[0] = v0Idx;   face.uv[0] = uv0Idx; face.n[0] = n0Idx;
                    parseVertex(tokens[i],     face.v[1], face.uv[1], face.n[1]);
                    parseVertex(tokens[i + 1], face.v[2], face.uv[2], face.n[2]);
                    face.materialIndex = currentMaterialIndex;
                    faces.push_back(face);
                }
            }
        }
    }

    std::cout << "[Model Loader] Loaded OBJ: " << filepath << std::endl;
    std::cout << "  Vertices: " << vertices.size() << std::endl;
    std::cout << "  UVs:      " << uvs.size() << std::endl;
    std::cout << "  Normals:  " << normals.size() << std::endl;
    std::cout << "  Faces:    " << faces.size() << std::endl;
    std::cout << "  Materials:" << materials.size() << std::endl;
    return true;
}
