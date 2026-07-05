# Assimp Scene Import and Fingerprint Plan

## Scope

This ports only two DX12Lab spike features into VisibilityBufferInfo:

- import real model files through Assimp into a new CPU-only `ImportedScene` structure
- extract scene fingerprint CSV data through a standalone `SceneFingerprint` console tool

The four renderer algorithms and their actual rendering passes are intentionally not ported here.

## Existing Scene Code

The existing synthetic scene and `MeshGeometry` path stays intact. Imported model data uses new types:

- `ImportedScene`
- `ImportedSceneVertex`
- `ImportedSceneMesh`
- `ImportedSceneMaterial`
- `SceneAssimpImporter`

This keeps the controlled synthetic sphere path separate from real-scene ingestion.

## Assimp Setup

Assimp is cloned locally under:

- `external/assimp-src`

The generated Visual Studio build lives under:

- `external/assimp-build`

Configured command:

```powershell
powershell -ExecutionPolicy Bypass -File tools\bootstrap_assimp.ps1 -BuildRelease
```

Equivalent manual commands:

```powershell
git clone --depth 1 --branch v5.4.3 https://github.com/assimp/assimp.git external\assimp-src
cmake -S external\assimp-src -B external\assimp-build -G "Visual Studio 18 2026" -A x64 -DASSIMP_BUILD_TESTS=OFF -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DASSIMP_INSTALL=OFF -DASSIMP_WARNINGS_AS_ERRORS=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build external\assimp-build --config Debug --target assimp
cmake --build external\assimp-build --config Release --target assimp
```

The project files reference:

- `external\assimp-src\include`
- `external\assimp-build\include`
- `external\assimp-build\lib\$(Configuration)`
- `external\assimp-build\contrib\zlib\$(Configuration)`

This setup is x64-focused because Assimp was generated with `-A x64`.

## Importer Behavior

`load_imported_scene_with_assimp(path)` uses:

- triangulate
- join identical vertices
- generate smooth normals
- improve cache locality
- pre-transform vertices
- convert to left-handed
- sort by primitive type

It captures positions, normals, uv0, material colors, material texture paths, mesh draw ranges, indices, and bounds.

## Fingerprint Tool

`SceneFingerprint` is a standalone console project in the solution. It can run without creating a D3D12 device or window.

Example:

```powershell
x64\Debug\SceneFingerprint.exe --output docs\scene_fingerprint_sample.csv <model paths...>
```

Supported inputs:

- explicit model file paths
- `--inventory <path>`
- `--all-inventory-paths`
- `--output <csv path>`

The CSV includes geometry, material, texture, and cheap overdraw-related proxy metrics.

## Verification

Verified:

- `SceneFingerprint` Debug x64 builds through `TVBPerf.sln`
- `TVBPerf` Debug x64 builds with the new Assimp importer files included
- `SceneFingerprint` Release x64 builds after Assimp Release is built
- sample fingerprint extraction works for CornellBox and bunny from the DX12Lab scene payload

Not included:

- renderer pass integration
- visual correctness checks
- exact GPU overdraw
- pass timestamp metrics
