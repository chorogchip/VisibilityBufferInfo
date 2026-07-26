"""Patch Blender GLBs with Jungle hierarchy metadata and instance streams."""

from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from jungle_common import (
    SCHEMA_VERSION,
    cell_node_name,
    slug,
    system_node_name,
    terrain_node_name,
)
from jungle_glb import (
    FLOAT,
    UNSIGNED_INT,
    append_accessor,
    node_index,
    reachable_nodes,
    read_glb,
    set_node_metadata,
    write_glb,
)


def add_instance_node(
    document: dict[str, Any],
    binary: bytearray,
    group: dict[str, Any],
    mesh_index: int,
    parent_index: int,
    prototype: dict[str, Any],
) -> int:
    source = group["source"]
    cell = group["cell"]
    system = group["system"]
    count = len(group["translations"])
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

    owner = (
        f"cell:{cell['cell']}"
        if cell
        else f"unresolved:{group['unresolved_reason']}"
    )
    stable_id = (
        f"{source['stable_id']}:{owner}:"
        f"prototype:{group['prototype_index']}"
    )
    metadata: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "stable_id": stable_id,
        "entity_type": "instance_set",
        "provenance": "computed",
        "region": source["region"],
        "species": source["species"],
        "prototype": group["prototype_target_name"],
        "prototype_id": prototype["stable_id"],
        "source_prim": source["source_prim"],
        "source_layer": source["source_layer"],
        "instance_count": count,
    }
    if cell:
        metadata["cell"] = cell["cell"]
    if system:
        metadata["system"] = system["name"]
    if group["unresolved"]:
        metadata["unresolved_reason"] = group["unresolved_reason"]

    node_name = (
        f"JR_INST__{slug(Path(source['source_prim']).name)}__"
        f"{slug(group['prototype_target_name'])}"
    )
    if group["unresolved"]:
        node_name += f"__{slug(group['unresolved_reason'])}"
    node = {
        "name": node_name,
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
        "extras": {"jr": metadata},
    }
    index = len(document["nodes"])
    document["nodes"].append(node)
    document["nodes"][parent_index].setdefault(
        "children",
        [],
    ).append(index)
    return index


