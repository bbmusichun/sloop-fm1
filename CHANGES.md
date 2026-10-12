# Changes

## Snoop37 0.2

- **Based on SLOOP 2.6** (was 2.5): 27 punch-in effects (the new ones on the black keys F#3–F#5 with FX held),
  REV / ECHO THROW, an INSERT effect per track, LFO 2 with tempo sync, ENV SYNC, ANALOG filter types, note LEN and
  step velocity, MIDI LEARN (GLOBE + F#3), live drum mutes, TEXTURE, a MIDI channel per track, MIDI IN on one
  channel, pitch bend and sustain from MIDI IN, step entry from a MIDI keyboard, Program Change, project ERASE,
  the bold screen font, the look-ahead limiter. SLOOP's list: [README-SLOOP.md](README-SLOOP.md#whats-new-in-26).
- The SMK-37 pitch wheel keeps its own ±7 semitones; MIDI pitch bend from outside uses GLO > MIDI 2 > BEND.
- SYSTEM's about screen shows `0.2 / SLOOP 2.6`.
- HOME menu > SYSTEM gains MIDI LEARN CLEAR next to BOOT LOGO.
- The installers (web and `tools/fm1_install.py`) keep the SMK-37 model rules and port names and take SLOOP 2.6's
  additions (CLIPPER / SLOOP port names, "not found" lists the MIDI ports there are).
- Kept from 0.1.x: the pitch wheel fix, the Elite / MKE-P37 builds (`MODEL=elite|mke`), the installer's model check
  on resume.

**Settings: check once after updating from 0.1.x.** SLOOP 2.6 keeps the MIDI IN channel (GLO > MIDI 2 > IN) in the
settings bits where 0.1.x kept the BOOT LOGO. A 0.1.x BOOT LOGO of SLOOP or NONE therefore reads once as
IN = CH 1 or CH 2, and BOOT LOGO goes back to SNOOP37. After updating, check HOME menu > SYSTEM > BOOT LOGO and
GLO > MIDI 2 > IN once; set them, and they stay. Projects saved in 0.2 (project format 6) do not open in 0.1.x.

## Snoop37 0.1.x

- SLOOP 2.5 on the SMK-37 Pro, Elite and MKE-P37, with the Snoop37 intro, jingle and look (0.1.1: the pitch wheel
  fix, the Elite / MKE-P37 builds).
