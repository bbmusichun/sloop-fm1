#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com>
"""Host preview of the MAKKURO37 boot splash (firmware/src/makkuro_splash.c).

Compiles the splash's drawing code with the host C compiler (MK_HOST: no LCD, no timers), renders
whole frames at the given times and writes them as PNG files (Pillow, 1x and --scale), a contact sheet,
the update screens (ota_<step>.png) and an animated GIF of the intro (intro.gif, 25 frames/s).

    python3 tools/makkuro_preview.py OUTDIR [--ms 0,400,...] [--scale 2] [--no-gif]
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
#include <string.h>
#define MK_HOST 1
static void str_cpy(char *d, const char *s, unsigned n)
{
    while (n > 1 && *s) {
        *d++ = *s++;
        n--;
    }
    *d = 0;
}
#include "makkuro_splash.c"
static uint16_t strip[240 * MK_SH];
int main(int argc, char **argv)
{
    int a, y;
    for (a = 1; a < argc; a++) {
        uint32_t t = (uint32_t)strtoul(argv[a] + (argv[a][0] == 'o'), 0, 10);
        mk_ota = argv[a][0] == 'o';
        if (mk_ota) {                    /* o<step>,<code> */
            mk_step = (uint8_t)t;
            mk_code = (int8_t)strtol(strchr(argv[a], ',') + 1, 0, 10);
            t = 4000;
        }
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
    ap.add_argument("--ms", default="60,300,700,1100,1500,1900,2300,2900,3020,3200,3600,4100,4400,4700,4900")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--no-gif", action="store_true")
    a = ap.parse_args()
    times = [int(v) for v in a.ms.split(",")]
    otas = ["o1,0", "o2,0", "o3,0", "o4,0", "o5,0", "o9,-14", "o9,1"]
    gif = [] if a.no_gif else list(range(0, 5000, 40))
    a.outdir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        c, exe = Path(td) / "h.c", Path(td) / "h"
        c.write_text(HARNESS)
        subprocess.run(["cc", "-O2", "-Wall", "-Wno-unused-function", "-I", str(SRC.parent), str(c),
                        "-o", str(exe)], check=True)
        raw = subprocess.run([str(exe), *map(str, times + otas + gif)], check=True,
                             capture_output=True).stdout

    def frame(n):
        px = raw[n * 240 * 240 * 2:(n + 1) * 240 * 240 * 2]
        img = Image.new("RGB", (240, 240))
        img.putdata([((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)
                     for v in (int.from_bytes(px[i:i + 2], "big") for i in range(0, len(px), 2))])
        return img

    def save(img, stem):
        img.save(a.outdir / f"{stem}_1x.png")
        big = img.resize((240 * a.scale, 240 * a.scale), Image.NEAREST) if a.scale > 1 else img
        big.save(a.outdir / f"{stem}.png")
        print(a.outdir / f"{stem}.png")
        return big

    frames = [save(frame(n), f"frame_{n:02d}") for n in range(len(times))]
    for k, o in enumerate(otas):
        save(frame(len(times) + k), "ota_" + o[1:].replace(",", "_").replace("-", "m"))
    if gif:
        g = [frame(len(times) + len(otas) + k) for k in range(len(gif))]
        g[0].save(a.outdir / "intro.gif", save_all=True, append_images=g[1:], duration=40, loop=0)
        print(a.outdir / "intro.gif")
    cols = min(3, len(frames))
    w, h = frames[0].size
    sheet = Image.new("RGB", (cols * w, (len(frames) + cols - 1) // cols * h))
    for n, img in enumerate(frames):
        sheet.paste(img, (n % cols * w, n // cols * h))
    sheet.save(a.outdir / "sheet.png")


if __name__ == "__main__":
    main()
