/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* MAKKURO37 boot splash and update screens (build flag MAKKURO37): a neon doghouse whose roof is a piano
 * keyboard, a Tron-style dog asleep on the ridge, a synthwave sun and a scrolling grid, the rainbow title in
 * a rounded neon-tube font. All procedural, fixed point, no bitmaps.
 *
 * Everything is a function of the time t (ms): eased (easeOutExpo / easeOutCubic), sprung (a damped spring
 * table) and staggered. The intro (MK_LEN): CRT power-on, the sun rises and the grid rushes in, the house
 * draws itself, the 12 roof keys drop in with a bounce (each landing flashes its pad and plays a note of a
 * rising pentatonic jingle, seq.c mkj_*), the dog falls onto the ridge (squash and stretch), the title lights
 * letter by letter (flicker, RGB split, a shine sweep, a starburst), CRT power-off. A key, pad or button
 * press ends it at once (the jingle's notes are released).
 *
 * A frame is rendered in five strips of 240 x 48 px into the two halves of the canvas pool (cv_px, gfx.c),
 * alternately, so one strip renders while the last one goes out (~12 frames/s at 12 MHz SPI).
 * makkuro_splash_bg() draws the first frame at boot, makkuro_splash() animates (main.c), makkuro_ota() draws
 * one still frame per firmware-update step (felucca.c ota_show). tools/makkuro_preview.py renders it all on
 * the host (MK_HOST). */

#define MK_SH 48                   /* strip rows: 5 strips per frame, 2 strip buffers in cv_px */
#define MK_LEN 5000u               /* ms */
#define MK_H 166                   /* the horizon */
#define Q(v) ((v) * 16)            /* pixels -> 1/16 px */
#define MK_CYAN 0x28DCFFu

static uint16_t *mk_buf;
static int32_t mk_y0, mk_y1;       /* strip rows [y0, y1) */
static int32_t mk_core = 12, mk_glow = 36;    /* neon line: bright core, glow (1/16 px) */
static int32_t mk_hot = 110;       /* how white the core gets (0..255) */
static uint32_t mk_mask = 0xFFFFFFu;          /* colour channels drawn (the RGB split) */
static int32_t mk_shine = -9999;   /* the shine band (x + y / 2, px), -9999 = none */
static uint8_t mk_ota, mk_step;    /* an update screen: its step (1..5, 9 = result) */
static int8_t mk_code;

/* ---- maths */
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

/* progress of t through [t0, t0 + d): 0..256 */
static int32_t mk_prog(uint32_t t, uint32_t t0, uint32_t d)
{
    if (t <= t0)
        return 0;
    if (t >= t0 + d)
        return 256;
    return (int32_t)((t - t0) * 256u / d);
}

static int32_t mk_cubic(int32_t p)          /* easeOutCubic */
{
    int32_t q = 256 - p;
    return 256 - (q * q / 256 * q / 256);
}

static int32_t mk_expo(int32_t p)           /* easeOutExpo: 1 - 2^(-10 p) ~ cubic-bezier(0.16, 1, 0.3, 1) */
{
    int32_t k = p * 10, i = k >> 8, v;
    if (p >= 256)
        return 256;
    v = (65536 >> i) * (512 - (k & 255)) / 512;
    return 256 - (v >> 8);
}

/* a damped spring from 0 to 256 (overshoot 30 %, crosses 256 after 128 ms, settled ~600 ms): 25 ms steps */
static const int16_t MK_SPRING[25] = {0, 19, 67, 130, 195, 252, 295, 321, 332, 329, 316, 298, 279,
                                      261, 247, 238, 234, 234, 237, 242, 248, 253, 258, 261, 262};
#define MK_LAND 128u
static int32_t mk_spring(uint32_t ms)
{
    uint32_t i = ms / 25u, f = ms % 25u;
    int32_t v;
    if (i >= 24u)
        return 256;
    v = MK_SPRING[i] + (MK_SPRING[i + 1] - MK_SPRING[i]) * (int32_t)f / 25;
    return i >= 20u ? v + (256 - v) * (int32_t)(ms - 500u) / 100 : v;    /* (settles at 256) */
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
    if (k > 256u)
        k = 256u;
    return (((c >> 16) & 255u) * k >> 8) << 16 | (((c >> 8) & 255u) * k >> 8) << 8 | ((c & 255u) * k >> 8);
}

/* ---- pixels: blend colour c at intensity i (0..256), whitened by w, into (x, y): mode 0 = the brightest
 * wins (light), mode 1 = mix toward c by i (paint) */
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
        c = mk_mask;
        r = (r * i >> 8) + (c & 0xFF0000u ? w : 0);
        g = (g * i >> 8) + (c & 0xFF00u ? w : 0);
        b = (b * i >> 8) + (c & 0xFFu ? w : 0);
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
        w = (mk_core - e) * mk_hot / mk_core;
    } else {
        e -= mk_core;
        if (e >= mk_glow)
            return;
        i = (mk_glow - e) * 256 / mk_glow;
        i = (i * i >> 8) * 210 >> 8;
    }
    if (mk_shine > -9000) {                         /* the shine: a white band sweeping across */
        int32_t s = x + y / 2 - mk_shine;
        s = s < 0 ? -s : s;
        if (s < 10)
            w += (10 - s) * 22 * i >> 8;
    }
    mk_put(x, y, c & mk_mask, i * lvl >> 8, w * lvl >> 8, mode);
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

static int32_t mk_lo(int32_t v, int32_t lo) { return v < lo ? lo : v; }
static int32_t mk_hi(int32_t v, int32_t hi) { return v > hi ? hi : v; }

/* a neon line (1/16 px end points); only the pixels near it are visited */
static void mk_line(int32_t ax, int32_t ay, int32_t bx, int32_t by, uint32_t c, int32_t lvl, int mode)
{
    mk_seg_t s;
    int32_t pad = mk_core + mk_glow, y, ylo, yhi, xlo, xhi, m;
    ylo = mk_lo(((ay < by ? ay : by) - pad) >> 4, mk_y0);
    yhi = mk_hi(((ay > by ? ay : by) + pad) >> 4, mk_y1 - 1);
    if (ylo > yhi || lvl <= 0)
        return;
    xlo = mk_lo(((ax < bx ? ax : bx) - pad) >> 4, 0);
    xhi = mk_hi(((ax > bx ? ax : bx) + pad) >> 4, 239);
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
            x0 = mk_lo(u >> 4, xlo);
            x1 = mk_hi((v >> 4) + 1, xhi);
        }
        for (x = x0; x <= x1; x++)
            mk_shade(x, y, mk_dist(&s, Q(x) + 8, py), c, lvl, mode);
    }
}

