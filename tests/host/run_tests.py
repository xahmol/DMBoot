#!/usr/bin/env python3
"""
Host tests for DMBoot's hardware-independent C modules (see README.md here).

Builds the test programs with gcc so they behave as under Oscar64 on the
C128 (32-bit long, packed structures) and checks them:
- timeconv.c against Python's datetime,
- v4convert.c against the reference converter tests/tools/convert_v4_slots.py
  on the real v4 files in tests/data.

Usage: python3 tests/host/run_tests.py   (exit code 0 = all passed)
"""

import datetime
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
DATA = os.path.join(ROOT, "tests", "data")
# As Oscar64: packed structures, unsigned char (host.h: 32-bit long)
CFLAGS = ["-std=gnu99", "-O1", "-fpack-struct=1", "-funsigned-char", "-Wno-unknown-pragmas",
          "-I" + os.path.join(HERE, "stub"), "-I" + os.path.join(ROOT, "src"),
          "-I" + os.path.join(ROOT, "include")]

failures = 0


def check(condition, message):
    """Count and report a failed check."""
    global failures
    if not condition:
        failures += 1
        print("FAIL:", message)


def build(name, outdir):
    """Compile tests/host/<name>.c; return the executable path."""
    exe = os.path.join(outdir, name)
    subprocess.run(["gcc"] + CFLAGS + ["-o", exe, os.path.join(HERE, name + ".c")], check=True)
    return exe


def test_timeconv(exe):
    """Compare epoch_to_uiitime with datetime for chosen and random times."""
    rng = random.Random(1128)
    cases = [(0, 0), (951782400, 0), (951868799, 0), (1709164800, 7200), (1735689599, 3600),
             (1735689600, -3600), (4102444799, 0), (4102444800, 0), (1790000000, -50400),
             (1790000000, 50400)]
    cases += [(rng.randrange(0, 2 ** 32 - 60000), rng.randrange(-50400, 50401)) for _ in range(3000)]
    cases = [(e, o) for e, o in cases if 0 <= e + o < 2 ** 32]
    stdin = "".join("%d %d\n" % c for c in cases)
    out = subprocess.run([exe], input=stdin, capture_output=True, text=True, check=True).stdout.split("\n")
    for (epoch, offset), line in zip(cases, out):
        t = datetime.datetime(1970, 1, 1) + datetime.timedelta(seconds=epoch + offset)
        expected = "%d %d %d %d %d %d" % (t.year, t.month, t.day, t.hour, t.minute, t.second)
        check(line == expected, "timeconv %d %+d: got %s, expected %s" % (epoch, offset, line, expected))
    print("timeconv: %d cases" % len(cases))


def test_v4convert(exe, outdir):
    """Byte compare the C conversion with the Python reference; show settings."""
    sets = [("first stick", "v4_dmbootconf.prg", "v4_DMBCFGFILE"),
            ("second stick", os.path.join("stick2", "dmbootconf.prg"), os.path.join("stick2", "DMBCFGFILE"))]
    for label, slots, cfg in sets:
        slotfile = os.path.join(DATA, slots)
        cfgfile = os.path.join(DATA, cfg)
        got = os.path.join(outdir, "c_slots.cfg")
        expected = os.path.join(outdir, "py_slots.cfg")
        result = subprocess.run([exe, slotfile, cfgfile, got], capture_output=True, text=True)
        check(result.returncode == 0, "%s: test program failed: %s" % (label, result.stderr))
        subprocess.run([sys.executable, os.path.join(ROOT, "tests", "tools", "convert_v4_slots.py"),
                        slotfile, expected], check=True)
        a = open(got, "rb").read()
        b = open(expected, "rb").read()
        check(a == b, "%s: C and Python slot files differ (first difference at byte %s)"
              % (label, next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), "length")))
        print("v4convert %s: slots %s" % (label, "identical" if a == b else "DIFFERENT"))
        print("  " + result.stdout.replace("\n", "\n  ").rstrip())


def pet(text):
    """PETSCII bytes as a drive sends them: lower case ASCII -> unshifted
    letters ($41-$5A), upper case -> shifted ($C1-$DA); bytes pass as is."""
    if isinstance(text, bytes):
        return text
    out = bytearray()
    for ch in text:
        c = ord(ch)
        if 0x61 <= c <= 0x7a:
            out.append(c - 0x20)
        elif 0x41 <= c <= 0x5a:
            out.append(c + 0x80)
        else:
            out.append(c)
    return bytes(out)


def hexs(data):
    """Hex as printed by the test program ("-" for empty)."""
    return data.hex() if data else "-"


