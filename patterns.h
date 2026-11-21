/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records)
 * Copyright (C) 2026 swtdr34mz (PIXEL RUN: the chip kits and grooves) */
/* PIXEL RUN: the drum kits (8-bit and 16-bit consoles), the 20 built-in grooves, the bass sounds and riffs. Written by hand.
 * The DRUM STUDIO keeps 20 grooves of the player's own beside them (app.c, kept in the flash). */
#pragma once

/* the kits: the Game Boy's channels as its games and trackers made drums of them (dsp/engine.h kit_t):
 * kick (wave channel: from Hz, to Hz, drop s, length s, volume, envelope n, click), snare (noise: clock Hz, 7-bit, volume,
 * envelope n, length s, square body Hz), hats (noise: clock, 7-bit, volume, envelope n, closed s, open s), perc (pulse: Hz,
 * duty, volume, envelope n, length s), space. An envelope n: the 4-bit volume one step down every n/64 s */
#define NKIT 5
static const char *const KIT_NAME[NKIT] = {"POCKET", "BRICK", "TRACKER", "GRITTY", "TINY"};
static const kit_t KIT[NKIT] = {
    /* POCKET: the handheld's own games: a short sine kick, a bright hiss snare, thin hats */
    {220.0f, 45.0f, 0.035f, 0.16f, 15.0f, 2.0f, 0.25f,  16384.0f, 0.0f, 13.0f, 1.0f, 0.22f, 0.0f,
     131072.0f, 0.0f, 8.0f, 1.0f, 0.045f, 0.22f,  1047.0f, 0.25f, 10.0f, 1.0f, 0.07f,  0.0f},
    /* BRICK: heavier, the snare with a square body, a longer kick */
    {180.0f, 40.0f, 0.050f, 0.22f, 15.0f, 3.0f, 0.35f,  8192.0f, 0.0f, 15.0f, 2.0f, 0.28f, 185.0f,
     65536.0f, 0.0f, 10.0f, 1.0f, 0.040f, 0.28f,  523.0f, 0.5f, 11.0f, 1.0f, 0.08f,  0.02f},
    /* TRACKER: as the music trackers on the handheld: a punchy clicky kick, a crisp snare, metallic 7-bit hats */
    {300.0f, 48.0f, 0.025f, 0.14f, 15.0f, 1.0f, 0.50f,  32768.0f, 0.0f, 15.0f, 1.0f, 0.14f, 220.0f,
     262144.0f, 1.0f, 7.0f, 1.0f, 0.030f, 0.18f,  1568.0f, 0.125f, 9.0f, 1.0f, 0.05f,  0.0f},
    /* GRITTY: low clocks, the rumbly noise of the old games' explosions */
    {150.0f, 35.0f, 0.060f, 0.25f, 15.0f, 4.0f, 0.60f,  4096.0f, 0.0f, 15.0f, 2.0f, 0.30f, 0.0f,
     32768.0f, 0.0f, 9.0f, 1.0f, 0.060f, 0.30f,  880.0f, 0.5f, 10.0f, 2.0f, 0.09f,  0.0f},
    /* TINY: soft and short, quiet hats */
    {330.0f, 60.0f, 0.020f, 0.10f, 12.0f, 1.0f, 0.10f,  24576.0f, 0.0f, 10.0f, 1.0f, 0.12f, 0.0f,
     196608.0f, 0.0f, 5.0f, 1.0f, 0.025f, 0.15f,  2093.0f, 0.25f, 8.0f, 1.0f, 0.04f,  0.0f},
};

/* the built-in grooves: 16 steps, X accent, x soft, . rest; hi-hat row: o open, 2 / 3 / 4 a roll of that many hits; their kit
 * and their own swing (0..100) */
