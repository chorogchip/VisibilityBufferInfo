#!/usr/bin/env python3
"""Merge a glTF overlay scene into a base glTF without copying binary assets."""

from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
from typing import Any


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Append an aligned overlay glTF to a base glTF scene."
    )
    parser.add_argument("base", type=Path)
    parser.add_argument("overlay", type=Path)
    parser.add_argument("output", type=Path)
    return parser.parse_args()


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as stream:
        document = json.load(stream)
    if document.get("asset", {}).get("version") != "2.0":
        raise ValueError(f"Expected glTF 2.0: {path}")
    return document


def rebase_uri(uri: str, source_dir: Path, output_dir: Path) -> str:
    if uri.startswith("data:"):
        return uri
    absolute = (source_dir / uri).resolve()
    return os.path.relpath(absolute, output_dir.resolve()).replace("\\", "/")


def rebase_document_uris(
    document: dict[str, Any],
    source_dir: Path,
    output_dir: Path,
) -> None:
    for buffer in document.get("buffers", []):
        uri = buffer.get("uri")
        if uri:
            buffer["uri"] = rebase_uri(uri, source_dir, output_dir)
    for image in document.get("images", []):
        uri = image.get("uri")
        if uri:
            image["uri"] = rebase_uri(uri, source_dir, output_dir)


def offset_texture_references(material: dict[str, Any], offset: int) -> None:
    texture_keys = {
        "baseColorTexture",
        "metallicRoughnessTexture",
        "normalTexture",
        "occlusionTexture",
        "emissiveTexture",
        "diffuseTexture",
        "specularGlossinessTexture",
        "clearcoatTexture",
        "clearcoatRoughnessTexture",
        "clearcoatNormalTexture",
        "sheenColorTexture",
        "sheenRoughnessTexture",
        "specularTexture",
        "specularColorTexture",
        "transmissionTexture",
        "thicknessTexture",
        "iridescenceTexture",
        "iridescenceThicknessTexture",
        "anisotropyTexture",
    }

    def visit(value: Any) -> None:
        if isinstance(value, dict):
            for key, child in value.items():
                if (
                    key in texture_keys
                    and isinstance(child, dict)
                    and "index" in child
                ):
                    child["index"] += offset
                visit(child)
        elif isinstance(value, list):
            for child in value:
                visit(child)

    visit(material)


def offset_accessor(accessor: dict[str, Any], buffer_view_offset: int) -> None:
    if "bufferView" in accessor:
        accessor["bufferView"] += buffer_view_offset
    sparse = accessor.get("sparse")
    if sparse:
        sparse["indices"]["bufferView"] += buffer_view_offset
        sparse["values"]["bufferView"] += buffer_view_offset


def offset_primitive(
    primitive: dict[str, Any],
    accessor_offset: int,
    material_offset: int,
) -> None:
    primitive["attributes"] = {
        semantic: accessor + accessor_offset
        for semantic, accessor in primitive.get("attributes", {}).items()
    }
    if "indices" in primitive:
        primitive["indices"] += accessor_offset
    if "material" in primitive:
        primitive["material"] += material_offset
    for target in primitive.get("targets", []):
        for semantic in tuple(target):
            target[semantic] += accessor_offset


def append_overlay(
    base: dict[str, Any],
    overlay: dict[str, Any],
) -> None:
    offsets = {
        "buffers": len(base.get("buffers", [])),
        "bufferViews": len(base.get("bufferViews", [])),
        "accessors": len(base.get("accessors", [])),
        "samplers": len(base.get("samplers", [])),
        "images": len(base.get("images", [])),
        "textures": len(base.get("textures", [])),
        "materials": len(base.get("materials", [])),
        "meshes": len(base.get("meshes", [])),
        "cameras": len(base.get("cameras", [])),
        "nodes": len(base.get("nodes", [])),
    }

    for buffer in overlay.get("buffers", []):
        base.setdefault("buffers", []).append(buffer)

    for buffer_view in overlay.get("bufferViews", []):
        buffer_view["buffer"] += offsets["buffers"]
        base.setdefault("bufferViews", []).append(buffer_view)

    for accessor in overlay.get("accessors", []):
        offset_accessor(accessor, offsets["bufferViews"])
        base.setdefault("accessors", []).append(accessor)

    for sampler in overlay.get("samplers", []):
        base.setdefault("samplers", []).append(sampler)

    for image in overlay.get("images", []):
        if "bufferView" in image:
            image["bufferView"] += offsets["bufferViews"]
        base.setdefault("images", []).append(image)

    for texture in overlay.get("textures", []):
        if "sampler" in texture:
            texture["sampler"] += offsets["samplers"]
        if "source" in texture:
            texture["source"] += offsets["images"]
        base.setdefault("textures", []).append(texture)

    for material in overlay.get("materials", []):
        offset_texture_references(material, offsets["textures"])
        base.setdefault("materials", []).append(material)

    for mesh in overlay.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            offset_primitive(
                primitive,
                offsets["accessors"],
                offsets["materials"],
            )
        base.setdefault("meshes", []).append(mesh)

    for camera in overlay.get("cameras", []):
        base.setdefault("cameras", []).append(camera)

    for node in overlay.get("nodes", []):
        if "mesh" in node:
            node["mesh"] += offsets["meshes"]
        if "camera" in node:
            node["camera"] += offsets["cameras"]
        if "children" in node:
            node["children"] = [
                child + offsets["nodes"] for child in node["children"]
            ]
        if "skin" in node:
            raise ValueError("Overlay skins are not supported by this merger.")
        base.setdefault("nodes", []).append(node)

    overlay_scene_index = overlay.get("scene", 0)
    overlay_roots = overlay["scenes"][overlay_scene_index].get("nodes", [])
    base_scene_index = base.get("scene", 0)
    base["scenes"][base_scene_index].setdefault("nodes", []).extend(
        node + offsets["nodes"] for node in overlay_roots
    )


