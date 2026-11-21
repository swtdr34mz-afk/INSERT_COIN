/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records)
 * Copyright (C) 2026 swtdr34mz (PIXEL RUN: the chip kits and grooves) */
/* PIXEL RUN: the drum kits (8-bit and 16-bit consoles), the 20 built-in grooves, the bass sounds and riffs. Written by hand.
 * The DRUM STUDIO keeps 20 grooves of the player's own beside them (app.c, kept in the flash). */
#pragma once

/* the kits: kick (base Hz, sweep Hz, pitch drop s, body s, drive, hold s, click), hi-hat (tone, closed s, open s, metal),
 * perc (Hz, Hz, decay s, click), snare (body Hz, body 0..1, decay s, bright 0..1, clap 0 / 1), space (reverb), chip */
#define NKIT 5
static const char *const KIT_NAME[NKIT] = {"8-BIT", "HANDHELD", "16-BIT", "ROBOT", "ARCADE"};
static const kit_t KIT[NKIT] = {
    /* 8-BIT: a triangle kick that drops, the long noise for hats and snare, square blips */
    {50.0f, 240.0f, 0.025f, 0.12f, 1.0f, 0.010f, 0.0f, 0.80f, 0.030f, 0.15f, 0.0f, 880.0f, 1320.0f, 0.040f, 0.20f,
     180.0f, 0.35f, 0.12f, 0.45f, 0.0f, 0.08f, 1.0f},
    /* HANDHELD: brighter and shorter, a thin kick */
    {55.0f, 300.0f, 0.018f, 0.09f, 1.0f, 0.008f, 0.0f, 1.00f, 0.020f, 0.10f, 0.0f, 1568.0f, 2093.0f, 0.030f, 0.10f,
     220.0f, 0.20f, 0.09f, 0.70f, 0.0f, 0.05f, 1.0f},
    /* 16-BIT: the 16-bit consoles' punchier drums (their sampled kits): a driven kick, a crisp snare, a little room */
    {52.0f, 170.0f, 0.022f, 0.16f, 3.5f, 0.020f, 0.55f, 0.75f, 0.025f, 0.12f, 0.50f, 1700.0f, 500.0f, 0.010f, 0.30f,
     210.0f, 0.40f, 0.12f, 0.75f, 0.0f, 0.15f, 0.0f},
    /* ROBOT: the noise's short, tonal mode (metallic hats), a deep long kick */
    {45.0f, 200.0f, 0.040f, 0.35f, 1.0f, 0.040f, 0.0f, 0.60f, 0.025f, 0.12f, 1.0f, 660.0f, 990.0f, 0.050f, 0.40f,
     160.0f, 0.30f, 0.10f, 0.35f, 0.0f, 0.10f, 1.0f},
    /* ARCADE: a big sweeping kick, a long noise snare, a big room */
    {45.0f, 400.0f, 0.030f, 0.25f, 1.0f, 0.020f, 0.0f, 0.90f, 0.030f, 0.20f, 0.0f, 1047.0f, 1568.0f, 0.060f, 0.30f,
     150.0f, 0.40f, 0.18f, 0.55f, 0.0f, 0.22f, 1.0f},
};

/* the built-in grooves: 16 steps, X accent, x soft, . rest; hi-hat row: o open, 2 / 3 / 4 a roll of that many hits; their kit
 * and their own swing (0..100) */
#define NGROOVE 20
#define NUSER 20
#define NDRUM (NGROOVE + NUSER)          /* STYLE: the 20 grooves, then the 20 user slots */
static const char *const GROOVE_NAME[NGROOVE] = {
    "PLATFORMER", "OVERWORLD", "SHUFFLE", "CASTLE", "BOSS FIGHT", "RACING", "DUNGEON", "SHOOTER", "PUZZLE", "TITLE",
    "VICTORY", "UNDERWATER", "SPACE", "CHIP HOUSE", "CHIP TRAP", "CHIP BREAKS", "CHIP FUNK", "CHIP HIPHOP", "GALLOP",
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
    {"X.X.X.X.X.......", "xxxxxxxxxx......", "X.X.X.X.X.......", "........X.......", 0, 0},    /* victory fanfare */
    {"X.........x.....", "..o.......o.....", "........x.......", "......x.......x.", 1, 30},   /* underwater */
    {"X......X..X.....", "x.x.x.x.x.x.x.x.", "....X.......X...", ".x...x...x...x..", 3, 0},    /* space */
    {"X...X...X...X...", "..o...o...o...o.", "....X.......X...", "...x......x...x.", 2, 0},    /* chip house */
    {"X.......X.......", "x.x.x.x3x.x.x.4.", "....X.......X...", "................", 3, 0},    /* chip trap */
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
