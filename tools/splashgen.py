#!/usr/bin/env python3
"""
DMBoot 128 v5 - Splash logo generator

Draws the splash logo (a boot and "DMBoot 128") on an 80x50 grid of
sub-pixels and converts it to two Petmate9 screens:
- VIC 40x25: a character is 2x2 sub-pixels (PETSCII quarter blocks),
  one colour per character, VIC palette.
- VDC 80x25: a VDC character shows half as wide as a VIC one, so a
  character is 1x2 sub-pixels (half blocks), VDC RGBI palette.
Both screens therefore show the same image with the same proportions.

Output: assets/splash.petmate (for editing in Petmate9,
https://github.com/wbochar/petmate9) and PNG previews.

Usage: python3 tools/splashgen.py <charset-lower.bin> <outdir>
The character ROM is Petmate9's assets/c128-charset-lower.bin (only used
for the previews).

Code and resources from others used:
-   Petmate9 by wbochar, based on Petmate by Janne Hellsten
    (https://github.com/wbochar/petmate9): workspace file format (version 4,
    c128Lower and c128vdc frames, VDC attribute byte) and its C128
    character ROM for the previews. Adapted: only the file layout is
    produced; nothing of its code is used.
"""

import json
import sys

from PIL import Image

W, H = 80, 50                       # Sub-pixel grid
BG = '.'

# Logical colours -> (VIC index, VDC RGBI index)
COLOURS = {
    'L': (8, 12),    # Leather (VIC orange, VDC brown)
    'D': (9, 8),     # Leather shadow (VIC brown, VDC dark red)
    'Y': (7, 13),    # Collar / highlight (yellow)
    'W': (1, 15),    # Laces (white)
    'S': (11, 1),    # Sole (dark grey)
    'G': (12, 14),   # Welt (grey)
    'C': (14, 3),    # "DM" (light blue)
    'c': (6, 2),     # "DM" lower half (blue)
    'O': (7, 13),    # "Boot" top (yellow)
    'o': (8, 9),     # "Boot" bottom (VIC orange, VDC light red)
    'N': (1, 15),    # "128" (white)
    'T': (15, 14),   # Text (light grey)
}

VIC_PALETTE = [
    (0, 0, 0), (255, 255, 255), (136, 57, 50), (103, 182, 189),
    (139, 63, 150), (85, 160, 73), (64, 49, 141), (191, 206, 114),
    (139, 84, 41), (87, 66, 0), (184, 105, 98), (80, 80, 80),
    (120, 120, 120), (148, 224, 137), (120, 105, 196), (159, 159, 159),
]
VDC_PALETTE = [
    (0, 0, 0), (85, 85, 85), (0, 0, 170), (85, 85, 255),
    (0, 170, 0), (85, 255, 85), (0, 170, 170), (85, 255, 255),
    (170, 0, 0), (255, 85, 85), (170, 0, 170), (255, 85, 255),
    (170, 85, 0), (255, 255, 85), (170, 170, 170), (255, 255, 255),
]

# Quarter blocks (UL, UR, LL, LR) -> screen code, same in both C128 sets
QUARTERS = {
    (0, 0, 0, 0): 0x20, (0, 0, 0, 1): 0x6c, (0, 0, 1, 0): 0x7b, (0, 0, 1, 1): 0x62,
    (0, 1, 0, 0): 0x7c, (0, 1, 0, 1): 0xe1, (0, 1, 1, 0): 0xff, (0, 1, 1, 1): 0xfe,
    (1, 0, 0, 0): 0x7e, (1, 0, 0, 1): 0x7f, (1, 0, 1, 0): 0x61, (1, 0, 1, 1): 0xfc,
    (1, 1, 0, 0): 0xe2, (1, 1, 0, 1): 0xfb, (1, 1, 1, 0): 0xec, (1, 1, 1, 1): 0xa0,
}
# Half blocks (top, bottom) -> screen code
HALVES = {(0, 0): 0x20, (1, 0): 0xe2, (0, 1): 0x62, (1, 1): 0xa0}

VDC_ALTCHAR = 0x80                  # VDC attribute: lower case set

grid = [[BG] * W for _ in range(H)]


def plot(x, y, c):
    if 0 <= x < W and 0 <= y < H:
        grid[y][x] = c


def stamp(x0, y0, rows, colour_top, colour_bottom=None, split=None):
    """Draw a '#' bitmap; rows from `split` on get colour_bottom."""
    for dy, row in enumerate(rows):
        for dx, ch in enumerate(row):
            if ch == '#':
                c = colour_bottom if (split is not None and dy >= split) else colour_top
                plot(x0 + dx, y0 + dy, c)


