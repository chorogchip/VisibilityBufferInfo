# Result bundle

Input: `combined_sobol_1000_narrowed.csv`

Rows:
- all: 1000
- successful and modeled: 508
- excluded failures: 492

Main method:
- Candidate basis: all products of 1, 2, or 3 distinct variables among G, Q, O, A
- Selection: Elastic Net, 5-fold CV, one-standard-error rule
- Hierarchy: interaction constituent main effects retained
- Final constants: least-squares refit on retained terms
- Evaluation: nested 5-fold out-of-fold predictions

Files:
- `FORMULAS_AND_METRICS.md`: final equations and metrics
- `model_summary.csv`: one row per renderer/pass model
- `selected_coefficients.csv`: normalized and raw-unit coefficients
- `out_of_fold_predictions.csv`: actual, prediction, residual
- `01_pairplot_scatter_matrix.png`: pre-ML pair plot
- `02_actual_vs_predicted_grid.png`: nested-CV fit diagnostics
- `03_residual_diagnostics_grid.png`: residual diagnostics
- `04_selected_coefficient_matrix.png`: retained coefficients
- `05_total_time_response_surfaces.png`: total-time 2D projections

Note: the source data contains Forward, ForwardPrepass, and VisBuf, not Deferred.
