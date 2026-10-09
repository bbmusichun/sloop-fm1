/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* MAKKURO37 boot splash (build flag MAKKURO37): a neon doghouse with a piano-key roof, a sleeping low-poly
 * beagle on the ridge, a scrolling synthwave grid, waveforms, hexagons and the rainbow MAKKURO37 title.
 * All procedural (fixed point, no bitmaps). A frame is rendered in five strips of 240 x 48 px into the two
 * halves of the canvas pool (cv_px, gfx.c), alternately, so one strip renders while the last one goes out;
 * the title strip is redrawn only while it lights up and then every third frame. ~12 frames/s at 12 MHz SPI.
 * makkuro_splash_bg() draws the first frame at boot (where the SLOOP logo was); makkuro_splash() animates
 * it for MK_LEN ms (instead of the 900 ms logo wait) with a rainbow sweep on the pads, and ends early on a
 * key, pad or button press. tools/makkuro_preview.py renders frames of it on the host (MK_HOST). */

#define MK_SH 48                   /* strip rows: 5 strips per frame, 2 strip buffers in cv_px */
#define MK_LEN 3600u               /* ms */
#define MK_H 166                   /* the horizon */
#define Q(v) ((v) * 16)            /* pixels -> 1/16 px */

static uint16_t *mk_buf;
static int32_t mk_y0, mk_y1;       /* strip rows [y0, y1) */
static int32_t mk_core = 12, mk_glow = 36;    /* neon line: bright core, glow (1/16 px) */

static uint32_t mk_isqrt(uint32_t v)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v)
        b >>= 2;
    while (b) {
        if (v >= r + b) {
            v -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return r;
}

/* sine of a (1024 = a full turn), -256..256 (Bhaskara) */
static int32_t mk_sin(int32_t a)
{
    int32_t s = 1, p;
    a &= 1023;
    if (a >= 512) {
        a -= 512;
        s = -1;
    }
    p = a * (512 - a);
    return s * (4096 * p / (1310720 - 4 * p));
}

/* rainbow: h 0..1535 = red, yellow, green, cyan, blue, magenta */
static uint32_t mk_hue(uint32_t h)
{
    uint32_t f = h % 256u, r, g, b;
    switch ((h % 1536u) >> 8) {
    case 0: r = 255; g = f; b = 0; break;
    case 1: r = 255 - f; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = f; break;
    case 3: r = 0; g = 255 - f; b = 255; break;
    case 4: r = f; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - f; break;
    }
    return r << 16 | g << 8 | b;
}

static uint32_t mk_scale(uint32_t c, uint32_t k)       /* colour * k / 256 */
{
    return (((c >> 16) & 255u) * k >> 8) << 16 | (((c >> 8) & 255u) * k >> 8) << 8 | ((c & 255u) * k >> 8);
}

static inline uint16_t mk_565(uint32_t c)
{
    return (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u));
}

/* blend colour c at intensity i (0..256), whitened by w, into (x, y): mode 0 = brightest wins (light),
 * mode 1 = mix toward c by i (paint) */
static void mk_put(int32_t x, int32_t y, uint32_t c, int32_t i, int32_t w, int mode)
{
    uint16_t *p;
    int32_t o, r, g, b, cr, cg, cb;
    if ((uint32_t)x >= 240u || y < mk_y0 || y >= mk_y1)
        return;
    p = mk_buf + (y - mk_y0) * 240 + x;
    o = *p;
    cr = (o >> 11) << 3;
    cg = ((o >> 5) & 63) << 2;
    cb = (o & 31) << 3;
    r = (int32_t)(c >> 16) & 255;
    g = (int32_t)(c >> 8) & 255;
    b = (int32_t)c & 255;
    if (mode) {
        r = cr + ((r - cr) * i >> 8);
        g = cg + ((g - cg) * i >> 8);
        b = cb + ((b - cb) * i >> 8);
    } else {
        r = (r * i >> 8) + w;
        g = (g * i >> 8) + w;
        b = (b * i >> 8) + w;
        if (r < cr) r = cr;
        if (g < cg) g = cg;
        if (b < cb) b = cb;
    }
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    *p = (uint16_t)((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3));
}

/* neon shading at distance e (1/16 px) from the tube's centre line, level lvl (0..256) */
static void mk_shade(int32_t x, int32_t y, int32_t e, uint32_t c, int32_t lvl, int mode)
{
    int32_t i, w = 0;
    if (e < mk_core) {
        i = 256;
        w = (mk_core - e) * 110 / mk_core;
    } else {
        e -= mk_core;
        if (e >= mk_glow)
            return;
        i = (mk_glow - e) * 256 / mk_glow;
        i = (i * i >> 8) * 190 >> 8;
    }
    mk_put(x, y, c, i * lvl >> 8, w * lvl >> 8, mode);
}

