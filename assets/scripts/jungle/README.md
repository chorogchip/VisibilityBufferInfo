# JungleRuins Canonical Scene Pipeline

This directory owns the renderer-independent conversion contract for
JungleRuins. The original package under `assets/scenes/unpacked/JungleRuins`
is an immutable input. The scripts never save its Blend or USD files.

## Versioned and generated files

Versioned:

- `../../../docs/jungle/`: contracts, decisions, and validation notes.
- `analyze_jungle_scene.py`: source inventory and canonical manifest.
- `build_jungle_packages.py`: final four-region package build.
- `jungle_common.py`: stable IDs, cell ownership, and naming.
- `jungle_usd.py`: cached USD instancer decoding and spatial assignment.
- `jungle_blender.py`: temporary Blender hierarchy and base GLB export.
- `jungle_glb.py`: GLB container and accessor operations.
- `jungle_package.py`: metadata/instancing patch and validation.
- `validate_jungle_packages.py`: mmap source-index coverage validation.
- `jr_scene_extras.schema.json`: `extras.jr` metadata schema.
- `build_contract_probe.py` and `validate_probe_reimport.py`: scoped probe
  tools retained for regression checks.

Ignored generated data:

```text
assets/scenes/generated/jungle/
|-- reports/
|   |-- source_inventory.json
|   |-- canonical_scene_manifest.json
|   |-- package_build.json
|   `-- package_validation.json
`-- packages/
    |-- jungle_packages.json
    |-- jungle_global.glb
    |-- jungle_cinematic.glb
    |-- jungle_extended.glb
    `-- jungle_pyramid.glb
```

Generated GLBs/reports and original scene data must not be committed.

## Sources of truth

- Blender supplies hierarchy, terrain/static geometry, linked prototype
  meshes, material slots, textures, and the source camera.
- USD supplies complete baked `UsdGeomPointInstancer` transforms.
- GScatter is not installed, imported, or evaluated.
- Every USD record remains present. Exact-origin records and any non-origin
  record outside the authored ownership bounds live below a region-level
  unresolved node with an explicit reason.

## Build the inventory

Run from the repository root:

```powershell
$repo = (Get-Location).Path
$scene = Join-Path $repo 'assets\scenes\unpacked\JungleRuins'
$generated = Join-Path $repo 'assets\scenes\generated\jungle'
$blender = 'C:\Program Files\Blender Foundation\Blender 4.2\blender.exe'

& $blender --background --factory-startup `
  (Join-Path $scene 'Blender\JungleRuins_Main.blend') `
  --python (Join-Path $repo 'assets\scripts\jungle\analyze_jungle_scene.py') `
  -- `
  --scene-root $scene `
  --output (Join-Path $generated 'reports\source_inventory.json')
```

The analyzer writes `canonical_scene_manifest.json` next to the inventory.

## Build the final packages

```powershell
& $blender --background --factory-startup `
  (Join-Path $scene 'Blender\JungleRuins_Main.blend') `
  --python (Join-Path $repo 'assets\scripts\jungle\build_jungle_packages.py') `
  -- `
  --scene-root $scene `
  --manifest (Join-Path $generated 'reports\canonical_scene_manifest.json') `
  --output-dir (Join-Path $generated 'packages') `
  --report (Join-Path $generated 'reports\package_build.json')
```

Use `--regions global cinematic` to rebuild selected packages. Existing
validated entries remain in `jungle_packages.json`; omitting the option builds
all four.

Validate that every original USD source index occurs exactly once:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.2\4.2\python\bin\python.exe' `
  (Join-Path $repo 'assets\scripts\jungle\validate_jungle_packages.py') `
  --manifest (Join-Path $generated 'reports\canonical_scene_manifest.json') `
  --package-dir (Join-Path $generated 'packages') `
  --report (Join-Path $generated 'reports\package_validation.json')
```

Validate the C++ Jungle GLB-to-`SceneSourceData` path against the same package
set:

```powershell
cmake --build out/build/x64-Debug --config Debug `
  --target JungleSceneSourceValidate

& out/build/x64-Debug/bin/Debug/JungleSceneSourceValidate.exe `
  assets/scenes/generated/jungle/packages
```

This calls the public `JungleSceneSourceBuilder` for every region and checks
source-index coverage, hierarchy metadata, geometry attributes, materials,
and cameras after conversion.

## Run the renderer paths

The final global package is the smallest final artifact that reaches every
registered renderer:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 4.2\4.2\python\bin\python.exe' `
  scripts/run.py `
  assets/scripts/jungle/runtime_all_renderers.json
```

To include the compact scatter-instance path, first generate the complete
`M_3x4_01` cell probe:

```powershell
& $blender --background --factory-startup `
  (Join-Path $scene 'Blender\JungleRuins_Main.blend') `
  --python (Join-Path $repo 'assets\scripts\jungle\build_contract_probe.py') `
  -- `
  --scene-root $scene `
  --manifest (Join-Path $generated 'reports\canonical_scene_manifest.json') `
  --output (Join-Path $generated 'probes\jungle_M_3x4_01_complete_cell.glb') `
  --report (Join-Path $generated 'reports\runtime_probe_build.json') `
  --docs-output (Join-Path $generated 'reports\runtime_probe_build.md') `
  --cell M_3x4_01 `
  --species all

& 'C:\Program Files\Blender Foundation\Blender 4.2\4.2\python\bin\python.exe' `
  scripts/run.py `
  assets/scripts/jungle/runtime_instanced_all_renderers.json
```

Variants 1-9 and 11 complete on the 74,206-instance probe. Variant 10
(`RendererRasterStats`) currently fails because it allocates a realized
triangle record for every instance/triangle pair. The Jungle importer and
normal renderer paths do not depend on that diagnostic renderer's scale
limit. See `../../../docs/jungle/RUNTIME_RENDERER_PATHS.md`.

## Physical package policy

- `global`: River, Creek, linked Banyan object, and source camera.
- `cinematic`: 16 high-detail terrain cells and their systems.
- `extended`: 64 background terrain cells and their systems.
- `pyramid`: Pyramid shell and its Grass/Moss systems.

Cells and systems remain separate nodes inside each package. Shared prototype
meshes are emitted once per package and referenced through
`EXT_mesh_gpu_instancing`. This keeps cell-level culling possible without
duplicating a standalone GLB for every cell.