/* a line drawn from a toward b up to p (0..256) */
static void mk_grow(int32_t ax, int32_t ay, int32_t bx, int32_t by, int32_t p, uint32_t c, int32_t lvl)
{
    if (p > 0)
        mk_line(ax, ay, ax + (bx - ax) * p / 256, ay + (by - ay) * p / 256, c, lvl, 0);
}

/* a polyline of n points (1/16 px, x y pairs), closed or open, drawn up to p (0..256) of its segments */
static void mk_path(const int32_t *pt, int n, int closed, uint32_t c, int32_t lvl, int32_t p)
{
    int k, ns = closed ? n : n - 1;
    int32_t a = p * ns;                             /* segments done, 1/256 */
    for (k = 0; k < ns && a > 0; k++, a -= 256) {
        int j = (k + 1) % n;
        mk_grow(pt[2 * k], pt[2 * k + 1], pt[2 * j], pt[2 * j + 1], a > 256 ? 256 : a, c, lvl);
    }
}

/* a filled polygon (1/16 px, even-odd), colour c mixed in by a (256 = solid) */
static void mk_poly(const int32_t *p, int n, uint32_t c, int32_t a)
{
    int32_t ylo = 1 << 20, yhi = -(1 << 20), y, k;
    uint16_t col = (uint16_t)(((c >> 19) & 31u) << 11 | ((c >> 10) & 63u) << 5 | ((c >> 3) & 31u));
    if (a <= 0)
        return;
    for (k = 0; k < n; k++) {
        if (p[2 * k + 1] < ylo)
            ylo = p[2 * k + 1];
        if (p[2 * k + 1] > yhi)
            yhi = p[2 * k + 1];
    }
    ylo = mk_lo(ylo >> 4, mk_y0);
    yhi = mk_hi(yhi >> 4, mk_y1 - 1);
    for (y = ylo; y <= yhi; y++) {
        int32_t py = Q(y) + 8, xs[12], m = 0, i, x;
        for (k = 0; k < n && m < 12; k++) {
            int32_t ax = p[2 * k], ay = p[2 * k + 1], j = (k + 1) % n, bx = p[2 * j], by = p[2 * j + 1];
            if ((ay <= py) != (by <= py)) {
                x = ax + (py - ay) * (bx - ax) / (by - ay);
                for (i = m++; i > 0 && xs[i - 1] > x; i--)
                    xs[i] = xs[i - 1];
                xs[i] = x;
            }
        }
        for (i = 0; i + 1 < m; i += 2) {
            int32_t xl = mk_lo((xs[i] + 7) >> 4, 0), xr = mk_hi((xs[i + 1] - 8) >> 4, 239);
            for (x = xl; x <= xr; x++) {
                if (a >= 256)
                    mk_buf[(y - mk_y0) * 240 + x] = col;
                else
                    mk_put(x, y, c, a, 0, 1);
            }
        }
    }
}

static void mk_quad(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3,
                    uint32_t fill, uint32_t line, int32_t lvl)
{
    int32_t p[8];
    p[0] = x0; p[1] = y0; p[2] = x1; p[3] = y1; p[4] = x2; p[5] = y2; p[6] = x3; p[7] = y3;
    mk_poly(p, 4, fill, 256);
    if (lvl)
        mk_path(p, 4, 1, line, lvl, 256);
}

/* ---- the font: geometric neon tubes (Tron style), strokes on a 20 x 28 grid, every inner corner rounded.
 * A glyph: the character, its width, the points; MK_UP lifts the pen, MK_END ends it. */
#define MK_UP 100
#define MK_END 101
static const int8_t MK_FONT[] = {
    'A', 20, 0, 28, 0, 10, 10, 0, 20, 10, 20, 28, MK_UP, 0, 18, 20, 18, MK_END,
    'C', 20, 20, 0, 0, 0, 0, 28, 20, 28, MK_END,
    'D', 20, 0, 14, 0, 0, 13, 0, 20, 7, 20, 21, 13, 28, 0, 28, 0, 14, MK_END,
    'E', 19, 19, 0, 0, 0, 0, 28, 19, 28, MK_UP, 0, 14, 13, 14, MK_END,
    'F', 19, 19, 0, 0, 0, 0, 28, MK_UP, 0, 14, 13, 14, MK_END,
    'G', 20, 20, 0, 0, 0, 0, 28, 20, 28, 20, 14, 11, 14, MK_END,
    'H', 20, 0, 0, 0, 28, MK_UP, 20, 0, 20, 28, MK_UP, 0, 14, 20, 14, MK_END,
    'I', 0, 0, 0, 0, 28, MK_END,
    'K', 19, 0, 0, 0, 28, MK_UP, 19, 0, 19, 6, 11, 14, 0, 14, MK_UP, 11, 14, 19, 22, 19, 28, MK_END,
    'L', 18, 0, 0, 0, 28, 18, 28, MK_END,
    'M', 24, 0, 28, 0, 0, 12, 13, 24, 0, 24, 28, MK_END,
    'N', 20, 0, 28, 0, 0, 20, 28, 20, 0, MK_END,
    'O', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, MK_END,
    'P', 20, 0, 28, 0, 0, 20, 0, 20, 15, 0, 15, MK_END,
    'R', 20, 0, 28, 0, 0, 20, 0, 20, 14, 0, 14, MK_UP, 12, 14, 20, 21, 20, 28, MK_END,
    'S', 20, 20, 0, 0, 0, 0, 14, 20, 14, 20, 28, 0, 28, MK_END,
    'T', 20, 0, 0, 20, 0, MK_UP, 10, 0, 10, 28, MK_END,
    'U', 20, 0, 0, 0, 28, 20, 28, 20, 0, MK_END,
    'W', 24, 0, 0, 0, 28, 24, 28, 24, 0, MK_UP, 12, 28, 12, 8, MK_END,
    'Y', 20, 0, 0, 0, 13, 20, 13, MK_UP, 20, 0, 20, 28, 0, 28, MK_END,
    '0', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, MK_UP, 6, 20, 14, 8, MK_END,
    '1', 12, 0, 5, 7, 0, 7, 28, MK_END,
    '2', 20, 0, 0, 20, 0, 20, 14, 0, 14, 0, 28, 20, 28, MK_END,
    '3', 20, 0, 0, 20, 0, 20, 28, 0, 28, MK_UP, 6, 14, 20, 14, MK_END,
    '4', 20, 0, 0, 0, 17, 20, 17, MK_UP, 15, 7, 15, 28, MK_END,
    '5', 20, 20, 0, 0, 0, 0, 13, 20, 13, 20, 28, 0, 28, MK_END,
    '6', 20, 20, 0, 0, 0, 0, 28, 20, 28, 20, 14, 0, 14, MK_END,
    '7', 20, 0, 0, 20, 0, 20, 7, 9, 28, MK_END,
    '8', 20, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, 0, 14, 20, 14, MK_END,
    '9', 20, 20, 14, 0, 14, 0, 0, 20, 0, 20, 28, 0, 28, MK_END,
    '-', 14, 0, 15, 14, 15, MK_END,
    '.', 0, 0, 27, 0, 28, MK_END,
    '\'', 0, 0, 0, 0, 7, MK_END,
    '!', 0, 0, 0, 0, 19, MK_UP, 0, 27, 0, 28, MK_END,
};
#define MK_GAP 9                   /* between letters, grid units */
#define MK_SPACE 9

