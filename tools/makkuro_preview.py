#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
"""Host preview of the MAKKURO37 boot splash (firmware/src/makkuro_splash.c).

Compiles the splash's drawing code with the host C compiler (MK_HOST: no LCD, no timers), renders
whole frames at the given times and writes them as PNG files (Pillow), plus a contact sheet.

    python3 tools/makkuro_preview.py OUTDIR [--ms 0,400,...] [--scale 2]
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "firmware" / "src" / "makkuro_splash.c"

HARNESS = r"""
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define MK_HOST 1
#include "makkuro_splash.c"
static uint16_t strip[240 * MK_SH];
int main(int argc, char **argv)
{
    int a, y;
    for (a = 1; a < argc; a++) {
        uint32_t t = (uint32_t)strtoul(argv[a], 0, 10);
        for (y = 0; y < 240; y += MK_SH) {
            mk_render(strip, y, t);
            fwrite(strip, 2, 240 * MK_SH, stdout);   /* big-endian RGB565, as the panel takes it */
        }
    }
    return 0;
}
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("outdir", type=Path)
    ap.add_argument("--ms", default="0,300,600,900,1300,1800,2400,3000,3500")
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args()
    times = [int(v) for v in a.ms.split(",")]
    a.outdir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        c, exe = Path(td) / "h.c", Path(td) / "h"
        c.write_text(HARNESS)
        subprocess.run(["cc", "-O2", "-Wall", "-Wno-unused-function", "-I", str(SRC.parent), str(c),
                        "-o", str(exe)], check=True)
        raw = subprocess.run([str(exe), *map(str, times)], check=True, capture_output=True).stdout
    frames = []
    for n, t in enumerate(times):
        px = raw[n * 240 * 240 * 2:(n + 1) * 240 * 240 * 2]
        img = Image.new("RGB", (240, 240))
        img.putdata([((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                     for v in (int.from_bytes(px[i:i + 2], "big") for i in range(0, len(px), 2))])
        if a.scale > 1:
            img = img.resize((240 * a.scale, 240 * a.scale), Image.NEAREST)
        out = a.outdir / f"frame_{n:02d}.png"
        img.save(out)
        frames.append(img)
        print(f"{out}  t={t} ms")
    cols = min(3, len(frames))
    w, h = frames[0].size
    sheet = Image.new("RGB", (cols * w, (len(frames) + cols - 1) // cols * h))
    for n, img in enumerate(frames):
        sheet.paste(img, (n % cols * w, n // cols * h))
    sheet.save(a.outdir / "sheet.png")


if __name__ == "__main__":
    main()
