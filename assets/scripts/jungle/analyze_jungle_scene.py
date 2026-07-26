"""Generate a renderer-independent inventory for the JungleRuins scene.

Run this script from Blender 4.2 with JungleRuins_Main.blend already opened.
The script reads Blender authoring data and the composed USD assembly. It does
not modify or save the blend file.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys
from typing import Any

import bpy
from mathutils import Vector
import numpy as np
from pxr import Usd, UsdGeom


SCHEMA_VERSION = "0.1"

USD_LAYER_SPECIES = {
    "Anthurium": "Anthurium",
    "Grass_A": "Grass_01",
    "Grass_B": "Grass_02",
    "Nettle": "Nettle",
    "Pyramid_Grass_B": "Grass_02",
    "Pyramid_Moss": "Moss",
    "QueenForest": "Queen_Forest",
    "RiverForest": "River_Forest",
    "RiverSapling": "River_Sapling",
    "RiverSeedling": "River_Seedling",
    "Shrub": "Shrub_04",
    "ShrubSorrel": "ShrubSorrel",
}


def parse_args() -> argparse.Namespace:
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    else:
        argv = []

    parser = argparse.ArgumentParser()
    parser.add_argument("--scene-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args(argv)


def clean_number(value: float) -> float:
    rounded = round(float(value), 6)
    return 0.0 if rounded == -0.0 else rounded


def clean_vec(values: Any) -> list[float]:
    return [clean_number(value) for value in values]


def slug(value: str) -> str:
    value = value.strip().lower()
    value = re.sub(r"\s+\[[^\]]+\]$", "", value)
    value = re.sub(r"[^a-z0-9]+", "_", value)
    return value.strip("_")


def source_datablock_name(value: str) -> str:
    return re.sub(
        r"\s+\[[^\]]+\.blend(?:\.\d+)?\]$",
        "",
        value,
    ).strip()


def source_stable_id(
    entity_type: str,
    name: str,
    source_library: str = "",
) -> str:
    parts = ["jr", entity_type]
    if source_library:
        parts.append(slug(source_library))
    parts.append(slug(name))
    return ":".join(parts)


def relative_path(
    path: str | Path,
    root: Path,
    library: bpy.types.Library | None = None,
) -> str:
    if not path:
        return ""
    resolved = Path(
        bpy.path.abspath(str(path), library=library)
    ).resolve()
    try:
        return resolved.relative_to(root.resolve()).as_posix()
    except ValueError:
        return resolved.as_posix()


def object_bounds(obj: bpy.types.Object) -> dict[str, list[float]]:
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    return {
        "min": [
            clean_number(min(corner[axis] for corner in corners))
            for axis in range(3)
        ],
        "max": [
            clean_number(max(corner[axis] for corner in corners))
            for axis in range(3)
        ],
    }


def object_materials(obj: bpy.types.Object) -> list[str]:
    return [
        slot.material.name_full if slot.material is not None else ""
        for slot in obj.material_slots
    ]


def object_material_ids(
    obj: bpy.types.Object,
    scene_root: Path,
) -> list[str]:
    return [
        (
            source_stable_id(
                "material",
                slot.material.name_full,
                datablock_library(slot.material, scene_root),
            )
            if slot.material is not None
            else ""
        )
        for slot in obj.material_slots
    ]


def datablock_library(datablock: Any, scene_root: Path) -> str:
    library = getattr(datablock, "library", None)
    return relative_path(library.filepath, scene_root) if library else ""


def mesh_record(
    obj: bpy.types.Object,
    scene_root: Path,
    entity_type: str = "prototype",
) -> dict[str, Any]:
    source_library = datablock_library(obj, scene_root)
    record: dict[str, Any] = {
        "name": obj.name_full,
        "source_name": source_datablock_name(obj.name_full),
        "stable_id": source_stable_id(
            entity_type,
            obj.name_full,
            source_library,
        ),
        "object_type": obj.type,
        "source_library": source_library,
        "materials": object_materials(obj),
        "material_ids": object_material_ids(obj, scene_root),
        "bounds": object_bounds(obj),
        "bounds_provenance": "computed",
    }
    if obj.type == "MESH":
        record.update(
            {
                "vertices": len(obj.data.vertices),
                "edges": len(obj.data.edges),
                "polygons": len(obj.data.polygons),
                "loops": len(obj.data.loops),
            }
        )
    return record


def canonical_extended_cell(raw: str) -> str:
    match = re.fullmatch(r"E_(\d+)", raw)
    if not match:
        return raw
    return f"E_{int(match.group(1)):02d}"


def system_record(obj: bpy.types.Object) -> dict[str, Any]:
    raw_cell, separator, species = obj.name_full.partition(" - ")
    if raw_cell.startswith("M_"):
        region = "cinematic"
        cell = raw_cell
    elif raw_cell.startswith("E_"):
        region = "extended"
        cell = canonical_extended_cell(raw_cell)
    else:
        region = "pyramid"
        cell = "Pyramid"

    modifier = next(
        (modifier for modifier in obj.modifiers if modifier.type == "NODES"),
        None,
    )
    emitter = ""
    if modifier is not None and modifier.node_group is not None:
        for node in modifier.node_group.nodes:
            if node.bl_idname != "GeometryNodeObjectInfo":
                continue
            socket = node.inputs.get("Object")
            if socket is not None and socket.default_value is not None:
                emitter = socket.default_value.name_full
                break

    return {
        "name": obj.name_full,
        "stable_id": f"jr:system:{region}:{cell}:{slug(species)}",
        "provenance": "source",
        "region": region,
        "cell": cell,
        "source_cell_name": raw_cell,
        "species": species if separator else "",
        "emitter_object": emitter,
        "geometry_nodes_group": (
            modifier.node_group.name_full
            if modifier is not None and modifier.node_group is not None
            else ""
        ),
        "modifier_enabled_viewport": bool(
            modifier.show_viewport if modifier is not None else False
        ),
        "modifier_enabled_render": bool(
            modifier.show_render if modifier is not None else False
        ),
        "hide_viewport": bool(obj.hide_viewport),
        "hide_render": bool(obj.hide_render),
    }


def terrain_grid(cell_name: str, region: str) -> dict[str, int]:
    if region == "extended":
        index = int(cell_name.removeprefix("E_"))
        return {
            "x": (index - 1) % 8,
            "y": (index - 1) // 8,
            "width": 8,
            "height": 8,
        }

    match = re.fullmatch(r"M_(\d)x(\d)_(\d+)", cell_name)
    if not match:
        return {}
    major_x, major_y, sub_index = (int(value) for value in match.groups())
    sub_index -= 1
    return {
        "x": (major_x - 3) * 2 + (sub_index % 2),
        "y": (major_y - 3) * 2 + (sub_index // 2),
        "width": 4,
        "height": 4,
    }


def terrain_record(
    obj: bpy.types.Object,
    collection_name: str,
) -> dict[str, Any]:
    if collection_name == "Main":
        region = "cinematic"
        cell = obj.name_full.removeprefix("T_")
    else:
        region = "extended"
        cell = canonical_extended_cell(obj.name_full.removeprefix("T_"))

    return {
        "name": obj.name_full,
        "stable_id": f"jr:cell:{region}:{cell}",
        "provenance": "source",
        "region": region,
        "cell": cell,
        "grid": terrain_grid(cell, region),
        "bounds": object_bounds(obj),
        "bounds_provenance": "computed",
        "vertices": len(obj.data.vertices) if obj.type == "MESH" else 0,
        "polygons": len(obj.data.polygons) if obj.type == "MESH" else 0,
        "materials": object_materials(obj),
    }


def collection_record(
    collection: bpy.types.Collection,
) -> dict[str, Any]:
    return {
        "name": collection.name_full,
        "source_library": (
            bpy.path.abspath(collection.library.filepath)
            if collection.library is not None
            else ""
        ),
        "direct_object_count": len(collection.objects),
        "recursive_object_count": len(collection.all_objects),
        "children": sorted(child.name_full for child in collection.children),
        "objects": sorted(obj.name_full for obj in collection.objects),
    }


def material_record(
    material: bpy.types.Material,
    scene_root: Path,
) -> dict[str, Any]:
    node_types: Counter[str] = Counter()
    if material.node_tree is not None:
        node_types.update(node.bl_idname for node in material.node_tree.nodes)

    source_library = datablock_library(material, scene_root)
    return {
        "name": material.name_full,
        "source_name": source_datablock_name(material.name_full),
        "stable_id": source_stable_id(
            "material",
            material.name_full,
            source_library,
        ),
        "provenance": "source",
        "source_library": source_library,
        "users": material.users,
        "use_nodes": bool(material.use_nodes),
        "node_types": dict(sorted(node_types.items())),
        "surface_render_method": str(
            getattr(
                material,
                "surface_render_method",
                getattr(material, "blend_method", ""),
            )
        ),
        "use_backface_culling": bool(material.use_backface_culling),
    }


def image_record(
    image: bpy.types.Image,
    scene_root: Path,
) -> dict[str, Any]:
    filepath = (
        relative_path(image.filepath, scene_root, image.library)
        if image.filepath
        else ""
    )
    resolved = scene_root / filepath if filepath else None
    source_library = datablock_library(image, scene_root)
    return {
        "name": image.name_full,
        "source_name": source_datablock_name(image.name_full),
        "stable_id": source_stable_id(
            "texture",
            image.name_full,
            source_library,
        ),
        "provenance": "source",
        "filepath": filepath,
        "exists": bool(resolved and resolved.exists()),
        "size": [int(image.size[0]), int(image.size[1])],
        "colorspace": image.colorspace_settings.name,
        "source": image.source,
        "packed": image.packed_file is not None,
        "source_library": source_library,
    }


def usd_source_layer(prim: Usd.Prim, usd_root: Path) -> str:
    stack = prim.GetPrimStack()
    if not stack:
        return ""
    identifier = Path(stack[0].layer.identifier).resolve()
    try:
        return identifier.relative_to(usd_root.resolve()).as_posix()
    except ValueError:
        return identifier.as_posix()


def analyze_positions(values: Any) -> dict[str, Any]:
    if values is None or len(values) == 0:
        return {
            "bounds": None,
            "non_origin_bounds": None,
            "exact_origin_count": 0,
        }
    array = np.asarray(values, dtype=np.float64)
    origin_mask = np.all(array == 0.0, axis=1)
    non_origin = array[~origin_mask]
    return {
        "bounds": {
            "min": clean_vec(array.min(axis=0)),
            "max": clean_vec(array.max(axis=0)),
        },
        "non_origin_bounds": (
            {
                "min": clean_vec(non_origin.min(axis=0)),
                "max": clean_vec(non_origin.max(axis=0)),
            }
            if len(non_origin) > 0
            else None
        ),
        "exact_origin_count": int(origin_mask.sum()),
    }


def usd_inventory(
    usd_path: Path,
) -> dict[str, Any]:
    stage = Usd.Stage.Open(str(usd_path), load=Usd.Stage.LoadNone)
    if stage is None:
        raise RuntimeError(f"Unable to open USD stage: {usd_path}")

    usd_root = usd_path.parent
    prims = list(stage.Traverse())
    type_counts = Counter(prim.GetTypeName() or "<none>" for prim in prims)
    layer_stats: dict[str, dict[str, int]] = defaultdict(
        lambda: {
            "point_instancers": 0,
            "instances": 0,
            "exact_origin_records": 0,
        }
    )
    point_instancers: list[dict[str, Any]] = []
    prototype_names: set[str] = set()

    for prim in prims:
        if not prim.IsA(UsdGeom.PointInstancer):
            continue
        instancer = UsdGeom.PointInstancer(prim)
        positions = instancer.GetPositionsAttr().Get(Usd.TimeCode.Default())
        orientations = instancer.GetOrientationsAttr().Get(
            Usd.TimeCode.Default()
        )
        scales = instancer.GetScalesAttr().Get(Usd.TimeCode.Default())
        proto_indices = instancer.GetProtoIndicesAttr().Get(
            Usd.TimeCode.Default()
        )
        prototypes = [
            str(path) for path in instancer.GetPrototypesRel().GetTargets()
        ]
        prototype_names.update(Path(path).name for path in prototypes)
        source_layer = usd_source_layer(prim, usd_root)
        instance_count = len(positions) if positions is not None else 0
        position_analysis = analyze_positions(positions)

        layer_stats[source_layer]["point_instancers"] += 1
        layer_stats[source_layer]["instances"] += instance_count
        layer_stats[source_layer]["exact_origin_records"] += position_analysis[
            "exact_origin_count"
        ]

        point_instancers.append(
            {
                "name": prim.GetName(),
                "stable_id": f"jr:point_instancer:{slug(str(prim.GetPath()))}",
                "provenance": "source",
                "source_prim": str(prim.GetPath()),
                "source_layer": source_layer,
                "instance_count": instance_count,
                "proto_index_count": (
                    len(proto_indices) if proto_indices is not None else 0
                ),
                "orientation_count": (
                    len(orientations) if orientations is not None else 0
                ),
                "scale_count": len(scales) if scales is not None else 0,
                "prototype_targets": prototypes,
                "position_bounds": position_analysis["bounds"],
                "non_origin_position_bounds": position_analysis[
                    "non_origin_bounds"
                ],
                "exact_origin_record_count": position_analysis[
                    "exact_origin_count"
                ],
                "position_bounds_provenance": "computed",
            }
        )

    root_layer = stage.GetRootLayer()
    return {
        "root_layer": usd_path.name,
        "meters_per_unit": UsdGeom.GetStageMetersPerUnit(stage),
        "up_axis": str(UsdGeom.GetStageUpAxis(stage)),
        "start_time_code": stage.GetStartTimeCode(),
        "end_time_code": stage.GetEndTimeCode(),
        "sublayers": list(root_layer.subLayerPaths),
        "prim_count": len(prims),
        "prim_types": dict(sorted(type_counts.items())),
        "mesh_prims": sorted(
            str(prim.GetPath())
            for prim in prims
            if prim.IsA(UsdGeom.Mesh)
        ),
        "material_prims": sorted(
            str(prim.GetPath())
            for prim in prims
            if prim.GetTypeName() == "Material"
        ),
        "point_instancer_count": len(point_instancers),
        "instance_count": sum(
            record["instance_count"] for record in point_instancers
        ),
        "exact_origin_record_count": sum(
            record["exact_origin_record_count"]
            for record in point_instancers
        ),
        "unique_prototype_names": sorted(prototype_names),
        "layers": [
            {"source_layer": layer, **stats}
            for layer, stats in sorted(layer_stats.items())
        ],
        "point_instancers": sorted(
            point_instancers,
            key=lambda record: record["source_prim"],
        ),
    }


def source_file_inventory(scene_root: Path) -> dict[str, Any]:
    files = sorted(path for path in scene_root.rglob("*") if path.is_file())
    return {
        "file_count": len(files),
        "byte_size": sum(path.stat().st_size for path in files),
        "top_level": [
            {
                "name": directory.name,
                "file_count": sum(1 for path in directory.rglob("*") if path.is_file()),
                "byte_size": sum(
                    path.stat().st_size
                    for path in directory.rglob("*")
                    if path.is_file()
                ),
            }
            for directory in sorted(
                (path for path in scene_root.iterdir() if path.is_dir()),
                key=lambda path: path.name,
            )
        ],
    }


def build_inventory(scene_root: Path) -> dict[str, Any]:
    main_collection = bpy.data.collections.get("Main")
    extended_collection = bpy.data.collections.get("Extended")
    systems_collection = bpy.data.collections.get("Systems")
    sources_collection = bpy.data.collections.get("Sources")
    global_collection = bpy.data.collections.get("Global")
    required = {
        "Main": main_collection,
        "Extended": extended_collection,
        "Systems": systems_collection,
        "Sources": sources_collection,
        "Global": global_collection,
    }
    missing = [name for name, collection in required.items() if collection is None]
    if missing:
        raise RuntimeError(f"Missing required Blender collections: {missing}")

    terrain = [
        terrain_record(obj, collection.name_full)
        for collection in (main_collection, extended_collection)
        for obj in sorted(collection.objects, key=lambda item: item.name_full)
    ]
    systems = [
        system_record(obj)
        for obj in sorted(systems_collection.objects, key=lambda item: item.name_full)
    ]

    source_prototypes: list[dict[str, Any]] = []
    for source_obj in sorted(
        sources_collection.objects,
        key=lambda item: item.name_full,
    ):
        instance_collection = source_obj.instance_collection
        source_prototypes.append(
            {
                "source_object": source_obj.name_full,
                "instance_type": source_obj.instance_type,
                "source_collection": (
                    instance_collection.name_full if instance_collection else ""
                ),
                "members": (
                    [
                        mesh_record(member, scene_root)
                        for member in sorted(
                            instance_collection.all_objects,
                            key=lambda item: item.name_full,
                        )
                    ]
                    if instance_collection
                    else []
                ),
            }
        )

    global_objects = [
        mesh_record(obj, scene_root, "object")
        for obj in sorted(global_collection.objects, key=lambda item: item.name_full)
    ]

    images = [
        image_record(image, scene_root)
        for image in sorted(bpy.data.images, key=lambda item: item.name_full)
    ]

    usd_path = scene_root / "USD" / "JungleRuins_Karma.usda"
    return {
        "schema_version": SCHEMA_VERSION,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "generator": {
            "name": "analyze_jungle_scene.py",
            "blender_version": bpy.app.version_string,
            "python_version": sys.version.split()[0],
            "usd_version": ".".join(str(value) for value in Usd.GetVersion()),
        },
        "contract": {
            "renderer_independent": True,
            "metadata_provenance_classes": [
                "source",
                "computed",
                "inferred",
            ],
            "target_gltf_units": "meter",
            "target_gltf_up_axis": "Y",
            "source_mutated": False,
        },
        "blender_scene_settings": {
            "unit_system": bpy.context.scene.unit_settings.system,
            "unit_scale_length": clean_number(
                bpy.context.scene.unit_settings.scale_length
            ),
            "up_axis": "Z",
        },
        "source": {
            "scene_root": scene_root.as_posix(),
            "blend_file": relative_path(bpy.data.filepath, scene_root),
            "usd_file": usd_path.relative_to(scene_root).as_posix(),
            **source_file_inventory(scene_root),
        },
        "blender": {
            "collections": [
                collection_record(collection)
                for collection in sorted(
                    bpy.data.collections,
                    key=lambda item: item.name_full,
                )
            ],
            "object_count": len(bpy.data.objects),
            "mesh_count": len(bpy.data.meshes),
            "material_count": len(bpy.data.materials),
            "image_count": len(bpy.data.images),
            "library_count": len(bpy.data.libraries),
            "missing_images": [
                record["filepath"]
                for record in images
                if record["filepath"] and not record["exists"]
            ],
            "terrain_cells": terrain,
            "scatter": {
                "system_count": len(systems),
                "counts_by_region": dict(
                    sorted(Counter(system["region"] for system in systems).items())
                ),
                "counts_by_species": dict(
                    sorted(Counter(system["species"] for system in systems).items())
                ),
                "systems": systems,
            },
            "source_prototypes": source_prototypes,
            "global_objects": global_objects,
            "materials": [
                material_record(material, scene_root)
                for material in sorted(
                    bpy.data.materials,
                    key=lambda item: item.name_full,
                )
            ],
            "images": images,
        },
        "usd": usd_inventory(usd_path),
        "open_issues": [
            {
                "id": "JR-ISSUE-0001",
                "status": "open",
                "description": (
                    "Map every composed USD PointInstancer group to Blender "
                    "M/E/Pyramid systems without relying on name similarity."
                ),
            },
            {
                "id": "JR-ISSUE-0002",
                "status": "open",
                "description": (
                    "Audit the intended visibility relationship between the "
                    "16 cinematic terrain cells and the overlapping central "
                    "extended terrain cells."
                ),
            },
            {
                "id": "JR-ISSUE-0003",
                "status": "open",
                "description": (
                    "Classify material semantics only after source material "
                    "slots and visual references are reviewed."
                ),
            },
            {
                "id": "JR-ISSUE-0004",
                "status": "open",
                "description": (
                    "Audit exact-origin records found in USD PointInstancer "
                    "arrays. Preserve them until it is proven whether they "
                    "are authored instances or export sentinels."
                ),
            },
        ],
    }


def duplicate_ids(records: list[dict[str, Any]]) -> list[str]:
    counts = Counter(record["stable_id"] for record in records)
    return sorted(stable_id for stable_id, count in counts.items() if count > 1)


def usd_layer_key(source_layer: str) -> str:
    parts = Path(source_layer).parts
    if len(parts) < 2 or parts[0] != "elements":
        return ""
    return parts[1]


def source_region(point_source: dict[str, Any]) -> tuple[str, str]:
    layer_key = usd_layer_key(point_source["source_layer"])
    if layer_key.startswith("Pyramid_"):
        return "pyramid", "source_layer"

    if layer_key in {"QueenForest", "RiverForest"}:
        match = re.match(
            rf"^{re.escape(layer_key)}_(\d{{2}})_",
            point_source["name"],
        )
        if match and 1 <= int(match.group(1)) <= 64:
            return "extended", "source_prim_numbered_group"

    return "cinematic", "source_layer_and_authoring_systems"


def scaled_xy_bounds(
    point_source: dict[str, Any],
    unit_scale: float,
) -> dict[str, list[float]] | None:
    source_bounds = (
        point_source["non_origin_position_bounds"]
        or point_source["position_bounds"]
    )
    if source_bounds is None:
        return None
    return {
        "min": [
            clean_number(source_bounds["min"][0] * unit_scale),
            clean_number(source_bounds["min"][1] * unit_scale),
        ],
        "max": [
            clean_number(source_bounds["max"][0] * unit_scale),
            clean_number(source_bounds["max"][1] * unit_scale),
        ],
    }


def bounds_contains_xy(
    container: dict[str, list[float]],
    candidate: dict[str, list[float]] | None,
    tolerance: float = 0.01,
) -> bool:
    if candidate is None:
        return False
    return all(
        candidate["min"][axis] >= container["min"][axis] - tolerance
        and candidate["max"][axis] <= container["max"][axis] + tolerance
        for axis in range(2)
    )


def bounds_overlap_xy(
    left: dict[str, list[float]],
    right: dict[str, list[float]] | None,
    tolerance: float = 0.01,
) -> bool:
    if right is None:
        return False
    return all(
        left["max"][axis] >= right["min"][axis] - tolerance
        and right["max"][axis] >= left["min"][axis] - tolerance
        for axis in range(2)
    )


def build_manifest(inventory: dict[str, Any]) -> dict[str, Any]:
    blender = inventory["blender"]
    usd = inventory["usd"]
    terrain_cells = [dict(record) for record in blender["terrain_cells"]]
    systems = [dict(record) for record in blender["scatter"]["systems"]]

    pyramid_shell = next(
        (
            record
            for record in blender["global_objects"]
            if record["name"] == "Pyramid_EmitterShell"
        ),
        None,
    )
    pyramid_cell = {
        "name": "Pyramid",
        "stable_id": "jr:cell:pyramid:Pyramid",
        "provenance": "inferred",
        "region": "pyramid",
        "cell": "Pyramid",
        "grid": {},
        "bounds": pyramid_shell["bounds"] if pyramid_shell else None,
        "bounds_provenance": "computed" if pyramid_shell else "unresolved",
        "source_object_id": pyramid_shell["stable_id"] if pyramid_shell else "",
        "vertices": 0,
        "polygons": 0,
        "materials": [],
    }
    cells = terrain_cells + [pyramid_cell]

    systems_by_region_species: dict[
        tuple[str, str],
        list[dict[str, Any]],
    ] = defaultdict(list)
    systems_by_cell: dict[str, list[str]] = defaultdict(list)
    for system in systems:
        systems_by_region_species[
            (system["region"], system["species"])
        ].append(system)
        cell_id = f"jr:cell:{system['region']}:{system['cell']}"
        systems_by_cell[cell_id].append(system["stable_id"])

    for cell in cells:
        cell["entity_type"] = "cell"
        cell["system_ids"] = sorted(systems_by_cell[cell["stable_id"]])

    prototypes: list[dict[str, Any]] = []
    prototype_name_to_ids: dict[str, list[str]] = defaultdict(list)
    for source_group in blender["source_prototypes"]:
        for member in source_group["members"]:
            prototype = {
                **member,
                "entity_type": "prototype",
                "provenance": "source",
                "source_group_object": source_group["source_object"],
                "source_collection": source_group["source_collection"],
            }
            prototypes.append(prototype)
            prototype_name_to_ids[prototype["source_name"]].append(
                prototype["stable_id"]
            )
    prototypes.sort(key=lambda record: record["stable_id"])

    unit_scale = float(usd["meters_per_unit"])
    cells_by_id = {cell["stable_id"]: cell for cell in cells}
    point_sources: list[dict[str, Any]] = []
    unknown_layers: set[str] = set()
    unknown_prototype_targets: set[str] = set()
    missing_candidate_systems: list[str] = []

    for source in usd["point_instancers"]:
        layer_key = usd_layer_key(source["source_layer"])
        species = USD_LAYER_SPECIES.get(layer_key, "")
        if not species:
            unknown_layers.add(source["source_layer"])
        region, region_evidence = source_region(source)
        candidate_systems = sorted(
            systems_by_region_species[(region, species)],
            key=lambda record: record["stable_id"],
        )
        candidate_system_ids = [
            record["stable_id"] for record in candidate_systems
        ]
        candidate_cell_ids = sorted(
            {
                f"jr:cell:{record['region']}:{record['cell']}"
                for record in candidate_systems
            }
        )
        position_bounds_meters_xy = scaled_xy_bounds(source, unit_scale)
        containing_cell_ids = [
            cell_id
            for cell_id in candidate_cell_ids
            if cells_by_id.get(cell_id, {}).get("bounds")
            and bounds_contains_xy(
                cells_by_id[cell_id]["bounds"],
                position_bounds_meters_xy,
            )
        ]
        overlapping_cell_ids = [
            cell_id
            for cell_id in candidate_cell_ids
            if cells_by_id.get(cell_id, {}).get("bounds")
            and bounds_overlap_xy(
                cells_by_id[cell_id]["bounds"],
                position_bounds_meters_xy,
            )
        ]

        if len(containing_cell_ids) == 1:
            classification = "direct_cell"
            target_cell_id = containing_cell_ids[0]
            direct_systems = [
                record["stable_id"]
                for record in candidate_systems
                if (
                    f"jr:cell:{record['region']}:{record['cell']}"
                    == target_cell_id
                )
            ]
            target_system_id = (
                direct_systems[0] if len(direct_systems) == 1 else ""
            )
        else:
            classification = "requires_spatial_split"
            target_cell_id = ""
            target_system_id = ""

        if not candidate_system_ids:
            missing_candidate_systems.append(source["stable_id"])

        prototype_ids: list[str] = []
        prototype_target_names: list[str] = []
        for target in source["prototype_targets"]:
            target_name = Path(target).name
            prototype_target_names.append(target_name)
            matches = prototype_name_to_ids.get(target_name, [])
            if len(matches) == 1:
                prototype_ids.append(matches[0])
            else:
                unknown_prototype_targets.add(target_name)

        non_origin_count = (
            source["instance_count"]
            - source["exact_origin_record_count"]
        )
        point_sources.append(
            {
                "stable_id": source["stable_id"],
                "entity_type": "instance_source",
                "provenance": "source",
                "source_layer": source["source_layer"],
                "source_prim": source["source_prim"],
                "source_array": {
                    "first_index": 0,
                    "count": source["instance_count"],
                    "identity_rule": (
                        "source_layer + source_prim + source_array_index"
                    ),
                },
                "species": species,
                "region": region,
                "region_assignment_provenance": "inferred",
                "region_assignment_evidence": region_evidence,
                "placement_classification": classification,
                "placement_classification_provenance": "computed",
                "target_cell_id": target_cell_id,
                "target_system_id": target_system_id,
                "candidate_cell_ids": candidate_cell_ids,
                "containing_cell_ids": containing_cell_ids,
                "overlapping_cell_ids": overlapping_cell_ids,
                "candidate_system_ids": candidate_system_ids,
                "prototype_target_names": prototype_target_names,
                "prototype_ids": sorted(set(prototype_ids)),
                "instance_count": source["instance_count"],
                "non_origin_instance_count": non_origin_count,
                "exact_origin_record_count": source[
                    "exact_origin_record_count"
                ],
                "exact_origin_assignment": (
                    "unresolved"
                    if source["exact_origin_record_count"]
                    else "not_applicable"
                ),
                "position_bounds_source": source["position_bounds"],
                "non_origin_position_bounds_source": source[
                    "non_origin_position_bounds"
                ],
                "non_origin_position_bounds_meters_xy": (
                    position_bounds_meters_xy
                ),
            }
        )

    point_sources.sort(key=lambda record: record["source_prim"])
    classification_point_counts = Counter(
        record["placement_classification"] for record in point_sources
    )
    classification_instance_counts = Counter()
    for record in point_sources:
        classification_instance_counts[
            record["placement_classification"]
        ] += record["non_origin_instance_count"]

    regions: list[dict[str, Any]] = []
    for region_name in ("global", "cinematic", "extended", "pyramid"):
        region_cells = sorted(
            cell["stable_id"]
            for cell in cells
            if cell["region"] == region_name
        )
        region_systems = sorted(
            system["stable_id"]
            for system in systems
            if system["region"] == region_name
        )
        regions.append(
            {
                "name": region_name,
                "stable_id": f"jr:region:{region_name}",
                "entity_type": "region",
                "provenance": "source",
                "cell_ids": region_cells,
                "system_ids": region_systems,
            }
        )

    materials = [
        {**record, "entity_type": "material"}
        for record in blender["materials"]
    ]
    textures = [
        {**record, "entity_type": "texture"}
        for record in blender["images"]
    ]
    validations = {
        "duplicate_cell_ids": duplicate_ids(cells),
        "duplicate_system_ids": duplicate_ids(systems),
        "duplicate_prototype_ids": duplicate_ids(prototypes),
        "duplicate_material_ids": duplicate_ids(materials),
        "duplicate_texture_ids": duplicate_ids(textures),
        "unknown_usd_layers": sorted(unknown_layers),
        "unknown_prototype_targets": sorted(unknown_prototype_targets),
        "point_sources_without_candidate_systems": sorted(
            set(missing_candidate_systems)
        ),
        "missing_images": blender["missing_images"],
        "pyramid_bounds_resolved": pyramid_shell is not None,
    }
    list_validation_keys = [
        key
        for key, value in validations.items()
        if isinstance(value, list)
    ]
    validation_error_count = sum(
        len(validations[key]) for key in list_validation_keys
    )
    if not validations["pyramid_bounds_resolved"]:
        validation_error_count += 1

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_utc": inventory["generated_utc"],
        "generator": inventory["generator"],
        "contract": {
            **inventory["contract"],
            "logical_master_scene": True,
            "prototype_geometry_shared": True,
            "instances_unrealized": True,
            "renderer_policy_embedded": False,
        },
        "coordinate_system": {
            "source": {
                "up_axis": usd["up_axis"],
                "meters_per_unit": unit_scale,
            },
            "target_gltf": {
                "up_axis": "Y",
                "meters_per_unit": 1.0,
                "source_to_target_position": (
                    "[x * 0.01, z * 0.01, -y * 0.01]"
                ),
                "provenance": "computed",
            },
        },
        "source_files": {
            "blend": inventory["source"]["blend_file"],
            "usd": inventory["source"]["usd_file"],
        },
        "regions": regions,
        "cells": sorted(cells, key=lambda record: record["stable_id"]),
        "systems": sorted(
            (
                {
                    **record,
                    "entity_type": "system",
                }
                for record in systems
            ),
            key=lambda record: record["stable_id"],
        ),
        "prototypes": prototypes,
        "materials": sorted(
            materials,
            key=lambda record: record["stable_id"],
        ),
        "textures": sorted(
            textures,
            key=lambda record: record["stable_id"],
        ),
        "instance_sources": point_sources,
        "statistics": {
            "region_count": len(regions),
            "cell_count": len(cells),
            "system_count": len(systems),
            "prototype_count": len(prototypes),
            "material_count": len(materials),
            "texture_count": len(textures),
            "instance_source_count": len(point_sources),
            "instance_count": usd["instance_count"],
            "exact_origin_record_count": usd[
                "exact_origin_record_count"
            ],
            "point_sources_by_classification": dict(
                sorted(classification_point_counts.items())
            ),
            "non_origin_instances_by_classification": dict(
                sorted(classification_instance_counts.items())
            ),
        },
        "validation": {
            "status": "pass" if validation_error_count == 0 else "fail",
            "error_count": validation_error_count,
            **validations,
        },
        "pending_transform_work": {
            "spatial_split_required": (
                classification_point_counts["requires_spatial_split"] > 0
            ),
            "rule": (
                "For each requires_spatial_split source, assign every "
                "source transform to an authored cell XY bound while "
                "retaining source layer, prim path, and array index."
            ),
            "exact_origin_policy": (
                "Preserve exact-origin records as unresolved until source "
                "or visual evidence establishes their meaning."
            ),
        },
    }


def write_manifest_summary(
    manifest: dict[str, Any],
    path: Path,
) -> None:
    stats = manifest["statistics"]
    validation = manifest["validation"]
    lines = [
        "# JungleRuins Canonical Scene Manifest",
        "",
        "This catalog defines scene facts and source-to-canonical mappings. "
        "It does not encode renderer culling, LOD, draw batching, or pass policy.",
        "",
        f"- Schema: `{manifest['schema_version']}`",
        f"- Validation: `{validation['status']}` "
        f"({validation['error_count']} errors)",
        f"- Regions: `{stats['region_count']}`",
        f"- Cells: `{stats['cell_count']}`",
        f"- Systems: `{stats['system_count']}`",
        f"- Shared prototypes: `{stats['prototype_count']}`",
        f"- Materials: `{stats['material_count']}`",
        f"- Textures: `{stats['texture_count']}`",
        f"- USD PointInstancer sources: `{stats['instance_source_count']}`",
        f"- USD transform records: `{stats['instance_count']}`",
        f"- Exact-origin records kept unresolved: "
        f"`{stats['exact_origin_record_count']}`",
        "",
        "## Regions",
        "",
        "| Region | Cells | Systems |",
        "|---|---:|---:|",
    ]
    lines.extend(
        f"| `{region['name']}` | {len(region['cell_ids'])} | "
        f"{len(region['system_ids'])} |"
        for region in manifest["regions"]
    )
    lines.extend(
        [
            "",
            "## Instance-source placement",
            "",
            "| Classification | PointInstancers | Non-origin transforms |",
            "|---|---:|---:|",
        ]
    )
    classifications = sorted(
        stats["point_sources_by_classification"]
    )
    lines.extend(
        f"| `{classification}` | "
        f"{stats['point_sources_by_classification'][classification]} | "
        f"{stats['non_origin_instances_by_classification'].get(classification, 0)} |"
        for classification in classifications
    )
    lines.extend(
        [
            "",
            "A `direct_cell` source is wholly contained by one authored cell "
            "that owns a matching Blender scatter system. "
            "`requires_spatial_split` sources remain intact in this catalog "
            "and must be divided at transform-record granularity.",
            "",
            "## Validation details",
            "",
        ]
    )
    for key, value in validation.items():
        if key in {"status", "error_count"}:
            continue
        if isinstance(value, list):
            lines.append(f"- `{key}`: `{len(value)}`")
        else:
            lines.append(f"- `{key}`: `{value}`")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_summary(inventory: dict[str, Any], path: Path) -> None:
    source = inventory["source"]
    blender = inventory["blender"]
    scatter = blender["scatter"]
    usd = inventory["usd"]
    lines = [
        "# JungleRuins Source Inventory",
        "",
        f"- Schema: `{inventory['schema_version']}`",
        f"- Generated UTC: `{inventory['generated_utc']}`",
        f"- Blender: `{inventory['generator']['blender_version']}`",
        f"- Source files: `{source['file_count']}`",
        f"- Source bytes: `{source['byte_size']}`",
        f"- Blender objects: `{blender['object_count']}`",
        f"- Blender meshes: `{blender['mesh_count']}`",
        f"- Blender materials: `{blender['material_count']}`",
        f"- Blender images: `{blender['image_count']}`",
        f"- Missing images: `{len(blender['missing_images'])}`",
        f"- Terrain cells: `{len(blender['terrain_cells'])}`",
        f"- Scatter systems: `{scatter['system_count']}`",
        f"- Scatter by region: `{json.dumps(scatter['counts_by_region'], sort_keys=True)}`",
        f"- USD prims: `{usd['prim_count']}`",
        f"- USD point instancers: `{usd['point_instancer_count']}`",
        f"- USD instances: `{usd['instance_count']}`",
        f"- USD exact-origin records: `{usd['exact_origin_record_count']}`",
        f"- USD unique prototype names: `{len(usd['unique_prototype_names'])}`",
        "",
        "## USD point-instancer layers",
        "",
        "| Layer | Point instancers | Instances | Exact-origin records |",
        "|---|---:|---:|---:|",
    ]
    lines.extend(
        (
            f"| `{row['source_layer']}` | {row['point_instancers']} | "
            f"{row['instances']} | {row['exact_origin_records']} |"
        )
        for row in usd["layers"]
    )
    lines.extend(
        [
            "",
            "## Open issues",
            "",
        ]
    )
    lines.extend(
        f"- `{issue['id']}`: {issue['description']}"
        for issue in inventory["open_issues"]
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    args = parse_args()
    scene_root = args.scene_root.resolve()
    output = args.output.resolve()
    if not scene_root.is_dir():
        raise RuntimeError(f"Scene root does not exist: {scene_root}")

    inventory = build_inventory(scene_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    docs_output = output.parent.parent / "docs"
    docs_output.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(inventory, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    inventory_docs = docs_output / "SOURCE_INVENTORY.md"
    write_summary(inventory, inventory_docs)
    manifest = build_manifest(inventory)
    manifest_output = output.parent / "canonical_scene_manifest.json"
    manifest_output.write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    manifest_docs = docs_output / "CANONICAL_SCENE_MANIFEST.md"
    write_manifest_summary(
        manifest,
        manifest_docs,
    )
    print(f"JR_INVENTORY_JSON={output}")
    print(f"JR_INVENTORY_MD={inventory_docs}")
    print(f"JR_MANIFEST_JSON={manifest_output}")
    print(f"JR_MANIFEST_MD={manifest_docs}")


if __name__ == "__main__":
    main()