static const int8_t *mk_glyph_of(char ch)
{
    const int8_t *g = MK_FONT;
    while (g < MK_FONT + sizeof MK_FONT) {
        if (g[0] == ch)
            return g;
        while (*g != MK_END)
            g++;
        g++;
    }
    return 0;
}

/* text width, 1/16 px, at k (1/256 px per grid unit) */
static int32_t mk_text_w(const char *s, int32_t k)
{
    int32_t w = 0;
    for (; *s; s++) {
        const int8_t *g = mk_glyph_of(*s);
        w += ((g ? g[1] : MK_SPACE) + (s[1] ? MK_GAP : 0)) * k / 16;
    }
    return w;
}

/* one glyph at (x, y) (1/16 px, top left), k = 1/256 px per grid unit; corners rounded by quadratic
 * curves (radius ~5 units); returns the advance */
static int32_t mk_glyph(char ch, int32_t x, int32_t y, int32_t k, uint32_t c, int32_t lvl)
{
    const int8_t *g = mk_glyph_of(ch);
    int32_t pt[24], n = 0, i;
    if (!g)
        return MK_SPACE * k / 16;
    if (y - Q(4) > Q(mk_y1) || y + 28 * k / 16 + Q(4) < Q(mk_y0))
        return g[1] * k / 16;
    for (i = 2;; i += 2) {
        if (g[i] == MK_UP || g[i] == MK_END) {   /* draw the stroke */
            int32_t cx = pt[0], cy = pt[1], j;
            for (j = 1; j < n; j++) {
                int32_t bx = pt[2 * j], by = pt[2 * j + 1];
                if (j < n - 1) {                      /* a corner: line to its start, then the curve */
                    int32_t ax = pt[2 * j - 2] - bx, ay = pt[2 * j - 1] - by, dx = pt[2 * j + 2] - bx,
                            dy = pt[2 * j + 3] - by, la = (int32_t)mk_isqrt((uint32_t)(ax * ax + ay * ay)),
                            lc = (int32_t)mk_isqrt((uint32_t)(dx * dx + dy * dy)), r = 5 * k / 16, t1x, t1y, t2x,
                            t2y, q, px = 0, py = 0;
                    if (r > la / 2)
                        r = la / 2;
                    if (r > lc / 2)
                        r = lc / 2;
                    t1x = bx + ax * r / la;
                    t1y = by + ay * r / la;
                    t2x = bx + dx * r / lc;
                    t2y = by + dy * r / lc;
                    mk_line(cx, cy, t1x, t1y, c, lvl, 0);
                    cx = t1x;
                    cy = t1y;
                    for (q = 1; q <= 3; q++) {        /* (1-t)^2 T1 + 2t(1-t) B + t^2 T2, t = q / 3 */
                        px = ((3 - q) * (3 - q) * t1x + 2 * q * (3 - q) * bx + q * q * t2x) / 9;
                        py = ((3 - q) * (3 - q) * t1y + 2 * q * (3 - q) * by + q * q * t2y) / 9;
                        mk_line(cx, cy, px, py, c, lvl, 0);
                        cx = px;
                        cy = py;
                    }
                } else {
                    mk_line(cx, cy, bx, by, c, lvl, 0);
                    cx = bx;
                    cy = by;
                }
            }
            n = 0;
            if (g[i] == MK_END)
                break;
            i--;                                       /* (MK_UP is one byte) */
            continue;
        }
        if (n < 12) {
            pt[2 * n] = x + g[i] * k / 16;
            pt[2 * n + 1] = y + g[i + 1] * k / 16;
            n++;
        }
    }
    return g[1] * k / 16;
}

/* a line of text centred on cx (px) at y (px), k, colour c (0: a rainbow along it) */
static void mk_text(const char *s, int32_t cx, int32_t y, int32_t k, uint32_t c, int32_t lvl)
{
    int32_t x = Q(cx) - mk_text_w(s, k) / 2, n = 0, len = 0;
    while (s[len])
        len++;
    for (; *s; s++, n++)
        x += mk_glyph(*s, x, Q(y), k, c ? c : mk_hue(1350u - (uint32_t)n * 1350u / (uint32_t)(len > 1 ? len - 1 : 1)),
                      lvl) + MK_GAP * k / 16;
}

/* ---- the title: lights letter by letter (flicker + an RGB split converging), a shine sweeps across it,
 * a starburst at the end */
