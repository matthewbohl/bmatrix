#!/usr/bin/env python3
"""Render bmatrix's actual logical output for visual review (requires Pillow)."""
import argparse
import json
import math
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("image", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--font", help="Monospace TrueType font; defaults to Menlo or DejaVu Sans Mono")
parser.add_argument("--image-strength", type=float, default=.70)
parser.add_argument("--seconds", type=float, default=2.0)
parser.add_argument("--fps", type=int, default=30)
parser.add_argument("--snapshot-time", type=float, default=10.0, help="Palette stills use a settled frame, default 10 seconds")
args = parser.parse_args()
if not math.isfinite(args.seconds) or not .1 <= args.seconds <= 60 or not 1 <= args.fps <= 60:
    parser.error("seconds must be .1..60 and fps must be 1..60")
if not math.isfinite(args.snapshot_time) or not 0 < args.snapshot_time <= 3600:
    parser.error("snapshot time must be greater than zero and at most 3600")
args.output.mkdir(parents=True, exist_ok=True)
font_path = args.font or next((path for path in (
    "/System/Library/Fonts/Menlo.ttc",
    "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
) if Path(path).is_file()), None)
if not font_path:
    parser.error("no supported default font found; pass --font PATH")
font = ImageFont.truetype(font_path, 15)
cell_width, cell_height = 9, 18


def frame(mode, seconds=0, width=160, height=54):
    result = subprocess.run(
        [str(args.binary.resolve()), str(args.image.resolve()), "--mode", mode,
         "--size", f"{width}x{height}", "--effect", "matrix", "--image-strength", str(args.image_strength),
         "--output", "json", "--time", str(seconds)],
        check=True, capture_output=True, text=True)
    data = json.loads(result.stdout)
    image = Image.new("RGB", (width * cell_width, height * cell_height), "black")
    draw = ImageDraw.Draw(image)
    for index, (glyph, red, green, blue) in enumerate(data["cells"]):
        if glyph != 32:
            draw.text(((index % width) * cell_width, (index // width) * cell_height),
                      chr(glyph), font=font, fill=(red, green, blue))
    return image


for mode in ("green", "mono", "full"):
    frame(mode, args.snapshot_time).save(args.output / f"{mode}.png")
    count = max(2, math.ceil(args.seconds * args.fps))
    frames = [frame(mode, i / args.fps, 120, 40) for i in range(count)]
    # GIF timing is in 10 ms units. Distribute rounding over the timeline so
    # 30 fps remains six seconds for a six-second capture, rather than 5.4.
    durations = [(round((i + 1) * 100 / args.fps) - round(i * 100 / args.fps)) * 10
                 for i in range(count)]
    animation_name = "matrix" if mode == "green" else f"matrix-{mode}"
    frames[0].save(args.output / f"{animation_name}.gif", save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, disposal=2)
    strip = Image.new("RGB", (frames[0].width * 3, frames[0].height), "black")
    for x, index in enumerate((count // 4, count // 2, count - 1)):
        strip.paste(frames[index], (x * frames[0].width, 0))
    strip.save(args.output / f"{animation_name}-stages.png")
print(args.output.resolve())
