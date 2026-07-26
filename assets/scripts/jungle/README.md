# JungleRuins Canonical Scene Pipeline

This directory owns the renderer-independent conversion contract for the
JungleRuins scene. The source package is treated as immutable.

## Directory layout

- `../../../docs/jungle/SCENE_CONTRACT.md`: canonical hierarchy, identity, provenance, and
  non-goals.
- `../../../docs/jungle/DECISIONS.md`: append-only design decisions.
- `../../../docs/jungle/SOURCE_INVENTORY.md`: generated human-readable source summary.
- `../../../docs/jungle/CANONICAL_SCENE_MANIFEST.md`: generated canonical mapping summary.
- `build_contract_probe.py`: builds a scoped GLB with Blender
  geometry/materials and USD instance accessors.
- `validate_probe_reimport.py`: re-imports the probe in Blender and
  records consumer behavior and bounds.
- `jr_scene_extras.schema.json`: schema for the `extras.jr` metadata
  attached to glTF entities.
- `analyze_jungle_scene.py`: Blender 4.2 inventory generator.

Generated GLB, JSON inventory, and copied scene files belong in an ignored
external working directory such as `visbufscene`; they are not repository
assets.

## Source of truth

The pipeline uses both authoring representations:

- Blender is the source for collections, terrain cells, GScatter system names,
  linked prototype assets, and Blender materials.
- USD is the source for the complete assembled scene and baked
  `UsdGeomPointInstancer` transforms.

Conflicts are recorded; they are not silently resolved.

## Generate the inventory

Run with Blender 4.2:

```powershell
$repoRoot = (Resolve-Path '..\..\..').Path
$sceneWork = 'C:\path\to\visbufscene'

& 'C:\Program Files\Blender Foundation\Blender 4.2\blender.exe' `
  --background --factory-startup `
  "$sceneWork\JungleRuins\Blender\JungleRuins_Main.blend" `
  --python "$repoRoot\assets\scripts\jungle\analyze_jungle_scene.py" `
  -- `
  --scene-root "$sceneWork\JungleRuins" `
  --output "$sceneWork\reports\source_inventory.json"
```

The generated JSON is deterministic except for `generated_utc`.
