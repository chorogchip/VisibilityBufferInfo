# -*- coding: utf-8 -*-
"""Build 1,000 shuffled Sobol samples from the uploaded TVBPerf JSON spaces.

The sampled dimensions are kept discrete and are derived from values that
actually occur in the source JSON files. Sampled variables are prefixed with
"_" in ``base`` and appear without the prefix in each ``samples`` entry.
"""

from __future__ import annotations

import json
import math
import random
from pathlib import Path
from typing import Any

from scipy.stats import qmc

ROOT = Path(__file__).resolve().parent
SOURCE_FILES = (
    ROOT / "quad_alu_orthogonal_samples_div_plus_1(1).json",
    ROOT / "overdraw_alu_orthogonal(1).json",
    ROOT / "geometry_time_orthogonal(2).json",
)
OUTPUT_FILE = ROOT / "combined_sobol_samples_1000.json"

SAMPLED_VARIABLES = (
    "geometry_div",
    "object_count",
    "overdraw_count",
    "alu_calc_count",
    "renderer_variant",
)
SAMPLE_COUNT = 1000
SOBOL_SEED = 0
SHUFFLE_SEED = 0


def plain_name(name: str) -> str:
    return name[1:] if name.startswith("_") else name


def load_sources() -> list[dict[str, Any]]:
    with_sources: list[dict[str, Any]] = []
    for path in SOURCE_FILES:
        with path.open("r", encoding="utf-8") as file:
            with_sources.append(json.load(file))
    return with_sources


def collect_allowed_values(sources: list[dict[str, Any]]) -> dict[str, list[int]]:
    values: dict[str, set[int]] = {name: set() for name in SAMPLED_VARIABLES}

    for spec in sources:
        base = spec.get("base", {})
        for raw_name, value in base.items():
            name = plain_name(raw_name)
            if name in values and isinstance(value, int) and not isinstance(value, bool):
                values[name].add(value)

        for name, candidates in spec.get("sweep", {}).items():
            if name in values:
                values[name].update(int(value) for value in candidates)

        for sample in spec.get("samples", []):
            for name in SAMPLED_VARIABLES:
                if name in sample:
                    values[name].add(int(sample[name]))

    allowed = {name: sorted(candidates) for name, candidates in values.items()}
    missing = [name for name, candidates in allowed.items() if not candidates]
    if missing:
        raise ValueError(f"No values found for sampled variables: {missing}")

    # The requested renderer variants are exact, even if a future source file
    # happens to contain another variant.
    allowed["renderer_variant"] = [1, 2, 4]
    return allowed


def build_base(sources: list[dict[str, Any]]) -> dict[str, Any]:
    # Use the samples-format JSON as the ordering/value template because it
    # contains the extra camera-retention field requested by the user.
    template = dict(sources[0]["base"])

    # Normalize all names first, then reapply underscores only to sampled vars.
    normalized: dict[str, Any] = {}
    for spec in sources:
        for raw_name, value in spec.get("base", {}).items():
            normalized.setdefault(plain_name(raw_name), value)
    for raw_name, value in template.items():
        normalized[plain_name(raw_name)] = value

    normalized["renderer_variant"] = 1
    normalized["object_count"] = 256
    normalized["overdraw_count"] = 0
    normalized["geometry_div"] = 16
    normalized["alu_calc_count"] = 0
    normalized["to-remain-only-in-camera"] = False

    base: dict[str, Any] = {}
    ordered_plain_names = [plain_name(name) for name in template]
    for name in normalized:
        if name not in ordered_plain_names:
            ordered_plain_names.append(name)

    for name in ordered_plain_names:
        output_name = f"_{name}" if name in SAMPLED_VARIABLES else name
        base[output_name] = normalized[name]

    return base


def sobol_samples(allowed: dict[str, list[int]]) -> list[dict[str, int]]:
    dimensions = list(SAMPLED_VARIABLES)
    cardinalities = [len(allowed[name]) for name in dimensions]
    seen: set[tuple[int, ...]] = set()
    samples: list[dict[str, int]] = []

    # Rebuild the same scrambled sequence at increasing powers of two so the
    # prefix remains deterministic while giving constraint rejection headroom.
    start_power = math.ceil(math.log2(SAMPLE_COUNT * 2))
    for power in range(start_power, 20):
        sampler = qmc.Sobol(d=len(dimensions), scramble=True, seed=SOBOL_SEED)
        points = sampler.random_base2(power)

        for point in points:
            indices = tuple(
                min(int(float(coord) * size), size - 1)
                for coord, size in zip(point, cardinalities, strict=True)
            )
            if indices in seen:
                continue
            seen.add(indices)

            selected = {
                name: allowed[name][index]
                for name, index in zip(dimensions, indices, strict=True)
            }
            if selected["overdraw_count"] >= selected["object_count"]:
                continue

            # Keep a stable, human-friendly key order matching the source
            # sample style, with the new overdraw dimension inserted nearby.
            samples.append(
                {
                    "geometry_div": selected["geometry_div"],
                    "object_count": selected["object_count"],
                    "overdraw_count": selected["overdraw_count"],
                    "alu_calc_count": selected["alu_calc_count"],
                    "renderer_variant": selected["renderer_variant"],
                }
            )
            if len(samples) == SAMPLE_COUNT:
                random.Random(SHUFFLE_SEED).shuffle(samples)
                return samples

    raise RuntimeError("Could not generate enough unique constrained Sobol samples.")


def validate(spec: dict[str, Any], allowed: dict[str, list[int]]) -> None:
    base = spec["base"]
    samples = spec["samples"]

    assert len(samples) == SAMPLE_COUNT
    assert len({tuple(sample.items()) for sample in samples}) == SAMPLE_COUNT
    assert base["to-remain-only-in-camera"] is False

    for name in SAMPLED_VARIABLES:
        assert f"_{name}" in base
        assert name not in base

    for raw_name in base:
        if plain_name(raw_name) not in SAMPLED_VARIABLES:
            assert not raw_name.startswith("_")

    expected_sample_keys = set(SAMPLED_VARIABLES)
    for sample in samples:
        assert set(sample) == expected_sample_keys
        assert sample["overdraw_count"] < sample["object_count"]
        for name in SAMPLED_VARIABLES:
            assert sample[name] in allowed[name]


def main() -> None:
    sources = load_sources()
    allowed = collect_allowed_values(sources)
    spec = {
        "experiment": "combined_sobol_1000",
        "executable": sources[0]["executable"],
        "output": "combined_sobol_1000.csv",
        "repeat": 1,
        "keep_individual_csv": False,
        "base": build_base(sources),
        "samples": sobol_samples(allowed),
    }
    validate(spec, allowed)

    with OUTPUT_FILE.open("w", encoding="utf-8", newline="\n") as file:
        json.dump(spec, file, ensure_ascii=False, indent=2)
        file.write("\n")

    print(f"Wrote {OUTPUT_FILE}")
    print("Allowed values:")
    for name in SAMPLED_VARIABLES:
        print(f"  {name}: {allowed[name]}")


if __name__ == "__main__":
    main()