typedef struct {
    int32_t ax, ay, dx, dy, len2, inv;
} mk_seg_t;

static void mk_seg(mk_seg_t *s, int32_t ax, int32_t ay, int32_t bx, int32_t by)
{
    uint32_t len;
    s->ax = ax;
    s->ay = ay;
    s->dx = bx - ax;
    s->dy = by - ay;
    s->len2 = s->dx * s->dx + s->dy * s->dy;
    len = mk_isqrt((uint32_t)s->len2);
    s->inv = len ? (int32_t)((1u << 20) / len) : 0;
}

static int32_t mk_dist(const mk_seg_t *s, int32_t px, int32_t py)     /* 1/16 px */
{
    int32_t qx = px - s->ax, qy = py - s->ay, dot = qx * s->dx + qy * s->dy, c;
    if (dot > 0 && dot < s->len2) {
        c = qx * s->dy - qy * s->dx;
        if (c < 0)
            c = -c;
        return (int32_t)(((uint32_t)c * (uint32_t)s->inv) >> 20);
    }
    if (dot > 0) {
        qx -= s->dx;
        qy -= s->dy;
    }
    return (int32_t)mk_isqrt((uint32_t)(qx * qx + qy * qy));
}

static int32_t mk_clip_lo(int32_t v, int32_t lo) { return v < lo ? lo : v; }
static int32_t mk_clip_hi(int32_t v, int32_t hi) { return v > hi ? hi : v; }

/* a neon line (1/16 px end points); only the pixels near it are visited */
static void mk_line(int32_t ax, int32_t ay, int32_t bx, int32_t by, uint32_t c, int32_t lvl, int mode)
{
    mk_seg_t s;
    int32_t pad = mk_core + mk_glow, y, ylo, yhi, xlo, xhi, m;
    ylo = mk_clip_lo(((ay < by ? ay : by) - pad) >> 4, mk_y0);
    yhi = mk_clip_hi(((ay > by ? ay : by) + pad) >> 4, mk_y1 - 1);
    if (ylo > yhi || lvl <= 0)
        return;
    xlo = mk_clip_lo(((ax < bx ? ax : bx) - pad) >> 4, 0);
    xhi = mk_clip_hi(((ax > bx ? ax : bx) + pad) >> 4, 239);
    mk_seg(&s, ax, ay, bx, by);
    m = (pad + 24) * (int32_t)mk_isqrt((uint32_t)s.len2);
    for (y = ylo; y <= yhi; y++) {
        int32_t py = Q(y) + 8, x0 = xlo, x1 = xhi, x;
        if (s.dy) {               /* the row's pixels within pad of the line */
            int32_t base = (py - ay) * s.dx, u = ax + (base - m) / s.dy, v = ax + (base + m) / s.dy;
            if (u > v) {
                x = u;
                u = v;
                v = x;
            }
            x0 = mk_clip_lo(u >> 4, xlo);
            x1 = mk_clip_hi((v >> 4) + 1, xhi);
        }
        for (x = x0; x <= x1; x++)
            mk_shade(x, y, mk_dist(&s, Q(x) + 8, py), c, lvl, mode);
    }
}

/* a closed or open polyline of n points (1/16 px, x y pairs) */
static void mk_path(const int32_t *p, int n, int closed, uint32_t c, int32_t lvl)
{
    int k;
    for (k = 0; k + 1 < n; k++)
        mk_line(p[2 * k], p[2 * k + 1], p[2 * k + 2], p[2 * k + 3], c, lvl, 0);
    if (closed)
        mk_line(p[2 * n - 2], p[2 * n - 1], p[0], p[1], c, lvl, 0);
}

/* a filled convex polygon (1/16 px), solid colour c */
static void mk_poly(const int32_t *p, int n, uint32_t c)
{
    int32_t ylo = 1 << 20, yhi = -(1 << 20), y, k;
    uint16_t col = mk_565(c);
    for (k = 0; k < n; k++) {
        if (p[2 * k + 1] < ylo)
            ylo = p[2 * k + 1];
        if (p[2 * k + 1] > yhi)
            yhi = p[2 * k + 1];
    }
    ylo = mk_clip_lo(ylo >> 4, mk_y0);
    yhi = mk_clip_hi(yhi >> 4, mk_y1 - 1);
    for (y = ylo; y <= yhi; y++) {
        int32_t py = Q(y) + 8, xl = 1 << 20, xr = -(1 << 20), x;
        for (k = 0; k < n; k++) {
            int32_t ax = p[2 * k], ay = p[2 * k + 1], j = (k + 1) % n, bx = p[2 * j], by = p[2 * j + 1];
            if ((ay <= py) != (by <= py)) {
                x = ax + (py - ay) * (bx - ax) / (by - ay);
                if (x < xl)
                    xl = x;
                if (x > xr)
                    xr = x;
            }
        }
        if (xl > xr)
            continue;
        xl = mk_clip_lo((xl + 7) >> 4, 0);
        xr = mk_clip_hi((xr - 8) >> 4, 239);
        for (x = xl; x <= xr; x++)
            mk_buf[(y - mk_y0) * 240 + x] = col;
    }
}