# ---------------------------------------------------------------------------
# The boot: side view, toe to the right. Drawn row by row as spans.
# ---------------------------------------------------------------------------
BOOT = [
    # 0         1         2         3
    # 0123456789012345678901234567890123
    "....YYYYYYYYYYYY..................",   # 0 collar
    "....YYYYYYYYYYYY..................",
    "....DDLLLLLLLLLLWW................",   # 2
    "....DDLLLLLLLLLLWW................",
    "....DDLLLLLLLLWW..WW..............",   # 4 lace ends
    "....DDLLLLLLLLWW..WW..............",
    "....DDLLLLLLLLLLWW................",   # 6
    "....DDLLLLLLLLLLWW................",
    "....DDLLLLLLLLWWLL................",   # 8
    "....DDLLLLLLLLWWLL................",
    "....DDLLLLLLLLLLWWLL..............",   # 10
    "....DDLLLLLLLLLLWWLL..............",
    "....DDLLLLLLLLLLLLWWLL............",   # 12
    "....DDLLLLLLLLLLLLWWLL............",
    "....DDLLLLLLLLLLLLLLWWLL..........",   # 14
    "....DDLLLLLLLLLLLLLLWWLL..........",
    "....DDLLLLLLLLLLLLLLLLWWLL........",   # 16
    "....DDLLLLLLLLLLLLLLLLWWLL........",
    "....DDLLLLLLLLLLLLLLLLLLWWLL......",   # 18
    "....DDLLLLLLLLLLLLLLLLLLWWLL......",
    "....DDLLLLLLLLLLLLLLLLLLLLLLLL....",   # 20
    "....DDLLLLLLLLLLLLLLLLLLLLLLLLLL..",
    "....DDLLLLLLLLLLLLLLLLLLLLLLYYLL.",   # 22 toe cap highlight
    "....DDDDLLLLLLLLLLLLLLLLLLLLYYLLL.",
    "....DDDDDDLLLLLLLLLLLLLLLLLLLLLLLL",   # 24
    "....DDDDDDDDLLLLLLLLLLLLLLLLLLLLLL",
    "..GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG",   # 26 welt
    "..GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG.",
    "..SSSSSSSSSSSS......SSSSSSSSSSSS..",   # 28 heel + sole
    "..SSSSSSSSSSSS......SSSSSSSSSSS...",
]

BOOT_X, BOOT_Y = 2, 18
for dy, row in enumerate(BOOT):
    for dx, ch in enumerate(row):
        if ch != '.':
            plot(BOOT_X + dx, BOOT_Y + dy, ch)

# ---------------------------------------------------------------------------
# Big letters: strokes of 2 sub-pixels (one VIC character), capitals 14
# high, lower case 10 high on the same baseline.
# ---------------------------------------------------------------------------
LETTER_D = [
    "######..", "######..", "##..####", "##..####", "##....##", "##....##",
    "##....##", "##....##", "##....##", "##....##", "##..####", "##..####",
    "######..", "######..",
]
LETTER_M = [
    "##......##", "##......##", "####..####", "####..####", "##..##..##",
    "##..##..##", "##......##", "##......##", "##......##", "##......##",
    "##......##", "##......##", "##......##", "##......##",
]
LETTER_B = [
    "######..", "######..", "##....##", "##....##", "##....##", "##....##",
    "######..", "######..", "##....##", "##....##", "##....##", "##....##",
    "######..", "######..",
]
LETTER_O = [
    "..####..", "..####..", "##....##", "##....##", "##....##", "##....##",
    "##....##", "##....##", "..####..", "..####..",
]
LETTER_T = [
    "..##..", "..##..", "..##..", "..##..", "######", "######", "..##..",
    "..##..", "..##..", "..##..", "..##..", "..##..", "..####", "..####",
]

TEXT_X, CAP_Y = 12, 2               # Letters start (even: VIC cell aligned)
x = TEXT_X
stamp(x, CAP_Y, LETTER_D, 'C', 'c', 8); x += 10
stamp(x, CAP_Y, LETTER_M, 'C', 'c', 8); x += 12
stamp(x, CAP_Y, LETTER_B, 'O', 'o', 8); x += 10
stamp(x, CAP_Y + 4, LETTER_O, 'O', 'o', 4); x += 10
stamp(x, CAP_Y + 4, LETTER_O, 'O', 'o', 4); x += 10
stamp(x, CAP_Y, LETTER_T, 'O', 'o', 8)

# "128": digits 6 wide, 10 high, below "Boot"
DIGIT_1 = ["..##..", "####..", "..##..", "..##..", "..##..", "..##..", "..##..",
           "..##..", "######", "######"]
DIGIT_2 = ["######", "######", "....##", "....##", "######", "######", "##....",
           "##....", "######", "######"]
DIGIT_8 = ["######", "######", "##..##", "##..##", "######", "######", "##..##",
           "##..##", "######", "######"]
