# Memory Allocation Project 2026 (Dear ImGui + CMake)

Desktop implementation of the segmentation memory-allocation assignment using:

- Dear ImGui UI
- First-Fit and Best-Fit algorithms
- Holes table + allocated partitions table updates after every operation
- Process segment tables
- Memory layout drawing after allocation/deallocation

## Features Implemented

- Input total memory size
- Input initial holes (start + size)
- Allocate process segments one-by-one with:
  - First Fit
  - Best Fit
- Rollback if any process segment does not fit (entire process fails)
- Deallocate selected process and merge neighboring holes
- View:
  - Current holes table
  - Allocated partitions table
  - Segment table for each process
  - Graphical memory layout

## Build and Run (PowerShell, Windows)

From project root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
.\build\Release\memory_allocation_imgui.exe
```

## One-command script (MSVC)

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1
```

Useful options:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1 -NoRun
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1 -Reconfigure
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1 -Clean
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1 -Config Debug
powershell -ExecutionPolicy Bypass -File .\scripts\run-gui.ps1 -Config RelWithDebInfo
```