#define NGROOVE 20
#define NUSER 20
#define NDRUM (NGROOVE + NUSER)          /* STYLE: the 20 grooves, then the 20 user slots */
static const char *const GROOVE_NAME[NGROOVE] = {
    "PLATFORMER", "OVERWORLD", "SHUFFLE", "CASTLE", "BOSS FIGHT", "RACING", "DUNGEON", "SHOOTER", "PUZZLE", "TITLE",
    "BOUNCE", "UNDERWATER", "SPACE", "CHIP HOUSE", "TURBO", "CHIP BREAKS", "CHIP FUNK", "CHIP HIPHOP", "GALLOP",
    "CREDITS"};
static const struct { const char *k, *h, *s, *r; uint8_t kit, swing; } GROOVE_DEF[NGROOVE] = {
    /* kick                hi-hat              snare               perc                kit swing */
    {"X.......X.x.....", "x.x.x.x.x.x.x.x.", "....X.......X...", "................", 0, 0},    /* platformer */
    {"X.....X...X.....", "xxxxxxxxxxxxxxxx", "....X.......X..x", "................", 0, 0},    /* overworld: a march */
    {"X.....x.X.....x.", "x.x.x.x.x.x.x.x.", "....X.......X...", "................", 1, 60},   /* shuffle */
    {"X..X....X..X....", "x.x.x.x.x.x.x.x.", "....X.......X...", "......x.......x.", 2, 0},    /* castle */
    {"X.X.X.X.X.X.X.X.", "xxxxxxxxxxxxxxxx", "....X.......X.XX", "................", 4, 0},    /* boss fight */
    {"X...X...X...X...", "..o...o...o...o.", "....X.......X...", "x.x.x.x.x.x.x.x.", 2, 0},    /* racing */
    {"X.........X.....", "x...x...x...x...", "........X.......", "...x......x...x.", 0, 0},    /* dungeon */
    {"X.X...X.X.X...X.", "x3x.x3x.x3x.x3x.", "....X.......X...", "................", 4, 0},    /* shooter */
    {"X.......X.......", "x.x.x.x.x.x.x.x.", "....x.......x...", "x...x...x...x...", 1, 0},    /* puzzle */
    {"X...............", "....x.......x...", "........X.......", "................", 4, 0},    /* title screen */
    {"X.....X.X.......", "x.x.x.x.x.x.x.x.", "....X.......X...", "x...x...x...x...", 0, 0},    /* bounce: a candy land's hop */
    {"X.........x.....", "..o.......o.....", "........x.......", "......x.......x.", 1, 30},   /* underwater */
    {"X......X..X.....", "x.x.x.x.x.x.x.x.", "....X.......X...", ".x...x...x...x..", 3, 0},    /* space */
    {"X...X...X...X...", "..o...o...o...o.", "....X.......X...", "...x......x...x.", 2, 0},    /* chip house */
    {"X...X...X...X...", "xxxxxxxxxxxxxxxx", "....X.......X...", "..x...x...x...x.", 2, 0},    /* turbo: a race at full speed */
    {"X.........X.....", "x.x.x.x.x.x.x.x.", "....X..x.x..X...", "................", 4, 0},    /* chip breaks */
    {"X..x..X...X..x..", "x.xxx.xxx.xxx.xx", "....X..x....X...", "................", 0, 35},   /* chip funk */
    {"X......X..X.....", "x.x.x.x.x.x.x.x.", "....X.......X...", "................", 1, 45},   /* chip hip hop */
    {"X.XX.XX.X.XX.XX.", "x.x.x.x.x.x.x.x.", "....X.......X...", "................", 2, 0},    /* gallop */
    {"X.......x.......", "x...x...x...x...", "........X.......", "..x.......x.....", 0, 0},    /* credits: half-time */
};

static const char *const BASS_NAME[NBASS_SOUND] = {"OFF", "SUB", "ROUND", "ACID", "REESE", "PLUCK", "FM", "TRIANGLE", "PULSE"};
static const char *const MOTIF_NAME[NMOTIF] = {"FOLLOW", "HOLD", "OFFBEAT", "OCTAVES", "HALVES", "GALLOP",
                                               "SYNCOPE", "ROLLING"};

static const char *const ONOFF_NAME[2] = {"OFF", "ON"};
