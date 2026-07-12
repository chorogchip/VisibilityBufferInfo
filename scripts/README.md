# Benchmark scripts

`run.py` and `plot.py` are configured by JSON files. Their only command-line
argument is the path to that JSON file.

```powershell
python .\run.py .\experiment_specs\example_geometry_sweep.json
python .\plot.py .\plot_specs\representation_overdraw.json
```

`plot.py` needs Matplotlib. Install the declared dependency once in the Python
environment selected by `python`:

```powershell
python -m pip install -r .\requirements.txt
```

## Experiment spec

`run.py` builds the Cartesian product of `sweep`, repeats it, and appends one
row per execution to `output`. `base` and `sweep` accept every `ProgramArgument`
field from `include/util/ProgramArgument.h`; both `snake_case` and `kebab-case`
keys are accepted. `base` values are shared by every run and `sweep` values must
be non-empty arrays.

The output CSV prepends `experiment`, `repeat`, and `run_index` to the renderer's
normal result row. Existing output files must have the same column schema.

## Plot spec

Every visual choice is stored in the JSON spec. Supported `kind` values are:

- `line`: `x`, `y`, optional `series`, `aggregate`, and `error`
- `grouped_bar`: `x`, `y`, optional `series`
- `stacked_bar`: `x`, `y_columns`, optional `y_labels`
- `scatter`: `x`, `y`, optional numeric `color`
- `heatmap`: `x`, `y`, `value`

Common fields:

```json
{
  "input": "../results/example.csv",
  "output": "../results/plots/example.png",
  "filter": { "material-count": 1 },
  "labels": {
    "title": "My title",
    "x": "X-axis label",
    "y": "Y-axis label",
    "legend": "Legend label",
    "color": "Colorbar label"
  },
  "figure": { "width": 10, "height": 6, "dpi": 180 },
  "style": { "grid": true, "log_x": false, "log_y": false }
}
```

`input` may be a path or a list of glob paths, and every relative path is
resolved from the JSON spec's directory. `aggregate` supports `mean`, `median`,
`min`, `max`, and `none`. `line` error bars support `none`, `std`, and `minmax`.
