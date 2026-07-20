# VisibilityBufferInfo Camera-Path Timing Analysis

## Dataset validation

- Six renderer variants were loaded.
- Every file contains 133 samples.
- Frame indices align exactly from 0 to 1320 at intervals of 10.
- No missing timing values were found.
- No explicit total column exists, so total GPU time is calculated as the sum of recorded pass columns.
- The CSV files do not contain unit metadata. Figures label the values as milliseconds based on the project context; change the labels if the logger uses another unit.

## Main result

Mean total GPU time, fastest to slowest:

1. Forward: 0.27749
2. Deferred: 0.29064 (4.74% above Forward)
3. TVB: 0.35775 (28.93% above Forward)
4. TVB + G-buffer: 0.37469 (35.03% above Forward)
5. Forward + Prepass: 0.44742 (61.24% above Forward)
6. Deferred + Prepass: 0.49074 (76.85% above Forward)

Forward is fastest at 127/133 sampled frames. Deferred is fastest at 6/133 sampled frames: [100, 310, 570, 790, 1070, 1270].

TVB is not faster than Forward or Deferred at any sampled frame in this run. Its mean total time is 28.93% above Forward and 23.09% above Deferred.

## Pass-level observations

- TVB mean visibility time: 0.31535; mean resolve time: 0.04240.
- TVB visibility accounts for 88.15% of TVB total time.
- TVB + G-buffer mean visibility time: 0.32645; G-buffer pass: 0.04102; lighting: 0.00723.
- Deferred mean geometry time: 0.28152; lighting: 0.00912.
- Forward + Prepass costs 61.24% more than Forward on average.
- Deferred + Prepass costs 68.85% more than Deferred on average.

## Interpretation limits

- There is only one timing value per sampled frame and renderer in these files. Without repeated runs, confidence intervals and run-to-run variance cannot be estimated.
- The analysis assumes frame N corresponds to the same camera pose and scene state in every renderer run.
- Differences in nominally shared passes, such as depth prepass or visibility, may include run-order, thermal, clock, or implementation differences. They should not automatically be interpreted as algorithmic cost.
- The camera path appears to favor direct Forward shading. This result does not invalidate synthetic crossover results; it identifies this particular scene/path/workload as lying on the Forward-favorable side of the measured crossover.
