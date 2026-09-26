#!/usr/bin/env python3
"""
DMBoot 128 v5 - Splash logo generator

Builds the splash screen (a boot and "DMBoot 128", credits at the bottom,
as the UBoot64 splash) as two Petmate9 screens:
- VIC 40x25, VIC palette, upper case/graphics set. The title and "128"
  are block letters from quarter blocks; the boot is placed by hand, one
  PETSCII glyph and colour per character (BOOT_GLYPHS / BOOT_COLOURS).
- VDC 80x25, VDC RGBI palette. A VDC character shows half as wide, so each
  VIC character becomes two VDC characters: the VIC screen is drawn,
  stretched to 640x200 and every VDC character gets the glyph of the upper
  case set that matches it best, in the colour of the VIC character. Laces,
  the patch and the text are set directly; text uses the lower case set
  (VDC attribute bit 7) for proper case.

Output: splash.petmate (for editing in Petmate9) and PNG previews.

Usage: python3 tools/splashgen.py [--edge=progress|footprints|stitch|lace] <petmate9-assets-dir> <outdir>
Uses c128-charset-upper.bin and c128-charset-lower.bin from Petmate9's
assets directory.

Code and resources from others used:
-   Petmate9 by wbochar, based on Petmate by Janne Hellsten
    (https://github.com/wbochar/petmate9): workspace file format (version 4,
    c128Upper and c128vdc frames, VDC attribute byte) and its C128
    character ROMs. Adapted: only the file layout is produced; nothing of
    its code is used.
"""

import json
import sys

import numpy as np
from PIL import Image

