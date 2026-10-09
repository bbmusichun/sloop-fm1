# Makkuro37

**Alternative firmware for the M-Vave SMK-37 Pro, based on FM-1 firmwares.**

Makkuro37 turns the SMK-37 Pro MIDI keyboard into a stand-alone four-track groovebox: three synth tracks and a
drum machine, synthesised, played and sequenced on the keyboard's own chip. It is a port of [SLOOP](https://github.com/isod89/sloop-fm1) 2.5 by **isod89**, the
live groovebox firmware for the M-Vave FM-1, which in turn is based on
[Felucca](https://github.com/hugelton/Felucca) by **Leo Kuroshita / Hügelton Instruments**. The FM-1 and the
SMK-37 Pro share the same JieLi AC791N (WL82) chip, boot chain and update loader; only the panel wiring differs.
Makkuro37 adds the SMK-37 Pro board support (keys, pads, faders, wheels, knobs, display, LEDs, flash map,
installer) and its own look and sound (animated intro and boot jingle).

> **Status: experimental.** It runs on the author's unit, but it is a hobby project: install it only if you can
> restore your keyboard yourself, and read [Safety](#safety) first.
Everything SLOOP does is described in [README-SLOOP.md](README-SLOOP.md) (SLOOP's own README),
[GUIDE.md](GUIDE.md) and [SLOOP.md](SLOOP.md).

## Features on the SMK-37 Pro

Everything of SLOOP 2.5 (four tracks, the synthesis engines and sounds, drum kits, samples, song mode, parameter
locks, USB audio, MIDI clock, the menus and screens), mapped onto the SMK-37 Pro's controls:

- **37 velocity keys.** The middle 27 keys (F3–G5) are the FM-1's note keys (notes, steps, menus), now with
  velocity (the stock two-contact timing and the unit's own factory velocity curves). The 10 outer keys play notes on
  the synth tracks and drums on the drum track: C3 kick, D3 snare, E3 hat; G#5 cowbell, A5 open hat, B5 crash,
  C6 ride (black keys as their left white key).
- **16 RGB pads** are the FM-1's 16 white keys: steps, drum sounds and layer tiles, played with velocity. Their LEDs
  use the screen's track colours (blue, green, yellow, orange): the steps of the selected track lit, playhead and hits
  bright, off is dark; the SCALE layer shows a piano layout (scale green, root brighter), the SAVE layer used slots
  green and free slots white.
- **4 faders with banks.** FADER BANK cycles A level, B filter (low-pass – open – high-pass), C reverb, D delay for
  the four tracks (track 4 = drums); a fader takes over once it is moved; the bank is shown on screen.
- **Wheels.** Pitch wheel: pitch bend ±7 semitones on the selected synth track. Mod wheel: vibrato (10 Hz, up to
  ±1 semitone). Both calibrate themselves.
- **8 knobs as the FM-1's top row:** K1 master volume, K2 SELECT (tempo, pages), K3 track, K4 sound / kit,
  K5–K8 the four dials on the screen. One encoder step per detent.
- **Buttons** keep SLOOP's functions (PATCH = HOME, PARA = EDIT, CHORD = ENV, NOTE REPEAT = LFO, SCALE = SEL,
  GLOBE = GLO, BT = SAVE, SEQ PLAY / SEQ REC = PLAY / REC); `<` and `>` walk pages, step pages (SEQ held) and
  visualiser styles; the ARP LED shows whether the selected track's arpeggiator runs.
- **Makkuro37 look and sound** (build flag `MAKKURO37=1`): an animated 3D intro (a neon doghouse with a piano-key
  roof, a low-poly beagle, a synthwave sun and grid, mountains that follow the music) with its own boot jingle
  (a small FM synth: a deep-note swell into D major, key bells, a final ping), fading into the UI; a key, pad or
  button press skips it. The firmware-update screens use the same scene, and the visualiser gains the MAKKURO and
  WATERFALL styles.
- **Flash safety built in:** the firmware and its update loader write only to an explicit allow-list of flash
  ranges (checked at compile time and again in the flash driver); the factory calibration, Bluetooth pairing and
  key_mac areas are never written.

The full control map (FM-1 → SLOOP → SMK-37 Pro → Makkuro37) is in [docs/CONTROLS.md](docs/CONTROLS.md).
Not used yet: KNOB BANK, the pedal jack, TRS MIDI OUT, pad pressure.

## Branches

| Branch | What |
| --- | --- |
| `main` | Makkuro37: SLOOP 2.5 + the SMK-37 Pro port + the Makkuro37 intro, jingle and look |
| `smk37-board` | SLOOP 2.5 + the SMK-37 Pro board support only (no Makkuro37 additions): the base for porting other FM-1 firmwares (Felucca, X0X, FoMni, Melodee, ...) to the SMK-37 Pro. See [fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) for the hardware facts and the porting guide |

## Build

The JieLi toolchain is Linux x86-64 only: build on Linux or in WSL2 (Ubuntu) on Windows. See
[BUILDING.md](BUILDING.md) for details (macOS with Docker works as for SLOOP).

```sh
# once: the toolchain (to ~/.jieli/toolchain), the three AC79 SDK files, Python 3 with Pillow
sh tools/get_toolchain.sh
git clone --depth 1 --filter=blob:none --sparse --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
git -C ~/fw-AC79_AIoT_SDK sparse-checkout set cpu/wl82/tools
sudo apt install python3-pil        # or: pip3 install Pillow

# Makkuro37 for the SMK-37 Pro -> build/sloop-smk37.fwsc (identity "SMK-37 Pro_900")
BOARD=smk37 MAKKURO37=1 sh build.sh

# the plain SMK-37 Pro port, without the intro and jingle
BOARD=smk37 sh build.sh

# without BOARD: SLOOP for the FM-1 (build/felucca.fwsc), byte-identical to upstream SLOOP 2.5
sh build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations. The host tests run after an FM-1 build:
`sh build.sh && sh tests/run_tests.sh` (they include the SMK-37 Pro flash-map, package, loader and installer
tests). One check, the static cost budget of `fm1_alnk0_irq`, fails the same way on unmodified SLOOP 2.5
(a stale budget file upstream).

## Install

1. Read [Safety](#safety) and back up the flash.
2. Connect the SMK-37 Pro over USB and run SLOOP's installer (Python 3 with `mido` and `python-rtmidi`):

   ```sh
   python3 tools/fm1_install.py build/sloop-smk37.fwsc
   ```

   It finds the SMK-37 Pro's MIDI port, checks the model and the package, asks the running firmware (the stock one
   or Makkuro37) to start the update loader and writes the package; then the keyboard restarts and the installed
   identity is checked. The web installer in `docs/webapp/installer/` has the same SMK-37 Pro support.
3. Updating later works the same way. Projects, presets, samples and settings are kept.

**Roll back to the stock firmware:** install the official SMK-37 Pro firmware with M-Vave's own updater
(M-UPGRADE), or pass the official package to the installer: `python3 tools/fm1_install.py --force
SMK-37_Pro_016.fwsc` (it handles the stock package format). Makkuro37 keeps the stock update identity, so the
stock tools see the keyboard as usual.

**If it does not boot:** after two failed starts the firmware opens its USB recovery screen (identity
`SMK-37 Pro_000`), where the installer can write a package again. After a third failed start it drops into the
chip's mask-ROM update mode (UBOOT), where the app area can be rewritten with a JieLi UBOOT tool.

## Safety

- **Experimental firmware, no warranty** (see the GPL-3.0). You install it at your own risk.
- **Back up the whole 1 MiB flash first**, over the chip's mask-ROM UBOOT mode (e.g. with `jl-uboot-tool`). The stock
  firmware has no way into UBOOT, so you need a USB_KEY dongle (as for the FM-1); with Makkuro37 installed, hold
  OCTAVE− and OCTAVE+ for 5 seconds while stopped to enter it. Keep the dump private: it holds data unique to your
  unit.
- **Never write 0xC0000–0xC1FFF** (Bluetooth pairing and the factory key / pad calibration, "trimdata") **or
  key_mac (0xFF000–0xFFFFF)**, and leave the stock resources (0xF4000–0xFDFFF) alone. They are unique to each unit
  and cannot be recreated. Makkuro37 itself never writes them (compile-time allow-list plus a check in the flash
  driver), but a manual UBOOT write can. When you restore with a UBOOT tool, write only the ranges you need.
- Power the keyboard from a reliable USB port during an update and do not unplug it while the update screen says so.

## License and credits

Makkuro37 is free software under the **GNU General Public License v3.0** ([LICENSE](LICENSE)), like SLOOP and Felucca;
third-party parts keep their own licenses (see [LICENSING.md](LICENSING.md) and [LICENSES/](LICENSES)).

- **SLOOP** – isod89, <https://github.com/isod89/sloop-fm1> (the groovebox, its engines, sounds, UI, installer and
  tools; this repository is a fork of SLOOP 2.5).
- **Felucca** – Leo Kuroshita (@kurogedelic), Hügelton Instruments, <https://github.com/hugelton/Felucca> (the
  original open FM-1 firmware SLOOP is built on).
- **SMK-37 Pro port and Makkuro37** – genkigenki.
- The CC0 instrument samples are by Versilian Studios (see `assets/samples-cc0/ATTRIBUTION.txt`).

M-Vave, SMK-37 Pro and FM-1 are names of their respective owners. This project is not affiliated with or endorsed by
M-Vave or JieLi; the stock firmware is not part of this repository.
