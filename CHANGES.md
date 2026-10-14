# Changes

## Snoop37 2.6.5

- **LIMITER:** HOME menu > AUDIO > **LIMITER** (K8): SOFT (default) / CLASSIC. SOFT gives the mix 9 dB more headroom
  at the power-on MASTER position and lets the limiter act only near full scale, so a held chord no longer turns the
  whole mix down ("one note loud, a chord quieter"). MASTER's top is as loud as before (louder peaks pass): turn
  MASTER up a few clicks after updating. CLASSIC is SLOOP's limiter and MASTER curve.
- **BEND UP / BEND DOWN** replace BEND RANGE: HOME menu > MIDI > **BEND UP** (K6), **BEND DOWN** (K7), each OFF, 1, 2,
  3, 4, 5, 7 or 12 semitones, default 7 / 7 (e.g. +2 up, −12 down). A 2.6.4 BEND RANGE comes back as both.
- **KNOB LAYOUT:** HOME menu > SCREEN > **KNOB LAYOUT** (K7): FM-1 (default, as before) / SMK: K1–K4 the four dials on
  the screen, K5 master volume, K6 SELECT (tempo / pages), K7 PRESETS (sound / kit), K8 ALGORITHM (track). Menu rows,
  help lines and the calibration screen name the knobs as they are; the hardware calibration stays per printed knob.
- **Pads: DRUM KIT by default.** On the PLAY screen the pads play the drum kit whatever track is selected (a pad a
  sound, with velocity; the keys play the selected track). **KNOB BANK + FADER BANK** (the stock PAD BANK combo)
  switches the pads to **STEPS** (the selected track's 16 steps of the page: tap to set / clear, `<` `>` the page;
  drums: the sound last played) and back. The pads no longer mirror the keys there. Layers keep their pads (SEQ, FX,
  GLO, SAVE grids, SCALE piano, EDIT / ARP held: the notes).
- HOME menu: KEY VELOCITY moves to MIDI (still K8); AUDIO's K8 is LIMITER.

## Snoop37 2.6.4

