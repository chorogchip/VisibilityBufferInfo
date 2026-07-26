# JungleRuins Region Package Layout

## Output

Generated output is ignored by git and lives below:

```text
assets/scenes/generated/jungle/packages/
```

The package set contains one JSON catalog and four GLBs:

| Package | Authored contents | Internal culling boundary |
|---|---|---|
| `jungle_global.glb` | River, Creek, linked Banyan, source camera | Static object |
| `jungle_cinematic.glb` | 16 detailed terrain cells and 75 systems | Cell, then system |
| `jungle_extended.glb` | 64 background terrain cells and 128 systems | Cell, then system |
| `jungle_pyramid.glb` | Pyramid shell, Grass, and Moss | Pyramid cell, then system |

## Verified build

The 2026-07-27 build completed with:

| Package | Bytes | Cells | Systems | Instance sets | Instances |
|---|---:|---:|---:|---:|---:|
| `global` | 543,076,428 | 0 | 0 | 0 | 0 |
| `cinematic` | 659,120,372 | 16 | 75 | 442 | 3,620,525 |
| `extended` | 978,718,668 | 64 | 128 | 384 | 2,975,541 |
| `pyramid` | 157,933,008 | 1 | 2 | 143 | 2,078,610 |
| **Total** | **2,338,848,476** | **81** | **205** | **969** | **8,674,676** |

The source-index coverage validator observed all 778 USD streams and every
source-array index exactly once, with no missing, duplicate, or out-of-range
records. All 197 exact-origin records are present. Cinematic has six unresolved
records: five exact-origin records and one Grass B record outside every cell
ownership bound.

## Logical hierarchy

```text
package root
|-- region
|   |-- static objects
|   |-- unresolved source records
|   `-- cell
|       |-- terrain/static geometry
|       `-- systems
|           `-- instance set -> shared prototype mesh
`-- non-rendered prototype source container
```

Prototype export nodes are removed from the reachable scene after Blender
creates their meshes. Each generated instance-set node references the shared
mesh with `EXT_mesh_gpu_instancing`.

## Instance identity

Every instance set has four equal-length accessors:

- `TRANSLATION`;
- `ROTATION`;
- `SCALE`;
- `_JR_SOURCE_INDEX`.

The last accessor is the index in the original USD point-instancer array.
Combined with `source_layer` and `source_prim` metadata, it provides a
reversible identity. Spatial splitting changes ownership only; it does not
change transforms or reorder identity without recording the original index.

## Why packages are not per cell

A standalone GLB for each of 81 cells would repeatedly embed the same tree
meshes and textures. A single monolithic GLB would be close to the GLB
container's 32-bit size ceiling and require all regions to load together.
Four region files keep meaningful loading boundaries while retaining cell
nodes for later culling.

## Validation gates

The package build checks:

- all four instance accessors exist and have equal counts;
- total instance count matches the arrays selected by the manifest;
- the complete package-set instance count matches all 8,674,676 USD records;
- prototype export nodes are not reachable/rendered;
- the GLB extension is declared used and required;
- each GLB remains below the 32-bit GLB byte limit;
- exact-origin and out-of-ownership records are counted rather than removed.

Texture encoding is WebP quality 85 without fallback. Structural validation
does not approve visual equivalence; alpha edges, foliage normals, terrain
normal maps, river material, and transmission require side-by-side review.