#define MK_TK 224                  /* 0.875 px per grid unit */
#define MK_TY 7
#define MK_T0 3000u                /* the title's start */
static void mk_title(uint32_t t)
{
    static const char T[] = "MAKKURO37";
    int32_t x, li;
    if (mk_y0 > MK_TY + 34 || t < MK_T0)
        return;
    mk_core = 24;
    mk_glow = 48;
    mk_hot = 220;
    x = Q(120) - mk_text_w(T, MK_TK) / 2;
    if (t >= MK_T0 + 950u && t < MK_T0 + 1450u)
        mk_shine = -30 + mk_cubic(mk_prog(t, MK_T0 + 950u, 500u)) * 330 / 256;
    for (li = 0; li < 9; li++) {
        uint32_t on = MK_T0 + (uint32_t)li * 70u, f, c = mk_hue(1350u - (uint32_t)li * 1350u / 8u);
        int32_t lvl, split, adv = 0;
        if (t < on) {
            x += (mk_glyph_of(T[li])[1] + MK_GAP) * MK_TK / 16;
            continue;
        }
        f = (t - on) / 35u;
        lvl = f == 1u || f == 3u ? 60 : 236 + (mk_sin((int32_t)(t / 2u) - li * 90) * 20 >> 8);
        split = 6 * 16 * (256 - mk_cubic(mk_prog(t, on, 320u))) / 256;
        if (split > 4) {                           /* red and blue copies, converging */
            mk_mask = 0xFF0000u;
            mk_glyph(T[li], x + split, Q(MK_TY), MK_TK, c | 0xA00000u, lvl);
            mk_mask = 0x0000FFu;
            mk_glyph(T[li], x - split, Q(MK_TY), MK_TK, c | 0x0000A0u, lvl);
            mk_mask = 0x00FF00u;
            adv = mk_glyph(T[li], x, Q(MK_TY), MK_TK, c, lvl);
            mk_mask = 0xFFFFFFu;
        } else
            adv = mk_glyph(T[li], x, Q(MK_TY), MK_TK, c, lvl);
        x += adv + MK_GAP * MK_TK / 16;
    }
    mk_shine = -9999;
    mk_hot = 110;
    if (t >= MK_T0 + 1250u && t < MK_T0 + 1700u) {  /* starburst at the 7 */
        int32_t p = mk_prog(t, MK_T0 + 1250u, 450u), r = Q(4) + Q(14) * mk_expo(p) / 256,
                lvl = 256 - p, a;
        int32_t cx = Q(229), cy = Q(MK_TY + 1);
        mk_core = 10;
        mk_glow = 26;
        for (a = 0; a < 1024; a += 128) {
            int32_t rr = a % 256 ? r / 2 : r, sx = mk_sin(a + 256) * rr / 256, sy = mk_sin(a) * rr / 256;
            mk_line(cx + sx / 3, cy + sy / 3, cx + sx, cy + sy, 0xFFF8D0, lvl, 0);
        }
    }
}

/* ---- the sky: a synthwave sun rising, a haze over the horizon, twinkling stars */
static void mk_sky(uint32_t t)
{
    static const uint8_t STARS[] = {34, 52, 70, 88, 200, 44, 186, 98, 50, 140, 226, 128, 140, 58,
                                    10, 104, 164, 72, 112, 62, 232, 104, 60, 100, 20, 150, 214, 158};
    int32_t y, x, k, cy, r = 54;
    cy = 150 + 80 - 80 * mk_cubic(mk_prog(t, 300, 1000)) / 256;
    for (y = mk_lo(cy - r, mk_y0); y < mk_y1 && y < MK_H; y++) {
        int32_t d = y - cy, hw = (int32_t)mk_isqrt((uint32_t)(r * r - d * d)), rel = (d + r) * 256 / (2 * r);
        uint32_t c;
        if (d >= r)
            break;
        if (rel > 120) {                                         /* the stripes, wider toward the bottom */
            int32_t ph = (y - (cy - r)) % 9;
            if (ph < (rel - 120) * 6 / 136)
                continue;
        }
        c = rel < 128 ? (0xFFE03Cu & 0xFF0000u) | (uint32_t)(224 - rel * 120 / 128) << 8 | (uint32_t)(60 - rel * 30 / 128)
                      : 0xFF0000u | (uint32_t)(104 - (rel - 128) * 80 / 128) << 8 | (uint32_t)(30 + (rel - 128) * 140 / 128);
        for (x = mk_lo(128 - hw, 0); x <= mk_hi(128 + hw, 239); x++)
            mk_put(x, y, c, 200, 0, 0);
    }
    for (k = 0; k < (int)sizeof STARS; k += 2) {
        int32_t i = 128 + (mk_sin((int32_t)t * (2 + k % 3) / 3 + k * 97) >> 1);
        mk_put(STARS[k], STARS[k + 1], 0xC8C8FF, i, 0, 0);
        mk_put(STARS[k] + 1, STARS[k + 1], 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k] - 1, STARS[k + 1], 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k], STARS[k + 1] + 1, 0xC8C8FF, i / 3, 0, 0);
        mk_put(STARS[k], STARS[k + 1] - 1, 0xC8C8FF, i / 3, 0, 0);
    }
}

/* ---- the floor: a magenta perspective grid rushing in from the horizon, then scrolling toward the viewer */
static void mk_floor(uint32_t t)
{
    int32_t y, x, rush = mk_expo(mk_prog(t, 300, 1000)), ext = MK_H + 1 + (240 - MK_H) * rush / 256;
    int32_t phase = ((int32_t)(t * 256u / 520u) + 256 * 5 * rush / 256) & 255;
    for (y = mk_lo(MK_H - 16, mk_y0); y < mk_y1; y++) {
        uint16_t *row = mk_buf + (y - mk_y0) * 240;
        int32_t dy, s, fr, ph, ppu, lvl;
        if (y < MK_H) {                                     /* haze */
            uint32_t k = (uint32_t)(16 - (MK_H - y)) * 16u * (uint32_t)rush / 256u;
            uint32_t c = mk_scale(0x5A0A78, k * k >> 8);
            for (x = 0; x < 240; x++)
                mk_put(x, y, c, 256, 0, 0);
            continue;
        }
        if (y > ext)
            break;
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
            row[x] = 0;
            mk_put(x, y, 0xFF32C8, i * lvl >> 8, pd < 6 ? 60 * lvl >> 8 : 0, 0);
        }
    }
}

/* ---- hexagons, waveforms */
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
    mk_path(p, 6, 1, c, lvl, 256);
}