def test_dirparse(exe):
    """Directory lines, image names and the dirtrace against expectations."""
    RVS, FREE, HEADER, OTHER = 0x12, 0x64, 5, 4
    PRG, SEQ, USR, REL, DEL, CBM, DIR, VRP, LNK = 0x11, 0x10, 0x12, 0x13, 0, 1, 2, 0x14, 3
    longname = "a" * 60
    lines = [
        # (line, rc, type, name, diskid)
        (bytes([RVS]) + b'"' + pet("geckos 2.0").ljust(16, b" ") + b'" ' + pet("g2 2a"), 0, HEADER, "geckos 2.0", "g2 2a"),
        (bytes([RVS]) + b'"' + pet("MyDisk") + b'" ' + pet("01 2a") + b"  ", 0, HEADER, "MyDisk", "01 2a"),
        ('   "loader"           prg  ', 0, PRG, "loader", ""),
        ('   "data"             seq<', 0, SEQ, "data", ""),
        ('  "game"             *prg ', 0, PRG, "game", ""),
        ('  "locked game"       prg< ', 0, PRG, "locked game", ""),
        (b'"' + pet("usb1") + b'"' + b"\xa0\xa0  " + pet("dir"), 0, DIR, "usb1", ""),
        ('"part1"  cbm', 0, CBM, "part1", ""),
        ('"x"  usr', 0, USR, "x", ""),
        ('"x"  rel', 0, REL, "x", ""),
        ('"x"  del', 0, DEL, "x", ""),
        ('"x"  vrp', 0, VRP, "x", ""),
        ('"x"  lnk', 0, LNK, "x", ""),
        ('"odd"  xyz', 0, OTHER, "odd", ""),
        ('"MyGame"   prg', 0, PRG, "MyGame", ""),
        ('"' + longname + '" prg', 0, PRG, "a" * 50, ""),
        ("blocks free.             ", 0, FREE, "", ""),
        ("BLOCKS FREE.", 0, FREE, "", ""),
        ("ab", 2, None, "", ""),
    ]
    images = [("game.d64", 1), ("GAME.D64", 1), ("x.g64", 1), ("a.d71", 1), ("a.g71", 1), ("a.d81", 1),
              ("a.g81", 1), ("a.dnp", 1), ("cpm.reu", 2), ("CPM.REU", 2), ("a.d82", 0), (".d64", 0),
              ("d64", 0), ("a.prg", 0), ("abc.d6", 0)]
    trace_steps = [
        # (command, expected output); trace buffer is 32 bytes
        ("Z", "ok"),
        ("A " + pet("usb1").hex(), hexs(pet("usb1/"))),
        ("A " + pet("c128demo").hex(), hexs(pet("usb1/c128demo/"))),
        ("C 1", hexs(pet("cd:/usb1/c128demo/"))),
        ("C 0", hexs(pet("cd//usb1/c128demo/"))),
        ("R", hexs(b"/usb1/c128demo/")),
        ("F " + pet("x" * 16).hex(), "1"),
        ("F " + pet("x" * 17).hex(), "0"),
        ("A " + pet("x" * 17).hex(), hexs(pet("usb1/c128demo/"))),
        ("U", "5 " + hexs(pet("usb1/"))),
        ("U", "0 -"),
        ("U", "0 -"),
        ("A " + pet("Usb1").hex(), hexs(pet("Usb1/"))),
        ("R", hexs(b"/Usb1/")),
    ]

    stdin = "".join("P %s\n" % pet(line).hex() for line, *_ in lines)
    stdin += "".join("I %s\n" % pet(name).hex() for name, _ in images)
    stdin += "".join(cmd + "\n" for cmd, _ in trace_steps)
    out = subprocess.run([exe], input=stdin, capture_output=True, text=True, check=True).stdout.split("\n")
    pos = 0
    for line, rc, ftype, name, diskid in lines:
        got = out[pos].split()
        pos += 1
        check(int(got[0]) == rc, "dirparse %r: rc %s, expected %d" % (line, got[0], rc))
        if rc:
            continue
        check(int(got[1]) == ftype, "dirparse %r: type %s, expected %d" % (line, got[1], ftype))
        check(got[2] == hexs(pet(name)), "dirparse %r: name %s, expected %s" % (line, got[2], hexs(pet(name))))
        if ftype == HEADER:
            check(got[3] == hexs(pet(diskid)), "dirparse %r: id %s, expected %s" % (line, got[3], hexs(pet(diskid))))
        check(int(got[4]) == len(pet(name)) + 1, "dirparse %r: length %s" % (line, got[4]))
    for name, kind in images:
        check(out[pos] == str(kind), "imagekind %r: %s, expected %d" % (name, out[pos], kind))
        pos += 1
    for cmd, expected in trace_steps:
        check(out[pos] == expected, "trace %s: %s, expected %s" % (cmd[:12], out[pos], expected))
        pos += 1
    print("dirparse: %d lines, %d names, %d trace steps" % (len(lines), len(images), len(trace_steps)))


def main():
    with tempfile.TemporaryDirectory() as outdir:
        test_timeconv(build("test_timeconv", outdir))
        test_v4convert(build("test_v4convert", outdir), outdir)
        test_dirparse(build("test_dirparse", outdir))
    print("ALL PASSED" if not failures else "%d FAILURES" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