static void mk_quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
                    uint32_t fill, uint32_t line, int32_t lvl)
{
    int32_t p[8];
    p[0] = x0; p[1] = y0; p[2] = x1; p[3] = y1; p[4] = x2; p[5] = y2; p[6] = x3; p[7] = y3;
    mk_poly(p, 4, fill);
    if (lvl)
        mk_path(p, 4, 1, line, lvl);
}

/* ---- the title: hollow neon letters, a tube outline around each letter's strokes (grid 16 x 24) */
#define MK_UP 100                  /* pen up */
#define MK_END 101
static const int8_t MK_GLYPH[] = {
    0, 24, 0, 0, 8, 14, 16, 0, 16, 24, MK_END,                                                 /* M */
    0, 24, 8, 0, 16, 24, MK_UP, 3, 15, 13, 15, MK_END,                                         /* A */
    0, 0, 0, 24, MK_UP, 16, 0, 1, 15, MK_UP, 6, 10, 16, 24, MK_END,                            /* K */
    0, 0, 0, 18, 4, 24, 12, 24, 16, 18, 16, 0, MK_END,                                         /* U */
    0, 24, 0, 0, 11, 0, 16, 4, 16, 9, 11, 13, 0, 13, MK_UP, 9, 13, 16, 24, MK_END,             /* R */
    4, 0, 12, 0, 16, 5, 16, 19, 12, 24, 4, 24, 0, 19, 0, 5, 4, 0, MK_END,                      /* O */
    0, 3, 4, 0, 12, 0, 16, 4, 16, 8, 12, 12, 6, 12, MK_UP, 12, 12, 16, 16, 16, 20, 12, 24, 4, 24, 0, 21, MK_END, /* 3 */
    0, 0, 16, 0, 6, 24, MK_END,                                                                /* 7 */
};
static const uint8_t MK_TITLE[9] = {0, 1, 2, 2, 3, 4, 5, 6, 7};          /* MAKKURO37 */
static const uint32_t MK_TCOL[9] = {0xFF30D0, 0xD040FF, 0x8A5CFF, 0x4C8DFF, 0x22D8FF,
                                    0x30FF8A, 0xC0FF30, 0xFFB020, 0xFF5A3C};
#define MK_TX 12                   /* first letter's left edge; 25 px per letter */
#define MK_TY 8
#define MK_TS 17                   /* 1/16 px per grid unit */
#define MK_TR 40                   /* tube radius (1/16 px) */

static void mk_letter(int li, int32_t lvl)
{
    mk_seg_t s[12];
    const int8_t *g = MK_GLYPH;
    int n = 0, k, first = 1;
    int32_t ox = Q(MK_TX + 25 * li), oy = Q(MK_TY), px = 0, py = 0, x, y, ylo, yhi, xlo, xhi;
    int32_t pad = MK_TR + mk_core + mk_glow;
    uint32_t c = MK_TCOL[li];
    for (k = MK_TITLE[li]; k; g++)                /* find the glyph */
        if (*g == MK_END)
            k--;
    for (; *g != MK_END && n < 12; g++) {
        if (*g == MK_UP) {
            first = 1;
            continue;
        }
        x = ox + g[0] * MK_TS;
        y = oy + g[1] * MK_TS;
        g++;
        if (!first)
            mk_seg(&s[n++], px, py, x, y);
        first = 0;
        px = x;
        py = y;
    }
    ylo = mk_clip_lo((oy - pad) >> 4, mk_y0);
    yhi = mk_clip_hi((oy + 24 * MK_TS + pad) >> 4, mk_y1 - 1);
    xlo = mk_clip_lo((ox - pad) >> 4, 0);
    xhi = mk_clip_hi((ox + 16 * MK_TS + pad) >> 4, 239);
    for (y = ylo; y <= yhi; y++)
        for (x = xlo; x <= xhi; x++) {
            int32_t d = 1 << 20, e;
            for (k = 0; k < n; k++) {
                e = mk_dist(&s[k], Q(x) + 8, Q(y) + 8);
                if (e < d)
                    d = e;
            }
            e = d - MK_TR;
            if (e < 0) {
                if (d < MK_TR - mk_core - mk_glow)
                    continue;
                e = -e;
            }
            mk_shade(x, y, e, c, lvl, 0);
        }
}

