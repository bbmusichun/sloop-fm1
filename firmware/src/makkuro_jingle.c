/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* MAKKURO37 boot jingle: a tiny synth of its own, mixed in the audio ISR (fx.c mix_block, before the buses:
 * dry into the mix, a send into SLOOP's reverb, then the master volume), independent of the project's sounds.
 * A cinematic "deep note": four voices start as a quiet cluster wobbling around G3..D4, glide apart (ease in
 * and out, in cents) into a wide Dmaj9 (D2, F#3, C#4, E4) while swelling, bloom when the dog lands and the
 * title lights (3.0 s), shimmer, a chime on the title's starburst, and fade out with the CRT (4.6..5.0 s).
 * Each voice: two oscillators 5 cents apart (left and right), 2-operator FM at a low index (a soft electric
 * piano / pad tone), a gentle low-pass. Peak around -18 dBFS at the master volume's middle. Silent and free
 * (nothing rendered) when not running; mkj_stop() fades out in 150 ms. */

#define MKJ_NV 4
#define MKJ_LEN 5000u              /* ms, as the splash */
static const uint32_t MKJ_SEMI[12] = {    /* phase increments of C9..B9 at 44.1 kHz */
    815363807u, 863847862u, 915214929u, 969636441u, 1027294024u, 1088380105u,
    1153098554u, 1221665363u, 1294309365u, 1371273005u, 1452813141u, 1539201906u};
static const int16_t MKJ_FROM[MKJ_NV] = {5730, 5980, 5640, 6120};   /* cents: the cluster */
static const int16_t MKJ_TO[MKJ_NV] = {3800, 5400, 6100, 6400};     /* D2 F#3 C#4 E4 */
static const int16_t MKJ_LVL[MKJ_NV] = {150, 256, 230, 210};        /* (the bass at about half) */

static struct {
    volatile uint8_t on, stop;
    uint32_t s;                    /* samples since the start */
    int32_t fade;                  /* the stop fade, Q15 */
    uint32_t ph[MKJ_NV + 1][2];
    int32_t amp[MKJ_NV + 1], lpl, lpr;
} mkj;

static inline int32_t mkj_sin(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = SINE[i], b = SINE[i + 1u];
    return a + (((b - a) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

static uint32_t mkj_inc(int32_t cents)           /* cents above MIDI note 0 -> phase increment */
{
    int32_t n = cents / 100, c = cents % 100;
    uint32_t inc = MKJ_SEMI[n % 12] >> (10 - n / 12);
    return inc + (inc >> 12) * (uint32_t)(c * 37 / 16);
}

static int32_t mkj_ramp(uint32_t ms, uint32_t t0, uint32_t d)   /* 0..256 */
{
    return ms <= t0 ? 0 : ms >= t0 + d ? 256 : (int32_t)((ms - t0) * 256u / d);
}

/* a voice's two oscillators into l, r: amplitude a0 -> a1 (Q15 x level) over the block, FM index k */
static void mkj_voice(uint32_t *ph, uint32_t inc0, uint32_t inc1, int32_t a0, int32_t a1, int32_t k, int32_t *l,
                      int32_t *r, uint32_t n)
{
    uint32_t i;
    int32_t da = (a1 - a0) / (int32_t)n;
    for (i = 0; i < n; i++, a0 += da) {
        int32_t m0 = mkj_sin(ph[0]) * k, m1 = mkj_sin(ph[1]) * k;
        l[i] += (mkj_sin(ph[0] + (uint32_t)m0) * a0) >> 15;
        r[i] += (mkj_sin(ph[1] + (uint32_t)m1) * a0) >> 15;
        ph[0] += inc0;
        ph[1] += inc1;
    }
}

/* the audio ISR, one block (n <= 32): adds into the dry mix and the reverb send */
static void mkj_render(int32_t *mix_l, int32_t *mix_r, int32_t *send, uint32_t n)
{
    int32_t l[32], r[32], v, g, idx, glide, ms;
    uint32_t i;
    if (!mkj.on || n > 32u)
        return;
    ms = (int32_t)(mkj.s * 10u / 441u);
    mkj.s += n;
    if (mkj.stop) {                                 /* skipped: out in ~150 ms */
        mkj.fade -= (int32_t)(32767u * n / 6615u);
        if (mkj.fade <= 0) {
            mkj.on = 0;
            return;
        }
    }
    if ((uint32_t)ms >= MKJ_LEN) {
        mkj.on = 0;
        return;
    }
    /* the level: almost silent, a swell (ease in) while gliding, the bloom, a shimmer, the fade out */
    if (ms < 500)
        g = ms * 2000 / 500;
    else if (ms < 3000) {
        int32_t p = mkj_ramp((uint32_t)ms, 500, 2500);
        g = 2000 + (32767 - 2000) * (p * p >> 8) / 256;
    } else if (ms < 4600)
        g = 32767 - ((mkj_sin((uint32_t)ms * 21474836u) + 32768) >> 4);   /* 5 Hz, -6 % */
    else {
        int32_t q = 256 - mkj_ramp((uint32_t)ms, 4600, 400);
        g = 32767 * (q * q >> 8) / 256;
    }
    g = g * mkj.fade >> 15;
    {   /* the pitch: ease in and out from 0.5 to 3.0 s */
        int32_t p = mkj_ramp((uint32_t)ms, 500, 2500);
        glide = p * p * (768 - 2 * p) >> 16;        /* 3p^2 - 2p^3, 0..256 */
    }
    idx = 7000 + 14000 * mkj_ramp((uint32_t)ms, 2800, 500) / 256 - 5000 * mkj_ramp((uint32_t)ms, 3300, 1300) / 256;
    for (i = 0; i < n; i++)
        l[i] = r[i] = 0;
    for (v = 0; v < MKJ_NV; v++) {
        int32_t c = MKJ_FROM[v] + (MKJ_TO[v] - MKJ_FROM[v]) * glide / 256, a;
        c += (mkj_sin((uint32_t)ms * (2400000u + (uint32_t)v * 900000u) + (uint32_t)v * 0x40000000u) * 38 +
              mkj_sin((uint32_t)ms * (700000u + (uint32_t)v * 300000u)) * 18) / 32768 * (256 - glide) / 256;
        a = (g >> 3) * MKJ_LVL[v] >> 8;             /* 4096 at full: four voices ~ -12 dB below the ceiling */
        mkj_voice(mkj.ph[v], mkj_inc(c - 5), mkj_inc(c + 5), mkj.amp[v], a, v ? idx : idx / 2, l, r, n);
        mkj.amp[v] = a;
    }
    {   /* the chime on the starburst: E6, a bell-like FM ping decaying in ~0.6 s */
        int32_t a = 0;
        if (ms >= 4250) {
            int32_t d = ms - 4250;
            a = d < 6 ? d * 1400 / 6 : 1400 * 600 / (600 + d * 3);
            a = a * mkj.fade >> 15;
            if (ms >= 4600)
                a = a * (256 - mkj_ramp((uint32_t)ms, 4600, 400)) / 256;
        }
        if (a || mkj.amp[MKJ_NV])
            mkj_voice(mkj.ph[MKJ_NV], mkj_inc(8800), mkj_inc(8803), mkj.amp[MKJ_NV], a, 3000, l, r, n);
        mkj.amp[MKJ_NV] = a;
    }
    for (i = 0; i < n; i++) {                       /* a gentle low-pass (~4.5 kHz), into the mix and the reverb */
        mkj.lpl += ((l[i] << 2) - mkj.lpl) * 15 >> 5;
        mkj.lpr += ((r[i] << 2) - mkj.lpr) * 15 >> 5;
        mix_l[i] += mkj.lpl >> 2;
        mix_r[i] += mkj.lpr >> 2;
        send[i] += (mkj.lpl + mkj.lpr) >> 3;
    }
}

/* the main loop: start (from the splash) and stop (a fade; nothing sounds after it) */
static void mkj_start(void)
{
    uint32_t v;
    fm1_irq_off();
    mkj.s = 0;
    mkj.fade = 32767;
    mkj.stop = 0;
    mkj.lpl = mkj.lpr = 0;
    for (v = 0; v <= MKJ_NV; v++) {
        mkj.amp[v] = 0;
        mkj.ph[v][0] = v * 0x51EB851Fu;
        mkj.ph[v][1] = v * 0x3A5E353Fu + 0x40000000u;
    }
    mkj.on = 1;
    fm1_irq_on();
}

static void mkj_stop(void) { mkj.stop = 1; }
