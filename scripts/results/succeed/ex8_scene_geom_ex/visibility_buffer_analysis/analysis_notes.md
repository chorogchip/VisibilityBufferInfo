# Sponza renderer-variant experiment: analysis notes

## Dataset
- 30 runs: 6 renderer variants × 5 ALU levels.
- 240 profile windows per run, frames 0–2390 in steps of 10.
- The culled index count is identical across all renderer runs at each frame, so aligned frame comparison is valid.
- Variant mapping inferred from populated pass columns:
  1. Forward
  2. Forward+Prepass
  3. Deferred
  4. TVB
  5. Deferred+Prepass
  6. TVB+GBuffer

## Main observations
- TVB has the lowest median total time at every measured ALU level.
- Its median speedup over Forward grows from about 1.37× at ALU 10 to 2.58× at ALU 160.
- At ALU 160, TVB is about 2.59× faster than Deferred, 1.40× faster than Forward+Prepass, and 1.43× faster than Deferred+Prepass.
- TVB+GBuffer remains close to direct TVB. The median difference at ALU 160 is about 0.028 ms.
- Linear fits of median total time against ALU count have R² above 0.999:
  - Forward and Deferred: about 0.0598 ms per ALU count.
  - Forward+Prepass and Deferred+Prepass: about 0.031 ms per ALU count.
  - TVB and TVB+GBuffer: about 0.0215 ms per ALU count.
- The measured ALU sensitivity of Forward/Deferred is about 2.78 times the TVB sensitivity. This is consistent with TVB avoiding a substantial amount of discarded or quad-wasted material work, but the ratio should not be interpreted directly as an exact overdraw factor without fragment and quad counters.
- The TVB visibility pass is nearly ALU-independent and strongly tracks the culled index count. Its correlation with index count is about 0.994.
- The TVB resolve/G-buffer pass has much weaker correlation with index count, around 0.38, because it is more closely tied to visible screen samples and material work than submitted geometry count.
- TVB wins most frames, but near-empty frames favor Forward because TVB retains fullscreen/visibility fixed costs. At ALU 160, direct TVB wins about 67.1% of frames and TVB+GBuffer about 26.7%.
- Frames around 840–910 and 1500–1610 show positive visibility-pass residuals even after index count is accounted for. These are useful candidates for capture-based analysis of projected triangle size, rasterization behavior, geometry locality, and draw structure.
