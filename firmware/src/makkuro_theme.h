/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* MAKKURO37: the name on the screens. felucca.c includes this with MAKKURO37 (else it defines SLOOP's own
 * words); the screens use the BRAND_ and VIS_ macros. The credit stays: "based on SLOOP + Felucca". */
#pragma once

#define BRAND_NAME "MAKKURO37"                     /* the about screen, in the large font */
#define BRAND_SAIL_X 160                           /* (the four track bars beside it) */
#define BRAND_BASED "BASED ON SLOOP + FELUCCA"
#define BRAND_CRASH "MAKKURO37 CRASH"
#define BRAND_RESCUE "MAKKURO37 USB RESCUE"

/* the visualiser: SLOOP's twelfth style becomes MAKKURO, a thirteenth WATERFALL (makkuro_vis.c) */
#define VIS_STYLES 13u
#define VIS_LAST_NAMES "MAKKURO", "WATERFALL"
#define VIS_N_STR "/13"
static int mkv_draw(uint32_t s);
