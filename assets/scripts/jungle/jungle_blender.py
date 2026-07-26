"""Build the Blender-side base GLB for one JungleRuins region package."""

from __future__ import annotations

from pathlib import Path
from typing import Any

import bpy

from jungle_common import (
    TEXTURE_ENCODING,
    TEXTURE_QUALITY,
    cell_node_name,
    slug,
    source_datablock_name,
    system_node_name,
    terrain_node_name,
)


def new_empty(
    collection: bpy.types.Collection,
    name: str,
    parent: bpy.types.Object | None = None,
) -> bpy.types.Object:
    obj = bpy.data.objects.new(name, None)
    collection.objects.link(obj)
    obj.parent = parent
    return obj


def copy_object(
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


def blender_prototype_objects(
    manifest: dict[str, Any],
) -> dict[str, bpy.types.Object]:
    prototype_names = {
        record["source_name"] for record in manifest["prototypes"]
    }
    result: dict[str, bpy.types.Object] = {}
    for obj in bpy.data.objects:
        source_name = source_datablock_name(obj.name_full)
        if source_name in prototype_names and obj.type == "MESH":
            result[source_name] = obj
    return result


def build_region_export_scene(
    manifest: dict[str, Any],
    region: str,
    cells: list[dict[str, Any]],
    systems: list[dict[str, Any]],
    groups: list[dict[str, Any]],
) -> tuple[list[bpy.types.Object], dict[str, str]]:
    collection = bpy.data.collections.new(
        f"JR_PACKAGE_{region.upper()}"
    )
    bpy.context.scene.collection.children.link(collection)

    root = new_empty(collection, "JR_ROOT")
    region_node = new_empty(
        collection,
        f"JR_REGION__{region}",
        root,
    )
    new_empty(
        collection,
        f"JR_UNRESOLVED__{region}",
        region_node,
    )
    static_group = new_empty(
        collection,
        f"JR_STATIC__{region}",
        region_node,
    )
    prototypes_group = new_empty(
        collection,
        "JR_PROTOTYPES",
        root,
    )

    system_by_cell: dict[str, list[dict[str, Any]]] = {}
    for system in systems:
        system_by_cell.setdefault(system["cell"], []).append(system)

    cell_source_ids = {
        cell.get("source_object_id", "") for cell in cells
    }
    static_records = [
        record
        for record in manifest.get("static_objects", [])
        if record["region"] == region
        and record["stable_id"] not in cell_source_ids
    ]
    for record in static_records:
        source = bpy.data.objects.get(record["name"])
        if source is None:
            raise RuntimeError(
                f"Missing static object: {record['name']}"
            )
        copy_object(
            collection,
            source,
            f"JR_OBJECT__{slug(record['stable_id'])}",
            static_group,
        )

    static_by_id = {
        record["stable_id"]: record
        for record in manifest.get("static_objects", [])
    }
    for cell in cells:
        cell_node = new_empty(
            collection,
            cell_node_name(cell),
            region_node,
        )
        terrain_group = new_empty(
            collection,
            f"JR_TERRAIN_GROUP__{cell['cell']}",
            cell_node,
        )
        systems_group = new_empty(
            collection,
            f"JR_SYSTEMS__{cell['cell']}",
            cell_node,
        )
        for system in sorted(
            system_by_cell.get(cell["cell"], []),
            key=lambda record: record["stable_id"],
        ):
            new_empty(
                collection,
                system_node_name(system),
                systems_group,
            )

        source_name = cell["name"]
        source_record = static_by_id.get(
            cell.get("source_object_id", "")
        )
        if source_record:
            source_name = source_record["name"]
        terrain_source = bpy.data.objects.get(source_name)
        if terrain_source is None:
            raise RuntimeError(
                f"Missing terrain object: {source_name}"
            )
        copy_object(
            collection,
            terrain_source,
            terrain_node_name(cell),
            terrain_group,
        )

    prototype_sources = blender_prototype_objects(manifest)
    prototype_export_names: dict[str, str] = {}
    required_names = sorted(
        {group["prototype_target_name"] for group in groups}
    )
    for source_name in required_names:
        source = prototype_sources.get(source_name)
        if source is None:
            raise RuntimeError(
                f"Missing Blender prototype object: {source_name}"
            )
        export_name = f"JR_PROTO__{slug(source_name)}"
        copy_object(
            collection,
            source,
            export_name,
            prototypes_group,
        )
        prototype_export_names[source_name] = export_name
    return list(collection.objects), prototype_export_names


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
        export_cameras=True,
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


def remove_export_scene(objects: list[bpy.types.Object]) -> None:
    collections = {
        collection
        for obj in objects
        for collection in obj.users_collection
        if collection.name.startswith("JR_PACKAGE_")
    }
    for obj in objects:
        if obj.name in bpy.data.objects:
            bpy.data.objects.remove(obj, do_unlink=True)
    for collection in collections:
        if collection.name in bpy.data.collections:
            bpy.data.collections.remove(collection)
