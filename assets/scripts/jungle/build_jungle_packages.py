"""Build renderer-independent JungleRuins region GLB packages.

Run from Blender 4.2 with JungleRuins_Main.blend opened. GScatter is neither
loaded nor evaluated: Blender provides authored geometry/materials and USD
provides baked point-instancer arrays.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
from typing import Any

from pxr import Usd

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from jungle_blender import (
    build_region_export_scene,
    export_base_glb,
    remove_export_scene,
)
from jungle_common import (
    SCHEMA_VERSION,
    cell_ownership_bounds_xy,
    load_json,
)
from jungle_package import (
    patch_region_glb,
    validate_region_package,
)
from jungle_usd import (
    SourceArrays,
    collect_cell_groups,
    collect_unresolved_groups,
)


REGIONS = ("global", "cinematic", "extended", "pyramid")


def parse_args() -> argparse.Namespace:
    argv = sys.argv
    argv = argv[argv.index("--") + 1 :] if "--" in argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("--scene-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument(
        "--regions",
        nargs="+",
        choices=REGIONS,
        default=list(REGIONS),
    )
    return parser.parse_args(argv)


def region_records(
    manifest: dict[str, Any],
    region: str,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    cells = [
        dict(record)
        for record in manifest["cells"]
        if record["region"] == region
    ]
    for cell in cells:
        cell["ownership_bounds_xy"] = cell_ownership_bounds_xy(
            manifest,
            cell,
        )
    systems = [
        record
        for record in manifest["systems"]
        if record["region"] == region
    ]
    cells.sort(key=lambda record: record["stable_id"])
    systems.sort(key=lambda record: record["stable_id"])
    return cells, systems


def collect_region_groups(
    manifest: dict[str, Any],
    stage: Usd.Stage | None,
    region: str,
    cells: list[dict[str, Any]],
    systems: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    if stage is None:
        return []

    systems_by_cell: dict[str, list[dict[str, Any]]] = {}
    for system in systems:
        systems_by_cell.setdefault(system["cell"], []).append(system)
    cache: dict[str, SourceArrays] = {}
    groups: list[dict[str, Any]] = []
    for cell in cells:
        for system in systems_by_cell.get(cell["cell"], []):
            groups.extend(
                collect_cell_groups(
                    manifest,
                    stage,
                    cell,
                    system,
                    cache,
                )
            )
    groups.extend(
        collect_unresolved_groups(
            manifest,
            stage,
            region,
            cells,
            cache,
        )
    )
    return groups


def build_region(
    manifest: dict[str, Any],
    stage: Usd.Stage | None,
    region: str,
    output_dir: Path,
) -> dict[str, Any]:
    print(f"JR_BUILD_REGION_START={region}", flush=True)
    cells, systems = region_records(manifest, region)
    groups = collect_region_groups(
        manifest,
        stage,
        region,
        cells,
        systems,
    )
    if region != "global":
        expected_instances = sum(
            source["instance_count"]
            for source in manifest["instance_sources"]
            if source["region"] == region
        )
        selected_instances = sum(
            len(group["translations"]) for group in groups
        )
        expected_origins = sum(
            source["exact_origin_record_count"]
            for source in manifest["instance_sources"]
            if source["region"] == region
        )
        selected_origins = sum(
            group["exact_origin_count"] for group in groups
        )
        if selected_instances != expected_instances:
            raise RuntimeError(
                f"{region} source coverage differs: "
                f"{selected_instances} != {expected_instances}"
            )
        if selected_origins != expected_origins:
            raise RuntimeError(
                f"{region} exact-origin coverage differs: "
                f"{selected_origins} != {expected_origins}"
            )
    objects, prototype_names = build_region_export_scene(
        manifest,
        region,
        cells,
        systems,
        groups,
    )
    output_path = output_dir / f"jungle_{region}.glb"
    base_path = output_dir / f".jungle_{region}_blender_base.glb"
    try:
        export_base_glb(objects, base_path)
        expected = patch_region_glb(
            base_path,
            output_path,
            manifest,
            region,
            cells,
            systems,
            groups,
            prototype_names,
        )
    finally:
        remove_export_scene(objects)
        if base_path.is_file():
            base_path.unlink()
    report = validate_region_package(output_path, expected)
    if report["status"] != "pass":
        raise RuntimeError(
            f"{region} package validation failed: {report['errors']}"
        )
    print(
        f"JR_BUILD_REGION_PASS={region}:"
        f"{report['instances']}:{report['artifact_bytes']}",
        flush=True,
    )
    return report


def main() -> None:
    args = parse_args()
    scene_root = args.scene_root.resolve()
    manifest_path = args.manifest.resolve()
    output_dir = args.output_dir.resolve()
    report_path = args.report.resolve()
    manifest = load_json(manifest_path)
    output_dir.mkdir(parents=True, exist_ok=True)

    stage: Usd.Stage | None = None
    if any(region != "global" for region in args.regions):
        usd_path = scene_root / manifest["source_files"]["usd"]
        stage = Usd.Stage.Open(str(usd_path), load=Usd.Stage.LoadNone)
        if stage is None:
            raise RuntimeError(f"Unable to open USD stage: {usd_path}")

    built_packages = [
        build_region(
            manifest,
            stage,
            region,
            output_dir,
        )
        for region in args.regions
    ]
    catalog_path = output_dir / "jungle_packages.json"
    packages_by_region: dict[str, dict[str, Any]] = {}
    if catalog_path.is_file():
        previous = load_json(catalog_path)
        for package in previous.get("packages", []):
            artifact = Path(package.get("artifact", ""))
            if (
                package.get("region") in REGIONS
                and package.get("status") == "pass"
                and artifact.is_file()
            ):
                package.setdefault(
                    "unresolved_instances",
                    package.get("unresolved_origin_instances", 0),
                )
                package.setdefault(
                    "exact_origin_instances",
                    sum(
                        source["exact_origin_record_count"]
                        for source in manifest["instance_sources"]
                        if source["region"] == package["region"]
                    ),
                )
                packages_by_region[package["region"]] = package
    for package in built_packages:
        packages_by_region[package["region"]] = package
    packages = [
        packages_by_region[region]
        for region in REGIONS
        if region in packages_by_region
    ]
    complete = set(packages_by_region) == set(REGIONS)
    coverage = {
        "expected_cells": len(manifest["cells"]),
        "actual_cells": sum(
            package["cells"] for package in packages
        ),
        "expected_systems": len(manifest["systems"]),
        "actual_systems": sum(
            package["systems"] for package in packages
        ),
        "expected_instances": manifest["statistics"][
            "instance_count"
        ],
        "actual_instances": sum(
            package["instances"] for package in packages
        ),
        "expected_exact_origin_instances": manifest["statistics"][
            "exact_origin_record_count"
        ],
        "actual_exact_origin_instances": sum(
            package["exact_origin_instances"]
            for package in packages
        ),
    }
    coverage["status"] = (
        "pass"
        if complete
        and coverage["actual_cells"] == coverage["expected_cells"]
        and coverage["actual_systems"] == coverage["expected_systems"]
        and coverage["actual_instances"] == coverage["expected_instances"]
        and coverage["actual_exact_origin_instances"]
        == coverage["expected_exact_origin_instances"]
        else ("incomplete" if not complete else "fail")
    )
    report = {
        "schema_version": SCHEMA_VERSION,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source_root": scene_root.as_posix(),
        "source_files": manifest["source_files"],
        "physical_layout": "four_region_packages",
        "requested_regions": list(args.regions),
        "complete": complete,
        "packages": packages,
        "coverage": coverage,
        "status": (
            "pass"
            if all(
                package["status"] == "pass"
                for package in built_packages
            )
            and coverage["status"] != "fail"
            else "fail"
        ),
    }
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    catalog_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"JR_PACKAGE_REPORT={report_path}")
    for package in packages:
        print(
            f"JR_PACKAGE_{package['region'].upper()}="
            f"{package['artifact']}"
        )


if __name__ == "__main__":
    main()