static void mk_wave(int32_t x0, int32_t x1, int32_t cy, uint32_t h0, uint32_t t, int32_t seed, int32_t amp)
{
    int32_t px = 0, py = 0, x;
    if (cy + 30 < mk_y0 || cy - 30 >= mk_y1 || amp <= 0)
        return;
    for (x = x0; x <= x1; x += 3) {
        int32_t k = (x - x0) * 1024 / (x1 - x0), env, a, y;
        env = mk_sin(k / 2) * amp >> 8;                     /* 0 at the ends */
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

static void mk_deco(uint32_t t)
{
    int32_t a = mk_cubic(mk_prog(t, 1200, 700));
    mk_core = 10;
    mk_glow = 28;
    mk_wave(2, 50, 118, 1280, t, 0, a);
    mk_wave(190, 238, 118, 600, t, 333, a);
    mk_core = 11;
    mk_glow = 34;
    mk_hex(20, 200, 13 * a / 256, 0xFFDC28, 180 + (mk_sin((int32_t)t / 2) * 60 >> 8));
    mk_hex(222, 204, 11 * a / 256, 0x50FF5A, 180 + (mk_sin((int32_t)t / 2 + 400) * 60 >> 8));
    mk_hex(224, 66, 7 * a / 256, 0xFF3CDC, 180 + (mk_sin((int32_t)t / 2 + 700) * 60 >> 8));
    mk_hex(16, 68, 6 * a / 256, 0x28DCFF, 180 + (mk_sin((int32_t)t / 2 + 200) * 60 >> 8));
}

/* ---- the doghouse: front face (x 56..118, y 148..208, apex 87,110), side wall back by V = (62, -14); its
 * lines draw themselves one after the other */
#define MK_VX 62
#define MK_VY (-14)
#define MK_NK 12                   /* roof keys */
static const uint8_t MK_HL[] = {   /* x0 y0 x1 y1 */
    56, 208, 56, 148,   56, 208, 118, 208,  118, 208, 118, 148, 118, 208, 180, 194,  180, 194, 180, 132,
    87, 105, 44, 153,   87, 112, 52, 155,   44, 153, 52, 155,
    56, 163, 118, 163,  118, 163, 180, 149, 56, 178, 72, 178,   102, 178, 118, 178, 118, 178, 180, 164,
    56, 193, 72, 193,   102, 193, 118, 193, 118, 193, 180, 179,
};

static void mk_house(uint32_t t)
{
    int32_t k, face = mk_cubic(mk_prog(t, 1300, 500));
    if (mk_y0 > 212 || mk_y1 < 90 || t < 900u)
        return;
    mk_core = 13;
    mk_glow = 34;
    {
        int32_t f[10] = {Q(56), Q(148), Q(87), Q(110), Q(118), Q(148), Q(118), Q(208), Q(56), Q(208)},
                s[8] = {Q(118), Q(148), Q(118 + MK_VX), Q(148 + MK_VY), Q(118 + MK_VX), Q(208 + MK_VY), Q(118), Q(208)},
                b[8] = {Q(87), Q(105), Q(87), Q(112), Q(52), Q(155), Q(44), Q(153)};
        mk_poly(f, 5, 0x0C0820, face);
        mk_poly(s, 4, 0x0A0618, face);
        mk_poly(b, 4, 0x300828, face);
    }
    {   /* the door: black, an arch */
        int32_t d[18] = {Q(72), Q(208), Q(72), Q(180), Q(74), 2780, Q(79) + 8, Q(167), Q(87), Q(165),
                         Q(94) + 8, Q(167), Q(100), 2780, Q(102), Q(180), Q(102), Q(208)};
        mk_poly(d, 9, 0, face);
        mk_path(d, 9, 0, MK_CYAN, 240, mk_cubic(mk_prog(t, 1150, 600)));
    }
    for (k = 0; k < (int)sizeof MK_HL / 4; k++) {
        const uint8_t *l = MK_HL + 4 * k;
        mk_grow(Q(l[0]), Q(l[1]), Q(l[2]), Q(l[3]), mk_cubic(mk_prog(t, 900u + (uint32_t)k * 40u, 380u)),
                k >= 5 && k < 8 ? 0xFF30C8 : MK_CYAN, k >= 8 ? 170 : 256);
    }
}

/* the roof: key i (0 = front) from the ridge (s = 0) to the eave (s = 256), u in 1/256 key */
static void mk_roof_pt(int32_t *p, int32_t u, int32_t s, int32_t dy)
{
    p[0] = Q(87) + Q(40) * s / 256 + Q(MK_VX) * u / (256 * MK_NK);
    p[1] = Q(110) + Q(44) * s / 256 + Q(MK_VY) * u / (256 * MK_NK) + dy;
}

#define MK_K0 1600u                /* the first key's drop */
#define MK_KD 100u                 /* between keys */
static uint32_t mk_key_col(int32_t k, uint32_t t)
{
    return mk_hue(1350u + 1536u * 4u - ((uint32_t)k * 1350u / (MK_NK - 1) + (t > 3000u ? (t - 3000u) / 3u : 0u)) % 1536u);
}

static void mk_roof(uint32_t t)
{
    int32_t a[2], b[2], c[2], d[2], k, lit = MK_NK;
    if (mk_y0 > 165 || mk_y1 < 20)
        return;
    if (mk_ota)
        lit = mk_step >= 5u ? MK_NK : mk_step * MK_NK / 5;
    mk_core = 9;
    mk_glow = 24;
    for (k = 0; k < MK_NK; k++) {
        uint32_t t0 = MK_K0 + (uint32_t)k * MK_KD, col = mk_key_col(k, t);
        int32_t dy = 0, lvl = 200, fl;
        if (!mk_ota && t < t0)
            continue;
        if (!mk_ota) {
            int32_t s = 256 - mk_spring(t - t0);
            dy = -Q(70) * (s < 0 ? -s : s) / 256;                /* falls, bounces */
            fl = t - t0 >= MK_LAND ? 256 - mk_prog(t, t0 + MK_LAND, 260u) : 0;
            lvl += fl * 56 / 256;                                 /* the landing flash */
        } else {
            fl = 0;
            if (mk_step == 9u && mk_code != 1)
                col = 0xFF2020;
            if (k >= lit) {
                col = 0x404060;
                lvl = 120;
            }
        }
        mk_roof_pt(a, k * 256 + 22, 0, dy);
        mk_roof_pt(b, k * 256 + 234, 0, dy);
        mk_roof_pt(c, k * 256 + 234, 256, dy);
        mk_roof_pt(d, k * 256 + 22, 256, dy);
        mk_quad(d[0], d[1], c[0], c[1], c[0], c[1] + 80, d[0], d[1] + 80, mk_scale(col, 70), col, lvl * 3 / 4);
        mk_quad(a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1],
                mk_scale(col, 50 + (uint32_t)lvl / 5 + (uint32_t)fl * 3 / 4), col, lvl);
    }
    for (k = 0; k + 1 < MK_NK; k++) {                         /* black keys: C# D#, F# G# A# */
        int n = k % 7;
        uint32_t t1 = MK_K0 + (uint32_t)(k + 1) * MK_KD + MK_LAND;
        if (n == 2 || n == 6 || (!mk_ota && t < t1))
            continue;
        mk_roof_pt(a, (k + 1) * 256 - 70, 20, 0);
        mk_roof_pt(b, (k + 1) * 256 + 70, 20, 0);
        mk_roof_pt(c, (k + 1) * 256 + 70, 150, 0);
        mk_roof_pt(d, (k + 1) * 256 - 70, 150, 0);
        {
            int32_t q[8] = {a[0], a[1], b[0], b[1], c[0], c[1], d[0], d[1]};
            mk_poly(q, 4, 0x06030A, mk_ota ? 256 : mk_cubic(mk_prog(t, t1, 200u)));
        }
    }
}

/* ---- the dog: an original Tron-style beagle asleep on the ridge: a dark body drawn by its glowing edge,
 * a few inner facet lines, a neon collar. Local units: u along the ridge, v up (x 1.125). */
#define MK_DOX 100                 /* origin on the ridge */
#define MK_DOY 108
#define MK_DN 26
static const int8_t MK_DSIL[] = {  /* the silhouette, from the nose over the back to the paws */
    -38, 10, -36, 14,  -30, 16,  -22, 18,  -18, 25,  -9, 29,  0, 27,   5, 21,   10, 18,  15, 22,
    28, 21,  40, 22,   49, 23,   56, 18,   59, 10,   65, 3,   65, -2,  46, -2,  32, 0,   14, -1,
    6, -2,   -36, -2,  -39, 1,   -36, 4,   -28, 5,   -34, 7,
};
static const int8_t MK_DFACET[] = { /* inner lines: pairs of points */
    -30, 16, -26, 10,  -26, 10, -34, 7,   -28, 5, -8, 3,    15, 22, 19, 9,   19, 9, 14, -1,
    19, 9, 35, 11,     28, 21, 35, 11,    35, 11, 46, 11,   49, 23, 46, 11,  46, 11, 51, 3,
    51, 3, 59, 10,
};
static int32_t mk_dsu = 256, mk_dsv = 256, mk_ddy;  /* squash and stretch (1/256), drop (1/16 px) */

static void mk_dpt(int32_t *p, int32_t u, int32_t v)       /* u, v in 1/16 px */
{
    u = u * 18 / 16 * mk_dsu / 256;
    v = v * 18 / 16 * mk_dsv / 256;
    p[0] = Q(MK_DOX) + (u * 250 - v * 56) / 256;          /* the ridge rises 14 px per 62: cos 250, sin 56 */
    p[1] = Q(MK_DOY) + (-u * 56 - v * 250) / 256 + mk_ddy;
}

/* mood: 0 asleep, 1 awake (eye open, ear up, tail up), 2 sad */
static void mk_dog(uint32_t t, int mood, int32_t ear, int32_t wag)
{
    int32_t pt[MK_DN * 2], k, q[4];
    if (mk_y0 > 132 + mk_ddy / 16 || mk_y1 < 40 + mk_ddy / 16)
        return;
    for (k = 0; k < MK_DN; k++)
        mk_dpt(pt + 2 * k, Q(MK_DSIL[2 * k]), Q(MK_DSIL[2 * k + 1]));
    mk_poly(pt, MK_DN, 0x03050C, 256);
    mk_core = 8;
    mk_glow = 20;
    for (k = 0; k < (int)sizeof MK_DFACET; k += 4) {          /* the facets, dim */
        mk_dpt(q, Q(MK_DFACET[k]), Q(MK_DFACET[k + 1]));
        mk_dpt(q + 2, Q(MK_DFACET[k + 2]), Q(MK_DFACET[k + 3]));
        mk_line(q[0], q[1], q[2], q[3], 0x1890C8, 150, 0);
    }
    mk_dpt(q, Q(19), Q(9));                                    /* circuit nodes */
    mk_line(q[0], q[1], q[0], q[1], 0xC0F8FF, 256, 0);
    mk_dpt(q, Q(46), Q(11));
    mk_line(q[0], q[1], q[0], q[1], 0xC0F8FF, 256, 0);
    {   /* tail: a neon curve, a bright tip */
        int32_t a[2], b[2], c[2], up = mood == 2 ? -Q(14) : mood == 1 ? Q(6) : 0;
        mk_dpt(a, Q(55), Q(18));
        mk_dpt(b, Q(61) + wag / 2, Q(26) + up / 2);
        mk_dpt(c, Q(63) + wag, Q(35) + up);
        mk_core = 14;
        mk_glow = 32;
        mk_line(a[0], a[1], b[0], b[1], MK_CYAN, 256, 0);
        mk_line(b[0], b[1], c[0], c[1], MK_CYAN, 256, 0);
        mk_core = 22;
        mk_line(c[0], c[1], c[0], c[1], 0xE0FFFF, 256, 0);
    }
    mk_core = 14;
    mk_glow = 34;
    mk_path(pt, MK_DN, 1, MK_CYAN, 256, 256);                  /* the edge */
    {   /* the collar */
        int32_t a[2], b[2];
        mk_dpt(a, Q(10), Q(18));
        mk_dpt(b, Q(12), Q(-1));
        mk_core = 16;
        mk_glow = 30;
        mk_line(a[0], a[1], b[0], b[1], 0xFF30C8, 256, 0);
        mk_dpt(a, Q(13), Q(-4));
        mk_core = 22;
        mk_line(a[0], a[1], a[0], a[1], 0xFFD040, 256, 0);
    }
    {   /* the floppy ear: dark, a neon edge; up when awake */
        static const int8_t E[] = {-8, 27, -3, 26, 0, 18, 0, 7, -3, 0, -9, -1, -13, 4, -12, 15};
        int32_t ep[16];
        for (k = 0; k < 8; k++) {
            int32_t u = E[2 * k], v = E[2 * k + 1], sw = (27 - v) * ear / 28;
            if (mood == 1) {                                   /* ear up, flipped back */
                u = -3 + (u + 5) / 2 + (27 - v) * 3 / 4;
                v = 27 + (27 - v) * 2 / 3;
                sw = 0;
            } else if (mood == 2)
                u -= (27 - v) / 4;
            mk_dpt(ep + 2 * k, Q(u) + sw, Q(v));
        }
        mk_poly(ep, 8, 0x0A0C18, 256);
        mk_core = 14;
        mk_glow = 30;
        mk_path(ep, 8, 1, 0x60F0FF, 256, 256);
    }
    {   /* the eye: a visor line when awake, a curve when asleep (down when sad), a nose */
        int32_t a[2], b[2], c[2];
        mk_core = 14;
        mk_glow = 30;
        mk_hot = 200;
        if (mood == 1) {
            mk_dpt(a, -Q(27), Q(21));
            mk_dpt(b, -Q(18), Q(21));
            mk_line(a[0], a[1], b[0], b[1], 0xA0F8FF, 256, 0);
        } else {
            int32_t m = mood == 2 ? Q(22) : Q(18);
            mk_dpt(a, -Q(26), Q(20));
            mk_dpt(b, -Q(22), m);
            mk_dpt(c, -Q(18), Q(20));
            mk_line(a[0], a[1], b[0], b[1], 0xA0F8FF, 256, 0);
            mk_line(b[0], b[1], c[0], c[1], 0xA0F8FF, 256, 0);
        }
        mk_dpt(a, -Q(38), Q(11));
        mk_core = 24;
        mk_line(a[0], a[1], a[0], a[1], 0xFF50D0, 256, 0);
        mk_hot = 110;
    }
    (void)t;
}

/* the dog in the intro: falls onto the ridge (stretched), lands (squash, spring), the ear swings, the
 * eyes close */
#define MK_DF 2750u                /* the fall starts */
#define MK_DL 2950u                /* lands */
static void mk_dog_intro(uint32_t t)
{
    int32_t ear, wag, sq;
    if (t < MK_DF)
        return;
    if (t < MK_DL) {
        int32_t p = mk_prog(t, MK_DF, MK_DL - MK_DF);
        mk_ddy = -Q(150) * (65536 - p * p) / 65536;
        mk_dsu = 236;
        mk_dsv = 290;
        ear = -40;
    } else {
        uint32_t s = t - MK_DL;
        sq = s < 600u ? mk_spring(s + MK_LAND) - 256 : 0;        /* + squash, - stretch */
        mk_ddy = 0;
        mk_dsv = 256 - sq * 9 / 10;
        mk_dsu = 256 + sq * 4 / 10;
        ear = s < 900u ? mk_sin((int32_t)(s * 1024u / 330u)) * (int32_t)(900u - s) / 900 * 3 / 2 : 0;
        ear += mk_sin((int32_t)(t * 1024u / 2300u)) * 16 / 256;
    }
    wag = mk_sin((int32_t)(t * 1024u / (t < MK_DL + 900u ? 300u : 700u))) * 3 / 8;
    mk_dog(t, t < MK_DL + 500u ? 1 : 0, ear, wag);
    mk_dsu = mk_dsv = 256;
    mk_ddy = 0;
}

/* Zz rising from the head */
static void mk_zz(uint32_t t, uint32_t t0)
{
    int k;
    if (mk_y0 > 100 || mk_y1 < 30 || t < t0)
        return;
    mk_core = 12;
    mk_glow = 26;
    for (k = 0; k < 2; k++) {
        int32_t p = (int32_t)((t - t0 + (uint32_t)k * 900u) % 1800u) * 256 / 1800, x, y, s, lvl;
        if (t - t0 + (uint32_t)k * 900u < 1800u && k)
            continue;
        x = Q(80) - p * 3 / 2;
        y = Q(78) - p * 6 / 4;
        s = 56 + p / 4;                                  /* half size, 1/16 px */
        lvl = mk_sin(p * 2);
        if (lvl < 0)
            lvl = 0;
        mk_line(x - s, y - s, x + s, y - s, 0xB4E6FF, lvl, 0);
        mk_line(x + s, y - s, x - s, y + s, 0xB4E6FF, lvl, 0);
        mk_line(x - s, y + s, x + s, y + s, 0xB4E6FF, lvl, 0);
    }
}

/* ---- CRT power on / off and scanlines (on the finished strip) */
static void mk_crt(uint32_t t)
{
    int32_t hh, hw, wh, y, x;                        /* half height, half width (1/16 px), whitening */
    if (t < 500u) {
        if (t < 140u) {
            hh = 24;
            hw = Q(120) * mk_expo(mk_prog(t, 0, 140)) / 256;
            wh = 256;
        } else {
            int32_t p = mk_expo(mk_prog(t, 140, 360));
            hh = 24 + (Q(120) - 24) * p / 256;
            hw = Q(121);
            wh = 220 - p * 220 / 256;
        }
    } else if (t >= 4600u && t < MK_LEN) {
        if (t < 4800u) {
            int32_t p = mk_prog(t, 4600, 200);
            hh = 24 + (Q(120) - 24) * (256 - p) / 256 * (256 - p) / 256;
            hw = Q(121);
            wh = p * 200 / 256;
        } else {
            int32_t p = mk_prog(t, 4800, 200);
            hh = 24 - 12 * p / 256;
            hw = Q(120) * (256 - mk_cubic(p)) / 256 + 24;
            wh = 256 - (p > 200 ? (p - 200) * 4 : 0);
        }
    } else {
        for (y = mk_y0 | 1; y < mk_y1; y += 2) {     /* scanlines: odd rows at 7/8 */
            uint16_t *r = mk_buf + (y - mk_y0) * 240;
            for (x = 0; x < 240; x++)
                r[x] = (uint16_t)(r[x] - ((r[x] >> 3) & 0x18E3u));
        }
        return;
    }
    for (y = mk_y0; y < mk_y1; y++) {
        uint16_t *r = mk_buf + (y - mk_y0) * 240;
        int32_t d = Q(y) + 8 - Q(120), w;
        d = d < 0 ? -d : d;
        if (d > hh) {
            for (x = 0; x < 240; x++)
                r[x] = 0;
            continue;
        }
        w = wh + (hh - d < 32 ? (32 - (hh - d)) * 6 : 0);       /* the edges glow */
        if (w > 256)
            w = 256;
        for (x = 0; x < 240; x++) {
            int32_t e = Q(x) + 8 - Q(120);
            if ((e < 0 ? -e : e) > hw)
                r[x] = 0;
            else if (w > 0)
                mk_put(x, y, 0xFFFFFF, w, 0, 1);
        }
    }
}

/* ---- update screens: big words, the scene, the keys as the progress */
static void mk_ota_text(void)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK HEAD", "LOADER", "CONFIRM", "RESTART"};
    char b[12];
    mk_core = 22;
    mk_glow = 44;
    mk_hot = 190;
    if (mk_step == 5u) {
        mk_text("UPDATING...", 120, 3, 230, 0, 250);
        mk_core = 20;
        mk_glow = 40;
        mk_text("DON'T", 120, 192, 184, 0xFFB020, 256);
        mk_text("SWITCH OFF", 120, 217, 184, 0xFFB020, 256);
    } else if (mk_step == 9u) {
        if (mk_code == 1) {
            mk_text("UPDATE", 120, 6, 340, 0, 250);
            mk_text("DRY RUN OK", 120, 206, 200, 0x40FF70, 256);
        } else {
            uint32_t v = (uint32_t)(mk_code < 0 ? -mk_code : mk_code), k = 0;
            mk_text("UPDATE", 120, 6, 340, 0xFF2828, 256);
            str_cpy(b, "FAILED -", sizeof b);
            while (b[k])
                k++;
            if (v >= 10u)
                b[k++] = (char)('0' + v / 10u % 10u);
            b[k++] = (char)('0' + v % 10u);
            b[k] = 0;
            mk_text(b, 120, 206, 200, 0xFF3030, 256);
        }
    } else {
        mk_text("UPDATE", 120, 6, 340, 0, 250);
        mk_core = 18;
        mk_glow = 38;
        mk_text(STEP[mk_step < 6u ? mk_step : 0], 120, 208, 196, 0x60F0FF, 256);
    }
    mk_hot = 110;
}

