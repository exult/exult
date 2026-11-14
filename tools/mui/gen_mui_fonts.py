#!/usr/bin/env python3
#  Copyright (C) 2026 The Exult Team
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
"""Render the anti-aliased bitmap fonts used by the themed menus.

The DejaVu Sans, Sans Bold, Serif and Serif Bold faces are rendered at
13, 16, 20, 24, 30, 36 and 48 px into Exult font shapes (one frame per byte
code 0-255) and packed into one Flex, data/mui_fonts.vga. Font index is
face * 7 + size index. Each glyph pixel holds its coverage level 1-4; the
engine (gumps/Mui_font.cc) remaps the levels to a ramp between the background
and text colours. Byte codes follow data/fonts/font_map.txt: ASCII 32-126,
the "builtin" special-character frames (UTF-8 text is translated to those),
and ISO-8859-1 160-255.

Each font is a Shape_file (shapes/vgafile.cc): u32 total length, u32 offset
per frame, then per frame xright, xleft, yabove, ybelow (s16) and RLE spans
(u16 len<<1, s16 x, s16 y, raw bytes), ending with a u16 0. Font::paint_text
draws a frame with its origin at the pen and advances by xleft + xright + 1.

Needs python3 with Pillow:
  gen_mui_fonts.py [--ttf DIR] [--map font_map.txt] -o data/mui_fonts.vga
  gen_mui_fonts.py --verify data/mui_fonts.vga
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

FLEX_TITLE_LEN = 80
FLEX_MAGIC1 = 0xFFFF1A00
FLEX_MAGIC2 = 0x000000CC
FLEX_HEADER_LEN = 0x80

FACES = ("DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSerif.ttf", "DejaVuSerif-Bold.ttf")
FACE_NAMES = ("sans", "sans_bold", "serif", "serif_bold")
SIZES = (13, 16, 20, 24, 30, 36, 48)
NUM_FRAMES = 256
LEVELS = 4
MIN_ALPHA = 0.12  # Coverage below this is left transparent.

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TTF = Path("/tmp/dejavu-fonts-ttf-2.37/ttf")
DEFAULT_MAP = ROOT / "data" / "fonts" / "font_map.txt"


def read_builtin_map(path: Path) -> dict[int, str]:
    """Frame code -> character from the 'builtin' section of font_map.txt."""
    out: dict[int, str] = {}
    section = None
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("%%section"):
            section = line.split()[1]
            continue
        if line.startswith("%%endsection"):
            section = None
            continue
        if section != "builtin" or not line.startswith(":"):
            continue
        hexpart, frame = line[1:].split("/")
        out[int(frame, 16)] = bytes.fromhex(hexpart).decode("utf-8")
    return out


def code_table(builtin: dict[int, str]) -> dict[int, str]:
    table = {c: chr(c) for c in range(32, 127)}
    table.update({c: chr(c) for c in range(160, 256)})
    table[160] = " "  # No-break space.
    table.update(builtin)
    return table


def coverage_level(alpha: int) -> int:
    a = alpha / 255.0
    if a < MIN_ALPHA:
        return 0
    return min(LEVELS, max(1, math.ceil(a * LEVELS)))


def render_font(ttf: Path, px: int, table: dict[int, str]):
    from PIL import Image, ImageDraw, ImageFont

    font = ImageFont.truetype(str(ttf), px)
    ascent, descent = font.getmetrics()
    pad = px  # Margin around the pen position in the scratch image.
    size = (px * 4, ascent + descent + 2 * pad)
    base = pad + ascent  # Baseline row in the scratch image.
    glyphs: dict[int, tuple[int, dict[tuple[int, int], int]]] = {}
    for code, ch in table.items():
        img = Image.new("L", size, 0)
        ImageDraw.Draw(img).text((pad, base), ch, font=font, fill=255, anchor="ls")
        advance = max(1, int(round(font.getlength(ch))))
        pixels: dict[tuple[int, int], int] = {}
        data = img.load()
        for py in range(size[1]):
            for qx in range(size[0]):
                lvl = coverage_level(data[qx, py])
                if lvl:
                    # Row 0 is the last row above the baseline (where the
                    # bottom of an 'x' sits), as in Exult's own fonts.
                    pixels[(qx - pad, py - base + 1)] = lvl
        glyphs[code] = (advance, pixels)
    # Line extents shared by every frame so all glyphs line up.
    yabove = max([ascent - 1] + [-y for _, p in glyphs.values() for (_, y) in p])
    ybelow = max([descent] + [y for _, p in glyphs.values() for (_, y) in p])
    return yabove, ybelow, glyphs


def encode_frame(advance: int, pixels: dict[tuple[int, int], int], yabove: int, ybelow: int) -> bytes:
    # Origin is the pen; xleft covers a negative left bearing and the
    # advance stays xleft + xright + 1. Pixels past the advance are clipped.
    minx = min((x for x, _ in pixels), default=0)
    xleft = max(0, -minx)
    xright = max(0, advance - 1 - xleft)
    out = bytearray(struct.pack("<hhhh", xright, xleft, yabove, ybelow))
    for y in range(-yabove, ybelow + 1):
        x = -xleft
        while x <= xright:
            if (x, y) not in pixels:
                x += 1
                continue
            start = x
            while x <= xright and (x, y) in pixels:
                x += 1
            run = bytes(pixels[(px, y)] for px in range(start, x))
            out += struct.pack("<Hhh", len(run) << 1, start, y) + run
    out += b"\0\0"
    return bytes(out)


def encode_shape(frames: list[bytes]) -> bytes:
    hdr = 4 + 4 * len(frames)
    offsets, pos = [], hdr
    for f in frames:
        offsets.append(pos)
        pos += len(f)
    return struct.pack("<I", pos) + struct.pack(f"<{len(offsets)}I", *offsets) + b"".join(frames)


def write_flex(entries: list[bytes], title: str) -> bytes:
    head = title.encode("ascii")[:FLEX_TITLE_LEN].ljust(FLEX_TITLE_LEN, b"\0")
    head += struct.pack("<III", FLEX_MAGIC1, len(entries), FLEX_MAGIC2)
    head = head.ljust(FLEX_HEADER_LEN, b"\0")
    pos = FLEX_HEADER_LEN + 8 * len(entries)
    table, body = bytearray(), bytearray()
    for e in entries:
        table += struct.pack("<II", pos if e else 0, len(e))
        body += e
        pos += len(e)
    return head + bytes(table) + bytes(body)


def read_flex(data: bytes) -> list[bytes]:
    if struct.unpack_from("<I", data, FLEX_TITLE_LEN)[0] != FLEX_MAGIC1:
        raise ValueError("not a Flex file")
    count = struct.unpack_from("<I", data, FLEX_TITLE_LEN + 4)[0]
    out = []
    for i in range(count):
        off, n = struct.unpack_from("<II", data, FLEX_HEADER_LEN + 8 * i)
        out.append(data[off:off + n])
    return out


def decode_shape(shape: bytes):
    """Decode as Shape_frame::read does: RLE iff the u32 length equals the entry length."""
    if struct.unpack_from("<I", shape, 0)[0] != len(shape):
        raise ValueError("shape length field does not match its Flex entry")
    hdr = struct.unpack_from("<I", shape, 4)[0]
    n = (hdr - 4) // 4
    offs = list(struct.unpack_from(f"<{n}I", shape, 4)) + [len(shape)]
    out = []
    for i in range(n):
        p, end = offs[i], offs[i + 1]
        xr, xl, ya, yb = struct.unpack_from("<hhhh", shape, p)
        p += 8
        pix: dict[tuple[int, int], int] = {}
        while True:
            sl = struct.unpack_from("<H", shape, p)[0]
            p += 2
            if sl == 0:
                break
            if sl & 1:
                raise ValueError(f"frame {i}: unexpected encoded span")
            x, y = struct.unpack_from("<hh", shape, p)
            p += 4
            for k in range(sl >> 1):
                if not (-xl <= x + k <= xr and -ya <= y <= yb):
                    raise ValueError(f"frame {i}: pixel ({x + k},{y}) outside its extents")
                pix[(x + k, y)] = shape[p + k]
            p += sl >> 1
        if p != end:
            raise ValueError(f"frame {i}: {end - p} trailing bytes")
        out.append((xr, xl, ya, yb, pix))
    return out


def generate(ttf_dir: Path, map_path: Path, out: Path) -> None:
    table = code_table(read_builtin_map(map_path))
    entries = []
    for face in FACES:
        for px in SIZES:
            yabove, ybelow, glyphs = render_font(ttf_dir / face, px, table)
            frames = []
            for code in range(NUM_FRAMES):
                if code in glyphs:
                    frames.append(encode_frame(*glyphs[code], yabove, ybelow))
                else:  # Unused code: empty, zero-advance frame.
                    frames.append(struct.pack("<hhhh", -1, 0, yabove, ybelow) + b"\0\0")
            entries.append(encode_shape(frames))
            print(f"gen_mui_fonts: {face} {px}px: line height {yabove + ybelow + 1}")
    out.write_bytes(write_flex(entries, "Exult themed menu fonts (DejaVu 2.37)"))
    print(f"gen_mui_fonts: wrote {out} ({len(entries)} fonts)")


def verify(vga: Path, map_path: Path) -> int:
    table = code_table(read_builtin_map(map_path))
    entries = read_flex(vga.read_bytes())
    errors: list[str] = []
    want = len(FACES) * len(SIZES)
    if len(entries) != want:
        errors.append(f"{len(entries)} fonts, want {want}")
    last_height: dict[int, int] = {}
    for num, e in enumerate(entries):
        face, size = divmod(num, len(SIZES))
        name = f"font {num} ({FACE_NAMES[face] if face < len(FACE_NAMES) else '?'} {SIZES[size % len(SIZES)]}px)"
        try:
            frames = decode_shape(e)
        except (ValueError, struct.error) as exc:
            errors.append(f"{name}: {exc}")
            continue
        if len(frames) != NUM_FRAMES:
            errors.append(f"{name}: {len(frames)} frames, want {NUM_FRAMES}")
            continue
        ya, yb = frames[ord("A")][2], frames[ord("A")][3]
        for code, (xr, xl, fya, fyb, pix) in enumerate(frames):
            if (fya, fyb) != (ya, yb):
                errors.append(f"{name} frame {code}: line extents differ")
            if any(v < 1 or v > LEVELS for v in pix.values()):
                errors.append(f"{name} frame {code}: pixel value outside 1-{LEVELS}")
            visible = code in table and not table[code].isspace()
            if visible and not pix:
                errors.append(f"{name} frame {code}: glyph {table[code]!r} is empty")
            if code not in table and (pix or xl + xr + 1 != 0):
                errors.append(f"{name} frame {code}: unused code is not empty")
        height = ya + yb + 1
        if size and height <= last_height.get(face, 0):
            errors.append(f"{name}: line height {height} not above the smaller size")
        last_height[face] = height
        print(f"{name}: {len(frames)} frames, line height {height}, 'M' advance "
              f"{frames[ord('M')][0] + frames[ord('M')][1] + 1}")
    for line in errors[:50]:
        print(f"FAIL {line}")
    print("verify: " + ("FAILED" if errors else "ok"))
    return 1 if errors else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-o", "--out", type=Path)
    ap.add_argument("--verify", type=Path, metavar="VGA")
    ap.add_argument("--ttf", type=Path, default=DEFAULT_TTF, help="folder with the DejaVu TTFs")
    ap.add_argument("--map", type=Path, default=DEFAULT_MAP, help="data/fonts/font_map.txt")
    a = ap.parse_args()
    try:
        if a.verify:
            return verify(a.verify, a.map)
        if not a.out:
            ap.error("need -o OUT (or --verify VGA)")
        generate(a.ttf, a.map, a.out)
    except (OSError, ValueError) as exc:
        print(f"gen_mui_fonts: error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
