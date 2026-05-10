#include "memory_manager.h"

#include <algorithm>
#include <sstream>
#include <utility>

using namespace std;

bool MemoryManager::Initialize(int totalMemorySize, string& message) {
    if (totalMemorySize <= 0) {
        message = "Total memory size must be greater than 0.";
        return false;
    }

    totalMemory = totalMemorySize;
    holes.clear();
    allocatedSegments.clear();
    processTables.clear();
    message = "Memory initialized successfully. Add holes to begin allocation.";
    return true;
}

bool MemoryManager::AddHole(int start, int size, string& message) {
    if (!IsInitialized()) {
        message = "Initialize memory first.";
        return false;
    }
    if (start < 0 || size <= 0 || start + size > totalMemory) {
        message = "Hole is out of memory bounds.";
        return false;
    }

    const int holeEnd = start + size;
    for (const Hole& existingHole : holes) {
        const int existingHoleEnd = existingHole.start + existingHole.size;
        if (start < existingHoleEnd && holeEnd > existingHole.start) {
            message = "Hole overlaps an existing hole.";
            return false;
        }
    }

    for (const SegmentAllocation& segment : allocatedSegments) {
        const int segmentEnd = segment.start + segment.size;
        if (start < segmentEnd && holeEnd > segment.start) {
            message = "Hole overlaps an allocated segment.";
            return false;
        }
    }

    holes.push_back({start, size});
    MergeHoles();
    message = "Hole added successfully.";
    return true;
}

bool MemoryManager::IsInitialized() const {
    return totalMemory > 0;
}

int MemoryManager::TotalMemory() const {
    return totalMemory;
}

const vector<Hole>& MemoryManager::Holes() const {
    return holes;
}

const vector<SegmentAllocation>& MemoryManager::AllocatedSegments() const {
    return allocatedSegments;
}

const map<string, vector<SegmentAllocation>>& MemoryManager::ProcessTables() const {
    return processTables;
}

bool MemoryManager::AllocateProcess(
    const string& processName,
    const vector<SegmentInput>& segments,
    AllocationMethod method,
    string& message) {
    if (!IsInitialized()) {
        message = "Initialize memory first.";
        return false;
    }
    if (processName.empty()) {
        message = "Process name cannot be empty.";
        return false;
    }
    if (processTables.find(processName) != processTables.end()) {
        message = "Process name already exists. Use a unique name.";
        return false;
    }
    if (segments.empty()) {
        message = "Add at least one segment.";
        return false;
    }
    for (const SegmentInput& segment : segments) {
        if (segment.name.empty()) {
            message = "Each segment must have a name.";
            return false;
        }
        if (segment.size <= 0) {
            message = "Segment size must be greater than 0.";
            return false;
        }
    }

    vector<SegmentAllocation> allocatedForProcess;
    allocatedForProcess.reserve(segments.size());

    for (const SegmentInput& segment : segments) {
        const int holeIndex = FindHoleForSegment(segment.size, method);
        if (holeIndex < 0) {
            for (const SegmentAllocation& rollback : allocatedForProcess) {
                holes.push_back({rollback.start, rollback.size});
            }
            MergeHoles();

            ostringstream oss;
            oss << "Process " << processName << " does not fit. Allocation rolled back.";
            message = oss.str();
            return false;
        }

        Hole& selectedHole = holes[holeIndex];
        SegmentAllocation allocation;
        allocation.processName = processName;
        allocation.segmentName = segment.name;
        allocation.size = segment.size;
        allocation.start = selectedHole.start;

        selectedHole.start += segment.size;
        selectedHole.size -= segment.size;
        if (selectedHole.size == 0) {
            holes.erase(holes.begin() + holeIndex);
        }

        allocatedForProcess.push_back(allocation);
    }

    for (const SegmentAllocation& allocation : allocatedForProcess) {
        allocatedSegments.push_back(allocation);
    }
    sort(allocatedSegments.begin(), allocatedSegments.end(), [](const SegmentAllocation& a, const SegmentAllocation& b) {
        return a.start < b.start;
    });

    processTables[processName] = move(allocatedForProcess);

    ostringstream oss;
    oss << "Allocated process " << processName << " successfully.";
    message = oss.str();
    return true;
}

bool MemoryManager::DeallocateProcess(const string& processName, string& message) {
    const auto processIt = processTables.find(processName);
    if (processIt == processTables.end()) {
        message = "Selected process was not found.";
        return false;
    }

    for (const SegmentAllocation& segment : processIt->second) {
        holes.push_back({segment.start, segment.size});
    }
    MergeHoles();

    allocatedSegments.erase(
        remove_if(allocatedSegments.begin(), allocatedSegments.end(),
                       [&](const SegmentAllocation& segment) { return segment.processName == processName; }),
        allocatedSegments.end());
    processTables.erase(processIt);

    ostringstream oss;
    oss << "Deallocated process " << processName << " and merged neighboring holes.";
    message = oss.str();
    return true;
}

void MemoryManager::MergeHoles() {
    sort(holes.begin(), holes.end(), [](const Hole& a, const Hole& b) {
        return a.start < b.start;
    });

    vector<Hole> merged;
    for (const Hole& hole : holes) {
        if (merged.empty()) {
            merged.push_back(hole);
            continue;
        }

        Hole& back = merged.back();
        const int backEnd = back.start + back.size;
        if (backEnd >= hole.start) {
            const int mergedEnd = max(backEnd, hole.start + hole.size);
            back.size = mergedEnd - back.start;
        } else {
            merged.push_back(hole);
        }
    }
    holes = move(merged);
}

int MemoryManager::FindHoleForSegment(int segmentSize, AllocationMethod method) const {
    if (method == AllocationMethod::FirstFit) {
        for (int i = 0; i < static_cast<int>(holes.size()); ++i) {
            if (holes[i].size >= segmentSize) {
                return i;
            }
        }
        return -1;
    }
    else if (method == AllocationMethod::BestFit) {
        int bestIndex = -1;
        int bestSize = 0;
        for (int i = 0; i < (holes.size()); ++i) {
            if (holes[i].size < segmentSize) {
                continue;
            }
            if (bestIndex < 0 || holes[i].size < bestSize) {
                bestIndex = i;
                bestSize = holes[i].size;
            }
        }
        return bestIndex;
    }
    return -1;
}
