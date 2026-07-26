"""Small shared helpers for the JungleRuins conversion tools."""

from __future__ import annotations

import json
from pathlib import Path
import re
from typing import Any


SCHEMA_VERSION = "0.2"
TEXTURE_ENCODING = "WEBP"
TEXTURE_QUALITY = 85


def slug(value: str) -> str:
    value = value.strip().lower()
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return value.strip("_")


def source_datablock_name(value: str) -> str:
    return re.sub(
        r"\s+\[[^\]]+\.blend(?:\.\d+)?\]$",
        "",
        value,
    ).strip()


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def find_one(
    records: list[dict[str, Any]],
    **criteria: Any,
) -> dict[str, Any]:
    matches = [
        record
        for record in records
        if all(record.get(key) == value for key, value in criteria.items())
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one record for {criteria}, found {len(matches)}"
        )
    return matches[0]


def cell_ownership_bounds_xy(
    manifest: dict[str, Any],
    cell: dict[str, Any],
) -> dict[str, Any]:
    region_cells = [
        record
        for record in manifest["cells"]
        if record["region"] == cell["region"]
        and record.get("grid")
        and record.get("bounds")
    ]
    if not cell.get("grid") or len(region_cells) == 1:
        bounds = cell.get("bounds")
        if not bounds:
            return {
                "min": [-float("inf"), -float("inf")],
                "max": [float("inf"), float("inf")],
                "max_inclusive": [True, True],
                "provenance": "computed",
            }
        return {
            "min": bounds["min"][:2],
            "max": bounds["max"][:2],
            "max_inclusive": [True, True],
            "provenance": "computed",
        }

    ownership_min: list[float] = []
    ownership_max: list[float] = []
    max_inclusive: list[bool] = []
    for axis, grid_key in enumerate(("x", "y")):
        grouped: dict[int, list[dict[str, Any]]] = {}
        for record in region_cells:
            grouped.setdefault(record["grid"][grid_key], []).append(record)
        ranges = []
        for key, records in grouped.items():
            lower = sum(
                record["bounds"]["min"][axis] for record in records
            ) / len(records)
            upper = sum(
                record["bounds"]["max"][axis] for record in records
            ) / len(records)
            ranges.append(
                {
                    "key": key,
                    "min": lower,
                    "max": upper,
                    "center": (lower + upper) * 0.5,
                }
            )
        ranges.sort(key=lambda record: record["center"])
        selected_key = cell["grid"][grid_key]
        selected_index = next(
            index
            for index, record in enumerate(ranges)
            if record["key"] == selected_key
        )
        selected = ranges[selected_index]
        lower = (
            selected["min"]
            if selected_index == 0
            else (
                ranges[selected_index - 1]["max"] + selected["min"]
            )
            * 0.5
        )
        is_last = selected_index == len(ranges) - 1
        upper = (
            selected["max"]
            if is_last
            else (
                selected["max"] + ranges[selected_index + 1]["min"]
            )
            * 0.5
        )
        ownership_min.append(round(float(lower), 6))
        ownership_max.append(round(float(upper), 6))
        max_inclusive.append(is_last)
    return {
        "min": ownership_min,
        "max": ownership_max,
        "max_inclusive": max_inclusive,
        "provenance": "computed",
    }


def terrain_node_name(cell: dict[str, Any]) -> str:
    return f"JR_TERRAIN__{cell['cell']}"


def cell_node_name(cell: dict[str, Any]) -> str:
    return f"JR_CELL__{cell['cell']}"


def system_node_name(system: dict[str, Any]) -> str:
    return (
        f"JR_SYSTEM__{slug(system['cell'])}__"
        f"{slug(system['species'])}"
    )
