# PIXEL RUN

A firmware for the M-VAVE FM-1 by **swtdr34mz**: the chiptune sibling of
[CHORD RUN](https://github.com/swtdr34mz-afk/CHORD_RUN), based on [Blop / OSR-1](https://github.com/OSR-Experiment/OSR-1)
by Obsimo (OSR Records). Built on [fm1-base](https://github.com/charlesvestal/fm1-base) and
[felucca-bsp](https://github.com/hugelton/felucca-bsp).

Play the chords of a video game tune with one finger and a melody over them, with chip sounds and Game Boy drums, build
your own drum grooves, loop 8 bars of chords to jam over, while an early-80s arcade shooter plays along: your ship
fires on every key you play.

**Play it in the browser:** https://swtdr34mz-afk.github.io/PIXEL_RUN/emu/ (the same firmware, built for the browser).

![PIXEL RUN](site/img/screen-main.png) ![DRUM STUDIO](site/img/screen-studio.png)

## What it does

- **Sets:** 20 levels, each a chord progression in the style of a famous video game tune, under a name of its own:
  grassland, underground, sky, speed zone, factory, hero field, block world, robot fortress, first route, lava level,
  boss castle, soldier base, beach race, crystal cave, time hall, underwater, candy land, alien planet, neon street,
  rainbow race. A set's chords sit on the **black keys** (left to right). Each set brings its own sound, chord rhythm,
  scale, tempo, bass riff, drum groove, effects and stage; three of them the chip arpeggio.
- **Melody:** the **white keys** follow the chord: FOLLOW CHORD (MELODY page) puts the chord's root on the leftmost white
  key and its scale above it, so every chord change moves the keys (a held note moves with it). FOLLOW SCALE keeps each
  key on the set's scale. Each set has its own scale (major, minor, harmonic minor, dorian, mixolydian, phrygian,
  phrygian dominant, lydian, pentatonics, blues). ASSIST (MELODY 2) lights up the chord's notes, or GUIDE: a melody that
  lights up key by key (BLOCK DROP: Korobeiniki, the folk song the falling-blocks game made famous).
- **Sounds:** 17 chip sounds: pulse waves (12.5, 25 and 50 %), a sweeping PWM, a handheld's 4-bit wave channel, the 8-bit
  consoles' stepped triangle, a home computer's stepped saw through its filter, a laser lead, a coin blip, a crushed pad,
  and the 16-bit consoles' FM (brass, bell, e-piano, strings). The bass has the chip TRIANGLE and PULSE too.
- **Chip arpeggio:** ARP's rates FAST and CHIP play a chord as one fast arpeggio (30 or 60 notes a second), the classic
  way 8-bit music plays chords.
- **Drums:** the Game Boy's sound channels as its games made drums of them: a kick on the wave channel (a 4-bit sine
  that drops, with a noise click), snare and hats on the noise channel (its shift register, low clocks, a 4-bit volume
  that steps down every n/64 s, the length counter; one channel, so a hat cuts the snare), perc on a pulse channel; each
  channel's level eased over half a millisecond, so nothing clicks. Five gritty kits (GRITTY, RUMBLE, CRUNCH, DUSTY,
  BLAST) and 20 grooves: platformer, overworld, shuffle, castle, boss fight, racing, dungeon, shooter, puzzle, title,
  bounce, underwater, space, chip house, turbo, chip breaks, chip funk, chip hip hop, gallop, credits.
- **DRUM STUDIO:** build a groove of your own: 16 steps on 4 tracks (kick, snare, hats, perc), a kit, a swing, and keep it
  in one of 20 user slots (kept in the flash, in the backup too).
- **FX:** a bitcrusher (fewer bits, a lower sample rate), delay, chorus, reverb, and the sidechain (GROOVE).
- **Loop:** press REC, play up to 8 bars of chords (REC again closes it at its bar), and they play round while you play the
  melody on top, or build drums in the studio.
- **The game:** an early-80s vertical shooter of its own on the screen: a dotted grid rushing towards you, a formation of
  pixel invaders swaying at the top, some diving at the ship in arcs and firing, a saucer now and then. The ship flies by
  itself (it dodges, and lines up under the invaders) and fires on every key you play: a chord a double shot, a melody
  note one. A wave shot down brings the next in new colours: six palettes of the era (radar, vector, neon, amber, ice,
  ultra), turning with the waves and the time played. One screen for every page. The high score, metres, missions and
  ten ships to unlock (SEQ) are kept.