/* one strip, rows y0 .. y0 + MK_SH - 1, into buf (byte-swapped for the panel) */
static void mk_render(uint16_t *buf, int32_t y0, uint32_t t)
{
    uint32_t i;
    int32_t sy = mk_ota ? (mk_step == 5u ? 22 : 10) : 0;                     /* (the update screens lift the scene) */
    mk_buf = buf;
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = 0;
    mk_y0 = y0 + sy;
    mk_y1 = y0 + MK_SH + sy;
    if (mk_ota) {
        mk_sky(t);
        mk_floor(t);
        mk_house(t);
        mk_roof(t);
        mk_dog(t, mk_step == 5u ? 0 : mk_step == 9u && mk_code != 1 ? 2 : 1, mk_step == 9u ? 30 : 0,
               mk_step == 5u ? 0 : 40);
        if (mk_step == 5u)
            mk_zz(t, 0);
        mk_y0 = y0;
        mk_y1 = y0 + MK_SH;
        mk_ota_text();
    } else {
        mk_sky(t);
        mk_floor(t);
        mk_deco(t);
        mk_house(t);
        mk_roof(t);
        mk_dog_intro(t);
        mk_zz(t, 3500u);
        mk_title(t);
        mk_crt(t);
    }
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = (uint16_t)(buf[i] >> 8 | buf[i] << 8);
}

