"""Build and validate a renderer-independent JungleRuins GLB contract probe.

The probe exports one authored terrain cell and either one or all of its
complete scatter systems.
Prototype geometry/materials come from Blender, while instance transforms come
from the composed USD PointInstancers. GScatter is not imported or executed.

The script never saves the opened source blend file.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import struct
import sys
from typing import Any

import bpy
import numpy as np
from pxr import Usd, UsdGeom


SCHEMA_VERSION = "0.1"
GLB_MAGIC = b"glTF"
GLB_VERSION = 2
JSON_CHUNK = b"JSON"
BIN_CHUNK = b"BIN\x00"
FLOAT = 5126
UNSIGNED_INT = 5125
TEXTURE_ENCODING = "WEBP"
TEXTURE_QUALITY = 85


def parse_args() -> argparse.Namespace:
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1 :]
    else:
        argv = []

    parser = argparse.ArgumentParser()
    parser.add_argument("--scene-root", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--docs-output", required=True, type=Path)
    parser.add_argument("--cell", default="M_3x4_01")
    parser.add_argument("--species", default="River_Seedling")
    return parser.parse_args(argv)


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
        selected_grid_key = cell["grid"][grid_key]
        selected_index = next(
            index
            for index, record in enumerate(ranges)
            if record["key"] == selected_grid_key
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
        "rule": (
            "Adjacent terrain bounds are split at their midpoint; lower "
            "bounds are inclusive and upper bounds are exclusive except "
            "at the outer region boundary."
        ),
    }


def point_source_arrays(
    stage: Usd.Stage,
    source: dict[str, Any],
    cell: dict[str, Any],
    unit_scale: float,
) -> list[dict[str, Any]]:
    prim = stage.GetPrimAtPath(source["source_prim"])
    if not prim or not prim.IsA(UsdGeom.PointInstancer):
        raise RuntimeError(
            f"Missing PointInstancer: {source['source_prim']}"
        )

    instancer = UsdGeom.PointInstancer(prim)
    positions_raw = instancer.GetPositionsAttr().Get(
        Usd.TimeCode.Default()
    )
    proto_indices_raw = instancer.GetProtoIndicesAttr().Get(
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
    proto_indices = np.asarray(proto_indices_raw, dtype=np.int64)
    if len(positions) != source["instance_count"]:
        raise RuntimeError(
            f"Position count changed for {source['source_prim']}"
        )
    if len(proto_indices) != len(positions):
        raise RuntimeError(
            f"Proto-index count mismatch for {source['source_prim']}"
        )

    source_indices = np.arange(len(positions), dtype=np.uint32)
    origin_mask = np.all(positions == 0.0, axis=1)
    selection = ~origin_mask
    target_cell_id = cell["stable_id"]
    if source["placement_classification"] == "direct_cell":
        if source["target_cell_id"] != target_cell_id:
            selection &= False
    else:
        if target_cell_id not in source["candidate_cell_ids"]:
            selection &= False
        bounds = cell["ownership_bounds_xy"]
        positions_meters_xy = positions[:, :2] * unit_scale
        for axis in range(2):
            selection &= positions_meters_xy[:, axis] >= bounds["min"][axis]
            if bounds["max_inclusive"][axis]:
                selection &= (
                    positions_meters_xy[:, axis] <= bounds["max"][axis]
                )
            else:
                selection &= (
                    positions_meters_xy[:, axis] < bounds["max"][axis]
                )

    selected = np.nonzero(selection)[0]
    if len(selected) == 0:
        return []

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

    groups: list[dict[str, Any]] = []
    for prototype_index in sorted(set(proto_indices[selected].tolist())):
        if prototype_index < 0 or prototype_index >= len(prototype_targets):
            raise RuntimeError(
                f"Invalid prototype index {prototype_index} in "
                f"{source['source_prim']}"
            )
        group_indices = selected[
            proto_indices[selected] == prototype_index
        ]
        groups.append(
            {
                "source": source,
                "prototype_index": int(prototype_index),
                "prototype_target_name": Path(
                    prototype_targets[prototype_index]
                ).name,
                "translations": np.ascontiguousarray(
                    translations[group_indices]
                ),
                "rotations": np.ascontiguousarray(
                    rotations[group_indices]
                ),
                "scales": np.ascontiguousarray(scales[group_indices]),
                "source_indices": np.ascontiguousarray(
                    source_indices[group_indices]
                ),
            }
        )
    return groups


def collect_instance_groups(
    manifest: dict[str, Any],
    stage: Usd.Stage,
    cell: dict[str, Any],
    system: dict[str, Any],
) -> list[dict[str, Any]]:
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
    unit_scale = float(
        manifest["coordinate_system"]["source"]["meters_per_unit"]
    )
    for source in sources:
        source_groups = point_source_arrays(
            stage,
            source,
            cell,
            unit_scale,
        )
        for group in source_groups:
            group["system"] = system
        groups.extend(source_groups)
    return groups


def new_empty(
    collection: bpy.types.Collection,
    name: str,
    parent: bpy.types.Object | None = None,
) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, None)
    collection.objects.link(obj)
    obj.parent = parent
    return obj


def copy_mesh_object(
    collection: bpy.types.Collection,
    source: bpy.types.Object,
    name: str,
    parent: bpy.types.Object,
) -> bpy.types.Object:
    obj = source.copy()
    obj.name = name
    collection.objects.link(obj)
    obj.parent = parent
    obj.matrix_world = source.matrix_world.copy()
    obj.hide_viewport = False
    obj.hide_render = False
    return obj


def build_blender_export_scene(
    manifest: dict[str, Any],
    cell: dict[str, Any],
    systems: list[dict[str, Any]],
    groups: list[dict[str, Any]],
) -> tuple[list[bpy.types.Object], dict[str, str]]:
    export_collection = bpy.data.collections.new("JR_CONTRACT_PROBE")
    bpy.context.scene.collection.children.link(export_collection)

    root = new_empty(export_collection, "JR_ROOT")
    regions = new_empty(export_collection, "JR_REGIONS", root)
    region = new_empty(
        export_collection,
        f"JR_REGION__{cell['region']}",
        regions,
    )
    cell_node = new_empty(
        export_collection,
        f"JR_CELL__{cell['cell']}",
        region,
    )
    terrain_group = new_empty(
        export_collection,
        "JR_TERRAIN_GROUP",
        cell_node,
    )
    systems_group = new_empty(
        export_collection,
        "JR_SYSTEMS",
        cell_node,
    )
    for system in systems:
        new_empty(
            export_collection,
            f"JR_SYSTEM__{slug(system['species'])}",
            systems_group,
        )
    prototypes_group = new_empty(
        export_collection,
        "JR_PROTOTYPES",
        root,
    )

    terrain_source = bpy.data.objects.get(cell["name"])
    if terrain_source is None:
        raise RuntimeError(f"Missing terrain object: {cell['name']}")
    terrain_obj = copy_mesh_object(
        export_collection,
        terrain_source,
        f"JR_TERRAIN__{cell['cell']}",
        terrain_group,
    )

    prototype_records = {
        record["source_name"]: record
        for record in manifest["prototypes"]
    }
    blender_prototypes: dict[str, bpy.types.Object] = {}
    for obj in bpy.data.objects:
        source_name = source_datablock_name(obj.name_full)
        if source_name in prototype_records and obj.type == "MESH":
            blender_prototypes[source_name] = obj

    prototype_export_names: dict[str, str] = {}
    required_names = sorted(
        {group["prototype_target_name"] for group in groups}
    )
    for source_name in required_names:
        source_obj = blender_prototypes.get(source_name)
        if source_obj is None:
            raise RuntimeError(
                f"Missing Blender prototype object: {source_name}"
            )
        export_name = f"JR_PROTO__{slug(source_name)}"
        copy_mesh_object(
            export_collection,
            source_obj,
            export_name,
            prototypes_group,
        )
        prototype_export_names[source_name] = export_name

    export_objects = list(export_collection.objects)
    if terrain_obj not in export_objects:
        raise RuntimeError("Terrain was not linked to export collection")
    return export_objects, prototype_export_names


def export_base_glb(
    objects: list[bpy.types.Object],
    path: Path,
) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]

    result = bpy.ops.export_scene.gltf(
        filepath=str(path),
        export_format="GLB",
        use_selection=True,
        export_yup=True,
        export_extras=False,
        export_cameras=False,
        export_lights=False,
        export_animations=False,
        export_skins=False,
        export_morph=False,
        export_texcoords=True,
        export_normals=True,
        export_tangents=False,
        export_attributes=True,
        export_materials="EXPORT",
        export_image_format=TEXTURE_ENCODING,
        export_image_quality=TEXTURE_QUALITY,
        export_image_webp_fallback=False,
        export_gpu_instances=False,
        export_draco_mesh_compression_enable=False,
    )
    if "FINISHED" not in result or not path.is_file():
        raise RuntimeError(f"Blender GLB export failed: {result}")


def read_glb(path: Path) -> tuple[dict[str, Any], bytearray]:
    data = path.read_bytes()
    if len(data) < 12:
        raise RuntimeError(f"GLB is too short: {path}")
    magic, version, total_length = struct.unpack_from("<4sII", data, 0)
    if magic != GLB_MAGIC or version != GLB_VERSION:
        raise RuntimeError(f"Unsupported GLB header: {magic}, {version}")
    if total_length != len(data):
        raise RuntimeError(
            f"GLB length mismatch: header={total_length}, file={len(data)}"
        )

    offset = 12
    document: dict[str, Any] | None = None
    binary = bytearray()
    while offset < len(data):
        chunk_length, chunk_type = struct.unpack_from("<I4s", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == JSON_CHUNK:
            document = json.loads(chunk.rstrip(b" \x00").decode("utf-8"))
        elif chunk_type == BIN_CHUNK:
            binary = bytearray(chunk)
    if document is None:
        raise RuntimeError("GLB has no JSON chunk")
    return document, binary


def write_glb(
    path: Path,
    document: dict[str, Any],
    binary: bytearray,
) -> None:
    while len(binary) % 4:
        binary.append(0)
    document.setdefault("buffers", [{"byteLength": 0}])
    document["buffers"][0]["byteLength"] = len(binary)

    json_bytes = json.dumps(
        document,
        ensure_ascii=False,
        separators=(",", ":"),
    ).encode("utf-8")
    json_bytes += b" " * ((-len(json_bytes)) % 4)
    total_length = 12 + 8 + len(json_bytes)
    if binary:
        total_length += 8 + len(binary)

    output = bytearray(
        struct.pack("<4sII", GLB_MAGIC, GLB_VERSION, total_length)
    )
    output += struct.pack("<I4s", len(json_bytes), JSON_CHUNK)
    output += json_bytes
    if binary:
        output += struct.pack("<I4s", len(binary), BIN_CHUNK)
        output += binary
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(output)


def append_accessor(
    document: dict[str, Any],
    binary: bytearray,
    values: np.ndarray,
    component_type: int,
    accessor_type: str,
    include_bounds: bool = False,
) -> int:
    while len(binary) % 4:
        binary.append(0)
    if component_type == FLOAT:
        values = np.asarray(values, dtype="<f4")
    elif component_type == UNSIGNED_INT:
        values = np.asarray(values, dtype="<u4")
    else:
        raise RuntimeError(f"Unsupported component type: {component_type}")
    values = np.ascontiguousarray(values)

    byte_offset = len(binary)
    payload = values.tobytes()
    binary.extend(payload)
    buffer_views = document.setdefault("bufferViews", [])
    buffer_view_index = len(buffer_views)
    buffer_views.append(
        {
            "buffer": 0,
            "byteOffset": byte_offset,
            "byteLength": len(payload),
        }
    )

    count = int(values.shape[0])
    accessor: dict[str, Any] = {
        "bufferView": buffer_view_index,
        "byteOffset": 0,
        "componentType": component_type,
        "count": count,
        "type": accessor_type,
    }
    if include_bounds and count:
        accessor["min"] = values.min(axis=0).astype(float).tolist()
        accessor["max"] = values.max(axis=0).astype(float).tolist()
    accessors = document.setdefault("accessors", [])
    accessor_index = len(accessors)
    accessors.append(accessor)
    return accessor_index


def node_index(
    document: dict[str, Any],
    name: str,
) -> int:
    matches = [
        index
        for index, node in enumerate(document.get("nodes", []))
        if node.get("name") == name
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one GLB node named {name}, found {len(matches)}"
        )
    return matches[0]


def set_node_metadata(
    document: dict[str, Any],
    name: str,
    metadata: dict[str, Any],
) -> int:
    index = node_index(document, name)
    document["nodes"][index]["extras"] = {"jr": metadata}
    return index


def patch_glb(
    base_path: Path,
    output_path: Path,
    manifest: dict[str, Any],
    cell: dict[str, Any],
    systems: list[dict[str, Any]],
    groups: list[dict[str, Any]],
    prototype_export_names: dict[str, str],
    complete_cell: bool,
) -> dict[str, Any]:
    document, binary = read_glb(base_path)
    nodes = document["nodes"]

    root_index = set_node_metadata(
        document,
        "JR_ROOT",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": "jr:artifact:contract_probe",
            "entity_type": "scene_root",
            "provenance": "computed",
            "artifact_role": "contract_probe",
            "complete_scene": False,
            "complete_cell": complete_cell,
            "complete_system": True,
            "renderer_policy_embedded": False,
        },
    )
    set_node_metadata(
        document,
        "JR_REGIONS",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": "jr:regions",
            "entity_type": "region_container",
            "provenance": "computed",
        },
    )
    set_node_metadata(
        document,
        f"JR_REGION__{cell['region']}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:region:{cell['region']}",
            "entity_type": "region",
            "provenance": "source",
            "region": cell["region"],
        },
    )
    cell_index = set_node_metadata(
        document,
        f"JR_CELL__{cell['cell']}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": cell["stable_id"],
            "entity_type": "cell",
            "provenance": "source",
            "region": cell["region"],
            "cell": cell["cell"],
            "bounds": cell["bounds"],
            "ownership_bounds_xy": cell["ownership_bounds_xy"],
            "complete": complete_cell,
        },
    )
    set_node_metadata(
        document,
        "JR_TERRAIN_GROUP",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"{cell['stable_id']}:terrain",
            "entity_type": "terrain_container",
            "provenance": "computed",
        },
    )
    terrain_index = set_node_metadata(
        document,
        f"JR_TERRAIN__{cell['cell']}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"{cell['stable_id']}:terrain_mesh",
            "entity_type": "terrain",
            "provenance": "source",
            "region": cell["region"],
            "cell": cell["cell"],
            "source_object": cell["name"],
        },
    )
    set_node_metadata(
        document,
        "JR_SYSTEMS",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"{cell['stable_id']}:systems",
            "entity_type": "system_container",
            "provenance": "computed",
        },
    )
    system_indices: dict[str, int] = {}
    for system in systems:
        system_indices[system["stable_id"]] = set_node_metadata(
            document,
            f"JR_SYSTEM__{slug(system['species'])}",
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": system["stable_id"],
                "entity_type": "system",
                "provenance": "source",
                "region": system["region"],
                "cell": system["cell"],
                "system": system["name"],
                "species": system["species"],
                "complete": True,
            },
        )
    prototypes_index = set_node_metadata(
        document,
        "JR_PROTOTYPES",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": "jr:prototypes",
            "entity_type": "prototype_container",
            "provenance": "computed",
            "renderable_children": False,
        },
    )

    prototype_records = {
        record["source_name"]: record
        for record in manifest["prototypes"]
    }
    mesh_by_prototype: dict[str, int] = {}
    prototype_node_indices: list[int] = []
    for source_name, export_name in prototype_export_names.items():
        prototype_node_index = node_index(document, export_name)
        prototype_node = nodes[prototype_node_index]
        if "mesh" not in prototype_node:
            raise RuntimeError(f"Prototype node has no mesh: {export_name}")
        mesh_index = prototype_node["mesh"]
        mesh_by_prototype[source_name] = mesh_index
        record = prototype_records[source_name]
        document["meshes"][mesh_index]["extras"] = {
            "jr": {
                "schema_version": SCHEMA_VERSION,
                "stable_id": record["stable_id"],
                "entity_type": "prototype",
                "provenance": "source",
                "source_name": source_name,
                "source_library": record["source_library"],
                "material_ids": record["material_ids"],
            }
        }
        prototype_node_indices.append(prototype_node_index)

    prototype_children = nodes[prototypes_index].get("children", [])
    nodes[prototypes_index]["children"] = [
        index
        for index in prototype_children
        if index not in prototype_node_indices
    ]

    instance_nodes: list[int] = []
    total_instances = 0
    source_counts: dict[str, int] = {}
    prototype_counts: dict[str, int] = {}
    system_counts: dict[str, int] = {}
    for group in groups:
        source = group["source"]
        system = group["system"]
        prototype_name = group["prototype_target_name"]
        mesh_index = mesh_by_prototype[prototype_name]
        count = len(group["translations"])
        if count == 0:
            continue

        translation_accessor = append_accessor(
            document,
            binary,
            group["translations"],
            FLOAT,
            "VEC3",
            include_bounds=True,
        )
        rotation_accessor = append_accessor(
            document,
            binary,
            group["rotations"],
            FLOAT,
            "VEC4",
        )
        scale_accessor = append_accessor(
            document,
            binary,
            group["scales"],
            FLOAT,
            "VEC3",
        )
        source_index_accessor = append_accessor(
            document,
            binary,
            group["source_indices"],
            UNSIGNED_INT,
            "SCALAR",
        )
        prototype = prototype_records[prototype_name]
        instance_set_id = (
            f"{source['stable_id']}:prototype:"
            f"{group['prototype_index']}"
        )
        instance_node = {
            "name": (
                f"JR_INST__{slug(Path(source['source_prim']).name)}__"
                f"{slug(prototype_name)}"
            ),
            "mesh": mesh_index,
            "extensions": {
                "EXT_mesh_gpu_instancing": {
                    "attributes": {
                        "TRANSLATION": translation_accessor,
                        "ROTATION": rotation_accessor,
                        "SCALE": scale_accessor,
                        "_JR_SOURCE_INDEX": source_index_accessor,
                    }
                }
            },
            "extras": {
                "jr": {
                    "schema_version": SCHEMA_VERSION,
                    "stable_id": instance_set_id,
                    "entity_type": "instance_set",
                    "provenance": "computed",
                    "region": cell["region"],
                    "cell": cell["cell"],
                    "system": system["name"],
                    "system_id": system["stable_id"],
                    "species": system["species"],
                    "prototype": prototype_name,
                    "prototype_id": prototype["stable_id"],
                    "source": {
                        "usd_layer": source["source_layer"],
                        "usd_prim": source["source_prim"],
                        "array_index_attribute": "_JR_SOURCE_INDEX",
                    },
                    "instance_count": count,
                }
            },
        }
        instance_node_index = len(nodes)
        nodes.append(instance_node)
        instance_nodes.append(instance_node_index)
        nodes[system_indices[system["stable_id"]]].setdefault(
            "children",
            [],
        ).append(instance_node_index)
        total_instances += count
        source_counts[source["stable_id"]] = (
            source_counts.get(source["stable_id"], 0) + count
        )
        prototype_counts[prototype["stable_id"]] = (
            prototype_counts.get(prototype["stable_id"], 0) + count
        )
        system_counts[system["stable_id"]] = (
            system_counts.get(system["stable_id"], 0) + count
        )

    extensions_used = document.setdefault("extensionsUsed", [])
    if "EXT_mesh_gpu_instancing" not in extensions_used:
        extensions_used.append("EXT_mesh_gpu_instancing")
    extensions_required = document.setdefault("extensionsRequired", [])
    if "EXT_mesh_gpu_instancing" not in extensions_required:
        extensions_required.append("EXT_mesh_gpu_instancing")

    document.setdefault("asset", {}).setdefault("extras", {})["jr"] = {
        "schema_version": SCHEMA_VERSION,
        "artifact_role": "contract_probe",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source_blend": manifest["source_files"]["blend"],
        "source_usd": manifest["source_files"]["usd"],
        "renderer_independent": True,
        "complete_scene": False,
        "complete_cell": complete_cell,
        "complete_system": True,
    }
    scene_index = int(document.get("scene", 0))
    document["scenes"][scene_index]["extras"] = {
        "jr": {
            "schema_version": SCHEMA_VERSION,
            "stable_id": "jr:scene:contract_probe",
            "entity_type": "scene",
            "provenance": "computed",
            "root_node": root_index,
            "cell_node": cell_index,
            "terrain_node": terrain_index,
        }
    }
    write_glb(output_path, document, binary)
    return {
        "total_instances": total_instances,
        "instance_set_count": len(instance_nodes),
        "source_counts": dict(sorted(source_counts.items())),
        "prototype_counts": dict(sorted(prototype_counts.items())),
        "system_counts": dict(sorted(system_counts.items())),
    }


def reachable_nodes(document: dict[str, Any]) -> set[int]:
    scene_index = int(document.get("scene", 0))
    pending = list(document["scenes"][scene_index].get("nodes", []))
    reachable: set[int] = set()
    while pending:
        index = pending.pop()
        if index in reachable:
            continue
        reachable.add(index)
        pending.extend(document["nodes"][index].get("children", []))
    return reachable


def validate_probe(
    path: Path,
    expected: dict[str, Any],
    cell: dict[str, Any],
    systems: list[dict[str, Any]],
    complete_cell: bool,
) -> dict[str, Any]:
    document, _ = read_glb(path)
    errors: list[str] = []
    warnings: list[str] = []
    instance_nodes = [
        node
        for node in document.get("nodes", [])
        if "EXT_mesh_gpu_instancing" in node.get("extensions", {})
    ]
    instance_count = 0
    for node in instance_nodes:
        attributes = node["extensions"]["EXT_mesh_gpu_instancing"][
            "attributes"
        ]
        required_attributes = {
            "TRANSLATION",
            "ROTATION",
            "SCALE",
            "_JR_SOURCE_INDEX",
        }
        missing = required_attributes - set(attributes)
        if missing:
            errors.append(
                f"Instance node {node.get('name')} misses {sorted(missing)}"
            )
            continue
        counts = {
            document["accessors"][accessor_index]["count"]
            for accessor_index in attributes.values()
        }
        if len(counts) != 1:
            errors.append(
                f"Accessor counts differ on {node.get('name')}: {counts}"
            )
            continue
        instance_count += counts.pop()

    if instance_count != expected["total_instances"]:
        errors.append(
            f"Instance count {instance_count} != "
            f"{expected['total_instances']}"
        )
    if len(instance_nodes) != expected["instance_set_count"]:
        errors.append(
            f"Instance-set count {len(instance_nodes)} != "
            f"{expected['instance_set_count']}"
        )
    if "EXT_mesh_gpu_instancing" not in document.get(
        "extensionsRequired",
        [],
    ):
        errors.append("EXT_mesh_gpu_instancing is not required")

    reachable = reachable_nodes(document)
    rendered_prototype_nodes = [
        node.get("name", "")
        for index, node in enumerate(document.get("nodes", []))
        if index in reachable
        and node.get("name", "").startswith("JR_PROTO__")
        and "mesh" in node
    ]
    if rendered_prototype_nodes:
        errors.append(
            "Prototype export nodes remain renderable: "
            + ", ".join(rendered_prototype_nodes)
        )

    jr_nodes = [
        node
        for node in document.get("nodes", [])
        if isinstance(node.get("extras", {}).get("jr"), dict)
    ]
    stable_ids = [
        node["extras"]["jr"].get("stable_id", "")
        for node in jr_nodes
        if node["extras"]["jr"].get("stable_id")
    ]
    duplicate_stable_ids = sorted(
        {
            stable_id
            for stable_id in stable_ids
            if stable_ids.count(stable_id) > 1
        }
    )
    if duplicate_stable_ids:
        errors.append(
            f"Duplicate reachable metadata IDs: {duplicate_stable_ids}"
        )

    materials = document.get("materials", [])
    images = document.get("images", [])
    textures = document.get("textures", [])
    image_mime_types = sorted(
        {
            image.get("mimeType", "<external-or-unspecified>")
            for image in images
        }
    )
    if not materials:
        errors.append("No materials exported")
    if not images:
        warnings.append("No images exported")

    return {
        "schema_version": SCHEMA_VERSION,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "status": "pass" if not errors else "fail",
        "errors": errors,
        "warnings": warnings,
        "artifact": path.as_posix(),
        "artifact_bytes": path.stat().st_size,
        "scope": {
            "artifact_role": "contract_probe",
            "complete_scene": False,
            "complete_cell": complete_cell,
            "complete_system": True,
            "cell": cell["cell"],
            "cell_ownership_bounds_xy": cell[
                "ownership_bounds_xy"
            ],
            "systems": [system["name"] for system in systems],
        },
        "counts": {
            "nodes": len(document.get("nodes", [])),
            "reachable_nodes": len(reachable),
            "meshes": len(document.get("meshes", [])),
            "materials": len(materials),
            "textures": len(textures),
            "images": len(images),
            "image_mime_types": image_mime_types,
            "instance_sets": len(instance_nodes),
            "instances": instance_count,
        },
        "contract": {
            "extension_used": (
                "EXT_mesh_gpu_instancing"
                in document.get("extensionsUsed", [])
            ),
            "extension_required": (
                "EXT_mesh_gpu_instancing"
                in document.get("extensionsRequired", [])
            ),
            "texture_webp_used": (
                "EXT_texture_webp"
                in document.get("extensionsUsed", [])
            ),
            "texture_webp_required": (
                "EXT_texture_webp"
                in document.get("extensionsRequired", [])
            ),
            "source_index_attribute": "_JR_SOURCE_INDEX",
            "renderer_policy_embedded": False,
            "rendered_prototype_nodes": rendered_prototype_nodes,
            "metadata_node_count": len(jr_nodes),
            "duplicate_stable_ids": duplicate_stable_ids,
        },
        "material_fidelity": {
            "status": "requires_visual_review",
            "texture_encoding": TEXTURE_ENCODING,
            "texture_quality": TEXTURE_QUALITY,
            "lossy_texture_encoding": True,
            "observed_exporter_warnings": [
                (
                    "Active vertex color was not exported because it is "
                    "not used by the source material node tree."
                ),
                (
                    "Multiple image-texture nodes may resolve to one glTF "
                    "sampler behavior; Blender chose the first sampler."
                ),
            ],
            "interpretation": (
                "Geometry, material slots, materials, textures, and images "
                "are present, but visual equivalence is not yet approved."
            ),
        },
        "source_counts": expected["source_counts"],
        "prototype_counts": expected["prototype_counts"],
        "system_counts": expected["system_counts"],
    }


def write_validation_docs(
    report: dict[str, Any],
    path: Path,
) -> None:
    scope = report["scope"]
    counts = report["counts"]
    contract = report["contract"]
    lines = [
        "# JungleRuins GLB Contract Probe",
        "",
        "This artifact proves the canonical GLB hierarchy, metadata, shared "
        "prototype, and instance-transform path without GScatter.",
        "",
        f"- Validation: `{report['status']}`",
        f"- Artifact: `{report['artifact']}`",
        f"- Artifact bytes: `{report['artifact_bytes']}`",
        f"- Cell: `{scope['cell']}`",
        f"- Included systems: `{len(scope['systems'])}`",
        f"- Complete scene: `{scope['complete_scene']}`",
        f"- Complete cell: `{scope['complete_cell']}`",
        f"- Complete system: `{scope['complete_system']}`",
        "",
        (
            "The terrain cell and all authored scatter systems for the "
            "cell are present."
            if scope["complete_cell"]
            else (
                "The terrain cell is present, but only the listed scatter "
                "system subset is included."
            )
        ),
        "This remains a contract probe, not a complete cinematic scene.",
        "",
        "## Included systems",
        "",
    ]
    lines.extend(f"- `{name}`" for name in scope["systems"])
    lines.extend(
        [
        "",
        "## Counts",
        "",
        "| Entity | Count |",
        "|---|---:|",
        ]
    )
    lines.extend(
        f"| `{key}` | {value} |"
        for key, value in counts.items()
    )
    lines.extend(
        [
            "",
            "## Contract",
            "",
            f"- `EXT_mesh_gpu_instancing` used: "
            f"`{contract['extension_used']}`",
            f"- `EXT_mesh_gpu_instancing` required: "
            f"`{contract['extension_required']}`",
            f"- `EXT_texture_webp` used: "
            f"`{contract.get('texture_webp_used', False)}`",
            f"- `EXT_texture_webp` required: "
            f"`{contract.get('texture_webp_required', False)}`",
            f"- Source-array identity attribute: "
            f"`{contract['source_index_attribute']}`",
            f"- Renderer policy embedded: "
            f"`{contract['renderer_policy_embedded']}`",
            f"- Rendered prototype-export nodes: "
            f"`{len(contract['rendered_prototype_nodes'])}`",
            f"- Metadata nodes: `{contract['metadata_node_count']}`",
            f"- Duplicate stable IDs: "
            f"`{len(contract['duplicate_stable_ids'])}`",
            "",
            "## Errors",
            "",
        ]
    )
    lines.extend(
        [f"- {message}" for message in report["errors"]]
        or ["- None."]
    )
    lines.extend(["", "## Warnings", ""])
    lines.extend(
        [f"- {message}" for message in report["warnings"]]
        or ["- None."]
    )
    material_fidelity = report.get("material_fidelity", {})
    if material_fidelity:
        lines.extend(
            [
                "",
                "## Material fidelity",
                "",
                f"- Status: `{material_fidelity['status']}`",
                f"- Texture encoding: "
                f"`{material_fidelity['texture_encoding']}`",
                f"- Texture quality: "
                f"`{material_fidelity['texture_quality']}`",
                f"- Lossy encoding: "
                f"`{material_fidelity['lossy_texture_encoding']}`",
                f"- Interpretation: {material_fidelity['interpretation']}",
            ]
        )
        lines.extend(
            f"- Exporter warning: {message}"
            for message in material_fidelity[
                "observed_exporter_warnings"
            ]
        )
    reimport = report.get("blender_reimport")
    if reimport:
        bounds = reimport["instance_origin_bounds_blender_z_up"]
        lines.extend(
            [
                "",
                "## Blender 4.2 re-import",
                "",
                f"- Status: `{reimport['status']}`",
                f"- Blender: `{reimport['blender_version']}`",
                f"- Instance objects: `{reimport['instance_objects']}`",
                f"- Mesh datablocks: `{reimport['mesh_datablocks']}`",
                f"- Behavior: `{reimport['extension_import_behavior']}`",
                f"- Instance-origin min: `{bounds['min']}`",
                f"- Instance-origin max: `{bounds['max']}`",
                "",
                "Blender expands `EXT_mesh_gpu_instancing` to editable "
                "objects on import. This is a Blender consumer behavior; "
                "the GLB itself stores compact accessor arrays.",
            ]
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    args = parse_args()
    scene_root = args.scene_root.resolve()
    manifest_path = args.manifest.resolve()
    output_path = args.output.resolve()
    report_path = args.report.resolve()
    docs_path = args.docs_output.resolve()
    manifest = load_json(manifest_path)
    cell = dict(
        find_one(
            manifest["cells"],
            region="cinematic",
            cell=args.cell,
        )
    )
    cell["ownership_bounds_xy"] = cell_ownership_bounds_xy(
        manifest,
        cell,
    )
    authored_systems = sorted(
        (
            system
            for system in manifest["systems"]
            if system["region"] == "cinematic"
            and system["cell"] == args.cell
        ),
        key=lambda record: record["stable_id"],
    )
    if not authored_systems:
        raise RuntimeError(f"No systems found for cell {args.cell}")
    if args.species.lower() == "all":
        systems = authored_systems
    else:
        systems = [
            find_one(
                authored_systems,
                species=args.species,
            )
        ]
    complete_cell = len(systems) == len(authored_systems)

    usd_path = scene_root / manifest["source_files"]["usd"]
    stage = Usd.Stage.Open(str(usd_path), load=Usd.Stage.LoadNone)
    if stage is None:
        raise RuntimeError(f"Unable to open USD stage: {usd_path}")
    groups: list[dict[str, Any]] = []
    for system in systems:
        groups.extend(
            collect_instance_groups(
                manifest,
                stage,
                cell,
                system,
            )
        )
    if not groups:
        raise RuntimeError(
            f"No instance groups selected for cell {cell['stable_id']}"
        )

    objects, prototype_export_names = build_blender_export_scene(
        manifest,
        cell,
        systems,
        groups,
    )
    temp_path = output_path.with_name(
        output_path.stem + ".blender_base.glb"
    )
    export_base_glb(objects, temp_path)
    expected = patch_glb(
        temp_path,
        output_path,
        manifest,
        cell,
        systems,
        groups,
        prototype_export_names,
        complete_cell,
    )
    report = validate_probe(
        output_path,
        expected,
        cell,
        systems,
        complete_cell,
    )
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(report, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    write_validation_docs(report, docs_path)
    if report["status"] != "pass":
        raise RuntimeError(
            f"Contract probe validation failed: {report['errors']}"
        )
    temp_path.unlink()
    print(f"JR_PROBE_GLB={output_path}")
    print(f"JR_PROBE_REPORT={report_path}")
    print(f"JR_PROBE_DOCS={docs_path}")
    print(f"JR_PROBE_INSTANCES={expected['total_instances']}")


if __name__ == "__main__":
    main()