NUM_X, NUM_Y = 48, 20
stamp(NUM_X, NUM_Y, DIGIT_1, 'N')
stamp(NUM_X + 8, NUM_Y, DIGIT_2, 'N')
stamp(NUM_X + 16, NUM_Y, DIGIT_8, 'N')

# Plain text lines: (text, row, first 40-column cell, 40-column cells
# reserved); in 80 columns the text is centred on the same area
TEXTS = [
    ("Device Manager", 17, 22, 16),
    ("Boot Menu", 18, 22, 16),
]

# ---------------------------------------------------------------------------
# Conversion
# ---------------------------------------------------------------------------


def screencode(ch):
    """PETSCII lower case set screen code of an ASCII character."""
    if 'a' <= ch <= 'z':
        return ord(ch) - ord('a') + 1
    if 'A' <= ch <= 'Z':
        return ord(ch) - ord('A') + 65
    return ord(ch)                  # Space, digits, punctuation


def cell_colour(pixels):
    lit = [p for p in pixels if p != BG]
    if not lit:
        return None
    return max(set(lit), key=lit.count)


def build_vic():
    rows = []
    for cy in range(25):
        row = []
        for cx in range(40):
            px = [grid[cy * 2][cx * 2], grid[cy * 2][cx * 2 + 1],
                  grid[cy * 2 + 1][cx * 2], grid[cy * 2 + 1][cx * 2 + 1]]
            col = cell_colour(px)
            bits = tuple(0 if p == BG else 1 for p in px)
            row.append({"code": QUARTERS[bits],
                        "color": COLOURS[col][0] if col else COLOURS['T'][0]})
        rows.append(row)
    for text, row, col, span in TEXTS:
        start = col + (span - len(text)) // 2
        for i, ch in enumerate(text):
            rows[row][start + i] = {"code": screencode(ch), "color": COLOURS['T'][0]}
    return rows


def build_vdc():
    rows = []
    for cy in range(25):
        row = []
        for cx in range(80):
            px = [grid[cy * 2][cx], grid[cy * 2 + 1][cx]]
            col = cell_colour(px)
            bits = tuple(0 if p == BG else 1 for p in px)
            colour = COLOURS[col][1] if col else COLOURS['T'][1]
            row.append({"code": HALVES[bits], "color": colour,
                        "attr": colour | VDC_ALTCHAR})
        rows.append(row)
    # Same area as in 40 columns (twice the columns), text centred on it
    for text, row, col, span in TEXTS:
        start = col * 2 + (span * 2 - len(text)) // 2
        for i, ch in enumerate(text):
            colour = COLOURS['T'][1]
            rows[row][start + i] = {"code": screencode(ch), "color": colour,
                                    "attr": colour | VDC_ALTCHAR}
    return rows


def framebuf(name, rows, width, charset, background, border, columnmode):
    return {
        "width": width, "height": 25, "columnMode": columnmode,
        "backgroundColor": background, "borderColor": border, "borderOn": charset != 'c128vdc',
        "charset": charset, "name": name, "framebuf": rows,
        "zoom": {"zoomLevel": 2, "alignment": "left"},
    }


def preview(rows, width, palette, font, path, xscale, yscale):
    img = Image.new("RGB", (width * 8, 200))
    for cy, row in enumerate(rows):
        for cx, cell in enumerate(row):
            glyph = font[cell["code"] & 0xff]
            fg = palette[cell["color"]]
            for y in range(8):
                for x in range(8):
                    if (glyph[y] >> (7 - x)) & 1:
                        img.putpixel((cx * 8 + x, cy * 8 + y), fg)
    img.resize((width * 8 * xscale, 200 * yscale), Image.NEAREST).save(path)


def main():
    rom = open(sys.argv[1], 'rb').read()
    font = [rom[i * 8:i * 8 + 8] for i in range(256)]
    out = sys.argv[2]

    vic = build_vic()
    vdc = build_vdc()
    workspace = {
        "version": 4,
        "screens": [0, 1],
        "framebufs": [
            framebuf("splash_vic40", vic, 40, "c128Lower", 0, 0, 40),
            framebuf("splash_vdc80", vdc, 80, "c128vdc", 0, 0, 80),
        ],
        "customFonts": {},
    }
    with open(f"{out}/splash.petmate", "w") as f:
        json.dump(workspace, f)

    # Previews at the real aspect ratio: VIC 320x200 (x3), VDC 640x200
    # shown on the same picture height (x1.5 wide, x3 high)
    preview(vic, 40, VIC_PALETTE, font, f"{out}/splash_vic40.png", 3, 3)
    preview(vdc, 80, VDC_PALETTE, font, f"{out}/splash_vdc80_raw.png", 1, 1)
    Image.open(f"{out}/splash_vdc80_raw.png").resize((960, 600), Image.NEAREST).save(
        f"{out}/splash_vdc80.png")


if __name__ == "__main__":
    main()
