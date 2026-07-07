# -*- coding: cp949 -*-

from dataclasses import dataclass, fields
from pathlib import Path
import subprocess


# ============================================================
# CONFIG
# ============================================================

# 이 파이썬 스크립트 기준 TVBPerf.exe 상대 경로
EXE_REL_PATH = "../out/build/x64-Debug/bin/TVBPerf.exe"


# ============================================================
# ProgramArgument
# ============================================================

@dataclass
class ProgramArgument:
    run_id: int = 0
    run_name: str = "no-run-name"
    run_current_time: str = ""
    output_filepath: str = ""

    renderer_variant: int = 4
    renderer_variant_name: str = "no-variant-name"

    to_use_scene: bool = False
    scene_path: str = "assets/scenes/unpacked/CornellBox/CornellBox-Empty-RG.obj"

    warmup_frames: int = 60
    measure_frames: int = 120

    window_width: int = 1280
    window_height: int = 720

    seed: int = 0

    camera_pos_x: float = 5.0
    camera_pos_y: float = 2.0
    camera_pos_z: float = -10.0

    camera_lookat_x: float = 0.0
    camera_lookat_y: float = 0.0
    camera_lookat_z: float = 0.0

    camera_near_z: float = 0.1
    camera_far_z: float = 1000.0
    camera_fov: float = 45.0

    sphere_count: int = 5
    material_count: int = 1
    geometry_count: int = 1

    z_min: float = -1.0
    z_max: float = 1.0
    xy_minmax: float = 1.0
    radius: float = 0.5

    geometry_div: int = 1

    gbuffer_cnt: int = 1

    texture_count: int = 0
    texture_size: int = 1
    texture_sampling_count: int = 0

    alu_calc_count: int = 0


# ============================================================
# Functions
# ============================================================

def snake_to_kebab(name: str) -> str:
    return name.replace("_", "-")


def value_to_arg_string(value) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


def build_command(arg: ProgramArgument) -> list[str]:
    script_dir = Path(__file__).resolve().parent
    exe_path = (script_dir / EXE_REL_PATH).resolve()

    cmd = [str(exe_path)]

    for f in fields(arg):
        name = f.name
        value = getattr(arg, name)

        cmd.append(f"--{snake_to_kebab(name)}")
        cmd.append(value_to_arg_string(value))

    return cmd


def run(arg: ProgramArgument):
    script_dir = Path(__file__).resolve().parent
    exe_path = (script_dir / EXE_REL_PATH).resolve()

    if not exe_path.exists():
        raise FileNotFoundError(f"TVBPerf.exe not found: {exe_path}")

    cmd = build_command(arg)

    print(" ".join(cmd))

    subprocess.run(
        cmd,
        cwd=str(exe_path.parent),
        check=True,
    )


# ============================================================
# Main
# ============================================================

def main():
    base_arg = ProgramArgument();

    for i in range(10):
        base_arg.run_id = i
        base_arg.run_name = f"sphere_count_{i + 1}"
        base_arg.sphere_count = 1 + i * 20;
        base_arg.geometry_div = i + 1;
        base_arg.renderer_variant = base_arg.renderer_variant + 1;
        if base_arg.renderer_variant == 5:
            base_arg.renderer_variant = 1;

        run(base_arg)


if __name__ == "__main__":
    main()