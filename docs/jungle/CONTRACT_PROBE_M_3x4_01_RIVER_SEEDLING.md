# JungleRuins GLB Contract Probe

This artifact proves the canonical GLB hierarchy, metadata, shared prototype, and instance-transform path without GScatter.

- Validation: `pass`
- Artifact: `visbufscene/exports/probes/jr_cinematic_M_3x4_01_river_seedling_probe.glb`
- Artifact bytes: `41158752`
- Cell: `M_3x4_01`
- Complete system: `M_3x4_01 - River_Seedling`
- Complete scene: `False`
- Complete cell: `False`
- Complete system: `True`

The terrain cell is complete, but the GLB intentionally contains only the named scatter system. It is a contract probe, not a visually complete cinematic package.

## Counts

| Entity | Count |
|---|---:|
| `nodes` | 19 |
| `reachable_nodes` | 14 |
| `meshes` | 6 |
| `materials` | 4 |
| `textures` | 13 |
| `images` | 13 |
| `image_mime_types` | ['image/webp'] |
| `instance_sets` | 5 |
| `instances` | 27147 |

## Contract

- `EXT_mesh_gpu_instancing` used: `True`
- `EXT_mesh_gpu_instancing` required: `True`
- `EXT_texture_webp` used: `True`
- `EXT_texture_webp` required: `True`
- Source-array identity attribute: `_JR_SOURCE_INDEX`
- Renderer policy embedded: `False`
- Rendered prototype-export nodes: `0`
- Metadata nodes: `14`
- Duplicate stable IDs: `0`

## Errors

- None.

## Warnings

- None.

## Material fidelity

- Status: `requires_visual_review`
- Texture encoding: `WEBP`
- Texture quality: `85`
- Lossy encoding: `True`
- Interpretation: Geometry, material slots, materials, textures, and images are present, but visual equivalence is not yet approved.
- Exporter warning: Active vertex color was not exported because it is not used by the source material node tree.
- Exporter warning: Multiple image-texture nodes may resolve to one glTF sampler behavior; Blender chose the first sampler.

## Blender 4.2 re-import

- Status: `pass`
- Blender: `4.2.23 LTS`
- Instance objects: `27147`
- Mesh datablocks: `7`
- Behavior: `expanded_to_editable_objects`
- Instance-origin min: `[12.007657, 262.000488, -15.743696]`
- Instance-origin max: `[261.997833, 511.998932, 74.329552]`

Blender expands `EXT_mesh_gpu_instancing` to editable objects on import. This is a Blender consumer behavior; the GLB itself stores compact accessor arrays.
