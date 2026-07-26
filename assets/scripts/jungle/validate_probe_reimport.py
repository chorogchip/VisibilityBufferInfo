"""Re-import a JungleRuins contract probe in Blender and record the result."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Any

import bpy


def parse_args() -> argparse.Namespace:
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    else:
        argv = []
    parser = argparse.ArgumentParser()
    parser.add_argument("--glb", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--docs-output", required=True, type=Path)
    parser.add_argument("--builder-module-dir", required=True, type=Path)
    return parser.parse_args(argv)


def clean(value: float) -> float:
    return round(float(value), 6)


def main() -> None:
    args = parse_args()
    module_dir = args.builder_module_dir.resolve()
    if str(module_dir) not in sys.path:
        sys.path.insert(0, str(module_dir))
    from build_contract_probe import write_validation_docs

    glb_path = args.glb.resolve()
    report_path = args.report.resolve()
    manifest_path = args.manifest.resolve()
    docs_path = args.docs_output.resolve()
    report: dict[str, Any] = json.loads(
        report_path.read_text(encoding="utf-8")
    )
    manifest: dict[str, Any] = json.loads(
        manifest_path.read_text(encoding="utf-8")
    )

    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    result = bpy.ops.import_scene.gltf(filepath=str(glb_path))
    if "FINISHED" not in result:
        raise RuntimeError(f"Blender re-import failed: {result}")

    instance_objects = [
        obj
        for obj in bpy.data.objects
        if obj.type == "MESH"
        and not obj.name.startswith("JR_")
    ]
    expected_count = int(report["counts"]["instances"])
    errors: list[str] = []
    if len(instance_objects) != expected_count:
        errors.append(
            f"Re-imported instance objects {len(instance_objects)} != "
            f"GLB accessor count {expected_count}"
        )

    cell_name = report["scope"]["cell"]
    cell = next(
        record
        for record in manifest["cells"]
        if record["region"] == "cinematic"
        and record["cell"] == cell_name
    )
    xs = [obj.location.x for obj in instance_objects]
    ys = [obj.location.y for obj in instance_objects]
    zs = [obj.location.z for obj in instance_objects]
    bounds = {
        "min": [clean(min(xs)), clean(min(ys)), clean(min(zs))],
        "max": [clean(max(xs)), clean(max(ys)), clean(max(zs))],
    }
    tolerance = 0.02
    for axis in range(2):
        if bounds["min"][axis] < cell["bounds"]["min"][axis] - tolerance:
            errors.append(
                f"Instance min axis {axis} is outside cell bounds"
            )
        if bounds["max"][axis] > cell["bounds"]["max"][axis] + tolerance:
            errors.append(
                f"Instance max axis {axis} is outside cell bounds"
            )

    jr_objects = [
        obj for obj in bpy.data.objects if obj.name.startswith("JR_")
    ]
    report["blender_reimport"] = {
        "status": "pass" if not errors else "fail",
        "errors": errors,
        "blender_version": bpy.app.version_string,
        "instance_objects": len(instance_objects),
        "jr_objects": len(jr_objects),
        "object_count": len(bpy.data.objects),
        "mesh_datablocks": len(bpy.data.meshes),
        "material_datablocks": len(bpy.data.materials),
        "image_datablocks": len(bpy.data.images),
        "instance_origin_bounds_blender_z_up": bounds,
        "source_cell_bounds_blender_z_up": cell["bounds"],
        "extension_import_behavior": "expanded_to_editable_objects",
    }
    if errors:
        report["status"] = "fail"
        report["errors"].extend(errors)
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_validation_docs(report, docs_path)
    if errors:
        raise RuntimeError(f"Re-import validation failed: {errors}")
    print(f"JR_REIMPORT_REPORT={report_path}")
    print(f"JR_REIMPORT_DOCS={docs_path}")
    print(f"JR_REIMPORT_INSTANCES={len(instance_objects)}")
    print(f"JR_REIMPORT_BOUNDS={bounds}")


if __name__ == "__main__":
    main()