# ---------------------------------------------------------------------------
# Colours: logical key -> (VIC index, VDC RGBI index)
# ---------------------------------------------------------------------------
COLOURS = {
    ' ': (0, 0),     # background (black)
    'L': (8, 12),    # leather (VIC orange, VDC brown)
    'D': (9, 8),     # leather shadow, pull tab (VIC brown, VDC dark red)
    'R': (10, 9),    # tongue (light red)
    'Y': (7, 13),    # collar, patch (yellow)
    'W': (1, 15),    # laces, "128" (white)
    'G': (15, 14),   # welt, text (light grey)
    'S': (11, 1),    # sole (dark grey)
    'B': (3, 6),     # title banner (cyan)
    '1': (2, 8),     # title letters, row 1 (red)
    '2': (8, 12),    # title letters, rows 2-3 (orange)
    '4': (7, 13),    # title letters, rows 4-5 and "128" (yellow)
    'T': (13, 5),    # credits (light green)
    'P': (13, 5),    # progress bar (light green)
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

VDC_ALTCHAR = 0x80                  # VDC attribute: lower case set
REVERSE = 0x80                      # Screen code offset of reversed glyphs

# ---------------------------------------------------------------------------
# The boot, by hand: 19 columns (screen columns 0-18), 16 rows (screen
# rows 7-22). Glyph symbols (upper case/graphics set screen codes):
# ---------------------------------------------------------------------------
GLYPH = {
    ' ': 0x20,          # empty
    '#': 0xa0,          # full block
    '<': 0x69,          # top-left triangle
    '>': 0x5f,          # top-right triangle
    '/': 0xe9,          # bottom-right triangle (reversed top-left)
    '\\': 0xdf,         # bottom-left triangle (reversed top-right)
    'x': 0x56,          # crossed lace
    'n': 0x55,          # rounded corner, top-left
    'u': 0x4a,          # rounded corner, bottom-left
    '[': 0x61,          # left half
    'h': 0xc3,          # full block with a black line through the middle
    '|': 0xc2,          # full block with a black vertical line (seam)
    'q': 0xc9,          # full block with a black rounded corner (seam)
    's': 0xce,          # full block with a black diagonal (toe cap seam)
    '-': 0xc0,          # full block with a black horizontal line
    'T': 0xe2,          # upper half (tread)
    't': 0x77,          # top quarter (tread)
    'd': 0x84,          # reversed "D" (patch)
    'm': 0x8d,          # reversed "M" (patch)
}

BOOT_GLYPHS = [
    #0123456789012345678
    "      /\\           ",   # 7  tongue
    "n/######\\          ",   # 8  pull tab, collar
    "u#####x[           ",   # 9  laces
    " ##dm#x[           ",   # 10 patch
    " #####x#\\          ",   # 12
    " ######x#\\         ",   # 13
    " #######xx#\\       ",   # 14
    " #########xx#\\     ",   # 15
    " -q###########s\\   ",   # 16 heel seam, toe cap seam
    " #|##########s##\\  ",   # 17
    " #|#########s####\\ ",   # 18
    " #|########s######\\",   # 19
    " hhhhhhhhhhhhhhhhhh",   # 20 welt with stitching
    " #################<",   # 21 sole
    " #####  TtTtTtTtTt ",   # 22 heel, arch, treads
]
BOOT_COLOURS = [
    #0123456789012345678
    "      RR           ",
    "DYYYYYYYY          ",
    "DDLLLLWL           ",
    " DLYYLWL           ",
    " DLLLLWLL          ",
    " DLLLLLWLL         ",
    " DLLLLLLWWLL       ",
    " DLLLLLLLLWWLL     ",
    " DLLLLLLLLLLLLLL   ",
    " DLLLLLLLLLLLLLLL  ",
    " DLLLLLLLLLLLLLLLL ",
    " DLLLLLLLLLLLLLLLLL",
    " GGGGGGGGGGGGGGGGGG",
    " SSSSSSSSSSSSSSSSSS",
    " SSSSS  SSSSSSSSSS ",
]
BOOT_ROW = 8

# Title letters as in the UBoot64 splash: bitmaps of sub-pixels (2x2 per
# character), strokes two sub-pixels (one character) wide, 10 high (5
# rows), rounded corners. In a banner each letter gets a one sub-pixel
# outline in the background colour (also inside the counters); the rest
# of the banner is the banner colour; the letter colour runs per row.
QUARTERS = {
    (0, 0, 0, 0): 0x20, (0, 0, 0, 1): 0x6c, (0, 0, 1, 0): 0x7b, (0, 0, 1, 1): 0x62,
    (0, 1, 0, 0): 0x7c, (0, 1, 0, 1): 0xe1, (0, 1, 1, 0): 0xff, (0, 1, 1, 1): 0xfe,
    (1, 0, 0, 0): 0x7e, (1, 0, 0, 1): 0x7f, (1, 0, 1, 0): 0x61, (1, 0, 1, 1): 0xfc,
    (1, 1, 0, 0): 0xe2, (1, 1, 0, 1): 0xfb, (1, 1, 1, 0): 0xec, (1, 1, 1, 1): 0xa0,
}
LETTERS = {
    'D': ["######..", "#######.", "##....##", "##....##", "##....##",
          "##....##", "##....##", "##....##", "#######.", "######.."],
    'M': ["##......##", "###....###", "####..####", "##.####.##", "##..##..##",
          "##......##", "##......##", "##......##", "##......##", "##......##"],
    'B': ["######..", "#######.", "##....##", "##....##", "#######.",
          "#######.", "##....##", "##....##", "#######.", "######.."],
    'O': ["..####..", ".######.", "##....##", "##....##", "##....##",
          "##....##", "##....##", "##....##", ".######.", "..####.."],
    'T': ["##########", "##########", "....##....", "....##....", "....##....",
          "....##....", "....##....", "....##....", "....##....", "....##...."],
    '1': ["..##..", ".###..", "####..", "..##..", "..##..",
          "..##..", "..##..", "..##..", "######", "######"],
    '2': [".######.", "########", "......##", "......##", ".######.",
          "#######.", "##......", "##......", "########", "########"],
    '8': [".######.", "########", "##....##", "##....##", ".######.",
          ".######.", "##....##", "##....##", "########", ".######."],
}
LETTER_GAP = 2                      # Sub-pixels between letters

# Banners: (text, first cell column, first cell row, width and height in
# cells, letter colour per letter row 1-5)
BANNERS = [
    ("DMBOOT", 0, 0, 40, 7, "12244"),
    ("128", 23, 9, 17, 7, "44444"),        # Right-aligned
]

# Edge under the title banner (row 7), selected with --edge:
# - "footprints": a trail of footprints walking towards the boot (grey)
# - "stitch": the banner as a sewn label, a stitched hem of short dashes
#   (top quarter blocks) in the banner colour
# - "lace": laced like the boot, a white zigzag lace
# - "progress": booting, a progress bar (light green, grey track)
EDGE_ROW = 7
EDGE_STYLES = {
    # Walking left (toe on the left): sole, gap, heel; left foot low
    # (lower quarter/half blocks), right foot high (upper ones)
    "footprints": ([0x20, 0x6c, 0x62, 0x7b, 0x20, 0x62, 0x20,
                    0x20, 0x7c, 0xe2, 0x7e, 0x20, 0xe2, 0x20] * 3)[:40],
    "stitch": [0x77, 0x20] * 20,
    # Laced like the boot: a zigzag lace (white)
    "lace": [0x4d, 0x4e] * 20,
    # Booting: a progress bar, 70% done (lower half blocks, a quarter
    # block at the tip, a thin grey track for the rest)
    "progress": [0x20] + [0x62] * 26 + [0x7b] + [0x64] * 11 + [0x20],
}
EDGE_COLOURS = {"footprints": 'G', "stitch": 'B', "lace": 'W',
                "progress": " " + "P" * 27 + "G" * 11 + " "}

# Plain text lines: (text, row, first 40-column cell, 40-column cells
# reserved, colour); in 80 columns centred on the same area
TEXTS = [
    ("Device Manager", 18, 23, 17, 'G'),
    ("Boot Menu", 19, 23, 17, 'G'),
    ("Written 2020-2026 by Xander Mol", 23, 0, 40, 'T'),
    ("idreamtin8bits.com", 24, 0, 40, 'T'),
]

# ---------------------------------------------------------------------------
# VIC screen
# ---------------------------------------------------------------------------


def banner(cells, text, col, row, width, height, rowcolours):
    """Draw a banner with outlined letters into the VIC cells."""
    w, h = width * 2, height * 2
    letters = [LETTERS[ch] for ch in text]
    total = sum(len(l[0]) for l in letters) + LETTER_GAP * (len(letters) - 1)
    x = ((w - total) // 2) & ~1                     # Strokes on whole characters
    y = (h - 10) // 2
    mask = np.zeros((h, w), dtype=bool)
    for l in letters:
        for dy, line in enumerate(l):
            for dx, ch in enumerate(line):
                if ch == '#':
                    mask[y + dy, x + dx] = True
        x += len(l[0]) + LETTER_GAP
    grown = mask.copy()                              # Outline: one sub-pixel around
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            grown |= np.roll(np.roll(mask, dy, axis=0), dx, axis=1)
    for cy in range(height):
        for cx in range(width):
            m = mask[cy * 2:cy * 2 + 2, cx * 2:cx * 2 + 2].reshape(4)
            g = grown[cy * 2:cy * 2 + 2, cx * 2:cx * 2 + 2].reshape(4)
            if m.any():
                letter_row = min(max((cy * 2 - y) // 2, 0), 4)
                cells[row + cy][col + cx] = (QUARTERS[tuple(int(v) for v in m)],
                                             rowcolours[letter_row])
            else:
                fill = tuple(int(not v) for v in g)  # Banner colour outside the outline
                cells[row + cy][col + cx] = (QUARTERS[fill], 'B')


def screencode(ch, lower):
    """Screen code of an ASCII character: lower case set (proper case) or
    upper case set (capitals only)."""
    if ch.isalpha():
        if lower and ch.isupper():
            return ord(ch) - ord('A') + 65
        return ord(ch.lower()) - ord('a') + 1
    return ord(ch)


def vic_screen():
    """40x25 cells of (screen code, colour key)."""
    cells = [[(0x20, 'G') for _ in range(40)] for _ in range(25)]
    for b in BANNERS:
        banner(cells, *b)
    colours = EDGE_COLOURS[EDGE]
    for cx, code in enumerate(EDGE_STYLES[EDGE]):
        colour = colours[cx] if len(colours) > 1 else colours
        cells[EDGE_ROW][cx] = (code, colour if colour != ' ' else 'G')
    for r, (glyphs, colours) in enumerate(zip(BOOT_GLYPHS, BOOT_COLOURS)):
        for c, (g, col) in enumerate(zip(glyphs, colours)):
            if g != ' ':
                cells[BOOT_ROW + r][c] = (GLYPH[g], col)
    for text, row, col, span, colour in TEXTS:
        start = col + (span - len(text)) // 2
        for i, ch in enumerate(text):
            cells[row][start + i] = (screencode(ch, False), colour)
    return cells


# ---------------------------------------------------------------------------
# VDC screen: the VIC screen stretched to twice the width, glyph matched
# ---------------------------------------------------------------------------


def load_rom(path):
    rom = open(path, 'rb').read()
    return [rom[i * 8:i * 8 + 8] for i in range(256)]


def glyph_bitmap(font, code):
    return np.array([[(font[code][y] >> (7 - x)) & 1 for x in range(8)] for y in range(8)],
                    dtype=bool)


def vdc_screen(vic, upper):
    # Candidate glyphs: graphics only (space, $40-$7F and reversed)
    candidates = [0x20, 0xa0] + list(range(0x40, 0x80)) + list(range(0xc0, 0x100))
    bank = np.array([glyph_bitmap(upper, c).reshape(64) for c in candidates], dtype=float)
    cells = [[(0x20, 'G', False) for _ in range(80)] for _ in range(25)]
    for cy in range(25):
        for cx in range(40):
            code, colour = vic[cy][cx]
            if code == 0x20:
                continue
            wide = np.repeat(glyph_bitmap(upper, code), 2, axis=1)   # 8x16
            for half in range(2):
                target = wide[:, half * 8:half * 8 + 8].reshape(64).astype(float)
                err = (bank - target) ** 2
                best = candidates[int(err.sum(axis=1).argmin())]
                cells[cy][cx * 2 + half] = (best, colour, False)
    # Laces: a small cross in both halves
    for r, glyphs in enumerate(BOOT_GLYPHS):
        for c, g in enumerate(glyphs):
            if g == 'x':
                cells[BOOT_ROW + r][c * 2] = (0x56, 'W', False)
                cells[BOOT_ROW + r][c * 2 + 1] = (0x56, 'W', False)
            if g == 'd':
                cells[BOOT_ROW + r][c * 2] = (0xa0, 'Y', False)
                cells[BOOT_ROW + r][c * 2 + 1] = (screencode('D', True) | REVERSE, 'Y', True)
            if g == 'm':
                cells[BOOT_ROW + r][c * 2] = (screencode('M', True) | REVERSE, 'Y', True)
                cells[BOOT_ROW + r][c * 2 + 1] = (0xa0, 'Y', False)
    for text, row, col, span, colour in TEXTS:
        start = col * 2 + (span * 2 - len(text)) // 2
        for i in range(span * 2):
            cells[row][col * 2 + i] = (0x20, colour, False)
        for i, ch in enumerate(text):
            cells[row][start + i] = (screencode(ch, True), colour, True)
    return cells


# ---------------------------------------------------------------------------
# Output
# ---------------------------------------------------------------------------


def framebuf(name, rows, width, charset):
    return {
        "width": width, "height": 25, "columnMode": width,
        "backgroundColor": 0, "borderColor": 0, "borderOn": charset != 'c128vdc',
        "charset": charset, "name": name, "framebuf": rows,
        "zoom": {"zoomLevel": 2, "alignment": "left"},
    }


def preview(rows, width, palette, upper, lower, path):
    img = Image.new("RGB", (width * 8, 200))
    px = img.load()
    for cy, row in enumerate(rows):
        for cx, cell in enumerate(row):
            font = lower if cell.get("attr", 0) & VDC_ALTCHAR else upper
            glyph = font[cell["code"] & 0xff]
            fg = palette[cell["color"]]
            for yy in range(8):
                for xx in range(8):
                    if (glyph[yy] >> (7 - xx)) & 1:
                        px[cx * 8 + xx, cy * 8 + yy] = fg
    img.resize((960, 600), Image.NEAREST).save(path)    # Real aspect ratio


EDGE = "progress"


def main():
    global EDGE
    args = [a for a in sys.argv[1:] if not a.startswith("--edge=")]
    for a in sys.argv[1:]:
        if a.startswith("--edge="):
            EDGE = a.split("=", 1)[1]
    assets, out = args[0], args[1]
    upper = load_rom(f"{assets}/c128-charset-upper.bin")
    lower = load_rom(f"{assets}/c128-charset-lower.bin")

    vic_cells = vic_screen()
    vdc_cells = vdc_screen(vic_cells, upper)

    vic = [[{"code": code, "color": COLOURS[col][0]} for code, col in row] for row in vic_cells]
    vdc = [[{"code": code, "color": COLOURS[col][1],
             "attr": COLOURS[col][1] | (VDC_ALTCHAR if alt else 0)}
            for code, col, alt in row] for row in vdc_cells]

    workspace = {
        "version": 4,
        "screens": [0, 1],
        "framebufs": [framebuf("splash_vic40", vic, 40, "c128Upper"),
                      framebuf("splash_vdc80", vdc, 80, "c128vdc")],
        "customFonts": {},
    }
    with open(f"{out}/splash.petmate", "w") as f:
        json.dump(workspace, f)

    preview(vic, 40, VIC_PALETTE, upper, lower, f"{out}/splash_vic40.png")
    preview(vdc, 80, VDC_PALETTE, upper, lower, f"{out}/splash_vdc80.png")


if __name__ == "__main__":
    main()
