param(
    [string]$QtRoot = "",
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $ProjectDir "build"
$DistDir = Join-Path $ProjectDir "dist"

if (-not $QtRoot) {
    $qtCandidates = @(
        Get-ChildItem "C:\Qt\*\msvc2022_64\bin\qmake.exe" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending
    )
    if ($qtCandidates.Count -eq 0) {
        throw "Qt was not found. Install a Qt 6 MSVC 2022 64-bit kit, or run: .\build-windows.ps1 -QtRoot C:\Qt\<version>\msvc2022_64"
    }
    $QtRoot = Split-Path -Parent (Split-Path -Parent $qtCandidates[0].FullName)
}

$QtRoot = (Resolve-Path $QtRoot).Path
$DeployTool = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $DeployTool)) {
    throw "windeployqt.exe was not found under $QtRoot. Select the MSVC 2022 64-bit Qt kit."
}

Write-Host "Configuring with Qt at $QtRoot" -ForegroundColor Cyan
cmake -S $ProjectDir -B $BuildDir -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed." }

cmake --build $BuildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Compilation failed." }

$Executable = Join-Path $BuildDir "$Configuration\solution-finder-enhanced.exe"
if (-not (Test-Path $Executable)) {
    throw "The compiled executable was not found at $Executable."
}

& $DeployTool --release --compiler-runtime $Executable
if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed." }

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$Archive = Join-Path $DistDir "Solution-Finder-Enhanced-Windows-x64.zip"
if (Test-Path $Archive) {
    Remove-Item $Archive -Force
}
Compress-Archive -Path (Join-Path $BuildDir "$Configuration\*") -DestinationPath $Archive -CompressionLevel Optimal

Write-Host ""
Write-Host "Build complete:" -ForegroundColor Green
Write-Host $Executable
Write-Host "Portable test bundle:"
Write-Host $Archive