/* letters light up one by one (with a flicker), then a slow shimmer runs along them */
static void mk_title(uint32_t t)
{
    int li;
    if (mk_y0 >= MK_TY + 34)
        return;
    mk_core = 11;
    mk_glow = 30;
    for (li = 0; li < 9; li++) {
        uint32_t on = 150u + (uint32_t)li * 90u, f;
        int32_t lvl;
        if (t < on)
            continue;
        f = (t - on) / 30u;
        if (f < 4u && (f == 1u || f == 3u))
            lvl = 70;                               /* flicker */
        else
            lvl = 210 + (mk_sin((int32_t)(t / 2u) - li * 90) * 46 >> 8);
        mk_letter(li, lvl);
    }
}

/* ---- the floor: a magenta perspective grid scrolling toward the viewer, a haze over the horizon */
static void mk_floor(uint32_t t)
{
    int32_t y, x, phase = (int32_t)(t * 256u / 520u) & 255;
    for (y = mk_clip_lo(MK_H - 16, mk_y0); y < mk_y1; y++) {
        uint16_t *row = mk_buf + (y - mk_y0) * 240;
        int32_t dy, s, fr, ph, ppu, lvl;
        if (y < MK_H) {                                     /* haze */
            uint32_t k = (uint32_t)(16 - (MK_H - y)) * 16u;
            uint16_t c = mk_565(mk_scale(0x5A0A78, k * k >> 8));
            for (x = 0; x < 240; x++)
                row[x] = c;
            continue;
        }
        dy = Q(y) + 8 - Q(MK_H);
        lvl = dy * 256 / Q(240 - MK_H);
        lvl = 50 + (lvl * lvl >> 8) * 206 / 256;
        s = 110 * 16 * 256 / dy + phase;                    /* depth of this row, 1/256 grid unit */
        fr = s & 255;
        fr = fr < 128 ? fr : 256 - fr;
        ph = fr * dy / 16 * dy / (256 * 110);               /* distance to the nearest cross line, 1/16 px */
        ppu = dy * 90 >> 8;                                  /* 1/16 px per grid unit along x */
        if (y == MK_H)
            ph = 0;
        for (x = 0; x < 240; x++) {
            int32_t fx = ((Q(x) + 8 - Q(120)) * 256 / ppu) & 255, pd, i;
            fx = fx < 128 ? fx : 256 - fx;
            pd = fx * ppu >> 8;
            if (ph < pd)
                pd = ph;
            if (pd >= 40)
                continue;
            i = pd < 9 ? 256 : (40 - pd) * 256 / 31;
            i = pd < 9 ? 256 : (i * i >> 8) * 170 >> 8;
            mk_buf[(y - mk_y0) * 240 + x] = 0;
            mk_put(x, y, 0xFF32C8, i * lvl >> 8, pd < 6 ? 60 * lvl >> 8 : 0, 0);
        }
    }
}

/* ---- the background: twinkling stars, hexagons, waveforms */
static void mk_hex(int32_t cx, int32_t cy, int32_t r, uint32_t c, int32_t lvl)
{
    int32_t p[12], h = r * 222 >> 8;
    cx = Q(cx);
    cy = Q(cy);
    r = Q(r);
    h = Q(h);
    p[0] = cx + r; p[1] = cy;
    p[2] = cx + r / 2; p[3] = cy + h;
    p[4] = cx - r / 2; p[5] = cy + h;
    p[6] = cx - r; p[7] = cy;
    p[8] = cx - r / 2; p[9] = cy - h;
    p[10] = cx + r / 2; p[11] = cy - h;
    mk_path(p, 6, 1, c, lvl);
}

static void mk_wave(int32_t x0, int32_t x1, int32_t cy, uint32_t h0, uint32_t t, int32_t seed)
{
    int32_t px = 0, py = 0, x;
    if (cy + 30 < mk_y0 || cy - 30 >= mk_y1)
        return;
    for (x = x0; x <= x1; x += 3) {
        int32_t k = (x - x0) * 1024 / (x1 - x0), env, a, y;
        env = mk_sin(k / 2);                                /* 0 at the ends, 256 in the middle */
        env = env * (200 + (mk_sin((int32_t)(t / 3u) + seed) >> 2)) >> 8;
        a = mk_sin(x * 37 - (int32_t)t / 2 + seed);
        a = mk_sin(x * 150 + seed + (int32_t)t) * (a < 0 ? -a : a) >> 8;
        y = Q(cy) - a * env * 5 / 1024;
        if (x > x0)
            mk_line(px, py, Q(x), y, mk_hue(h0 + (uint32_t)(x - x0) * 9u), 230, 0);
        px = Q(x);
        py = y;
    }
}

