#pragma once
#define NOMINMAX
#include <windows.h>

class HybridDetector {
private:
    DWORD_PTR pCoreMask = 0;
    DWORD_PTR eCoreMask = 0;
    bool isHybrid = false;

    void detect();

public:
    HybridDetector();
    DWORD_PTR getPCoreMask() const { return pCoreMask; }
    DWORD_PTR getECoreMask() const { return eCoreMask; }
    bool isHybridArchitecture() const { return isHybrid; }
};
