from __future__ import annotations

from pathlib import Path
from typing import Iterable

import pandas as pd

VARIANT_NAMES = {
    1: "Forward",
    2: "Forward+Prepass",
    3: "Deferred",
    4: "TVB",
    5: "Deferred+Prepass",
    6: "TVB+GBuffer",
}
VARIANT_ORDER = [VARIANT_NAMES[i] for i in range(1, 7)]
PASS_COLUMNS = [
    "forward",
    "depth_prepass",
    "geometry",
    "lighting",
    "visibility",
    "resolve",
    "gbuffer",
]


def parse_int_list(value: str) -> list[int]:
    result = [int(item.strip()) for item in value.split(",") if item.strip()]
    if not result:
        raise ValueError("정수 목록이 비어 있습니다.")
    return result


def load_experiment_csv(path: Path, alu_values: Iterable[int]) -> pd.DataFrame:
    """Restore sweep parameters from run_id.

    Assumed sweep order:
      renderer_variant: outer loop
      alu_calc_count: inner loop
    """
    alu_values = list(alu_values)
    data = pd.read_csv(path)
    required = {"run_id", "frame", "total"}
    missing = required - set(data.columns)
    if missing:
        raise ValueError(f"필수 열이 없습니다: {sorted(missing)}")

    count = len(alu_values)
    data["renderer_variant"] = data["run_id"] // count + 1
    data["alu_calc_count"] = data["run_id"].map(
        lambda run_id: alu_values[int(run_id) % count]
    )
    data["variant"] = data["renderer_variant"].map(VARIANT_NAMES)
    if data["variant"].isna().any():
        unknown = sorted(data.loc[data["variant"].isna(), "renderer_variant"].unique())
        raise ValueError(f"알 수 없는 renderer_variant: {unknown}")
    return data


def align_raster_stats(stats: pd.DataFrame, window_frames: int = 10) -> pd.DataFrame:
    """Average per-frame raster stats into GPU profile windows."""
    if "frame" not in stats.columns:
        raise ValueError("Raster stat CSV에 frame 열이 없습니다.")
    result = stats.copy()
    result["profile_frame"] = (
        (result["frame"] - result["frame"].min()) // window_frames
    ) * window_frames
    columns = [c for c in result.columns if c not in {"frame", "profile_frame"}]
    return (
        result.groupby("profile_frame", as_index=False)[columns]
        .mean()
        .rename(columns={"profile_frame": "frame"})
    )


def ensure_output_dir(path: Path, clean: bool = False) -> Path:
    if clean and path.exists():
        import shutil
        shutil.rmtree(path)
    path.mkdir(parents=True, exist_ok=True)
    return path
