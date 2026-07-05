param(
    [string]$Version = "v5.4.3",
    [string]$Generator = "Visual Studio 18 2026",
    [string]$Architecture = "x64",
    [switch]$BuildRelease
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $root "external\assimp-src"
$buildDir = Join-Path $root "external\assimp-build"

if (-not (Test-Path -LiteralPath $sourceDir)) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sourceDir) | Out-Null
    git clone --depth 1 --branch $Version https://github.com/assimp/assimp.git $sourceDir
}

cmake `
    -S $sourceDir `
    -B $buildDir `
    -G $Generator `
    -A $Architecture `
    -DASSIMP_BUILD_TESTS=OFF `
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF `
    -DASSIMP_INSTALL=OFF `
    -DASSIMP_WARNINGS_AS_ERRORS=OFF `
    -DBUILD_SHARED_LIBS=OFF

cmake --build $buildDir --config Debug --target assimp

if ($BuildRelease) {
    cmake --build $buildDir --config Release --target assimp
}
