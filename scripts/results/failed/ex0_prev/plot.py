"""Render benchmark plots from a JSON specification.

Usage:
    python plot.py path/to/plot.json

All plot choices belong in the JSON file. See plot_specs/ for examples.
"""

from __future__ import annotations

import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

import matplotlib.pyplot as plt


SUPPORTED_KINDS = {"line", "grouped_bar", "stacked_bar", "scatter", "heatmap"}
SUPPORTED_AGGREGATES = {"mean", "median", "min", "max", "none"}


def fail(message: str) -> None:
    raise ValueError(message)


def read_spec(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        spec = json.load(file)
    if not isinstance(spec, dict):
        fail("Plot spec root must be a JSON object.")
    return spec


def as_list(value: Any) -> list[Any]:
    return value if isinstance(value, list) else [value]


def resolve_inputs(spec_dir: Path, input_value: Any) -> list[Path]:
    paths: list[Path] = []
    for pattern in as_list(input_value):
        if not isinstance(pattern, str):
            fail("'input' must be a path or a list of paths.")
        candidate = Path(pattern)
        candidate = candidate if candidate.is_absolute() else spec_dir / candidate
        if any(character in str(candidate) for character in "*?["):
            paths.extend(sorted(candidate.parent.glob(candidate.name)))
        elif candidate.exists():
            paths.append(candidate)
        else:
            fail(f"Input CSV does not exist: {candidate}")
    if not paths:
        fail("No input CSV files matched the plot spec.")
    return paths


def load_rows(paths: Iterable[Path]) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for path in paths:
        with path.open("r", encoding="utf-8-sig", newline="") as file:
            reader = csv.DictReader(file)
            if not reader.fieldnames:
                fail(f"CSV has no header: {path}")
            for row in reader:
                row["__source_file"] = path.name
                rows.append(row)
    if not rows:
        fail("Input CSV files contain no data rows.")
    return rows


def require_columns(rows: list[dict[str, str]], columns: Iterable[str]) -> None:
    available = set().union(*(row.keys() for row in rows))
    missing = [column for column in columns if column and column not in available]
    if missing:
        fail(f"Missing CSV column(s): {', '.join(missing)}")


def filter_rows(rows: list[dict[str, str]], filters: dict[str, Any]) -> list[dict[str, str]]:
    if not filters:
        return rows
    require_columns(rows, filters.keys())
    filtered: list[dict[str, str]] = []
    for row in rows:
        if all(row[column] in filter_values(wanted) for column, wanted in filters.items()):
            filtered.append(row)
    if not filtered:
        fail("The filter removed every input row.")
    return filtered


def filter_values(value: Any) -> set[str]:
    """Match C++ bool output (0/1) when the JSON spec uses true/false."""
    values: set[str] = set()
    for item in as_list(value):
        if item is True:
            values.update({"1", "true", "True"})
        elif item is False:
            values.update({"0", "false", "False"})
        else:
            values.add(str(item))
    return values


def number(row: dict[str, str], column: str) -> float:
    try:
        return float(row[column])
    except (KeyError, TypeError, ValueError) as error:
        fail(f"Column '{column}' must contain numeric values; got '{row.get(column)}'.")
        raise error


def aggregate(values: list[float], method: str) -> float:
    if not values:
        return math.nan
    if method == "none":
        return values[-1]
    if method == "mean":
        return statistics.fmean(values)
    if method == "median":
        return statistics.median(values)
    if method == "min":
        return min(values)
    if method == "max":
        return max(values)
    fail(f"Unsupported aggregate: {method}")
    return math.nan


def numeric_sort_key(value: str) -> tuple[int, float | str]:
    try:
        return (0, float(value))
    except ValueError:
        return (1, value)


def ordered(values: Iterable[str], sort_mode: str) -> list[str]:
    unique = list(dict.fromkeys(values))
    if sort_mode == "input":
        return unique
    if sort_mode == "lexical":
        return sorted(unique)
    if sort_mode == "numeric":
        return sorted(unique, key=numeric_sort_key)
    fail("'sort_x' must be 'numeric', 'lexical', or 'input'.")
    return unique


def label_for(value: str, mapping: dict[str, str]) -> str:
    return mapping.get(value, value)


def setup_axes(spec: dict[str, Any]):
    figure_spec = spec.get("figure", {})
    if not isinstance(figure_spec, dict):
        fail("'figure' must be an object.")
    figure, axis = plt.subplots(
        figsize=(float(figure_spec.get("width", 9)), float(figure_spec.get("height", 5.5))),
        dpi=int(figure_spec.get("dpi", 180)),
    )
    labels = spec.get("labels", {})
    if not isinstance(labels, dict):
        fail("'labels' must be an object.")
    if labels.get("title"):
        axis.set_title(str(labels["title"]))
    if labels.get("x"):
        axis.set_xlabel(str(labels["x"]))
    if labels.get("y"):
        axis.set_ylabel(str(labels["y"]))
    style = spec.get("style", {})
    if style.get("grid", True):
        axis.grid(True, alpha=0.3)
    if style.get("log_x", False):
        axis.set_xscale("log")
    if style.get("log_y", False):
        axis.set_yscale("log")
    return figure, axis, labels


def grouped_values(rows: list[dict[str, str]], x: str, y: str, series: str | None) -> dict[tuple[str, str], list[float]]:
    grouped: dict[tuple[str, str], list[float]] = defaultdict(list)
    for row in rows:
        grouped[(row[x], row[series] if series else "")].append(number(row, y))
    return grouped


def error_values(values: list[float], mode: str) -> tuple[float, float] | None:
    if mode == "none" or len(values) < 2:
        return None
    center = statistics.median(values)
    if mode == "std":
        deviation = statistics.stdev(values)
        return deviation, deviation
    if mode == "minmax":
        return center - min(values), max(values) - center
    fail("'error.mode' must be 'none', 'std', or 'minmax'.")
    return None


def plot_line_or_grouped_bar(spec: dict[str, Any], rows: list[dict[str, str]], kind: str) -> None:
    x = spec.get("x")
    y = spec.get("y")
    series = spec.get("series")
    if not isinstance(x, str) or not isinstance(y, str):
        fail(f"'{kind}' requires string 'x' and 'y' columns.")
    if series is not None and not isinstance(series, str):
        fail("'series' must be a column name.")
    require_columns(rows, [x, y, series] if series else [x, y])
    aggregate_method = str(spec.get("aggregate", "median"))
    if aggregate_method not in SUPPORTED_AGGREGATES:
        fail(f"Unsupported aggregate: {aggregate_method}")
    x_values = ordered((row[x] for row in rows), str(spec.get("sort_x", "numeric")))
    series_values = ordered((row[series] for row in rows), "input") if series else [""]
    groups = grouped_values(rows, x, y, series)
    error_spec = spec.get("error", {})
    error_mode = str(error_spec.get("mode", "none")) if isinstance(error_spec, dict) else "none"
    series_labels = spec.get("series_labels", {})
    if not isinstance(series_labels, dict):
        fail("'series_labels' must be an object.")
    figure, axis, labels = setup_axes(spec)
    positions = list(range(len(x_values)))
    if kind == "line":
        for current_series in series_values:
            raw = [groups.get((x_value, current_series), []) for x_value in x_values]
            values = [aggregate(group, aggregate_method) for group in raw]
            errors = [error_values(group, error_mode) for group in raw]
            if any(error is not None for error in errors):
                lower = [error[0] if error else 0.0 for error in errors]
                upper = [error[1] if error else 0.0 for error in errors]
                axis.errorbar(positions, values, yerr=[lower, upper], marker="o", capsize=3,
                              label=label_for(current_series, series_labels))
            else:
                axis.plot(positions, values, marker="o", label=label_for(current_series, series_labels))
    else:
        width = 0.8 / max(1, len(series_values))
        for index, current_series in enumerate(series_values):
            offset = (index - (len(series_values) - 1) / 2) * width
            values = [aggregate(groups.get((x_value, current_series), []), aggregate_method) for x_value in x_values]
            axis.bar([position + offset for position in positions], values, width=width,
                     label=label_for(current_series, series_labels))
    axis.set_xticks(positions, x_values)
    if series:
        axis.legend(title=labels.get("legend", series))
    axis.tick_params(axis="x", rotation=spec.get("x_rotation", 0))
    save_figure(figure, spec)


def plot_stacked_bar(spec: dict[str, Any], rows: list[dict[str, str]]) -> None:
    x = spec.get("x")
    y_columns = spec.get("y_columns")
    if not isinstance(x, str) or not isinstance(y_columns, list) or not all(isinstance(column, str) for column in y_columns):
        fail("'stacked_bar' requires string 'x' and string-list 'y_columns'.")
    require_columns(rows, [x, *y_columns])
    aggregate_method = str(spec.get("aggregate", "median"))
    x_values = ordered((row[x] for row in rows), str(spec.get("sort_x", "input")))
    groups: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[row[x]].append(row)
    y_labels = spec.get("y_labels", {})
    if isinstance(y_labels, list):
        y_labels = dict(zip(y_columns, y_labels))
    if not isinstance(y_labels, dict):
        fail("'y_labels' must be an object or a list matching y_columns.")
    figure, axis, _ = setup_axes(spec)
    positions = list(range(len(x_values)))
    bottom = [0.0] * len(x_values)
    for column in y_columns:
        values = [aggregate([number(row, column) for row in groups[x_value]], aggregate_method) for x_value in x_values]
        axis.bar(positions, values, bottom=bottom, label=label_for(column, y_labels))
        bottom = [previous + value for previous, value in zip(bottom, values)]
    axis.set_xticks(positions, x_values)
    axis.legend()
    axis.tick_params(axis="x", rotation=spec.get("x_rotation", 0))
    save_figure(figure, spec)


def plot_scatter(spec: dict[str, Any], rows: list[dict[str, str]]) -> None:
    x = spec.get("x")
    y = spec.get("y")
    color = spec.get("color")
    if not isinstance(x, str) or not isinstance(y, str):
        fail("'scatter' requires string 'x' and 'y' columns.")
    columns = [x, y, color] if color else [x, y]
    require_columns(rows, columns)
    figure, axis, labels = setup_axes(spec)
    if color:
        artist = axis.scatter([number(row, x) for row in rows], [number(row, y) for row in rows],
                              c=[number(row, color) for row in rows], cmap=spec.get("colormap", "viridis"))
        figure.colorbar(artist, ax=axis, label=labels.get("color", color))
    else:
        axis.scatter([number(row, x) for row in rows], [number(row, y) for row in rows])
    save_figure(figure, spec)


def plot_heatmap(spec: dict[str, Any], rows: list[dict[str, str]]) -> None:
    x = spec.get("x")
    y = spec.get("y")
    value = spec.get("value")
    if not all(isinstance(column, str) for column in [x, y, value]):
        fail("'heatmap' requires string 'x', 'y', and 'value' columns.")
    require_columns(rows, [x, y, value])
    aggregate_method = str(spec.get("aggregate", "median"))
    x_values = ordered((row[x] for row in rows), str(spec.get("sort_x", "numeric")))
    y_values = ordered((row[y] for row in rows), str(spec.get("sort_y", "numeric")))
    groups: dict[tuple[str, str], list[float]] = defaultdict(list)
    for row in rows:
        groups[(row[x], row[y])].append(number(row, value))
    matrix = [[aggregate(groups.get((x_value, y_value), []), aggregate_method) for x_value in x_values] for y_value in y_values]
    figure, axis, labels = setup_axes(spec)
    artist = axis.imshow(matrix, aspect="auto", origin="lower", cmap=spec.get("colormap", "viridis"))
    axis.set_xticks(range(len(x_values)), x_values)
    axis.set_yticks(range(len(y_values)), y_values)
    axis.tick_params(axis="x", rotation=spec.get("x_rotation", 0))
    figure.colorbar(artist, ax=axis, label=labels.get("color", value))
    save_figure(figure, spec)


def save_figure(figure, spec: dict[str, Any]) -> None:
    output_value = spec.get("output")
    if not isinstance(output_value, str) or not output_value:
        fail("Plot spec requires a non-empty 'output' path.")
    spec_path = Path(spec["__spec_path"])
    output_path = Path(output_value)
    if not output_path.is_absolute():
        output_path = spec_path.parent / output_path
    output_path.parent.mkdir(parents=True, exist_ok=True)
    figure.tight_layout()
    figure.savefig(output_path)
    plt.close(figure)
    print(f"Saved plot: {output_path}")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("Usage: python plot.py path/to/plot.json")
    spec_path = Path(sys.argv[1]).resolve()
    if not spec_path.exists():
        raise SystemExit(f"Plot spec does not exist: {spec_path}")
    spec = read_spec(spec_path)
    spec["__spec_path"] = str(spec_path)
    kind = spec.get("kind", "line")
    if kind not in SUPPORTED_KINDS:
        fail(f"Unsupported plot kind '{kind}'. Supported: {', '.join(sorted(SUPPORTED_KINDS))}")
    if "input" not in spec:
        fail("Plot spec requires 'input'.")
    rows = filter_rows(load_rows(resolve_inputs(spec_path.parent, spec["input"])), spec.get("filter", {}))
    if kind in {"line", "grouped_bar"}:
        plot_line_or_grouped_bar(spec, rows, kind)
    elif kind == "stacked_bar":
        plot_stacked_bar(spec, rows)
    elif kind == "scatter":
        plot_scatter(spec, rows)
    else:
        plot_heatmap(spec, rows)


if __name__ == "__main__":
    main()
