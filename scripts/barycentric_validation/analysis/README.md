# Barycentric and derivative correctness validation

- Hardware: **NVIDIA GeForce RTX 5060 Ti 16GB**
- Reference: renderer variant 14, hardware `SV_Barycentrics` and native `ddx_coarse`/`ddy_coarse`
- Test path: renderer variant 13, analytic screen-space barycentrics and quotient-rule perspective UV gradients
- Resolution: 1920×1080
- Scenes: Sponza, Sponza Ivy, Bistro
- Debug modes: linear/perspective barycentrics, barycentric dx/dy, UV dx/dy, 1024-pixel reference-texture LOD proxy
- Compared capture pairs: 910
- Debug pass timing is intentionally not interpreted as performance.

## Aggregate result

- Mean interior MAE: **0.018412 LSB**
- Bit-exact interior channels: **98.3831%**
- Interior channels above 1 LSB: **0.083195%**
- Mean coverage mismatch: **0.000000% of pixels**
- Largest pair mean MAE: **0.076147 LSB** (Sponza Ivy, linear_barycentric)

These values are measured after excluding the background and eroding the common coverage mask by one pixel. Coverage mismatch is reported separately, so rasterization/alpha-cutout edges do not dominate the math comparison.

The captures are 8-bit UNORM. Therefore sub-LSB floating-point differences cannot be resolved; conclusions are limited to the encoded debug representations. Derivative modes use `0.5 + derivative * 16`; the LOD proxy encodes `log2(max(|du/dx|, |du/dy|) * 1024)` over [-8, 16].

## Reproduction

```powershell
python scripts/run.py scripts/barycentric_validation/01_sponza_barycentric_validation.json
python scripts/run.py scripts/barycentric_validation/02_sponza_ivy_barycentric_validation.json
python scripts/run.py scripts/barycentric_validation/03_bistro_barycentric_validation.json
python scripts/barycentric_validation/analyze_captures.py --all-heatmaps
```

All-frame heatmaps were generated locally.

## Outputs

- `analysis/data/frame_metrics.csv`: every captured frame pair
- `analysis/data/pair_summary.csv`: scene × mode aggregate
- `analysis/data/campaign_summary.json`: portable status and provenance
- `analysis/png` and `analysis/svg`: aggregate and time-series plots
- `analysis/representative`: worst-frame raster/VisBuf/diff comparisons
- `analysis/local_heatmaps`: every frame heatmap (local, not committed)
