#!/usr/bin/env python3
"""
Convert a DMBoot v4 slot file (dmbootconf) into a v5 dmbslots.cfg.

Test tool and reference for the v4 -> v5 upgrader (plan §10). Rules:
- menu, path, file, cmd: PETSCII, copied as-is (sizes grow in v5).
- runboot and command flags keep their v4 values.
- Mount/REU paths: v5 stores Ultimate (ASCII) paths without the v4 "cd:"
  prefix. v4 used image_a_path+3 as the REU directory; when that is empty
  the REU directory is derived from the program path.
- COMMAND_IMGA/IMGB are cleared when no image file name is present
  (seen in real v4 files).
- Optional test slots from an existing v5 file can be kept at given slots.

Usage: convert_v4_slots.py v4file out.cfg [--keep v5file src:dst ...]
"""

import argparse

V4_STRIDE = 512
V4_PAGE = 256
# v4 stores a slot as two 256-byte pages (getslotfromem in v4 bootmenu.c):
# page 1 holds path..cfgvs, page 2 the image fields.
V4_PAGE1 = [("path", 100), ("menu", 21), ("file", 20), ("cmd", 80),
            ("reu_image", 20), ("reusize", 1), ("runboot", 1), ("device", 1),
            ("command", 1), ("cfgvs", 1)]
V4_PAGE2 = [("image_a_path", 100), ("image_a_file", 20), ("image_a_id", 1),
            ("image_b_path", 100), ("image_b_file", 20), ("image_b_id", 1)]

SLOTS = 36
SLOTSIZE = 1360
CFGVERSION = 0x05
V5_FIELDS = [("cfgvs", 1), ("path", 256), ("menu", 31), ("file", 51),
             ("cmd", 81), ("reu_image", 51), ("reu_path", 256), ("reusize", 1),
             ("runboot", 1), ("device", 1), ("command", 1),
             ("image_a_path", 256), ("image_a_file", 51), ("image_a_id", 1),
             ("image_b_path", 256), ("image_b_file", 51), ("image_b_id", 1),
             ("isdefault", 1), ("partition", 1), ("padding", 11)]

COMMAND_REU = 0x02
COMMAND_IMGA = 0x04
COMMAND_IMGB = 0x08


def cstr(raw):
    """Bytes up to the first zero."""
    return raw.split(b"\0", 1)[0]


def pet2asc(raw):
    """PETSCII (lowercase charset) to ASCII for Ultimate file system names."""
    out = bytearray()
    for c in raw:
        if 0x41 <= c <= 0x5A:
            out.append(c + 0x20)
        elif 0xC1 <= c <= 0xDA:
            out.append(c - 0x80)
        else:
            out.append(c)
    return bytes(out)


def strip_cd(raw):
    """Remove the v4 "cd:" prefix (PETSCII "CD:") from a path."""
    return raw[3:] if raw[:3].upper() == b"CD:" else raw


def parse_v4(data, slot):
    raw = data[slot * V4_STRIDE:(slot + 1) * V4_STRIDE]
    fields = {}
    for layout, offset in ((V4_PAGE1, 0), (V4_PAGE2, V4_PAGE)):
        for name, size in layout:
            value = raw[offset:offset + size]
            fields[name] = value[0] if size == 1 else cstr(value)
            offset += size
    return fields


def build_v5(values):
    out = bytearray()
    for name, size in V5_FIELDS:
        value = values.get(name, 0 if size == 1 else b"")
        if size == 1:
            out.append(value & 0xFF)
        else:
            if len(value) > size - 1:
                raise ValueError(f"{name} too long: {value!r}")
            out += value.ljust(size, b"\0")
    assert len(out) == SLOTSIZE
    return bytes(out)


def convert(v4):
    if not v4["menu"]:
        return None
    command = v4["command"]
    if command & COMMAND_IMGA and not v4["image_a_file"]:
        command &= ~COMMAND_IMGA
    if command & COMMAND_IMGB and not v4["image_b_file"]:
        command &= ~COMMAND_IMGB
    reu_dir = v4["image_a_path"] or v4["path"]
    return build_v5({
        "cfgvs": CFGVERSION,
        "path": v4["path"], "menu": v4["menu"], "file": v4["file"],
        "cmd": v4["cmd"],
        "reu_image": pet2asc(v4["reu_image"]),
        "reu_path": pet2asc(strip_cd(reu_dir)) if command & COMMAND_REU else b"",
        "reusize": v4["reusize"], "runboot": v4["runboot"],
        "device": v4["device"], "command": command,
        "image_a_path": pet2asc(strip_cd(v4["image_a_path"])),
        "image_a_file": pet2asc(v4["image_a_file"]),
        "image_a_id": v4["image_a_id"] if command & COMMAND_IMGA else 0,
        "image_b_path": pet2asc(strip_cd(v4["image_b_path"])),
        "image_b_file": pet2asc(v4["image_b_file"]),
        "image_b_id": v4["image_b_id"] if command & COMMAND_IMGB else 0,
    })


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("v4file")
    parser.add_argument("outfile")
    parser.add_argument("--keep", nargs="+", metavar="ARG",
                        help="v5 file, then src:dst slot pairs to copy over")
    args = parser.parse_args()

    data = open(args.v4file, "rb").read()
    if args.v4file.lower().endswith(".prg"):
        data = data[2:]
    out = bytearray(SLOTS * SLOTSIZE)
    for slot in range(SLOTS):
        converted = convert(parse_v4(data, slot))
        if converted:
            out[slot * SLOTSIZE:(slot + 1) * SLOTSIZE] = converted

    if args.keep:
        keep = open(args.keep[0], "rb").read()
        for pair in args.keep[1:]:
            src, dst = (int(x) for x in pair.split(":"))
            out[dst * SLOTSIZE:(dst + 1) * SLOTSIZE] = \
                keep[src * SLOTSIZE:(src + 1) * SLOTSIZE]

    open(args.outfile, "wb").write(out)


if __name__ == "__main__":
    main()
