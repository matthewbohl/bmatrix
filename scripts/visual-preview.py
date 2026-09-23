#!/usr/bin/env python3
"""Render bmatrix's actual logical output for visual review (requires Pillow)."""
import argparse
import json
from pathlib import Path
import subprocess

from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("binary", type=Path)
parser.add_argument("image", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--font", default="/System/Library/Fonts/Menlo.ttc")
parser.add_argument("--image-strength", type=float, default=.70)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
font = ImageFont.truetype(args.font, 15)
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
    frame(mode).save(args.output / f"{mode}.png")
frames = [frame("green", i / 30, 120, 40) for i in range(60)]
frames[0].save(args.output / "matrix.gif", save_all=True, append_images=frames[1:],
               duration=[33] * 60, loop=0, disposal=2)
strip = Image.new("RGB", (frames[0].width * 3, frames[0].height), "black")
for x, index in enumerate((16, 34, 59)):
    strip.paste(frames[index], (x * frames[0].width, 0))
strip.save(args.output / "matrix-stages.png")
print(args.output.resolve())
