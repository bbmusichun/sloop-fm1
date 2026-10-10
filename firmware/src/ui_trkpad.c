/* SPDX-License-Identifier: GPL-3.0-only
 * Snoop37 (SMK-37 Pro): Copyright (C) 2026 M. Geisler (makkuro) <mcgeisler@gmail.com> */
/* The TRACKS screen's track pads: the 2 x 2 at the far left select the track (as ALGORITHM),
 * pads 0 1 (top) = tracks 1 2, pads 8 9 (bottom) = 3 4; lit in the track colours. */
#define TRKPAD_MASK (1u << 0 | 1u << 1 | 1u << 8 | 1u << 9)
#define TRKPAD_TRK(p) (((p) & 1u) | ((p) >> 3) << 1)   /* pad 0 1 8 9 -> track 0 1 2 3 */

static int trkpad_on(void)                         /* the TRACKS screen, nothing over it */
{
    return !ui.menu && ui.layer == LY_PLAY && !ui.hold_kind && !ui.home && cur_page()->scope == SC_TRK &&
           !vis_shown() && !pick_shown() && !fb_shown() && !rec_wait && !ft_on;
}
static void trkpad_hit(uint32_t p)
{
    if ((TRKPAD_MASK >> p) & 1u)
        track_select(TRKPAD_TRK(p));
}