static void mk_back(uint32_t t)
{
    static const uint8_t STARS[] = {34, 52, 70, 88, 200, 44, 186, 98, 50, 140, 226, 128, 140, 58,
                                    10, 104, 164, 72, 112, 62, 232, 104, 60, 100};
    int k;
    for (k = 0; k < (int)sizeof STARS; k += 2) {
        int32_t i = 128 + (mk_sin((int32_t)t * (2 + k % 3) / 3 + k * 97) >> 1);
        mk_put(STARS[k], STARS[k + 1], 0xC8C8FF, i, 0, 0);
        mk_put(STARS[k] + 1, STARS[k + 1], 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k] - 1, STARS[k + 1], 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k], STARS[k + 1] + 1, 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k], STARS[k + 1] - 1, 0xC8C8FF, i / 3, 0, 0);
    }
    mk_core = 10;
    mk_glow = 28;
    mk_wave(2, 50, 118, 1280, t, 0);
    mk_wave(190, 238, 118, 600, t, 333);
    mk_core = 11;
    mk_glow = 34;
    mk_hex(20, 200, 13, 0xFFDC28, 180 + (mk_sin((int32_t)t / 2) * 60 >> 8));
    mk_hex(222, 204, 11, 0x50FF5A, 180 + (mk_sin((int32_t)t / 2 + 400) * 60 >> 8));
    mk_hex(224, 66, 7, 0xFF3CDC, 180 + (mk_sin((int32_t)t / 2 + 700) * 60 >> 8));
    mk_hex(16, 68, 6, 0x28DCFF, 180 + (mk_sin((int32_t)t / 2 + 200) * 60 >> 8));
}

/* ---- the doghouse: front face (x 56..118, y 148..208, apex 87,110), side wall back by V = (62, -14),
 * the right roof plane = piano keys from the ridge down to the eave */
#define MK_VX 62
#define MK_VY (-14)
#define MK_NK 12                   /* keys */
#define MK_CYAN 0x28DCFF

static void mk_house(void)
{
    static const int16_t WALL[] = {    /* front planks y, side planks */
        163, 178, 193};
    int k;
    if (mk_y0 > 212 || mk_y1 < 90)
        return;
    mk_core = 11;
    mk_glow = 34;
    /* faces */
    mk_quad(Q(118), Q(148), Q(118 + MK_VX), Q(148 + MK_VY), Q(118 + MK_VX), Q(208 + MK_VY), Q(118), Q(208),
            0x0A0618, 0, 0);
    mk_quad(Q(56), Q(148), Q(118), Q(148), Q(118), Q(208), Q(56), Q(208), 0x0C0820, 0, 0);
    {
        int32_t g[6] = {Q(56), Q(149), Q(87), Q(110), Q(118), Q(149)};
        mk_poly(g, 3, 0x0C0820);
    }
    /* door: black, an arch */
    {
        int32_t d[18] = {Q(72), Q(208), Q(72), Q(180), Q(74), 2780, Q(79) + 8, Q(167), Q(87), Q(165),
                         Q(94) + 8, Q(167), Q(100), 2780, Q(102), Q(180), Q(102), Q(208)};
        mk_poly(d + 2, 7, 0x000000);
        mk_quad(Q(72), Q(180), Q(102), Q(180), Q(102), Q(208), Q(72), Q(208), 0x000000, 0, 0);
        mk_path(d, 9, 0, MK_CYAN, 240);
    }
    /* edges and planks */
    mk_line(Q(56), Q(148), Q(56), Q(208), MK_CYAN, 256, 0);
    mk_line(Q(56), Q(208), Q(118), Q(208), MK_CYAN, 256, 0);
    mk_line(Q(118), Q(148), Q(118), Q(208), MK_CYAN, 256, 0);
    mk_line(Q(118), Q(208), Q(118 + MK_VX), Q(208 + MK_VY), MK_CYAN, 256, 0);
    mk_line(Q(118 + MK_VX), Q(208 + MK_VY), Q(118 + MK_VX), Q(146 + MK_VY), MK_CYAN, 256, 0);
    for (k = 0; k < 3; k++) {
        int32_t y = WALL[k];
        if (k == 0)
            mk_line(Q(56), Q(y), Q(118), Q(y), MK_CYAN, 170, 0);
        else {
            mk_line(Q(56), Q(y), Q(72), Q(y), MK_CYAN, 170, 0);
            mk_line(Q(102), Q(y), Q(118), Q(y), MK_CYAN, 170, 0);
        }
        mk_line(Q(118), Q(y), Q(118 + MK_VX), Q(y + MK_VY), MK_CYAN, 170, 0);
    }
}

