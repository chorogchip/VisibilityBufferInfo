from __future__ import annotations

import itertools
import json
from pathlib import Path
from typing import Any, Iterator

from .common import fail
from .models import VALID_ARGUMENTS


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        spec = json.load(file)
    if not isinstance(spec, dict):
        fail("Experiment spec root must be an object.")
    return spec


def normalize_keys(values: dict[str, Any], section: str) -> dict[str, Any]:
    if not isinstance(values, dict):
        fail(f"Experiment spec '{section}' must be an object.")

    normalized = {
        normalized_key: value
        for key, value in values.items()
        if not (normalized_key := key.replace("-", "_")).startswith("_")
    }
    unknown = sorted(set(normalized) - VALID_ARGUMENTS)
    if unknown:
        fail(f"Unknown ProgramArgument in '{section}': {', '.join(unknown)}")
    return normalized


def sweep_over(base: dict[str, Any], sweep: dict[str, Any]) -> Iterator[dict[str, Any]]:
    names = list(sweep)
    value_lists: list[list[Any]] = []
    for name in names:
        values = sweep[name]
        if not isinstance(values, list) or not values:
            fail(f"Sweep '{name}' must be a non-empty JSON array.")
        value_lists.append(values)
    for combination in itertools.product(*value_lists):
        yield {**base, **dict(zip(names, combination))}


def samples_over(base: dict[str, Any], samples: Any) -> Iterator[dict[str, Any]]:
    if not isinstance(samples, list) or not samples:
        fail("Experiment spec 'samples' must be a non-empty JSON array.")
    for sample_index, sample in enumerate(samples):
        normalized = normalize_keys(sample, f"samples[{sample_index}]")
        yield {**base, **normalized}


def parameter_sets(spec: dict[str, Any]) -> tuple[list[dict[str, Any]], str]:
    base = normalize_keys(spec.get("base", {}), "base")
    sweep = normalize_keys(spec.get("sweep", {}), "sweep")
    has_samples = "samples" in spec

    if has_samples and sweep:
        fail("Experiment spec cannot use both non-empty 'sweep' and 'samples'.")
    if has_samples:
        return list(samples_over(base, spec["samples"])), "samples"
    if sweep:
        return list(sweep_over(base, sweep)), "sweep"
    return [base], "base"
