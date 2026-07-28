# MyDonut Shader Define Flattening Plan

## Goal

Make the shaders used by TVBPerf's own Donut-derived renderer readable and
directly auditable without changing the rendering ABI or deleting the NVIDIA
Donut reference shaders. The target is `assets/shaders/mydonut/`, not every
file in `assets/shaders/donut/`.

The depth pre-pass starts this boundary: both entry shaders now live under
`mydonut`. The vertex shader is initially a behavior-preserving copy, so its
Donut include chain and defines are still deliberately present.

## Current Boundary

| Pass | Current source ownership | Define-flattening priority |
| --- | --- | --- |
| Depth pre-pass | `mydonut/donut_depth_pre_VS.hlsl`, `mydonut/donut_depth_pre_PS.hlsl` | First |
| G-buffer | `mydonut/donut_gbuffer_VS.hlsl`, `mydonut/donut_gbuffer_PS.hlsl` exist, but the pass compile paths must be audited before migration | Second |
| Visibility raster | `mydonut/donut_visibility_VS.hlsl`, `mydonut/donut_visibility_PS.hlsl` exist, but the pass compile paths must be audited before migration | Second |
| Visibility resolve | `mydonut/donut_vis_gbuffer_CS.hlsl` | Third |
| Deferred lighting and tone map | Donut-derived paths remain in use | Last; not required for scene import or visibility experiments |

The G-buffer and visibility rows intentionally call out the compile-path audit:
having a file under `mydonut` does not mean the renderer currently compiles it.
Each path change must be reviewed alongside its root signature before the next
pass is considered migrated.

## Define Policy

Flatten only defines that conceal a fixed TVBPerf contract. Keep the contracts
that represent data ABI or a deliberate shader variant.

| Category | Action |
| --- | --- |
| Register and space aliases such as `DEPTH_BINDING_*`, `GBUFFER_*`, and `MATERIAL_*` | Replace with explicit `register(bN, spaceM)`, `register(tN, spaceM)`, and `register(sN, spaceM)` in MyDonut shaders after the corresponding C++ root-signature map is recorded. |
| `DECLARE_CBUFFER`, `DECLARE_PUSH_CONSTANTS`, and `REGISTER_*` helpers | Replace with ordinary HLSL declarations in MyDonut-only headers and entry shaders. |
| `TARGET_D3D11` branches | Remove from MyDonut code after its headers are local. TVBPerf is D3D12-only, so the `StructuredBuffer` branch is the intended one. |
| `MOTION_VECTORS` branches | Remove from the MyDonut G-buffer path after confirming the existing fixed `use_motion_vectors_ = false` policy remains intentional. |
| Material flag values, CPU/HLSL struct layouts, descriptor-array capacity | Preserve as explicit named constants and ABI structs. Do not change values, order, packing, or descriptor count without matching C++ changes and static assertions. |
| Runtime material feature checks such as opacity and normal-map flags | Keep. These describe scene data, not compile-time configuration. |
| Header guards | Keep. They are not a readability or behavior problem. |
| Donut-only imaging, Python, D3D11, and unused material-model options | Do not copy into new MyDonut headers. Leave the reference implementation untouched. |

## Execution Steps

1. Record a binding table for every MyDonut pass from its C++ root signature:
   root parameter, HLSL register and space, resource type, visibility, and the
   matching C++ struct. Treat this as the source of truth before replacing any
   register macro.

2. Add narrow MyDonut ABI headers for the depth pass first. They should contain
   only `DonutDepthPassConstants`, depth push constants, `InstanceData`,
   `DrawInstanceData`, and the byte strides used by manual vertex fetch. Use
   explicit D3D12 register declarations in both depth entry shaders.

3. Extract a shared MyDonut scene ABI header from the currently duplicated
   G-buffer and visibility structs. Retain the existing C++ layout checks in
   `include/scene/donut/DonutRenderConstants.h`; add offsets or size checks
   whenever a GPU-facing struct is changed.

4. Migrate G-buffer and visibility raster shaders one pass at a time. First
   switch the C++ compile path, then replace fixed binding/platform defines,
   and keep material behavior byte-for-byte equivalent. Do not combine this
   work with material-model simplification.

5. Migrate the visibility resolve compute shader after the shared ABI header
   is stable. Keep its bindless descriptor-array capacity as the one explicit
   compile-time configuration until descriptor heap sizing is centralized.

6. Decide separately whether deferred lighting and tone mapping are research
   code worth owning. Only then copy their entry shaders and flatten their
   defines; their lighting, IBL, shadow, and tone-mapping options are outside
   the geometry/visibility renderer boundary.

7. Remove a MyDonut dependency on a Donut header only after no MyDonut shader
   needs a symbol from it. Preserve `assets/shaders/donut/` as the reference
   baseline and retain the NVIDIA notice for copied or derived sources.

## Verification When Requested

No verification is performed with this change. When requested, validate each
migrated pass by building the relevant configuration, checking shader compile
logs and root-signature compatibility, then running the affected renderer with
camera playback through the final frame on both Sponza and Bistro with
`to_load_texture=true`. Compare the depth-prepass, G-buffer, and visibility
outputs before making performance claims.
