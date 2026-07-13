#Requires -RunAsAdministrator

$ErrorActionPreference = "Stop"

if (-not (Get-Command winget.exe -ErrorAction SilentlyContinue)) {
    throw "Windows Package Manager (winget) is unavailable. Install App Installer from Microsoft Store, then run this script again."
}

$packages = @(
    @{ Id = "Git.Git"; Name = "Git"; Override = $null },
    @{ Id = "Kitware.CMake"; Name = "CMake"; Override = $null },
    @{ Id = "EclipseAdoptium.Temurin.21.JRE"; Name = "Java 21 runtime"; Override = $null },
    @{
        Id = "Microsoft.VisualStudio.2022.BuildTools"
        Name = "Visual Studio 2022 C++ Build Tools"
        Override = "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    }
)

winget source update
foreach ($package in $packages) {
    Write-Host "Installing $($package.Name)..." -ForegroundColor Cyan
    $arguments = @(
        "install", "--id", $package.Id, "--exact",
        "--accept-package-agreements", "--accept-source-agreements",
        "--silent", "--disable-interactivity"
    )
    if ($package.Override) {
        $arguments += @("--override", $package.Override)
    }
    & winget.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "winget could not install $($package.Name) (exit $LASTEXITCODE)."
    }
}

Write-Host ""
Write-Host "Core tools are installed." -ForegroundColor Green
Write-Host "Next, install Qt 6 with the MSVC 2022 64-bit component from:"
Write-Host "https://www.qt.io/download-qt-installer-oss"
Write-Host ""
Write-Host "After Qt finishes, restart Windows and run build-windows.ps1 from this folder."
