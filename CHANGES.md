# Changes

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
