# JungleRuins Source Inventory

- Schema: `0.2`
- Generated UTC: `2026-07-26T18:15:12.878768+00:00`
- Blender: `4.2.23 LTS`
- Source files: `324`
- Source bytes: `7435345046`
- Blender objects: `355`
- Blender meshes: `342`
- Blender materials: `317`
- Blender images: `275`
- Missing images: `0`
- Terrain cells: `80`
- Scatter systems: `205`
- Scatter by region: `{"cinematic": 75, "extended": 128, "pyramid": 2}`
- USD prims: `3429`
- USD point instancers: `778`
- USD instances: `8674676`
- USD exact-origin records: `197`
- USD unique prototype names: `53`

## USD point-instancer layers

| Layer | Point instancers | Instances | Exact-origin records |
|---|---:|---:|---:|
| `elements/Anthurium/PI_Anthurium.usd` | 6 | 138 | 0 |
| `elements/Grass_A/PI_Grass_A.usd` | 6 | 280985 | 0 |
| `elements/Grass_B/PI_Grass_B.usd` | 5 | 339865 | 0 |
| `elements/Nettle/PI_Nettle.usd` | 6 | 330 | 0 |
| `elements/Pyramid_Grass_B/PI_Pyramid_GrassB.usd` | 5 | 44000 | 0 |
| `elements/Pyramid_Moss/PI_S_Moss.usd` | 138 | 2034610 | 0 |
| `elements/QueenForest/PI_S_QueenForest.usd` | 195 | 613806 | 0 |
| `elements/RiverForest/PI_S_RiverForest.usd` | 195 | 2407967 | 192 |
| `elements/RiverSapling/PI_RiverSapling.usd` | 5 | 45000 | 5 |
| `elements/RiverSeedling/PI_S_RiverSeedling.usd` | 80 | 2266462 | 0 |
| `elements/Shrub/PI_Shrub.usd` | 4 | 11337 | 0 |
| `elements/ShrubSorrel/PI_S_ShrubSorrel.usd` | 133 | 630176 | 0 |

## Open issues

- `JR-ISSUE-0001`: Map every composed USD PointInstancer group to Blender M/E/Pyramid systems without relying on name similarity.
- `JR-ISSUE-0002`: Audit the intended visibility relationship between the 16 cinematic terrain cells and the overlapping central extended terrain cells.
- `JR-ISSUE-0003`: Classify material semantics only after source material slots and visual references are reviewed.
- `JR-ISSUE-0004`: Audit exact-origin records found in USD PointInstancer arrays. Preserve them until it is proven whether they are authored instances or export sentinels.
