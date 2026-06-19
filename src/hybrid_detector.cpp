#include "hybrid_detector.h"
#include <iostream>
#include <vector>

HybridDetector::HybridDetector() {
    detect();
}

void HybridDetector::detect() {
    DWORD length = 0;
    // Call first to get required buffer length
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length) && 
        GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        
        std::vector<char> buffer(length);
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buffer.data()), &length)) {
            
            char* ptr = buffer.data();
            char* end = ptr + length;
            
            int maxEfficiency = 0;
            int minEfficiency = 1000;
            
            struct CoreInfo {
                DWORD_PTR mask;
                int efficiency;
            };
            std::vector<CoreInfo> cores;

            while (ptr < end) {
                auto info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(ptr);
                if (info->Relationship == RelationProcessorCore) {
                    DWORD_PTR mask = info->Processor.GroupMask[0].Mask;
                    int efficiency = info->Processor.EfficiencyClass;
                    
                    cores.push_back({mask, efficiency});
                    
                    if (efficiency > maxEfficiency) maxEfficiency = efficiency;
                    if (efficiency < minEfficiency) minEfficiency = efficiency;
                }
                ptr += info->Size;
            }

            if (maxEfficiency > minEfficiency && minEfficiency != 1000) {
                isHybrid = true;
                for (const auto& core : cores) {
                    if (core.efficiency == maxEfficiency) {
                        pCoreMask |= core.mask;
                    } else {
                        eCoreMask |= core.mask;
                    }
                }
            } else {
                DWORD_PTR processAffinity = 0;
                DWORD_PTR systemAffinity = 0;
                if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity)) {
                    pCoreMask = processAffinity;
                    eCoreMask = processAffinity;
                }
            }
        }
    }
    
    if (pCoreMask == 0) {
        DWORD_PTR processAffinity = 0;
        DWORD_PTR systemAffinity = 0;
        if (GetProcessAffinityMask(GetCurrentProcess(), &processAffinity, &systemAffinity)) {
            pCoreMask = processAffinity;
            eCoreMask = processAffinity;
        }
    }
}