/* key plank i (0 = front) from the ridge (s = 0) to the eave (s = 256), u in 1/256 key */
static void mk_roof_pt(int32_t *p, int32_t u, int32_t s)
{
    /* ridge 87,110 -> eave 127,154 (front); along the ridge by V per MK_NK keys */
    p[0] = Q(87) + Q(40) * s / 256 + Q(MK_VX) * u / (256 * MK_NK);
    p[1] = Q(110) + Q(44) * s / 256 + Q(MK_VY) * u / (256 * MK_NK);
}

static void mk_roof(uint32_t t)
{
    int32_t a[2], b[2], c[2], d[2], k;
    int32_t wave = (int32_t)(t * MK_NK * 256u / 1100u) % (MK_NK * 256 + 1024);   /* a light running along */
    if (mk_y0 > 165 || mk_y1 < 88)
        return;
    mk_core = 9;
    mk_glow = 24;
    /* the left (front gable) roof board */
    mk_quad(Q(87), Q(105), Q(87), Q(112), Q(52), Q(155), Q(44), Q(153), 0x300828, 0xFF30C8, 256);
    for (k = 0; k < MK_NK; k++) {
        uint32_t col = mk_hue(1300u + 1536u - ((uint32_t)k * 1350u / (MK_NK - 1) + t / 3u) % 1536u);
        int32_t g = k * 256 + 128 - wave, lvl;
        g = g < 0 ? -g : g;
        lvl = g < 512 ? 256 - g / 4 : 128;                       /* the light boosts nearby keys */
        lvl = 150 + (lvl - 128) * 106 / 128;
        mk_roof_pt(a, k * 256 + 22, 0);
        mk_roof_pt(b, k * 256 + 234, 0);
        mk_roof_pt(c, k * 256 + 234, 256);
        mk_roof_pt(d, k * 256 + 22, 256);
        /* the key's front face (its thickness) under the eave */
        mk_quad(d[0], d[1], c[0], c[1], c[0], c[1] + 80, d[0], d[1] + 80, mk_scale(col, 70), col, lvl * 3 / 4);
        mk_quad(a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1], mk_scale(col, 40 + lvl / 4), col, lvl);
    }
    for (k = 0; k + 1 < MK_NK; k++) {                         /* black keys: C# D#, F# G# A# */
        int n = k % 7;
        if (n == 2 || n == 6)
            continue;
        mk_roof_pt(a, (k + 1) * 256 - 70, 20);
        mk_roof_pt(b, (k + 1) * 256 + 70, 20);
        mk_roof_pt(c, (k + 1) * 256 + 70, 150);
        mk_roof_pt(d, (k + 1) * 256 - 70, 150);
        mk_quad(a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1], 0x06030A, 0x281C34, 256);
    }
}

/* ---- the dog: an original low-poly beagle asleep on the ridge. Local units: u along the ridge, v up. */
#define MK_DOX 97                  /* origin on the ridge */
#define MK_DOY 108
static const int8_t MK_DV[] = {    /* u, v */
    -24, 7,   -20, 12,  -10, 17,  1, 23,    9, 19,    13, 15,   5, 3,     -18, 2,   /* 0-7 head */
    -11, 4,   -8, 13,   18, 18,   32, 19,   46, 17,   56, 13,   58, 2,    44, 0,    /* 8-15 */
    28, 0,    12, 0,    30, 9,    47, 8,                                             /* 16-19 */
    -24, -2,  -24, 2,   6, 3,     8, -2,                                             /* 20-23 front paws */
    40, 9,    52, -2,   60, 3,    51, -3,   60, -3,                                  /* 24-28 hind leg */
    54, 13,   58, 9,                                                                 /* 29-30 tail base */
};
static const uint8_t MK_DT[] = {   /* triangles: 3 vertices, a colour index */
    20, 21, 22, 0,  20, 22, 23, 1,                                    /* paws */
    5, 10, 17, 4,   5, 17, 6, 0,                                      /* neck, chest */
    10, 11, 18, 6,  10, 18, 17, 7,  11, 12, 18, 7,  12, 19, 18, 6,    /* saddle */
    12, 13, 19, 3,  13, 14, 19, 4,  19, 14, 15, 0,  18, 19, 15, 1,  18, 15, 16, 0,  18, 16, 17, 1,
    24, 25, 26, 3,  25, 28, 26, 0,  27, 28, 25, 1,                   /* hind leg */
    0, 1, 8, 0,     0, 8, 7, 1,     1, 9, 8, 0,     1, 2, 9, 2,       /* head */
    2, 3, 9, 3,     3, 4, 9, 2,     9, 4, 6, 4,     8, 9, 6, 3,     7, 8, 6, 1,     4, 5, 6, 3,
};
static const uint32_t MK_DC[] = {0xF0ECE4, 0xD6D2CC, 0xD8913E, 0xC07A30, 0xA8662A, 0x000000, 0x30262C, 0x4A3A3E};
static const uint8_t MK_DSIL[] = {0, 1, 2, 3, 4, 5, 10, 11, 12, 13, 14};

