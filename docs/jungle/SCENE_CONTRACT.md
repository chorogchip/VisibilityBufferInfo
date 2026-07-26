# JungleRuins Canonical Scene Contract

Status: Draft 0.1

## 1. Purpose

The canonical JungleRuins scene describes the scene as authored. It must not
encode assumptions from a particular renderer, visibility algorithm, draw
batcher, descriptor layout, or material implementation.

Consumers depend on this scene contract. The scene contract does not depend on
a consumer.

## 2. Authoritative inputs

Two inputs are retained because each contains information the other does not
represent as clearly:

1. `Blender/JungleRuins_Main.blend`
   - collection hierarchy;
   - 16 cinematic terrain cells and 64 extended terrain cells;
   - 205 named scatter systems;
   - linked prototype collections;
   - Blender material graphs and texture paths.
2. `USD/JungleRuins_Karma.usda`
   - complete assembled scene;
   - baked point-instancer transforms;
   - USD mesh, material, camera, and environment prims;
   - stage units and up axis.

Neither input is overwritten.

## 3. Canonical logical hierarchy

```text
JungleRuins
├── Static
│   ├── Terrain
│   │   ├── Cinematic
│   │   └── Extended
│   ├── Architecture
│   ├── Water
│   └── HeroAssets
├── Scatter
│   ├── Cinematic
│   ├── Extended
│   └── Pyramid
├── Prototypes
├── Materials
├── Textures
└── Cameras
```

This is a logical hierarchy. It does not require separate files.

## 4. Spatial cells

The source cells are retained as authored:

- Cinematic: `M_3x3_01` through `M_4x4_04` (16 cells).
- Extended: `E_01` through `E_64` (64 cells).
- Pyramid scatter: `Pyramid`.

Extended cells form an 8 by 8 grid. Each cell is approximately 1000 Blender
units wide in X and Y. Cinematic cells form a 4 by 4 grid inside the central
area and are approximately 250 Blender units wide in X and Y.

Cell bounds are computed from the terrain objects and marked as `computed`.
The cell identifiers and object associations are marked as `source`.

The cinematic terrain overlaps the central portion of the extended terrain
grid. The pipeline must preserve that fact until a visual and topology audit
determines the intended visibility relationship. It must not silently delete
one representation as a duplicate.

## 5. Scatter organization

Scatter is represented as:

```text
region / cell / system / prototype / instance transforms
```

Examples:

```text
cinematic / M_3x3_01 / River_Forest / RiverForest_01 / transforms
extended  / E_01      / Queen_Forest / QueenForest_05 / transforms
pyramid   / Pyramid   / Moss         / Moss_02         / transforms
```

Prototype geometry is stored once. Instances remain transforms and must not be
realized into duplicated meshes in the canonical artifact.

The composed USD organizes the two extended forest species into numbered
groups whose non-origin XY bounds match `E_01` through `E_64`. Cinematic
species may instead be aggregated across multiple `M_*` cells. Their transforms
will be assigned to cinematic cells using the authored terrain-cell XY bounds.
The original USD prim path and original transform-array index remain attached
so this spatial normalization is reversible.

Exact-origin records have been observed in PointInstancer arrays. They are
reported separately and are not deleted until visual or source evidence proves
that they are exporter sentinels rather than authored instances.

## 6. Stable identity

Stable IDs use normalized source names, not array positions or renderer IDs.

```text
jr:region:cinematic
jr:cell:cinematic:M_3x3_01
jr:cell:extended:E_01
jr:system:cinematic:M_3x3_01:river_forest
jr:prototype:riverforest_01_animated_translucent
jr:material:mi_terrain_x3_y3
```

An individual scatter instance is identified by:

```text
(system stable ID, prototype stable ID, local transform index)
```

Ordering must therefore be deterministic.

When a source USD array is spatially normalized, the canonical record retains:

```text
(source layer, source prim path, source array index)
```

This makes the normalization reversible and independently auditable.

## 7. Metadata provenance

Every non-core metadata record carries a provenance class:

- `source`: read directly from Blender or USD.
- `computed`: deterministically calculated from source data.
- `inferred`: a semantic interpretation that requires review.

`inferred` metadata must never be presented as source truth. Initial GLB
artifacts may omit inferred metadata entirely.

## 8. glTF metadata

Renderer-independent metadata is stored under the standard glTF `extras`
field:

```json
{
  "extras": {
    "jr": {
      "schema_version": "0.1",
      "stable_id": "jr:system:cinematic:M_3x3_01:river_forest",
      "entity_type": "system",
      "provenance": "source",
      "region": "cinematic",
      "cell": "M_3x3_01",
      "system": "M_3x3_01 - River_Forest",
      "species": "River_Forest"
    }
  }
}
```

