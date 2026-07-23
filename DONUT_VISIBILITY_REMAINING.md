# Donut Visibility Remaining Notes

Last updated: 2026-07-24

## Current State

- Committed `45a7f17 Add Donut visibility G-buffer renderer`.
- New renderer variant: `renderer_variant=9`, renderer name `DonutVisGBuffer`.
- Path is `visibility -> gbuffer resolve -> deferred lighting -> tonemap`.
- Uses existing Donut scene CPU/GPU data and existing Donut deferred lighting/tonemap contracts.
- G-buffer output keeps the Donut ABI:
  - RT0: diffuse albedo RGB + opacity
  - RT1: specular F0 RGB + occlusion
  - RT2: world normal RGB + roughness
  - RT3: emissive RGB + unused
- New shaders are in root shader folder, not `assets/shaders/donut/`:
  - `assets/shaders/donut_visibility_VS.hlsl`
  - `assets/shaders/donut_visibility_PS.hlsl`
  - `assets/shaders/donut_vis_gbuffer_PS.hlsl`
- Texture path is wired through Donut material descriptors and the visibility G-buffer resolve samples material textures.

## Verified

- Debug build passed with VS dev environment:
  - `cmake --build out\build\x64-Debug --config Debug`
- Shader compile smoke passed earlier for:
  - `donut_visibility_VS.hlsl`
  - `donut_visibility_PS.hlsl`
  - `donut_vis_gbuffer_PS.hlsl`
- Debug runtime smoke passed with local Assimp textured glTF:
  - `renderer_variant=9`
  - `to_load_texture=true`
  - `external\assimp\test\models\glTF2\BoxTextured-glTF\BoxTextured.gltf`
  - CSV contained pass timings for `visibility`, `gbuffer`, `lighting`, `tonemap`.
- Release build passed earlier.

## Not Finished / Risks

- Sponza glTF was not tested because the scene asset was not available locally.
- Release runtime smoke still needs a clean retry after other active work settles. Earlier Release runtime failed even for synthetic variant 1, so it looked broader than this renderer.
- Existing DonutDeferred variant 7 debug smoke hit an existing constant-buffer initialization assert during depth pass. Variant 9 does not depend on that depth prepass path, but this should be fixed before comparing variant 7/8/9 seriously.
- No new visibility prepass variant is wired. Variant 9 currently uses its own depth write in the visibility pass.
- Script sweep labels / analysis mapping were not updated for variant 9. Add this before running large experiments.
- Worktree currently has unrelated active changes from other agents, including fastgltf and raster stats work. They were intentionally left uncommitted by this task.

## Suggested Next Steps

1. Rebuild after other agents finish and resolve `RendererFactory.cpp` / `ProgramArgumentValidator.cpp` variant-range conflicts.
2. Add variant 9 label to experiment specs and analysis scripts.
3. Run Sponza glTF with `--renderer-variant 9 --to-load-texture true`.
4. Capture a frame in PIX/Nsight or RenderDoc-style tooling to check:
   - visibility buffer IDs are non-zero on geometry,
   - material texture descriptor indexing matches material ID,
   - G-buffer channels match the Donut ABI.
5. Then compare variant 7/8/9 only after the existing DonutDeferred depth constant-buffer assert is fixed.