- **TRS MIDI OUT:** the 3.5 mm jack sends what goes out over USB MIDI (the keys and pads, the sequencer with
  GLO > SYSTEM > MIDI = SEQ, on each track's channel), with or without a computer. HOME menu > MIDI >
  **TRS MIDI OUT** (K5): ON (default) / OFF. Use a TRS-to-DIN MIDI adapter (type A; if a synth does not respond, try
  type B).
- **BEND RANGE:** HOME menu > MIDI > **BEND RANGE** (K6) sets the pitch wheel's range on the internal sounds: ±1, 2,
  3, 4, 5, 7 or 12 semitones, default **7** (the range it had). Pitch bend from MIDI IN
  keeps its own range (GLO > MIDI 2 > BEND).
- **Wheel calibration:** HARDWARE CALIBRATION (OCTAVE− + OCTAVE+ at power-on, or HOME menu > SYSTEM) now also
  teaches the wheels after the buttons and knobs: the pitch wheel at rest, full down, full up; the mod wheel at the
  bottom, full up (OCTAVE+ takes each step). The full travel then gives the full range; at power-on a resting pitch
  wheel is re-centred. Without it the wheels calibrate themselves as before. The calibration screen names the
  knobs as printed (K2 SELECT … K5–K8).

## Snoop37 2.6.3

- HOME menu > AUDIO > VELOCITY is now **KEY VELOCITY**, and it is set with **K8** (2.6.2 said K4).
- The HOME menu's rows name the SMK-37's knobs: **K5–K8** (the FM-1's KNOB 1–4; K1–K4 are MASTER, SELECT,
  ALGORITHM, PRESETS). The ARP mode is on K5.
- The visualiser no longer has the SNOOP and WATERFALL styles: it has SLOOP's 12 again (the last one: SLOOP).
  A style kept from 2.6.2 reads as SLOOP (was SNOOP) or OSCILLOSCOPE (was WATERFALL).

## Snoop37 2.6.2

- HOME menu > AUDIO > **VELOCITY** (K4): DYNAMIC (as played, the default), SOFT (a light touch plays louder), HARD
  (loud needs a hard hit), FIXED 64, FIXED 100, FIXED 127. It applies to the 37 keys and the pads: the notes, their
  MIDI out and the drum levels (FIXED 64: soft, 100: normal, 127: hard; OCT− / OCT+ held still play ghost / hard).
  Kept with the settings; after updating it is DYNAMIC, as before.

## Snoop37 2.6.1

- About screen without the repository link (it names SLOOP's repository, as SLOOP does).
- The SMK-37 hardware support now matches the upstream pull request
  [#112](https://github.com/isod89/sloop-fm1/pull/112); the look matches the branch `smk37-snoop-look`.
- Snoop37 lives in the SLOOP fork, <https://github.com/genkigenki/sloop-fm1> (branch `snoop37`).

## Snoop37 2.6

From here on Snoop37's version number follows the SLOOP release it is based on (the release before was 0.1.x).

- **Based on SLOOP 2.6** (was 2.5): 27 punch-in effects (the new ones on the black keys F#3–F#5 with FX held),
  REV / ECHO THROW, an INSERT effect per track, LFO 2 with tempo sync, ENV SYNC, ANALOG filter types, note LEN and
  step velocity, MIDI LEARN (GLOBE + F#3), live drum mutes, TEXTURE, a MIDI channel per track, MIDI IN on one
  channel, pitch bend and sustain from MIDI IN, step entry from a MIDI keyboard, Program Change, project ERASE,
  the bold screen font, the look-ahead limiter. SLOOP's list: [README-SLOOP.md](README-SLOOP.md#whats-new-in-26).
- The SMK-37 pitch wheel keeps its own ±7 semitones; MIDI pitch bend from outside uses GLO > MIDI 2 > BEND.
- SYSTEM's about screen shows `2.6 (SLOOP 2.6)`.
- FADER BANK: pressed again, `<` `>` or K2 change the bank; the screen closes after 4 s. KNOB BANK and FADER BANK
  close each other, so only one bank screen is up (before, `<` `>` went to the hidden pad picker).
- HOME menu > SYSTEM gains MIDI LEARN CLEAR next to BOOT LOGO.
- The installers (web and `tools/fm1_install.py`) keep the SMK-37 model rules and port names and take SLOOP 2.6's
  additions (CLIPPER / SLOOP port names, "not found" lists the MIDI ports there are).
- The intro ends on the SNOOP37 title alone (no port credit line under it).
- Snoop37 is now SLOOP's SMK-37 support (the pull request to SLOOP: every SMK-37 feature, also in a plain SLOOP
  build) plus the Snoop37 look behind the build flag `SNOOP37=1` (intro, jingle, update screens, name,
  visualisers, BOOT LOGO). The source is based on SLOOP's current main (firmware 2.6; the editor's audio preview
  and the offline app came with it).
- Installers: the official packages of the whole SMK-37 range are accepted for the way back (by their structure, so
  M-Vave's next release too); more port names probed (MAKU, RAINBOW).
- Kept from 0.1.x: the pitch wheel fix, the Elite / MKE-P37 builds (`MODEL=elite|mke`), the installer's model check
  on resume.

**Settings: check once after updating from 0.1.x.** SLOOP 2.6 keeps the MIDI IN channel (GLO > MIDI 2 > IN) in the
settings bits where 0.1.x kept the BOOT LOGO. A 0.1.x BOOT LOGO of SLOOP or NONE therefore reads once as
IN = CH 1 or CH 2, and BOOT LOGO goes back to SNOOP37. After updating, check HOME menu > SYSTEM > BOOT LOGO and
GLO > MIDI 2 > IN once; set them, and they stay. Projects saved in 2.6 (project format 6) do not open in 0.1.x.

## Snoop37 0.1.x

- SLOOP 2.5 on the SMK-37 Pro, Elite and MKE-P37, with the Snoop37 intro, jingle and look (0.1.1: the pitch wheel
  fix, the Elite / MKE-P37 builds).
