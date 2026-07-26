# JungleRuins C++ Source Data Contract

## Purpose

`scene::SceneSourceData` is the renderer-independent endpoint of Jungle GLB
loading. It represents what the scene contains. It does not select a renderer,
build draw calls, allocate descriptors, choose LOD, or decide culling policy.

Public data declarations live below `include/scene/data/source`. The glTF
decoder is owned by `scene::JungleSceneSourceBuilder` in
`include/scene/builder/source/JungleSceneSourceBuilder.h`. Its camera,
geometry, hierarchy, and material responsibilities are split below
`src/scene/builder/source`, along with source-scene validators.
`include/scene/data/source` contains passive data contracts rather than
file-format decoding or builder policy.

The Jungle builder has no recoverable build-result or threaded error string.
Its mutating conversion stages are `void` operations and invalid canonical
input fails immediately through `util::Logger::assert_with_log`. A successful
public `build` call returns the completed `SceneSourceData` owner.

## Data ownership

| Source type | Responsibility |
|---|---|
| `SceneSourceData` | Owns normalized nodes, meshes, materials, cameras, image/texture tables, and compact instance records. |
| `source::Node` | Owns hierarchy children and references one mesh, camera, and/or contiguous instance range. |
| `source::InstanceTransform` | Stores translation, quaternion, scale, and reversible source-array index. |
| `source::Mesh` / `Primitive` | Stores mesh names, indexed triangles, source material boundaries, normals, tangents, UV0/UV1, and COLOR_0/COLOR_1. |
| `source::Material` | Stores material names, PBR factors, alpha mode, transmission/specular/specular-color/IOR values used by Jungle, and texture references. |
| `source::Image` | References encoded image bytes by file path and optional byte range. |
| `source::Texture` / `Sampler` | Preserves glTF image and sampling relationships. |
| `source::Camera` | Preserves the source perspective/orthographic camera parameters. |

The 44-byte instance record is:

```text
translation float3   12 bytes
rotation    float4   16 bytes
scale       float3   12 bytes
source_index uint32   4 bytes
```

Nodes reference `[first_instance, first_instance + instance_count)`. They do
not own one `std::vector` per system and do not store 4x4 matrices or
per-instance AABBs.

## Jungle hierarchy mapping

`extras.jr.entity_type` maps to `source::NodeKind`:

| glTF metadata | C++ kind | Intended later use |
|---|---|---|
| `scene_root` | `SceneRoot` | Package root |
| `region` | `Region` | Region loading boundary |
| `cell` | `Cell` | Coarse spatial culling boundary |
| `system` | `System` | Authored scatter/material grouping |
| `instance_set` | `InstanceSet` | One prototype mesh plus compact TRS range |
| `static_object` | `StaticObject` | Terrain, river, creek, pyramid, or other unique geometry |
| `camera` | `Camera` | Authored camera node |
| `unresolved_container` | `UnresolvedContainer` | Records without a spatial cell owner |
| `static_container` | `StaticContainer` | Static-object grouping |
| `terrain_container` | `TerrainContainer` | Per-cell terrain grouping |
| `system_container` | `SystemContainer` | Per-cell system grouping |
| `prototype_container` | `PrototypeContainer` | Non-rendered prototype grouping |

Nodes retain their glTF name and the selected `extras.jr` fields required to
reconstruct Jungle ownership and identity:

- `stable_id`, `cell`, `system`, and `species`;
- `prototype`, `prototype_id`, and `source_object`;
- `source_prim` and `source_layer`;
- `provenance` and `unresolved_reason`.

Arbitrary unknown extras and exporter bookkeeping strings are not copied.
This is a lossless contract for the non-texture data actually authored in the
four canonical Jungle GLBs, not a general promise to preserve every possible
glTF extension.

## Geometry attribute normalization

All 242 Jungle primitives retain `POSITION`, `NORMAL`, `TEXCOORD_0`, and
triangle indices. Optional attributes are retained when present:

| Attribute | SourceData representation | Canonical package count |
|---|---|---:|
| `TANGENT` | `XMFLOAT4` | Source-dependent |
| `TEXCOORD_1` | `XMFLOAT2` | 19 primitives |
| `COLOR_0` | `XMFLOAT4` | 101 primitives |
| `COLOR_1` | `XMFLOAT4` | 101 primitives |

glTF `VEC3` colors receive semantic alpha `1.0`; `VEC4` colors retain their
alpha. Normalized integer accessors are decoded to their normalized float
values. Attribute arrays must be either absent or exactly match the position
count.

## Coordinate normalization

The exporter converts Blender/USD Z-up right-handed positions to glTF Y-up:

```text
[x, y, z] -> [x, z, -y]
```

The C++ loader then converts glTF right-handed data to DirectX left-handed data
by reflecting Z. The resulting runtime position corresponds to:

```text
[source x, source z, source y]
```

Triangle winding, normals, tangents, node matrices, translations, and
quaternions are converted consistently. UVs retain the glTF convention.

## Embedded image handling

Region GLBs embed WebP images. Copying every encoded image into
`SceneSourceData` would duplicate hundreds of megabytes while the GLB remains
mapped or resident. The loader instead records:

- source GLB path;
- byte offset of the image buffer view;
- encoded byte size;
- image format.

This is lossless and lazy. Texture decode and D3D12 upload are downstream
responsibilities.

## Validation scope

The source validator checks hierarchy/reference/range integrity, indexed
triangles, and texture references. It does not perform filesystem existence
walks, image decoding, per-instance AABB construction, or a second full scan of
all 8.67 million transforms. Instance finiteness is checked while accessors are
decoded.

The explicit package-set validator is a stronger offline check. Build and run
it from the repository root:

```powershell
cmake --build out/build/x64-Debug --config Debug `
  --target JungleSceneSourceValidate

& out/build/x64-Debug/bin/Debug/JungleSceneSourceValidate.exe `
  assets/scenes/generated/jungle/packages
```

The 2026-07-27 run called `JungleSceneSourceBuilder::build` once for each
region package and passed with:

- 8,674,676 instances across all 778 original source streams;
- every stream containing the exact `source_index` sequence `0..N-1`;
- 969 instance sets, 81 cells, and 205 systems;
- 148 meshes, 242 primitives, 127 materials, 281 images, and one camera;
- 197 exact-origin records, including five unresolved exact-origin records,
  plus one outside-cell-ownership record;
- expected UV1/COLOR attribute counts and the River specular color.

The generated GLBs used by this test remain ignored and are not committed.

## Deliberate downstream boundary

The current benchmark and Donut CPU/GPU scene types are derived consumers.
They may initially support only subsets such as opaque rendering. They must not
cause source data to discard alpha blend, transmission, embedded images,
system identity, cell identity, or source instance indices. Future renderer
work should consume selected source nodes/cells and build only the necessary
derived render data.
