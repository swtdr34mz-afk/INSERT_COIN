# Credits

INSERT COIN by swtdr34mz: CHORD RUN's chiptune sibling, based on OSR-1 / Blop by Obsimo (OSR Records), under GPL-3.0-only.
INSERT COIN's own parts: the 20 game-style sets (chord progressions written by ear, under names of their own), the chip
waveforms and sounds, the chip drums and their 20 grooves, the DRUM STUDIO, the bitcrusher, the 8-bar loop and the pixel
screens. From CHORD RUN (swtdr34mz): the melody voice, the runner, missions and the loop.

The sets' chord progressions are in the style of famous video game tunes. A chord progression is no one's own; no melody,
recording, name, character or picture of any game is used.

BLOCK DROP's GUIDE melody is Korobeiniki, a Russian folk song (Nikolay Nekrasov's poem, 1861), in the public domain.

All sounds are synthesized in the firmware: no samples. The drums emulate how the Game Boy's sound channels work (the noise
channel's shift register, the 4-bit volume envelopes, the length counter); no code or data of any console is used.

## Built on

| Project | Author | Licence | Used for |
| --- | --- | --- | --- |
| [fm1-base](https://github.com/charlesvestal/fm1-base) | Charles Vestal | MPL-2.0 | the platform: screen, panel, audio, MIDI, storage, simulator, build tools, the Terminus pixel fonts it carries (SIL OFL 1.1); `web/emu/worklet.js` |
| [felucca-bsp](https://github.com/hugelton/felucca-bsp) | Leo Kuroshita (Hügelton Instruments) and contributors | MPL-2.0 | the hardware layer, update loader, packaging |
| JieLi's AC79 SDK | JieLi Technology | Apache-2.0 | three files in every `.fwsc` package (fetched at build time, never copied here) |

## Code and assets from elsewhere

| Project | Author | Licence | Used for |
| --- | --- | --- | --- |
| [OSR-1 / Blop](https://github.com/OSR-Experiment/OSR-1) | Obsimo (OSR Records) | GPL-3.0-only | the base: the sound engine (`dsp/engine.c`), the chords, pages and settings (`app.c`) |
| [CHORD RUN](https://github.com/swtdr34mz-afk/CHORD_RUN) | swtdr34mz | GPL-3.0-only | the melody voice, the runner, missions, the loop |
| [Press Start 2P](https://fonts.google.com/specimen/Press+Start+2P) | CodeMan38 (Cody Boisclair) | SIL OFL 1.1 (`assets/fonts/PressStart2P-OFL.txt`) | the intro's title (`tools/gen_title.py`) |

Each new borrowing will be listed here: project, author, licence, files concerned.
