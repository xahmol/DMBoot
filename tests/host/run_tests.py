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
CFLAGS = ["-std=gnu99", "-O1", "-fpack-struct=1", "-Wno-unknown-pragmas",
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


def main():
    with tempfile.TemporaryDirectory() as outdir:
        test_timeconv(build("test_timeconv", outdir))
        test_v4convert(build("test_v4convert", outdir), outdir)
    print("ALL PASSED" if not failures else "%d FAILURES" % failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
