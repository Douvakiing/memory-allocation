#pragma once

#include <map>
#include <string>
#include <vector>

using namespace std;

struct SegmentInput {
    string name;
    int size = 1;
};

struct SegmentAllocation {
    string processName;
    string segmentName;
    int start = 0;
    int size = 0;
};

struct Hole {
    int start = 0;
    int size = 0;
};

enum class AllocationMethod {
    FirstFit = 0,
    BestFit = 1
};

class MemoryManager {
private:
    void MergeHoles();
    int FindHoleForSegment(int segmentSize, AllocationMethod method) const;

    int totalMemory = 0;
    vector<Hole> holes;
    vector<SegmentAllocation> allocatedSegments;
    map<string, vector<SegmentAllocation>> processTables;
public:
    bool Initialize(int totalMemorySize, string& message);
    bool AddHole(int start, int size, string& message);
    bool IsInitialized() const;
    int TotalMemory() const;
    const vector<Hole>& Holes() const;
    const vector<SegmentAllocation>& AllocatedSegments() const;
    const map<string, vector<SegmentAllocation>>& ProcessTables() const;
    bool AllocateProcess(
        const string& processName,
        const vector<SegmentInput>& segments,
        AllocationMethod method,
        string& message);
    bool DeallocateProcess(const string& processName, string& message);
};
