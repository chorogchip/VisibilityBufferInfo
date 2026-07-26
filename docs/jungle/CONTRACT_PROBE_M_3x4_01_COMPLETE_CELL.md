# JungleRuins GLB Contract Probe

This artifact proves the canonical GLB hierarchy, metadata, shared prototype, and instance-transform path without GScatter.

- Validation: `pass`
- Artifact: `visbufscene/exports/probes/jr_cinematic_M_3x4_01_complete_cell_probe.glb`
- Artifact bytes: `322784224`
- Cell: `M_3x4_01`
- Included systems: `5`
- Complete scene: `False`
- Complete cell: `True`
- Complete system: `True`

The terrain cell and all authored scatter systems for the cell are present.
This remains a contract probe, not a complete cinematic scene.

## Included systems

- `M_3x4_01 - Grass_02`
- `M_3x4_01 - Queen_Forest`
- `M_3x4_01 - River_Forest`
- `M_3x4_01 - River_Sapling`
- `M_3x4_01 - River_Seedling`

## Counts

| Entity | Count |
|---|---:|
| `nodes` | 55 |
| `reachable_nodes` | 34 |
| `meshes` | 22 |
| `materials` | 19 |
| `textures` | 50 |
| `images` | 30 |
| `image_mime_types` | ['image/webp'] |
| `instance_sets` | 21 |
| `instances` | 74206 |

## Contract

- `EXT_mesh_gpu_instancing` used: `True`
- `EXT_mesh_gpu_instancing` required: `True`
- `EXT_texture_webp` used: `True`
- `EXT_texture_webp` required: `True`
- Source-array identity attribute: `_JR_SOURCE_INDEX`
- Renderer policy embedded: `False`
- Rendered prototype-export nodes: `0`
- Metadata nodes: `34`
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
