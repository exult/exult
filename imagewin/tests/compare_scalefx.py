#!/usr/bin/env python3
"""Build a matched-size visual comparison from benchmark_scalefx PPM files.

Requires Pillow only for this optional report, not for production or tests.
Usage: python3 imagewin/tests/compare_scalefx.py /tmp/scalefx /tmp/comparison.png
"""
import sys
from PIL import Image, ImageDraw, ImageFont

prefix, destination = sys.argv[1:]
font_path = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
try:
    font = ImageFont.truetype(font_path, 20)
    title_font = ImageFont.truetype(font_path, 25)
except OSError:
    font = title_font = ImageFont.load_default()
size = (576, 384)  # Source 48x32, all panels displayed at 12x for comparison.
gap, top, row_height = 24, 100, 442
canvas = Image.new("RGB", (3 * (size[0] + gap) + gap, top + 2 * row_height), "#e9ecf0")
draw = ImageDraw.Draw(canvas)
draw.text((gap, 16), "ScaleFX: native contours at 3x, 4x and 6x", fill="#16202d", font=title_font)
draw.text((gap, 52), "Matched physical size; nearest display zoom exposes subpixel patterns. Synthetic scene, no game assets.", fill="#334155", font=font)
for column, factor in enumerate((3, 4, 6)):
    x = gap + column * (size[0] + gap)
    for row in (0, 1):
        y = top + row * row_height
        kind = "native" if row == 0 else "enlarged"
        if row == 1 and factor == 3:
            path, label = prefix + "-source.ppm", "Source pixels (nearest, for context)"
        else:
            path = f"{prefix}-{kind}-{factor}.ppm"
            label = f"Native {factor}x" if row == 0 else f"3x output enlarged to {factor}x (comparison only)"
        draw.text((x, y), label, fill="#16202d", font=font)
        image = Image.open(path).resize(size, Image.Resampling.NEAREST)
        canvas.paste(image, (x, y + 35))
canvas.save(destination)
print(destination)
