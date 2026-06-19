#include "tile_renderer.h"
#include <thread>
#include <vector>
#include <atomic>
#include <iostream>
#include <algorithm>

void TileRenderer::renderParallel(Framebuffer& fb,
                               const std::vector<RenderTask>& tasks,
                               const Vec3f& lightDir, float lightIntensity,
                               bool useSIMD,
                               Telemetry* telemetry,
                               int tileSize,
                               DWORD_PTR workerAffinityMask) {
    int width = fb.getWidth();
    int height = fb.getHeight();

    int numTilesX = (width + tileSize - 1) / tileSize;
    int numTilesY = (height + tileSize - 1) / tileSize;
    int totalTiles = numTilesX * numTilesY;

    // Phase 1: Tile Binning (Triangle Culling) on the main thread
    std::vector<std::vector<int>> tileBins(totalTiles);
    for (int i = 0; i < (int)tasks.size(); ++i) {
        const auto& task = tasks[i];
        
        // Find screen-space bounding box
        float minX = std::min({task.v0.x, task.v1.x, task.v2.x});
        float maxX = std::max({task.v0.x, task.v1.x, task.v2.x});
        float minY = std::min({task.v0.y, task.v1.y, task.v2.y});
        float maxY = std::max({task.v0.y, task.v1.y, task.v2.y});

        // Convert to tile ranges
        int startTileX = std::max(0, static_cast<int>(std::floor(minX)) / tileSize);
        int endTileX = std::min(numTilesX - 1, static_cast<int>(std::ceil(maxX)) / tileSize);
        int startTileY = std::max(0, static_cast<int>(std::floor(minY)) / tileSize);
        int endTileY = std::min(numTilesY - 1, static_cast<int>(std::ceil(maxY)) / tileSize);

        for (int ty = startTileY; ty <= endTileY; ++ty) {
            for (int tx = startTileX; tx <= endTileX; ++tx) {
                int tileIdx = tx + ty * numTilesX;
                tileBins[tileIdx].push_back(i);
            }
        }
    }

    struct Tile {
        int tx, ty;
    };
    std::vector<Tile> tiles;
    tiles.reserve(totalTiles);
    for (int ty = 0; ty < numTilesY; ++ty) {
        for (int tx = 0; tx < numTilesX; ++tx) {
            tiles.push_back({tx, ty});
        }
    }

    std::atomic<size_t> nextTileIndex(0);

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    const bool hiZEnabled = fb.isHiZActive();

    for (unsigned int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, workerAffinityMask]() {
            if (workerAffinityMask != 0) {
                SetThreadAffinityMask(GetCurrentThread(), workerAffinityMask);
            }

            while (true) {
                size_t idx = nextTileIndex.fetch_add(1, std::memory_order_relaxed);
                if (idx >= (size_t)totalTiles) {
                    break;
                }

                Tile tile = tiles[idx];
                int tileIdx = tile.tx + tile.ty * numTilesX;
                const auto& bin = tileBins[tileIdx];

                int tileMinX = tile.tx * tileSize;
                int tileMaxX = std::min(width - 1, tileMinX + tileSize - 1);
                int tileMinY = tile.ty * tileSize;
                int tileMaxY = std::min(height - 1, tileMinY + tileSize - 1);

                // ── Hi-Z: read this tile's max depth from the PREVIOUS frame ──────
                float tileHiZMax = hiZEnabled ? fb.getTileMaxDepth(tileIdx) : -1e10f;

                for (int taskIdx : bin) {
                    const auto& task = tasks[taskIdx];

                    // ── Hi-Z coarse cull ─────────────────────────────────────────
                    if (hiZEnabled) {
                        float triMaxZ = std::max({task.v0.z, task.v1.z, task.v2.z});
                        if (triMaxZ < tileHiZMax) {
                            // Triangle's closest vertex is behind the tile's nearest
                            // recorded surface — conservatively skip this tile.
                            if (telemetry) telemetry->hiZTileCulls.fetch_add(1, std::memory_order_relaxed);
                            continue;
                        }
                    }

                    // Use per-task texture (never null — we always assign a valid texture)
                    const Texture& tex = *task.texture;
                    if (useSIMD) {
                        Rasterizer::drawTriangleTexturedShadedSIMD(
                            task.v0, task.v1, task.v2,
                            task.uv0, task.uv1, task.uv2,
                            task.n0, task.n1, task.n2,
                            lightDir, lightIntensity,
                            tex, fb,
                            tileMinX, tileMaxX, tileMinY, tileMaxY,
                            telemetry,
                            task.worldPos0, task.worldPos1, task.worldPos2,
                            task.shadowMap,
                            task.t0, task.t1, task.t2,
                            task.normalMap,
                            task.jittedShader
                        );
                    } else {
                        Rasterizer::drawTriangleTexturedShadedTile(
                            task.v0, task.v1, task.v2,
                            task.uv0, task.uv1, task.uv2,
                            task.n0, task.n1, task.n2,
                            lightDir, lightIntensity,
                            tex, fb,
                            tileMinX, tileMaxX, tileMinY, tileMaxY,
                            telemetry,
                            task.worldPos0, task.worldPos1, task.worldPos2,
                            task.shadowMap,
                            task.t0, task.t1, task.t2,
                            task.normalMap,
                            task.jittedShader
                        );
                    }
                }

                // ── Hi-Z: rebuild this tile's max-depth entry for the NEXT frame ─
                if (hiZEnabled) {
                    float maxDepth = -1e10f;
                    for (int py = tileMinY; py <= tileMaxY; ++py) {
                        for (int px = tileMinX; px <= tileMaxX; ++px) {
                            float d = fb.getDepth(px, py);
                            if (d > maxDepth) maxDepth = d;
                        }
                    }
                    fb.updateTileMaxDepth(tileIdx, maxDepth);
                }
            }
        });
    }

    for (auto& th : threads) {
        if (th.joinable()) {
            th.join();
        }
    }
}
