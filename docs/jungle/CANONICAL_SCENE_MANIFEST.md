# JungleRuins Canonical Scene Manifest

This catalog defines scene facts and source-to-canonical mappings. It does not encode renderer culling, LOD, draw batching, or pass policy.

- Schema: `0.1`
- Validation: `pass` (0 errors)
- Regions: `4`
- Cells: `81`
- Systems: `205`
- Shared prototypes: `53`
- Materials: `317`
- Textures: `275`
- USD PointInstancer sources: `778`
- USD transform records: `8674676`
- Exact-origin records kept unresolved: `197`

## Regions

| Region | Cells | Systems |
|---|---:|---:|
| `global` | 0 | 0 |
| `cinematic` | 16 | 75 |
| `extended` | 64 | 128 |
| `pyramid` | 1 | 2 |

## Instance-source placement

| Classification | PointInstancers | Non-origin transforms |
|---|---:|---:|
| `direct_cell` | 750 | 8242919 |
| `requires_spatial_split` | 28 | 431560 |

A `direct_cell` source is wholly contained by one authored cell that owns a matching Blender scatter system. `requires_spatial_split` sources remain intact in this catalog and must be divided at transform-record granularity.

## Validation details

- `duplicate_cell_ids`: `0`
- `duplicate_system_ids`: `0`
- `duplicate_prototype_ids`: `0`
- `duplicate_material_ids`: `0`
- `duplicate_texture_ids`: `0`
- `unknown_usd_layers`: `0`
- `unknown_prototype_targets`: `0`
- `point_sources_without_candidate_systems`: `0`
- `missing_images`: `0`
- `pyramid_bounds_resolved`: `True`
