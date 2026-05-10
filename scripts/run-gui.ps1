param(
    [string]$BuildDir = "build",
    [string]$Config = "Release",
    [switch]$Reconfigure,
    [switch]$Clean,
    [switch]$NoRun
)

$ErrorActionPreference = "Stop"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "==> Removing build directory: $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

$cachePath = Join-Path $BuildDir "CMakeCache.txt"
$needsConfigure = $Reconfigure -or !(Test-Path $cachePath)
if ($needsConfigure) {
    Write-Host "==> Configuring project with MSVC (Visual Studio 2022)..."
    cmake -S . -B $BuildDir -G "Visual Studio 17 2022" -A x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE."
    }
}
else {
    Write-Host "==> Reusing existing CMake cache in $BuildDir (use -Reconfigure to force configure)."
}

Write-Host "==> Building ($Config)..."
cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

$exePath = Join-Path $BuildDir "$Config\memory_allocation_imgui.exe"
if (!(Test-Path $exePath)) {
    throw "Executable not found at: $exePath"
}

if ($NoRun) {
    Write-Host "Build complete. Skipping run because -NoRun was supplied."
    Write-Host "Executable: $exePath"
    exit 0
}

Write-Host "==> Launching GUI..."
Start-Process -FilePath $exePath
Write-Host "Launched: $exePath"