/* ---- the jingle and the pads */
static const uint8_t MK_NOTE[MK_NK] = {60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86};   /* C pentatonic up */

static uint32_t mk_land(int k) { return MK_K0 + (uint32_t)k * MK_KD + MK_LAND; }

/* the colour of pad r (reading order: 0..7 the top row, 8..15 the bottom row) at t, <= 60 per channel */
static uint32_t mk_pad(uint32_t r, uint32_t t)
{
    uint32_t c = 0, k;
    if (t < 160u)
        return mk_scale(0x282828, 256u - t * 256u / 160u);         /* the CRT flash */
    if (r < MK_NK && t >= mk_land((int)r)) {                       /* its key landed: a flash, then a glow */
        k = t - mk_land((int)r);
        k = k < 400u ? 240u - k * 160u / 400u : 80u;
        c = mk_scale(mk_key_col((int32_t)r, 0), k * 60u / 256u);
    }
    if (t >= MK_DL) {                                              /* the dog lands: all join a rainbow sweep */
        k = t < MK_DL + 300u ? (t - MK_DL) * 256u / 300u : 256u;
        if (t > 4600u)
            k = t < 4900u ? k * (4900u - t) / 300u : 0u;
        c = mk_scale(mk_hue(t * 3u / 2u + 1536u * 4u - (r & 7u) * 150u - (r >> 3) * 60u), k * 50u / 256u);
    }
    return c;
}

