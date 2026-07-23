# Benchmark Raster Stats Pending

2026-07-24 quick handoff.

Implemented:

- Added `RendererRasterStats` as benchmark renderer variant 10.
- Added `PassRasterStats` and `assets/shaders/benchmark_raster_stats_CS.hlsl`.
- The pass uses GPU compute to count raw raster coverage into UAV buffers:
  - total fragment candidates
  - covered pixels
  - overdraw extra fragments
  - max/avg overdraw
  - rasterized/skipped triangles
  - 2x2 quad instances, covered lanes, wasted lanes, efficiency
- Output sidecar path is `<output-stem>_<run-id>_raster_stats.csv`.
- Debug build compiles with:
  `cmd.exe /d /s /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && cmake --build out\build\x64-Debug --config Debug"`

Not finished:

- Runtime smoke test for variant 10 still is not proven. Test command used:
  `TVBPerf.exe --renderer-variant 10 --to-use-scene false --variable 1 --object-count 2 --material-count 1 --geometry-count 1 --overdraw-count 1 --geometry-div 1 --window-width 64 --window-height 64 --warmup-frames 0 --measure-frames 2 --auto-terminate true --vsync false --output-filepath raster_stats_smoke_wait2.csv`
- That run created only the raster stats CSV header, then exited with `-1073741819`.
- Under the current dirty worktree, variant 1 also exits with `-1073741819` after writing its normal CSV, so at least part of the exit crash may be shared lifecycle/destructor state, not only raster stats.
- Variant 10 still needs real frame row collection verification after the exit/lifecycle issue is fixed.
- The compute rasterizer is an MVP approximation:
  - no near-plane/homogeneous clipping yet
  - no exact D3D top-left fill rule
  - no depth test or alpha test
  - quad counting is per primitive software coverage, intended for diagnostic correlation rather than exact hardware counter parity

Important workspace note:

- Renderer variant 9 is already occupied by another agent's `RendererDonutVisGBuffer` work in the dirty worktree. Raster stats was assigned variant 10 to avoid colliding with that work.
- Do not rollback the existing fastgltf/Donut visibility changes; they are from another agent.
