"""GLB container and accessor operations used by JungleRuins exporters."""

from __future__ import annotations

import json
from pathlib import Path
import struct
from typing import Any

import numpy as np


GLB_MAGIC = b"glTF"
GLB_VERSION = 2
JSON_CHUNK = b"JSON"
BIN_CHUNK = b"BIN\x00"
FLOAT = 5126
UNSIGNED_INT = 5125
MAX_GLB_BYTES = (1 << 32) - 1


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
    if total_length > MAX_GLB_BYTES:
        raise RuntimeError(
            f"GLB exceeds the 32-bit container limit: {total_length} bytes"
        )

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


def node_index(document: dict[str, Any], name: str) -> int:
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
