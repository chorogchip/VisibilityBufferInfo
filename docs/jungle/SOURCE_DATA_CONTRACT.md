# JungleRuins C++ Source Data Contract

## Purpose

`scene::SceneSourceData` is the renderer-independent endpoint of Jungle GLB
loading. It represents what the scene contains. It does not select a renderer,
build draw calls, allocate descriptors, choose LOD, or decide culling policy.

Public declarations live below `include/scene/data/source`. The glTF decoder is
split across matching implementation files below `src/scene/data/source`.

## Data ownership

| Source type | Responsibility |
|---|---|
| `SceneSourceData` | Owns normalized nodes, meshes, materials, cameras, image/texture tables, and compact instance records. |
| `source::Node` | Owns hierarchy children and references one mesh, camera, and/or contiguous instance range. |
| `source::InstanceTransform` | Stores translation, quaternion, scale, and reversible source-array index. |
| `source::Mesh` / `Primitive` | Stores indexed triangles and source material boundaries. |
| `source::Material` | Stores PBR factors, alpha mode, transmission/specular/IOR values used by Jungle, and texture references. |
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

`stable_id` remains a string only on nodes because it is the canonical bridge
between reports, package metadata, and future runtime controls. Descriptive
source strings that do not affect identity are not copied into C++ structs.

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

## Deliberate downstream boundary

The current benchmark and Donut CPU/GPU scene types are derived consumers.
They may initially support only subsets such as opaque rendering. They must not
cause source data to discard alpha blend, transmission, embedded images,
system identity, cell identity, or source instance indices. Future renderer
work should consume selected source nodes/cells and build only the necessary
derived render data.