static void mk_dpt(int32_t *p, int32_t u, int32_t v)       /* u, v in 1/16 px */
{
    /* the ridge rises by 14 px per 62 px: cos 250/256, sin 56/256 */
    u = u * 21 / 16;                                       /* (drawn 1.3x) */
    v = v * 21 / 16;
    p[0] = Q(MK_DOX) + (u * 250 - v * 56) / 256;
    p[1] = Q(MK_DOY) + (-u * 56 - v * 250) / 256;
}

static void mk_dog(uint32_t t)
{
    int32_t pt[31 * 2], tri[6], k, breath = mk_sin((int32_t)(t * 1024u / 1700u));
    int32_t ear = mk_sin((int32_t)(t * 1024u / 2300u) + 300), wag = mk_sin((int32_t)(t * 1024u / 520u));
    if (mk_y0 > 132 || mk_y1 < 50)
        return;
    for (k = 0; k < 31; k++) {
        int32_t u = Q(MK_DV[2 * k]), v = Q(MK_DV[2 * k + 1]);
        if (k >= 10 && k <= 13 && v > 0)
            v += v * breath / 2600;                               /* breathing: the back rises */
        mk_dpt(pt + 2 * k, u, v);
    }
    for (k = 0; k < (int)sizeof MK_DT; k += 4) {
        tri[0] = pt[2 * MK_DT[k]];
        tri[1] = pt[2 * MK_DT[k] + 1];
        tri[2] = pt[2 * MK_DT[k + 1]];
        tri[3] = pt[2 * MK_DT[k + 1] + 1];
        tri[4] = pt[2 * MK_DT[k + 2]];
        tri[5] = pt[2 * MK_DT[k + 2] + 1];
        mk_poly(tri, 3, MK_DC[MK_DT[k + 3]]);
    }
    mk_core = 6;
    mk_glow = 6;
    for (k = 0; k < (int)sizeof MK_DT; k += 4) {                /* the facets: faint dark seams */
        int32_t i0 = MK_DT[k], i1 = MK_DT[k + 1], i2 = MK_DT[k + 2];
        mk_line(pt[2 * i0], pt[2 * i0 + 1], pt[2 * i1], pt[2 * i1 + 1], 0x3A2A30, 70, 1);
        mk_line(pt[2 * i1], pt[2 * i1 + 1], pt[2 * i2], pt[2 * i2 + 1], 0x3A2A30, 70, 1);
    }
    {   /* tail: up with a white tip, wagging slowly */
        int32_t tp[8], tw[2], u = Q(66) + wag * 3 / 16, v = Q(25) - (wag < 0 ? -wag : wag) / 32;
        mk_dpt(tp, Q(54), Q(13));
        mk_dpt(tp + 2, Q(58), Q(8));
        mk_dpt(tp + 4, u + Q(2), v - Q(6));
        mk_dpt(tp + 6, u - Q(1), v - Q(3));
        mk_dpt(tw, u, v);
        mk_poly(tp, 4, 0xC07A30);
        {
            int32_t tt[6] = {tp[4], tp[5], tp[6], tp[7], tw[0], tw[1]};
            mk_poly(tt, 3, 0xF4F0EA);
        }
        mk_core = 9;
        mk_glow = 26;
        mk_line(tp[0], tp[1], tw[0], tw[1], MK_CYAN, 170, 0);
        mk_line(tp[2], tp[3], tw[0], tw[1], MK_CYAN, 170, 0);
    }
    {   /* closed eye, nose */
        int32_t e0[2], e1[2], e2[2], n0[6];
        mk_dpt(e0, -Q(14), Q(12));
        mk_dpt(e1, -Q(11), Q(10) + 8);
        mk_dpt(e2, -Q(8), Q(12));
        mk_core = 10;
        mk_glow = 6;
        mk_line(e0[0], e0[1], e1[0], e1[1], 0x201418, 256, 1);
        mk_line(e1[0], e1[1], e2[0], e2[1], 0x201418, 256, 1);
        mk_dpt(n0, -Q(25), Q(7));
        mk_dpt(n0 + 2, -Q(21), Q(11));
        mk_dpt(n0 + 4, -Q(21), Q(4));
        mk_poly(n0, 3, 0x1A1216);
    }
    mk_core = 9;
    mk_glow = 30;
    for (k = 0; k + 1 < (int)sizeof MK_DSIL; k++)              /* the outline: soft cyan neon */
        mk_line(pt[2 * MK_DSIL[k]], pt[2 * MK_DSIL[k] + 1], pt[2 * MK_DSIL[k + 1]], pt[2 * MK_DSIL[k + 1] + 1],
                MK_CYAN, 200, 0);
    {   /* the floppy ear, swinging a little */
        int32_t ep[16], sw = ear * 3 / 32;
        mk_dpt(ep, Q(0), Q(21));
        mk_dpt(ep + 2, Q(7), Q(20));
        mk_dpt(ep + 4, Q(11), Q(13));
        mk_dpt(ep + 6, Q(9) + sw / 2, Q(1));
        mk_dpt(ep + 8, Q(4) + sw, Q(-6));
        mk_dpt(ep + 10, Q(-2) + sw, Q(-6));
        mk_dpt(ep + 12, Q(-5) + sw / 2, Q(2));
        mk_dpt(ep + 14, Q(-4), Q(14));
        mk_poly(ep, 8, 0x4A2810);
        {
            int32_t hi[8] = {ep[0], ep[1], ep[2], ep[3], ep[4], ep[5], ep[14], ep[15]};
            mk_poly(hi, 4, 0x6A3A1A);
        }
        mk_path(ep, 8, 1, MK_CYAN, 210);
    }
}

