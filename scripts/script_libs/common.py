from __future__ import annotations

from datetime import datetime
from typing import Any

ERROR_TEXT_LIMIT = 8000


def now_iso() -> str:
    return datetime.now().astimezone().isoformat(timespec="seconds")


def fail(message: str) -> None:
    raise ValueError(message)


def render_argument(value: Any) -> str:
    if isinstance(value, bool):
        return "1" if value else "0"
    return str(value)


def trim_error(value: Any) -> str:
    text = str(value or "").strip()
    if len(text) <= ERROR_TEXT_LIMIT:
        return text
    return text[-ERROR_TEXT_LIMIT:]
