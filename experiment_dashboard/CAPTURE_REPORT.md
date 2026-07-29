# Dashboard capture report

## Outcome

- Capture config: `capture_specs/dashboard_camera_capture.json`
- Status: `completed`
- Runs: 7 expected, 7 success, 0 salvaged, 0 failed, 0 skipped
- Raw frames: 348 PNG
- Raw size: 265,817,186 bytes
- Deployable frames: 344 WebP
- Deployable frame size: 39,547,836 bytes
- Capture resolution: 1280x720
- Deployable resolution: 960x540
- Capture hardware: NVIDIA GeForce RTX 5060 Ti 16GB

## Sequence audit

| Run | Sequence | Renderer | Debug mode | Raw frames | Playable frames |
|---:|---|---|---:|---:|---:|
| 0 | Sponza PBR | DonutVisGBuffer | n/a | 42 | 39 |
| 1 | Bistro PBR | DonutVisGBuffer | n/a | 92 | 91 |
| 2 | Sponza geometry instances | DonutVisDebug | 0 | 42 | 42 |
| 3 | Sponza primitives | DonutVisDebug | 1 | 42 | 42 |
| 4 | Sponza geometry + primitives | DonutVisDebug | 2 | 42 | 42 |
| 5 | Sponza barycentrics | DonutVisDebug | 3 | 42 | 42 |
| 6 | Bistro geometry + primitives | DonutVisDebug | 2 | 46 | 46 |

The three final Sponza beauty frames (image indexes 39–41) and the final
Bistro beauty frame (image index 91) are completely black after the camera path
finishes. They remain in raw capture evidence and are listed as
`excludedBlankFrames` in deployable manifests, but are not presented as useful
viewer frames.

## Renderer change

`visibility-debug-mode` is now a validated ProgramArgument:

- `0`: geometry instance hash
- `1`: primitive hash
- `2`: geometry + primitive hash
- `3`: barycentric coordinates
- `4`: perspective-correct barycentrics
- `5`: barycentric dx
- `6`: barycentric dy
- `7`: UV dx
- `8`: UV dy

Renderer variants 12 and 13 receive this value instead of always selecting
mode 2. The default remains 2, so prior debug-view behavior is preserved.

## Validation

- x64-Release build completed successfully with MSVC 14.51.
- Both Sponza and Bistro paths reached their full 2,500/5,500 measurement
  windows with textures and VFC enabled.
- Every capture manifest frame count matches its PNG directory.
- Every deployable frame is 960x540 WebP, has no alpha channel, is not
  effectively black, and matches its recorded SHA-256 digest.
- Capture diagnostics contain no machine-local user paths after sanitization.
- Dashboard source campaign remains 396/396 success.