/* Zz rising from the head */
static void mk_zz(uint32_t t)
{
    int k;
    if (mk_y0 > 100 || mk_y1 < 40)
        return;
    mk_core = 10;
    mk_glow = 24;
    for (k = 0; k < 2; k++) {
        int32_t p = (int32_t)((t + (uint32_t)k * 900u) % 1800u) * 256 / 1800, x, y, s, lvl;
        x = Q(84) - p * 3 / 2;
        y = Q(80) - p * 6 / 4;
        s = 64 + p / 4;                                  /* half size, 1/16 px */
        lvl = mk_sin(p * 2);
        if (lvl < 0)
            lvl = 0;
        mk_line(x - s, y - s, x + s, y - s, 0xB4E6FF, lvl, 0);
        mk_line(x + s, y - s, x - s, y + s, 0xB4E6FF, lvl, 0);
        mk_line(x - s, y + s, x + s, y + s, 0xB4E6FF, lvl, 0);
    }
}

/* one strip, rows y0 .. y0 + MK_SH - 1, into buf (byte-swapped for the panel) */
static void mk_render(uint16_t *buf, int32_t y0, uint32_t t)
{
    uint32_t i;
    mk_buf = buf;
    mk_y0 = y0;
    mk_y1 = y0 + MK_SH;
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = 0;
    mk_floor(t);
    mk_back(t);
    mk_house();
    mk_roof(t);
    mk_dog(t);
    mk_zz(t);
    mk_title(t);
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = (uint16_t)(buf[i] >> 8 | buf[i] << 8);
}

#ifndef MK_HOST
static uint16_t *mk_last;

static void mk_frame(uint32_t t, int title)
{
    uint32_t s;
    for (s = title ? 0u : 1u; s < 240u / MK_SH; s++) {
        uint16_t *b = cv_px + (s & 1u) * (240u * MK_SH);
        if (b == mk_last)
            lcd_sync();                     /* its last strip may still be going out */
        mk_render(b, (int32_t)(s * MK_SH), t);
        fm1_wdt_feed();
        lcd_blit(0, s * MK_SH, 240, MK_SH, b);
        mk_last = b;
    }
}

/* the first frame, at boot (before the input and the timers run) */
static void makkuro_splash_bg(void)
{
    mk_frame(0, 1);
    lcd_sync();
}

/* the animation, MK_LEN ms; a new key, pad or button press ends it */
static void makkuro_splash(void)
{
    uint32_t t0 = fm1_ms, t, held = fm1_in.notes | fm1_in.buttons, n = 0;
    for (;;) {
        uint32_t now = fm1_in.notes | fm1_in.buttons;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        t = fm1_ms - t0;
        if (t >= MK_LEN || (now & ~held))
            break;
        held &= now;                        /* (held at boot: a new press of it counts) */
        mk_frame(t, t < 1300u || n++ % 3u == 0u);
#if BOARD_SMK37
        {   /* the pads: a rainbow sweeping left to right, faded in and out, <= 60 per channel */
            uint32_t p, k = t < 300u ? t * 60u / 300u : t > MK_LEN - 400u ? (MK_LEN - t) * 60u / 400u : 60u;
            for (p = 0; p < 16u; p++) {
                uint32_t c = mk_scale(mk_hue(t * 3u / 2u + 1536u * 4u - (p & 7u) * 150u - (p >> 3) * 60u), k);
                padled_set(p, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
            }
            padled_flush();
        }
#endif
    }
#if BOARD_SMK37
    {
        uint32_t p;
        for (p = 0; p < 16u; p++)
            padled_set(p, 0, 0, 0);
        padled_flush();
    }
#endif
    lcd_sync();
}
#endif
