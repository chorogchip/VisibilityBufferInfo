#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageOps


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("--output", type=Path, default=Path("selected_pass_contact_sheet.jpg"))
    parser.add_argument("--columns", type=int, default=2)
    args = parser.parse_args()

    images = sorted(args.input_dir.glob("*.png"))
    if not images:
        raise FileNotFoundError("PNG 파일이 없습니다.")

    cols = args.columns
    rows = (len(images) + cols - 1) // cols
    thumb_w, thumb_h = 1100, 520
    label_h, pad, title_h, margin = 42, 24, 70, 30
    canvas_w = margin * 2 + cols * thumb_w + (cols - 1) * pad
    canvas_h = margin * 2 + title_h + rows * (thumb_h + label_h) + (rows - 1) * pad
    canvas = Image.new("RGB", (canvas_w, canvas_h), "white")
    draw = ImageDraw.Draw(canvas)

    try:
        title_font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 34)
        label_font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 24)
    except OSError:
        title_font = ImageFont.load_default()
        label_font = ImageFont.load_default()

    draw.text((margin, margin), "Selected Pass Line Plots", fill="black", font=title_font)
    y0 = margin + title_h
    for index, path in enumerate(images):
        row, col = divmod(index, cols)
        x = margin + col * (thumb_w + pad)
        y = y0 + row * (thumb_h + label_h + pad)
        image = Image.open(path).convert("RGB")
        image.thumbnail((thumb_w, thumb_h))
        thumb = Image.new("RGB", (thumb_w, thumb_h), "white")
        thumb.paste(image, ((thumb_w - image.width) // 2, (thumb_h - image.height) // 2))
        thumb = ImageOps.expand(thumb, border=1, fill="#cccccc")
        canvas.paste(thumb, (x, y))
        draw.text((x, y + thumb_h + 8), path.stem, fill="black", font=label_font)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(args.output, quality=92)
    print(f"[완료] {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
