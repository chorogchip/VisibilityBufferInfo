"""Read and spatially partition baked JungleRuins USD instancer arrays."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np
from pxr import Usd, UsdGeom


@dataclass
class SourceArrays:
    translations: np.ndarray
    rotations: np.ndarray
    scales: np.ndarray
    prototype_indices: np.ndarray
    prototype_targets: list[str]
    source_indices: np.ndarray
    origin_mask: np.ndarray


def read_source_arrays(
    stage: Usd.Stage,
    source: dict[str, Any],
    unit_scale: float,
) -> SourceArrays:
    prim = stage.GetPrimAtPath(source["source_prim"])
    if not prim or not prim.IsA(UsdGeom.PointInstancer):
        raise RuntimeError(
            f"Missing PointInstancer: {source['source_prim']}"
        )

    instancer = UsdGeom.PointInstancer(prim)
    positions_raw = instancer.GetPositionsAttr().Get(
        Usd.TimeCode.Default()
    )
    prototype_indices_raw = instancer.GetProtoIndicesAttr().Get(
        Usd.TimeCode.Default()
    )
    orientations_raw = instancer.GetOrientationsAttr().Get(
        Usd.TimeCode.Default()
    )
    scales_raw = instancer.GetScalesAttr().Get(Usd.TimeCode.Default())
    prototype_targets = [
        str(path) for path in instancer.GetPrototypesRel().GetTargets()
    ]

    positions = np.asarray(positions_raw, dtype=np.float32)
    prototype_indices = np.asarray(
        prototype_indices_raw,
        dtype=np.int64,
    )
    if len(positions) != source["instance_count"]:
        raise RuntimeError(
            f"Position count changed for {source['source_prim']}"
        )
    if len(prototype_indices) != len(positions):
        raise RuntimeError(
            f"Prototype-index count mismatch: {source['source_prim']}"
        )

    if orientations_raw is None or len(orientations_raw) == 0:
        rotations = np.zeros((len(positions), 4), dtype=np.float32)
        rotations[:, 3] = 1.0
    else:
        rotations = np.empty((len(positions), 4), dtype=np.float32)
        for index, orientation in enumerate(orientations_raw):
            imaginary = orientation.GetImaginary()
            rotations[index] = [
                imaginary[0],
                imaginary[2],
                -imaginary[1],
                orientation.GetReal(),
            ]

    if scales_raw is None or len(scales_raw) == 0:
        scales = np.ones((len(positions), 3), dtype=np.float32)
    else:
        source_scales = np.asarray(scales_raw, dtype=np.float32)
        scales = source_scales[:, [0, 2, 1]]

    translations = np.empty((len(positions), 3), dtype=np.float32)
    translations[:, 0] = positions[:, 0] * unit_scale
    translations[:, 1] = positions[:, 2] * unit_scale
    translations[:, 2] = -positions[:, 1] * unit_scale
    return SourceArrays(
        translations=translations,
        rotations=rotations,
        scales=scales,
        prototype_indices=prototype_indices,
        prototype_targets=prototype_targets,
        source_indices=np.arange(len(positions), dtype=np.uint32),
        origin_mask=np.all(positions == 0.0, axis=1),
    )


def groups_from_selection(
    arrays: SourceArrays,
    source: dict[str, Any],
    selection: np.ndarray,
) -> list[dict[str, Any]]:
    selected = np.nonzero(selection)[0]
    if len(selected) == 0:
        return []

    groups: list[dict[str, Any]] = []
    selected_prototypes = arrays.prototype_indices[selected]
    for prototype_index in sorted(set(selected_prototypes.tolist())):
        if (
            prototype_index < 0
            or prototype_index >= len(arrays.prototype_targets)
        ):
            raise RuntimeError(
                f"Invalid prototype index {prototype_index}: "
                f"{source['source_prim']}"
            )
        group_indices = selected[
            selected_prototypes == prototype_index
        ]
        groups.append(
            {
                "source": source,
                "prototype_index": int(prototype_index),
                "prototype_target_name": Path(
                    arrays.prototype_targets[prototype_index]
                ).name,
                "translations": np.ascontiguousarray(
                    arrays.translations[group_indices]
                ),
                "rotations": np.ascontiguousarray(
                    arrays.rotations[group_indices]
                ),
                "scales": np.ascontiguousarray(
                    arrays.scales[group_indices]
                ),
                "source_indices": np.ascontiguousarray(
                    arrays.source_indices[group_indices]
                ),
                "exact_origin_count": int(
                    np.count_nonzero(
                        arrays.origin_mask[group_indices]
                    )
                ),
            }
        )
    return groups


def select_for_cell(
    arrays: SourceArrays,
    source: dict[str, Any],
    cell: dict[str, Any],
) -> np.ndarray:
    if source["placement_classification"] == "direct_cell":
        return np.full(
            len(arrays.translations),
            source["target_cell_id"] == cell["stable_id"],
            dtype=bool,
        )

    selection = ~arrays.origin_mask
    if cell["stable_id"] not in source["candidate_cell_ids"]:
        return np.zeros(len(selection), dtype=bool)

    bounds = cell["ownership_bounds_xy"]
    # Runtime/glTF coordinates are (source X, source Z, -source Y).
    source_xy = arrays.translations[:, [0, 2]]
    source_xy[:, 1] *= -1.0
    for axis in range(2):
        selection &= source_xy[:, axis] >= bounds["min"][axis]
        if bounds["max_inclusive"][axis]:
            selection &= source_xy[:, axis] <= bounds["max"][axis]
        else:
            selection &= source_xy[:, axis] < bounds["max"][axis]
    return selection


def collect_cell_groups(
    manifest: dict[str, Any],
    stage: Usd.Stage,
    cell: dict[str, Any],
    system: dict[str, Any],
    cache: dict[str, SourceArrays],
) -> list[dict[str, Any]]:
    unit_scale = float(
        manifest["coordinate_system"]["source"]["meters_per_unit"]
    )
    sources = [
        source
        for source in manifest["instance_sources"]
        if source["species"] == system["species"]
        and (
            source["target_system_id"] == system["stable_id"]
            or system["stable_id"] in source["candidate_system_ids"]
        )
    ]
    groups: list[dict[str, Any]] = []
    for source in sources:
        arrays = cache.get(source["stable_id"])
        if arrays is None:
            arrays = read_source_arrays(stage, source, unit_scale)
            cache[source["stable_id"]] = arrays
        selection = select_for_cell(arrays, source, cell)
        source_groups = groups_from_selection(arrays, source, selection)
        for group in source_groups:
            group["cell"] = cell
            group["system"] = system
            group["unresolved"] = False
            group["unresolved_reason"] = ""
        groups.extend(source_groups)
    return groups


def collect_unresolved_groups(
    manifest: dict[str, Any],
    stage: Usd.Stage,
    region: str,
    cells: list[dict[str, Any]],
    cache: dict[str, SourceArrays],
) -> list[dict[str, Any]]:
    unit_scale = float(
        manifest["coordinate_system"]["source"]["meters_per_unit"]
    )
    groups: list[dict[str, Any]] = []
    for source in manifest["instance_sources"]:
        if (
            source["region"] != region
            or source["placement_classification"] == "direct_cell"
        ):
            continue
        arrays = cache.get(source["stable_id"])
        if arrays is None:
            arrays = read_source_arrays(stage, source, unit_scale)
            cache[source["stable_id"]] = arrays
        assigned = np.zeros(len(arrays.translations), dtype=bool)
        for cell in cells:
            if cell["stable_id"] in source["candidate_cell_ids"]:
                assigned |= select_for_cell(arrays, source, cell)

        unresolved = ~assigned
        selections = (
            ("exact_origin", unresolved & arrays.origin_mask),
            (
                "outside_cell_ownership",
                unresolved & ~arrays.origin_mask,
            ),
        )
        for reason, selection in selections:
            source_groups = groups_from_selection(
                arrays,
                source,
                selection,
            )
            for group in source_groups:
                group["cell"] = None
                group["system"] = None
                group["unresolved"] = True
                group["unresolved_reason"] = reason
            groups.extend(source_groups)
    return groups
