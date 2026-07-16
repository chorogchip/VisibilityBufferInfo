from __future__ import annotations

import subprocess
from dataclasses import asdict
from pathlib import Path

from .common import render_argument, trim_error
from .models import ProgramArgument


def command_for(executable: Path, arguments: ProgramArgument) -> list[str]:
    command = [str(executable)]
    for name, value in asdict(arguments).items():
        command.extend(["--" + name.replace("_", "-"), render_argument(value)])
    return command


def execute(
    command: list[str],
    cwd: Path,
    timeout_seconds: float | None,
) -> tuple[int | None, str, str, bool]:
    """Return return_code, process_error, stderr_text, interrupted."""
    try:
        completed = subprocess.run(
            command,
            cwd=cwd,
            check=False,
            stderr=subprocess.PIPE,
            text=True,
            errors="replace",
            timeout=timeout_seconds,
        )
        stderr_text = trim_error(completed.stderr)
        process_error = (
            "" if completed.returncode == 0
            else f"Process exited with code {completed.returncode}."
        )
        return completed.returncode, process_error, stderr_text, False
    except subprocess.TimeoutExpired as error:
        return (
            None,
            f"Process timed out after {timeout_seconds} second(s).",
            trim_error(error.stderr),
            False,
        )
    except KeyboardInterrupt:
        return None, "Interrupted by user.", "", True
    except Exception as error:
        return None, f"Could not execute benchmark: {error}", "", False