def validate_references(document: dict[str, Any]) -> None:
    counts = {
        "buffers": len(document.get("buffers", [])),
        "bufferViews": len(document.get("bufferViews", [])),
        "accessors": len(document.get("accessors", [])),
        "samplers": len(document.get("samplers", [])),
        "images": len(document.get("images", [])),
        "textures": len(document.get("textures", [])),
        "materials": len(document.get("materials", [])),
        "meshes": len(document.get("meshes", [])),
        "cameras": len(document.get("cameras", [])),
        "nodes": len(document.get("nodes", [])),
    }

    for buffer_view in document.get("bufferViews", []):
        if not 0 <= buffer_view["buffer"] < counts["buffers"]:
            raise ValueError("Invalid bufferView.buffer reference.")
    for accessor in document.get("accessors", []):
        if "bufferView" in accessor and not (
            0 <= accessor["bufferView"] < counts["bufferViews"]
        ):
            raise ValueError("Invalid accessor.bufferView reference.")
    for texture in document.get("textures", []):
        if "sampler" in texture and not (
            0 <= texture["sampler"] < counts["samplers"]
        ):
            raise ValueError("Invalid texture.sampler reference.")
        if "source" in texture and not (
            0 <= texture["source"] < counts["images"]
        ):
            raise ValueError("Invalid texture.source reference.")
    for mesh in document.get("meshes", []):
        for primitive in mesh.get("primitives", []):
            for accessor in primitive.get("attributes", {}).values():
                if not 0 <= accessor < counts["accessors"]:
                    raise ValueError("Invalid primitive attribute reference.")
            if "indices" in primitive and not (
                0 <= primitive["indices"] < counts["accessors"]
            ):
                raise ValueError("Invalid primitive indices reference.")
            if "material" in primitive and not (
                0 <= primitive["material"] < counts["materials"]
            ):
                raise ValueError("Invalid primitive material reference.")
    for node in document.get("nodes", []):
        if "mesh" in node and not 0 <= node["mesh"] < counts["meshes"]:
            raise ValueError("Invalid node.mesh reference.")
        if "camera" in node and not (
            0 <= node["camera"] < counts["cameras"]
        ):
            raise ValueError("Invalid node.camera reference.")
        for child in node.get("children", []):
            if not 0 <= child < counts["nodes"]:
                raise ValueError("Invalid node child reference.")
    for scene in document.get("scenes", []):
        for node in scene.get("nodes", []):
            if not 0 <= node < counts["nodes"]:
                raise ValueError("Invalid scene node reference.")


def main() -> int:
    args = parse_args()
    base_path = args.base.resolve()
    overlay_path = args.overlay.resolve()
    output_path = args.output.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    base = copy.deepcopy(load_json(base_path))
    overlay = copy.deepcopy(load_json(overlay_path))
    rebase_document_uris(base, base_path.parent, output_path.parent)
    rebase_document_uris(overlay, overlay_path.parent, output_path.parent)
    append_overlay(base, overlay)
    validate_references(base)

    generator = base.setdefault("asset", {}).get("generator", "")
    base["asset"]["generator"] = (
        f"{generator}; VisibilityBufferInfo merge_gltf_scenes.py"
        if generator
        else "VisibilityBufferInfo merge_gltf_scenes.py"
    )
    with output_path.open("w", encoding="utf-8", newline="\n") as stream:
        json.dump(base, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")

    print(f"Output: {output_path}")
    print(
        "Merged counts: "
        f"nodes={len(base.get('nodes', []))}, "
        f"meshes={len(base.get('meshes', []))}, "
        f"materials={len(base.get('materials', []))}, "
        f"accessors={len(base.get('accessors', []))}, "
        f"buffers={len(base.get('buffers', []))}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