def patch_region_glb(
    base_path: Path,
    output_path: Path,
    manifest: dict[str, Any],
    region: str,
    cells: list[dict[str, Any]],
    systems: list[dict[str, Any]],
    groups: list[dict[str, Any]],
    prototype_export_names: dict[str, str],
) -> dict[str, Any]:
    document, binary = read_glb(base_path)
    nodes = document["nodes"]
    root_index = set_node_metadata(
        document,
        "JR_ROOT",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:package:{region}",
            "entity_type": "scene_root",
            "provenance": "computed",
            "region": region,
        },
    )
    region_index = set_node_metadata(
        document,
        f"JR_REGION__{region}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:region:{region}",
            "entity_type": "region",
            "provenance": "source",
            "region": region,
        },
    )
    unresolved_index = set_node_metadata(
        document,
        f"JR_UNRESOLVED__{region}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:region:{region}:unresolved_origin",
            "entity_type": "unresolved_container",
            "provenance": "computed",
            "region": region,
        },
    )
    set_node_metadata(
        document,
        f"JR_STATIC__{region}",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:region:{region}:static",
            "entity_type": "static_container",
            "provenance": "computed",
            "region": region,
        },
    )

    system_indices: dict[str, int] = {}
    for cell in cells:
        set_node_metadata(
            document,
            cell_node_name(cell),
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": cell["stable_id"],
                "entity_type": "cell",
                "provenance": cell["provenance"],
                "region": region,
                "cell": cell["cell"],
                "bounds": cell.get("bounds"),
            },
        )
        set_node_metadata(
            document,
            f"JR_TERRAIN_GROUP__{cell['cell']}",
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": f"{cell['stable_id']}:terrain",
                "entity_type": "terrain_container",
                "provenance": "computed",
                "region": region,
                "cell": cell["cell"],
            },
        )
        set_node_metadata(
            document,
            terrain_node_name(cell),
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": f"{cell['stable_id']}:terrain_mesh",
                "entity_type": "static_object",
                "provenance": "source",
                "region": region,
                "cell": cell["cell"],
                "source_object": cell["name"],
            },
        )
        set_node_metadata(
            document,
            f"JR_SYSTEMS__{cell['cell']}",
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": f"{cell['stable_id']}:systems",
                "entity_type": "system_container",
                "provenance": "computed",
                "region": region,
                "cell": cell["cell"],
            },
        )

    for system in systems:
        system_indices[system["stable_id"]] = set_node_metadata(
            document,
            system_node_name(system),
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": system["stable_id"],
                "entity_type": "system",
                "provenance": "source",
                "region": region,
                "cell": system["cell"],
                "system": system["name"],
                "species": system["species"],
            },
        )

    static_records = [
        record
        for record in manifest.get("static_objects", [])
        if record["region"] == region
        and record["stable_id"]
        not in {cell.get("source_object_id") for cell in cells}
    ]
    for record in static_records:
        set_node_metadata(
            document,
            f"JR_OBJECT__{slug(record['stable_id'])}",
            {
                "schema_version": SCHEMA_VERSION,
                "stable_id": record["stable_id"],
                "entity_type": record["entity_type"],
                "provenance": "source",
                "region": region,
                "bounds": record.get("bounds"),
                "source_object": record["name"],
            },
        )

    prototypes_index = set_node_metadata(
        document,
        "JR_PROTOTYPES",
        {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:package:{region}:prototypes",
            "entity_type": "prototype_container",
            "provenance": "computed",
            "region": region,
        },
    )
    prototype_records = {
        record["source_name"]: record
        for record in manifest["prototypes"]
    }
    mesh_by_prototype: dict[str, int] = {}
    prototype_nodes: list[int] = []
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
                "source_object": source_name,
                "source_library": record["source_library"],
            }
        }
        prototype_nodes.append(prototype_node_index)
    nodes[prototypes_index]["children"] = [
        index
        for index in nodes[prototypes_index].get("children", [])
        if index not in prototype_nodes
    ]

    total_instances = 0
    unresolved_instances = 0
    unresolved_origin_instances = 0
    exact_origin_instances = 0
    for group in groups:
        count = len(group["translations"])
        if count == 0:
            continue
        prototype_name = group["prototype_target_name"]
        parent_index = (
            unresolved_index
            if group["unresolved"]
            else system_indices[group["system"]["stable_id"]]
        )
        add_instance_node(
            document,
            binary,
            group,
            mesh_by_prototype[prototype_name],
            parent_index,
            prototype_records[prototype_name],
        )
        total_instances += count
        exact_origin_instances += group["exact_origin_count"]
        if group["unresolved"]:
            unresolved_instances += count
            if group["unresolved_reason"] == "exact_origin":
                unresolved_origin_instances += count

    extensions_used = document.setdefault("extensionsUsed", [])
    extensions_required = document.setdefault("extensionsRequired", [])
    if groups:
        if "EXT_mesh_gpu_instancing" not in extensions_used:
            extensions_used.append("EXT_mesh_gpu_instancing")
        if "EXT_mesh_gpu_instancing" not in extensions_required:
            extensions_required.append("EXT_mesh_gpu_instancing")

    document.setdefault("asset", {}).setdefault("extras", {})["jr"] = {
        "schema_version": SCHEMA_VERSION,
        "artifact_role": "region_package",
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "region": region,
        "source_blend": manifest["source_files"]["blend"],
        "source_usd": manifest["source_files"]["usd"],
        "renderer_independent": True,
    }
    scene_index = int(document.get("scene", 0))
    document["scenes"][scene_index]["extras"] = {
        "jr": {
            "schema_version": SCHEMA_VERSION,
            "stable_id": f"jr:package:{region}:scene",
            "entity_type": "scene",
            "provenance": "computed",
            "region": region,
            "root_node": root_index,
            "region_node": region_index,
        }
    }
    write_glb(output_path, document, binary)
    return {
        "region": region,
        "artifact": output_path.as_posix(),
        "artifact_bytes": output_path.stat().st_size,
        "cells": len(cells),
        "systems": len(systems),
        "instance_sets": sum(
            1
            for node in document["nodes"]
            if "EXT_mesh_gpu_instancing"
            in node.get("extensions", {})
        ),
        "instances": total_instances,
        "unresolved_instances": unresolved_instances,
        "unresolved_origin_instances": unresolved_origin_instances,
        "exact_origin_instances": exact_origin_instances,
        "meshes": len(document.get("meshes", [])),
        "materials": len(document.get("materials", [])),
        "images": len(document.get("images", [])),
    }


def validate_region_package(
    path: Path,
    expected: dict[str, Any],
) -> dict[str, Any]:
    document, _ = read_glb(path)
    errors: list[str] = []
    instance_nodes = [
        node
        for node in document.get("nodes", [])
        if "EXT_mesh_gpu_instancing"
        in node.get("extensions", {})
    ]
    instance_count = 0
    for node in instance_nodes:
        attributes = node["extensions"][
            "EXT_mesh_gpu_instancing"
        ]["attributes"]
        required = {
            "TRANSLATION",
            "ROTATION",
            "SCALE",
            "_JR_SOURCE_INDEX",
        }
        if required - set(attributes):
            errors.append(
                f"Incomplete instance attributes: {node.get('name')}"
            )
            continue
        counts = {
            document["accessors"][index]["count"]
            for index in attributes.values()
        }
        if len(counts) != 1:
            errors.append(
                f"Instance accessor counts differ: {node.get('name')}"
            )
        else:
            instance_count += counts.pop()
    if instance_count != expected["instances"]:
        errors.append(
            f"Instance count {instance_count} != {expected['instances']}"
        )

    reachable = reachable_nodes(document)
    rendered_prototypes = [
        node.get("name", "")
        for index, node in enumerate(document.get("nodes", []))
        if index in reachable
        and node.get("name", "").startswith("JR_PROTO__")
        and "mesh" in node
    ]
    if rendered_prototypes:
        errors.append("Prototype source nodes remain renderable.")
    return {
        **expected,
        "status": "pass" if not errors else "fail",
        "errors": errors,
        "reachable_nodes": len(reachable),
        "rendered_prototype_nodes": rendered_prototypes,
    }
