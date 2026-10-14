# Controls: FM-1 → SLOOP → SMK-37 → Snoop37

Kept up to date with the firmware (this repository, branch `main`).
"Snoop37 (now)" = what is on the keyboard today; "Proposal" = suggested, not built yet.

## Knobs, faders, wheels

| FM-1 printed | SLOOP function | SMK-37 printed | Snoop37 (now) | Proposal |
|---|---|---|---|---|
| MASTER (pot, top left) | output volume | K1 | master volume (virtual pot, 24 steps a click; HOME menu > AUDIO > LIMITER: SOFT / CLASSIC) | cap: "M" |
| SELECT (big encoder) | tempo (TRACKS), page ◀▶, visualiser style, menu section | K2 | SELECT: tempo / pages | cap: "T" |
| ALGORITHM | select track 1–4 | K3 | track | cap: "4" |
| PRESETS | sound (synth) / kit (drums) | K4 | sound / kit, the pad picker shows them | cap: note symbol |
| KNOB 1 | dial 1 on screen (blue) | K5 | dial 1 | cap: blue 40,124,255 |
| KNOB 2 | dial 2 on screen (green) | K6 | dial 2 | cap: green 30,204,112 |
| KNOB 3 | dial 3 on screen (yellow) | K7 | dial 3 | cap: yellow 255,198,24 |
| KNOB 4 | dial 4 on screen (orange) | K8 | dial 4 | cap: orange 255,98,26 |
| — | — | KNOB LAYOUT = SMK | HOME menu > SCREEN > KNOB LAYOUT (K7): K1–K4 the dials 1–4, K5 master, K6 SELECT, K7 PRESETS, K8 track; FM-1 (default) as above | — |
| — | — | FADER 1–4 | tracks 1–4 (4 = drums), by FADER BANK: A level, B filter (low-pass ◀ open ▶ high-pass), C reverb, D delay; a fader takes over once moved | master FX bank |
| — (no wheels used by SLOOP) | — | PITCH wheel | pitch bend, selected synth track: HOME menu > MIDI > BEND UP (K6) / BEND DOWN (K7), OFF, 1, 2, 3, 4, 5, 7, 12 semitones each; default 7 / 7 | — |
| — | — | MOD wheel | vibrato 10 Hz, up to ±1 semitone (up = more) | — |
| — | — | PEDAL jack | — | sustain or start/stop |

## Buttons

| FM-1 printed | SLOOP tap | SLOOP hold (layer) | SMK-37 printed | Snoop37 (now) | Proposal |
|---|---|---|---|---|---|
| PLAY | start / stop (LED flashes on every beat) | — | SEQ PLAY | PLAY | — |
| REC | record / arm | 2 s: clear track | SEQ REC | REC | — |
| HOME | TRACKS screen, again: visualiser | menu | PATCH | HOME | — |
| ENV | envelope pages (amp/filter shape) | — | CHORD | ENV | — |
| LFO | LFO pages (modulation) | — | NOTE REPEAT | LFO | — |
| FX | effect pages | punch-in FX on pads | FX | FX | — |
| SEL | key / chord pages | key layer (pads = piano: scale green, root brighter) | SCALE | SEL | — |
| EDIT | sound pages, drum kit | erase layer | PARA | EDIT | — |
| ARP | arp pages (MODE on K5: OFF switches the arpeggiator off) | note repeat (rolls) | ARP | ARP; LED lit while the selected track's arpeggiator runs | — |
| SEQ | step / pattern pages | step layer (steps 1–16) | SEQ | SEQ | — |
| GLO | global / master / system | mix layer (mute, solo, fill, tap) | GLOBE | GLO | — |
| SAVE | song screen / save | song layer (slots A–D: used green, free white) | BT | SAVE (SLOOP has no Bluetooth) | — |
| OCT− | octave down | drums: ghost hits | OCTAVE− | OCT− | — |
| OCT+ | octave up | drums: hard hits | OCTAVE+ | OCT+ | — |
| — | — | — | KNOB BANK | pins the pad sound picker | — |
| — | — | — | FADER BANK | the fader bank screen; pressed again, < > or SELECT: the next bank A→B→C→D (closes after 4 s; lit while up; closes the pad picker) | — |
| — | — | — | KNOB BANK + FADER BANK | the pads on the PLAY screen: DRUM KIT (default) ↔ STEPS (as the stock PAD BANK combo) |
| — | — | — | < | page back (as SELECT, never tempo); SEQ held or pads on STEPS: step page back; visualiser: style | — |
| — | — | — | > | page forward; SEQ held or pads on STEPS: step page forward; visualiser: style | — |

## Keys and pads

| FM-1 | SLOOP | SMK-37 | Snoop37 (now) |
|---|---|---|---|
| 27 keys F3–G5 | notes; in layers: tiles (steps, FX, mix…) | keys F3–G5 (middle 27) | same, with velocity |
| — | — | keys C3–E3 (lowest 5) | synth tracks: notes; drum track: C3 kick, D3 snare, E3 hat (black keys = left white key) |
| — | — | keys G#5–C6 (highest 5) | synth tracks: notes; drum track: G#5 cowbell, A5 open hat, B5 crash, C6 ride |
| LEDs under 27 keys | what is on / playing | 16 RGB pads | PLAY screen: DRUM KIT (default; a pad a drum sound whatever track is selected, velocity; orange, dim / bright while it sounds) or STEPS (KNOB BANK + FADER BANK; the selected track's 16 steps of the page, top row 1–8: empty dim white, set in the track's colour, playhead white; tap sets / clears; drums: the sound last played). Layers: SEQ / FX / GLO / SAVE tiles, SCALE piano, EDIT / ARP held the white keys |

## Power-on and special combos (same as SLOOP on FM-1)

| Combo | Does |
|---|---|
| OCTAVE− held at power-on | USB rescue |
| OCTAVE− + OCTAVE+ held at power-on | hardware calibration (re-teach buttons, knobs and the wheels) |
| OCTAVE− + OCTAVE+ held 5 s, stopped | update mode (UBOOT) |
| KNOB BANK + FADER BANK | the pads: DRUM KIT ↔ STEPS |
