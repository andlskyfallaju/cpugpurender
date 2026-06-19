#pragma once
#include <atomic>
#include <chrono>

struct Telemetry {
    std::atomic<uint64_t> trianglesTested{0};
    std::atomic<uint64_t> earlyZRejections{0};
    std::atomic<uint64_t> pixelsShaded{0};
    std::atomic<uint64_t> hiZTileCulls{0};  // Tiles skipped by Hi-Z coarse test

    void reset() {
        trianglesTested = 0;
        earlyZRejections = 0;
        pixelsShaded = 0;
        hiZTileCulls = 0;
    }
};

struct Profiler {
    float geometryTimeMs = 0.0f;
    float binningTimeMs = 0.0f;
    float rasterizationTimeMs = 0.0f;
    float postProcessTimeMs = 0.0f;

    Telemetry telemetry;

    void reset() {
        geometryTimeMs = 0.0f;
        binningTimeMs = 0.0f;
        rasterizationTimeMs = 0.0f;
        postProcessTimeMs = 0.0f;
        telemetry.reset();
    }
};
