# JungleRuins Runtime Renderer Paths

## Runtime ownership

Jungle GLBs enter both renderer families through the same scene-owned path:

```text
Jungle GLB
  -> SceneSourceLoader
  -> JungleSceneSourceBuilder
  -> SceneSourceData
  -> SceneCPUBuilder
  -> BenchmarkSceneGPUBuilder or DonutSceneGPUBuilder
  -> renderer pass
```

The renderer does not parse Jungle metadata or own a Jungle-specific import
format. Region, cell, system, prototype, and source-index information remains
owned by `SceneSourceData`; CPU/GPU data is a derived rendering view.

## Renderer coverage

The current factory routes:

| Variants | Family | GPU builder |
|---|---|---|
| 1-6, 10, 11 | Benchmark | `BenchmarkSceneGPUBuilder` |
| 7-9 | Donut | `DonutSceneGPUBuilder` |

Variant 11 is `RendererDebugView`. It is a diagnostic renderer, but it uses the
same Jungle source, CPU, and Benchmark GPU construction path.

## 2026-07-27 execution

Two local, ignored artifacts were used with a Release build:

1. `jungle_global.glb`, a final generated region package with three meshes,
   three rendered objects, four materials, 9,869,978 vertices, and 4,965,865
   unique triangles.
2. `jungle_M_3x4_01_complete_cell.glb`, a complete cinematic cell probe with
   22 meshes, 21 instance sets, 74,206 source transforms, and all five authored
   scatter systems for the cell.

The final global package completed variants 1-11 with:

```text
success=11, salvaged=0, failed=0
```

The complete instanced cell completed variants 1-9 and 11. Variant 10
(`RendererRasterStats`) failed while allocating its realized-triangle buffer.
That renderer currently expands `triangle count * instance count` into one
16-byte record per triangle before dispatch. This is a RasterStats scale limit,
not a loss in Jungle GLB, `SceneSourceData`, CPU instance, or either normal GPU
scene path. The limit is intentionally not changed as part of the Jungle
import work.

All runtime smoke runs used one warm-up frame, one measured frame,
`to_load_texture=false`, `use_vfc=false`, and automatic termination. They prove
construction and draw-path reachability, not benchmark-quality timing or
visual material equivalence.

## Preserved and lowered data

- Geometry, hierarchy, compact TRS instances, material factors, alpha mode,
  camera data, and Jungle identity metadata are preserved by SourceData.
- CPU data retains opaque, mask, and blend alpha modes.
- Existing GPU material layouts currently lower both mask and blend to their
  alpha-tested flag/domain. A true ordered alpha-blend pass remains renderer
  work.
- Embedded WebP byte ranges remain lossless in SourceData, but downstream
  D3D12 texture decode/upload is not implemented for those GLB ranges.
  Consequently the runtime smoke specs set `to_load_texture=false`.

Generated GLBs, reports, CSVs, and logs remain under ignored directories and
must not be committed.