Generic glTF consumers may ignore this metadata.

## 9. Materials

The canonical artifact preserves:

- source material name and stable ID;
- mesh material-slot boundaries;
- base color, roughness, metallic, normal, emissive, occlusion, opacity, and
  transmission inputs where representable;
- alpha mode and cutoff;
- double-sided state;
- unresolved source translucency as source metadata.

The canonical artifact does not:

- assign pipeline-state objects;
- assign descriptor indices;
- merge geometry by material;
- atlas textures;
- replace materials with benchmark materials.

Semantic labels such as `foliage_leaf`, `bark`, `terrain`, or `water` are
initially `inferred` unless they come from an explicit source name or slot.

## 10. Physical packaging

The first canonical artifact is one logical scene with shared prototypes.
External textures are allowed. This avoids duplicating prototypes across
multiple standalone GLB files.

Smaller cinematic, streaming, LOD, or benchmark packages are derived artifacts.
They must reference the canonical inventory and document all filtering or
conversion.

## 11. Explicit non-goals

The canonical conversion must not:

- merge cells for draw-call reduction;
- split entities by renderer variant;
- store frustum, occlusion, or distance-culling policy;
- choose a runtime LOD;
- remove camera-invisible objects;
- create meshlets or renderer-specific clusters;
- bake D3D12 resource or shader binding information;
- reorder source instances without recording a reversible mapping.

## 12. Required validation

Every canonical build reports:

- source file count and byte size;
- Blender collection, object, material, image, and scatter-system counts;
- terrain-cell names and bounds;
- USD prim counts;
- point-instancer and instance-transform counts;
- unique prototype targets;
- missing files and unresolved references;
- coordinate-system conversion;
- all source-to-canonical ID mappings.

## 13. Working-directory layout

Human-readable documentation is versioned under the repository's
`docs/jungle/` directory. Pipeline code, schemas, and usage notes are versioned
under `assets/scripts/jungle/`. Machine-readable JSON reports, copied source
scenes, and generated GLBs stay in an ignored external working directory.

```text
VisibilityBufferInfo/
├── docs/jungle/
└── assets/scripts/jungle/

visbufscene/                         # ignored external working directory
├── JungleRuins/                     # immutable copied source
├── reports/                         # generated JSON
├── work/
└── exports/                         # generated GLB
```

## 14. Contract probes

A contract probe is a deliberately scoped GLB used to prove hierarchy,
metadata, coordinate conversion, shared prototype meshes, and
`EXT_mesh_gpu_instancing`. Its root metadata must state whether the scene, cell,
and system are complete. A probe must not be presented as the finished scene.

Blender 4.2 may expand `EXT_mesh_gpu_instancing` into editable objects when it
imports a GLB. The canonical renderer-facing contract remains the compact glTF
extension and its accessors; Blender's expanded re-import representation is
consumer behavior, not the storage contract.

Contract probes encode embedded textures as WebP quality 85 without a PNG
fallback and therefore require `EXT_texture_webp`. This is a size-control
experiment, not final visual approval. The final encoding is chosen only after
file-size, alpha, normal-map, and side-by-side material validation.

Transform-level spatial splitting uses deterministic half-open cell ownership
bounds. Where neighboring terrain mesh bounds differ or overlap slightly, the
ownership boundary is the midpoint between the adjacent bounds. The lower edge
is inclusive and the upper edge is exclusive, except for the outermost region
edge. This prevents one source transform from appearing in two cell packages.

## 15. Verified probes

| Probe | Scope | GLB bytes | Instance sets | Instances | Raw validation | Blender 4.2 re-import |
|---|---|---:|---:|---:|---|---|
| `M_3x4_01 River_Seedling` | terrain + 1 complete system | 41,158,752 | 5 | 27,147 | pass | pass |
| `M_3x4_01 complete cell` | terrain + all 5 systems | 322,784,224 | 21 | 74,206 | pass | pass |

Both probes use WebP quality 85 and require `EXT_texture_webp`. The complete
cell probe contains 22 shared meshes, 19 glTF materials, and 30 embedded
images. Its Blender re-import restored all 74,206 instance objects with origin
bounds:

```text
min = [12.001683, 262.000458, -15.743696]
max = [261.997833, 511.999023, 74.695442]
```

These values are in Blender's re-imported Z-up coordinates and remain inside
the authored `M_3x4_01` cell. Material presence is verified structurally;
visual equivalence remains open because the exporter reported unused vertex
colors and sampler consolidation, and WebP is lossy.
