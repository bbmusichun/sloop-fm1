/* SPDX-License-Identifier: GPL-3.0-only */
/* Snoop37 TRACKS track pads (firmware/src/ui_trkpad.c) on the UI harness: pads 0 1 8 9 select tracks 1..4
 * on the TRACKS screen only; KNOB BANK's PRESETS timer (ui_pick.c); which modes make the pads a grid (ui_padgrid.c). */
#define UI_PAGES_HARNESS_ONLY 1
#include "ui_pages_test.c"
static uint8_t smk_fbank, smk_fbank_new;           /* (ui_input.c, SMK build) */
#include "../firmware/src/ui_pick.c"
#include "../firmware/src/ui_fbank.c"
#include "../firmware/src/ui_trkpad.c"
#include "../firmware/src/ui_padgrid.c"

int main(int argc, char **argv)
{
    uint32_t i;
    outdir = argc > 1 ? argv[1] : "build/host";
    panel = PANEL_DEFAULT;
    layers_init();
    host_tracks_init();
    for (i = 0; i < NPART; i++) { set_engine_of(&trk[i], TRK_DEF[i][0]); apply_preset_to(&trk[i], TRK_DEF[i][1]); trk[i].engine = trk[i].eng_req; }
    go_home(); ui.force = 1; frame();
    song.sel = 0;

    check(TRKPAD_TRK(0) == 0 && TRKPAD_TRK(1) == 1 && TRKPAD_TRK(8) == 2 && TRKPAD_TRK(9) == 3, "pads 0 1 8 9 -> tracks 1..4");
    check(trkpad_on(), "TRACKS screen: the track pads on");
    trkpad_hit(9);
    check(song.sel == 3, "pad 9 (bottom row): track 4");
    trkpad_hit(8);
    check(song.sel == 2, "pad 8 (bottom-left): track 3");
    trkpad_hit(2);
    check(song.sel == 2, "pad 2: no track");
    trkpad_hit(0);
    check(song.sel == 0, "pad 0 (top-left): track 1");
    ui.layer = LY_STEP;
    check(!trkpad_on(), "off while a layer is held");
    ui.layer = LY_PLAY;
    fb_set(smk_fbank);
    check(!trkpad_on(), "off on the FADER BANK screen");
    fb_close();
    pick_open_tmp();
    check(!trkpad_on(), "off while the picker is up");
    check((int32_t)(fm1_ms - pick_k4_until) < 0, "a PRESETS detent: KNOB BANK lit");
    fm1_ms += 160u;
    check((int32_t)(fm1_ms - pick_k4_until) >= 0, "~150 ms later: off");
    pick_close();
    ui.menu = 1;
    check(!trkpad_on(), "off in the menu");
    ui.menu = 0;
    open_family(FAM_ENV);
    check(!trkpad_on(), "off on another page");
    go_home();
    check(trkpad_on(), "back on TRACKS: on");
    check(!padgrid_on(), "playing notes: not a grid (low notes at the bottom)");
    ui.layer = LY_STEP; check(padgrid_on(), "SEQ held (steps): a grid");
    ui.layer = LY_FX; check(padgrid_on(), "FX held (effects): a grid");
    ui.layer = LY_MIX; check(padgrid_on(), "GLO held (mix tiles): a grid");
    ui.layer = LY_SONG; check(padgrid_on(), "SAVE held (slots): a grid");
    ui.layer = LY_ERASE; check(!padgrid_on(), "EDIT held (erase notes / sounds): not a grid");
    ui.layer = LY_ROLL; check(!padgrid_on(), "ARP held (note repeat): not a grid");
    ui.layer = LY_PLAY; kb_grid = 1; check(padgrid_on(), "the DRUMS grid page: a grid");
    kb_grid = 0;
    printf("trkpad: %s\n", fails ? "FAIL" : "ok");
    return fails != 0;
}