## Controls

| Control | What it does |
| --- | --- |
| Black keys | The set's chords. With a rhythm the chord keeps playing (the same key again stops it); without one (HELD) it plays while you hold the key. The black keys right of the chords play the melody too |
| White keys | The melody |
| SELECT | Set (its own sounds, tempo, arpeggio and drums come with it) |
| ALGORITHM | Chord rhythm: HELD, FULL (held, with the drums), quarters, eighths, staccato, offbeat, pump, syncopated, stabs, tresillo, gallop, half-time. A set brings its own |
| PRESET | The chords' sound |
| K1..K4 | The page's four settings (shown at the bottom of the screen) |
| FX | Delay, crush, chorus, reverb |
| SEL | Chords: key, voicing, bass note, level. SEL again: CHORDS 2: width, spread |
| ARP | The arpeggiator: mode, rate (1/4 .. 1/32, triplets, FAST, CHIP), flow, swing |
| ENV | Groove: BPM, swing, density, sidechain |
| LFO | Bass: sound, level, riff, filter |
| EDIT | Melody: FOLLOW (chord / scale), level, space, preset. EDIT again: MELODY 2: glide, octave, vibrato, ASSIST |
| GLO | Drums: style (the 20 grooves, then USER 1 .. USER 20), kick, hats, snare. **GLO again: the DRUM STUDIO** |
| HOME | Back to the main page. Held: the title and your progress |
| REC | The loop: ready → the first chord records up to 8 bars (REC closes it) → it plays round. Held 1 s: cleared |
| PLAY | Stops the loop and starts it again from its start |
| SEQ | The next ship (the unlocked ones) |
| SAVE | The music heard from outside the room |
| OCT- / OCT+ | Octave |
| HOME + REC held 3 s | Back to the first-start settings (your progress and your grooves stay) |

### DRUM STUDIO (GLO twice)

| Control | What it does |
| --- | --- |
| The first 16 keys | The 16 steps of the track: a tap sets an accent, again a soft hit, again off. On the hats also an open hat and a roll |
| K1 | The track: KICK, SNARE, HATS, PERC |
| K2 | The kit |
| K3 | The groove's own swing |
| K4 | The user slot SAVE keeps it in (a `*`: a groove is there already, SAVE replaces it) |
| PLAY | The drums alone, to listen while you build (the loop still plays, if one runs) |
| REC | Clears the track |
| SAVE | Keeps the groove in the slot; STYLE is then that slot |

The studio starts from the groove that plays (a built-in one or a user slot): change one, then SAVE it as your own.

Settings save themselves a few seconds after the last change; the progress after a few quiet seconds.

Chords and bass go out as MIDI over USB (channel 1), and an incoming MIDI clock sets the tempo.

## Building

Push to GitHub: the workflow builds the firmware (`.fwsc`) on every push (Actions → Artifacts). A tag `v0.1`, `v0.2`...
(or a release made on GitHub with such a tag) builds a release with the `.fwsc` attached.

Locally (Linux x86-64), the simulator and its tests (the screens' pixel type is fm1-base's Terminus set):

    git clone --recursive https://github.com/swtdr34mz-afk/PIXEL_RUN && cd PIXEL_RUN
    X0X_FONTSET=terminus python3 fm1-base/tools/build.py . --host
    build/host/pixelrun_host tests/basic.txt build/host     # sets, sounds, crush, the drum studio, the loop
    build/host/pixelrun_host tests/run.txt build/host       # the run, a mission, the progress kept
    build/host/pixelrun_host tests/melody.txt build/host    # the melody follows the chords

## Licence and credits

GPL-3.0-only (see `LICENSE`), as Blop / OSR-1 it is based on. fm1-base and felucca-bsp stay under MPL-2.0. The intro's
title type is Press Start 2P (SIL OFL 1.1). The installable package carries three files of JieLi's AC79 SDK (Apache-2.0).
Every borrowed part is listed in `CREDITS.md`.