#ifndef MK_HOST
static uint16_t *mk_last;

static void mk_frame(uint32_t t)
{
    uint32_t s;
    for (s = 0; s < 240u / MK_SH; s++) {
        uint16_t *b = cv_px + (s & 1u) * (240u * MK_SH);
        if (b == mk_last || !s)
            lcd_sync();                     /* its last strip may still be going out */
        mk_render(b, (int32_t)(s * MK_SH), t);
        fm1_wdt_feed();
        lcd_blit(0, s * MK_SH, 240, MK_SH, b);
        mk_last = b;
    }
}

static void mk_pads(uint32_t t)
{
#if BOARD_SMK37
    uint32_t r, c;
    for (r = 0; r < 16u; r++) {
        c = mk_pad(r, t);
        padled_set(r < 8u ? 8u + r : r - 8u, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
    }
    padled_flush();
#else
    (void)t;
#endif
}

/* the first frame, at boot (before the input and the timers run): black (the CRT is off) */
static void makkuro_splash_bg(void)
{
    lcd_fill(0, 0, 240, 240, 0);
}

/* the animation, MK_LEN ms; a new key, pad or button press ends it */
static void makkuro_splash(void)
{
    uint32_t t0 = fm1_ms, t = 0, held = fm1_in.notes | fm1_in.buttons, on = 0, k;
    mk_ota = 0;
    for (;;) {
        uint32_t now = fm1_in.notes | fm1_in.buttons;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        t = fm1_ms - t0;
        if (t >= MK_LEN || (now & ~held))
            break;
        held &= now;                        /* (held at boot: a new press of it counts) */
        for (k = 0; k < MK_NK; k++) {       /* the jingle: each key's note from its landing, ~180 ms */
            uint32_t l = mk_land((int)k), want = t >= l && t < l + (k == MK_NK - 1u ? 600u : 180u);
            if (want != ((on >> k) & 1u)) {
                mkj_req(MK_NOTE[k], want ? 84u : 0u);
                on ^= 1u << k;
            }
        }
        mk_frame(t);
        mk_pads(t);
    }
    for (k = 0; k < MK_NK; k++)             /* no note may hang */
        if ((on >> k) & 1u)
            mkj_req(MK_NOTE[k], 0);
    mk_pads(MK_LEN);
    lcd_sync();
}

/* a firmware-update screen (felucca.c ota_show): one still frame; results stay 1.5 s */
static void makkuro_ota(uint32_t step, int32_t code)
{
    uint32_t r, c;
    mk_ota = 1;
    mk_step = (uint8_t)step;
    mk_code = (int8_t)(code < -99 ? -99 : code > 99 ? 99 : code);
    mk_frame(4000u);
    mk_ota = 0;
#if BOARD_SMK37
    for (r = 0; r < 16u; r++) {
        if (step == 5u)                                   /* calm: a dim rainbow */
            c = mk_scale(mk_hue(r * 96u), 40u);
        else if (step == 9u)
            c = code == 1 ? 0x00C040u : 0xC00000u;
        else
            c = r < step * 16u / 5u ? mk_scale(mk_hue(1350u - r * 90u), 200u) : 0u;
        padled_set(r < 8u ? 8u + r : r - 8u, (uint8_t)(c >> 16) * 60u / 255u, (uint8_t)(c >> 8) * 60u / 255u,
                   (uint8_t)c * 60u / 255u);
    }
    padled_flush();
#else
    (void)r;
    (void)c;
#endif
    lcd_sync();
    if (step >= 9u)
        fm1_delay_ms(1500);
}
#endif
