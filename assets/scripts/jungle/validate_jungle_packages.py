"""Validate JungleRuins GLB source-index coverage without loading images."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import mmap
from pathlib import Path
import struct
import sys
from typing import Any

import numpy as np


REGIONS = ("global", "cinematic", "extended", "pyramid")
INSTANCE_ATTRIBUTES = {
    "TRANSLATION",
    "ROTATION",
    "SCALE",
    "_JR_SOURCE_INDEX",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--package-dir", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    return parser.parse_args()


def read_document_and_binary_offset(
    file: Any,
) -> tuple[dict[str, Any], int, int]:
    magic, version, total_length = struct.unpack("<4sII", file.read(12))
    if magic != b"glTF" or version != 2:
        raise RuntimeError("Invalid GLB header")

    document: dict[str, Any] | None = None
    binary_offset: int | None = None
    while file.tell() < total_length:
        chunk_length, chunk_type = struct.unpack("<I4s", file.read(8))
        chunk_offset = file.tell()
        if chunk_type == b"JSON":
            document = json.loads(file.read(chunk_length))
        else:
            if chunk_type == b"BIN\x00":
                binary_offset = chunk_offset
            file.seek(chunk_length, 1)
    if document is None or binary_offset is None:
        raise RuntimeError("GLB is missing JSON or BIN data")
    return document, binary_offset, total_length


def validate_package(
    path: Path,
    expected_counts: dict[tuple[str, str], int],
    seen: dict[tuple[str, str], np.ndarray],
    errors: list[str],
) -> dict[str, Any]:
    instance_sets = 0
    instances = 0
    stable_ids: set[str] = set()
    with path.open("rb") as file:
        document, binary_offset, total_length = (
            read_document_and_binary_offset(file)
        )
        if total_length != path.stat().st_size:
            errors.append(f"{path.name}: header length differs")

        mapping = mmap.mmap(
            file.fileno(),
            0,
            access=mmap.ACCESS_READ,
        )
        try:
            for node in document.get("nodes", []):
                metadata = node.get("extras", {}).get("jr", {})
                stable_id = metadata.get("stable_id")
                if stable_id:
                    if stable_id in stable_ids:
                        errors.append(
                            f"{path.name}: duplicate {stable_id}"
                        )
                    stable_ids.add(stable_id)

                extension = node.get("extensions", {}).get(
                    "EXT_mesh_gpu_instancing"
                )
                if not extension:
                    continue
                instance_sets += 1
                attributes = extension.get("attributes", {})
                if INSTANCE_ATTRIBUTES - set(attributes):
                    errors.append(
                        f"{path.name}: incomplete instance attributes"
                    )
                    continue

                accessors = [
                    document["accessors"][index]
                    for index in attributes.values()
                ]
                counts = {accessor["count"] for accessor in accessors}
                if len(counts) != 1:
                    errors.append(
                        f"{path.name}: instance accessor count differs"
                    )
                    continue
                instances += counts.pop()

                key = (
                    metadata.get("source_layer", ""),
                    metadata.get("source_prim", ""),
                )
                if key not in expected_counts:
                    errors.append(
                        f"{path.name}: unknown source {key}"
                    )
                    continue

                accessor = document["accessors"][
                    attributes["_JR_SOURCE_INDEX"]
                ]
                if (
                    accessor["componentType"] != 5125
                    or accessor["type"] != "SCALAR"
                ):
                    errors.append(
                        f"{path.name}: source index is not uint scalar"
                    )
                    continue
                view = document["bufferViews"][
                    accessor["bufferView"]
                ]
                byte_offset = (
                    binary_offset
                    + view.get("byteOffset", 0)
                    + accessor.get("byteOffset", 0)
                )
                values = np.frombuffer(
                    mapping,
                    dtype="<u4",
                    count=accessor["count"],
                    offset=byte_offset,
                )
                expected_count = expected_counts[key]
                if len(values) and int(values.max()) >= expected_count:
                    errors.append(
                        f"{path.name}: source index out of range {key}"
                    )
                    del values
                    continue

                unique = np.unique(values)
                source_seen = seen.setdefault(
                    key,
                    np.zeros(expected_count, dtype=np.bool_),
                )
                if (
                    len(unique) != len(values)
                    or source_seen[unique].any()
                ):
                    errors.append(
                        f"{path.name}: duplicate source index {key}"
                    )
                source_seen[unique] = True
                del values, unique
        finally:
            mapping.close()

    return {
        "artifact": path.as_posix(),
        "artifact_bytes": path.stat().st_size,
        "instance_sets": instance_sets,
        "instances": instances,
    }


def main() -> None:
    args = parse_args()
    manifest = json.loads(
        args.manifest.resolve().read_text(encoding="utf-8")
    )
    package_dir = args.package_dir.resolve()
    expected_counts = {
        (source["source_layer"], source["source_prim"]):
            source["instance_count"]
        for source in manifest["instance_sources"]
    }
    seen: dict[tuple[str, str], np.ndarray] = {}
    errors: list[str] = []
    packages = []
    for region in REGIONS:
        path = package_dir / f"jungle_{region}.glb"
        if not path.is_file():
            errors.append(f"Missing package: {path}")
            continue
        packages.append(
            {
                "region": region,
                **validate_package(
                    path,
                    expected_counts,
                    seen,
                    errors,
                ),
            }
        )

    missing_sources = []
    missing_records = 0
    for key, expected_count in expected_counts.items():
        source_seen = seen.get(key)
        actual_count = (
            int(source_seen.sum())
            if source_seen is not None
            else 0
        )
        if actual_count != expected_count:
            missing_sources.append(
                {
                    "source_layer": key[0],
                    "source_prim": key[1],
                    "expected": expected_count,
                    "actual": actual_count,
                }
            )
            missing_records += expected_count - actual_count

    report = {
        "schema_version": manifest["schema_version"],
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "validation": "source_index_coverage",
        "packages": packages,
        "expected_source_streams": len(expected_counts),
        "observed_source_streams": len(seen),
        "expected_instances": sum(expected_counts.values()),
        "observed_instances": sum(
            package["instances"] for package in packages
        ),
        "missing_sources": missing_sources,
        "missing_records": missing_records,
        "errors": errors,
        "status": (
            "pass"
            if not errors and not missing_sources
            else "fail"
        ),
    }
    report_path = args.report.resolve()
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(f"JR_PACKAGE_VALIDATION={report['status']}")
    print(f"JR_PACKAGE_VALIDATION_REPORT={report_path}")
    if report["status"] != "pass":
        sys.exit(1)


if __name__ == "__main__":
    main()
