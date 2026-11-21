/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records)
 * Copyright (C) 2026 swtdr34mz (INSERT COIN: the chip sounds, the game sets) */
/* INSERT COIN data: the sounds (chip waveforms and 16-bit FM), the chord rhythms, and the 20 sets: chord progressions in the
 * style of famous video game tunes, under names of their own (no game's or company's name). Written by hand. */
#pragma once
static const char *const FAMILY_NAME[4] = {"PULSE", "WAVE", "COMPUTER", "16-BIT FM"};
#define NFAMILY 4
#define NSOUND 17
static const uint8_t FAMILY_OF[NSOUND] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 2, 3, 3, 3, 3};
static const char *const SOUND_NAME[NSOUND] = {
    "NES PULSE", "THIN PULSE", "QUARTER PULSE", "PWM SWEEP", "CHIP ORGAN", "POWER PULSE", "ZAP LEAD", "BLIP PLUCK",
    "8BIT PAD", "WAVE WARM", "WAVE VOICE", "TRI KEYS", "COMPUTER SAW", "FM BRASS", "FM BELL", "FM E.PIANO", "FM STRINGS",
};
/* engine, w1, w2, detune (cents), osc 2 (semitones), its level,
 * FM ratio, index, index decay, index sustain, feedback,   pluck brightness, damping,
 * attack, decay, sustain, release,   filter env (octaves), its decay,   chorus, wobble (cents), its rate,
 * noise, bass, level, own cutoff (Hz at CUTOFF 50 %),   chip: pitch drop (semitones), its time, PWM depth, its rate, steps */
static const sound_t SOUNDS[NSOUND] = {
    /* PULSE: the 8-bit consoles' square waves */
    {ENGINE_SUB, W_SQUARE, W_SQUARE, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.002f, 0.30f, 0.70f, 0.08f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.62f, 9000.0f, 0, 0, 0, 0, 0},      /* NES PULSE */
    {ENGINE_SUB, W_P12, W_P12, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.002f, 0.25f, 0.60f, 0.10f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.95f, 9000.0f, 0, 0, 0, 0, 0},      /* THIN PULSE */
    {ENGINE_SUB, W_P25, W_P25, 7.0f, 0.0f, 0.55f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.002f, 0.30f, 0.65f, 0.12f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.62f, 9000.0f, 0, 0, 0, 0, 0},      /* QUARTER PULSE */
    {ENGINE_SUB, W_PWM, W_PWM, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.010f, 0.40f, 0.80f, 0.25f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.70f, 9000.0f, 0, 0, 0.9f, 0.7f, 0}, /* PWM SWEEP */
    {ENGINE_SUB, W_SQUARE, W_SQUARE, 0.0f, 12.0f, 0.6f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.004f, 0.20f, 1.00f, 0.06f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.48f, 7000.0f, 0, 0, 0, 0, 0},      /* CHIP ORGAN */
    {ENGINE_SUB, W_SQUARE, W_P25, 0.0f, -12.0f, 0.8f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.002f, 0.30f, 0.90f, 0.10f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.50f, 8000.0f, 0, 0, 0, 0, 5.0f},   /* POWER PULSE */
    {ENGINE_SUB, W_SQUARE, W_SQUARE, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.001f, 0.30f, 0.70f, 0.10f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.62f, 9000.0f, 12.0f, 0.06f, 0, 0, 0}, /* ZAP LEAD: a laser's drop */
    {ENGINE_SUB, W_P25, W_P25, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.001f, 0.15f, 0.00f, 0.10f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.80f, 9000.0f, -12.0f, 0.02f, 0, 0, 0}, /* BLIP PLUCK: a coin's jump */
    {ENGINE_SUB, W_PWM, W_P25, 8.0f, 0.0f, 0.6f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.250f, 0.60f, 0.90f, 0.80f, 0.0f, 0.3f, 0.4f, 0.0f, 0.3f, 0.0f, 0.45f, 0.50f, 6000.0f, 0, 0, 0.5f, 0.3f, 7.0f}, /* 8BIT PAD */
    /* WAVE: the handheld's 4-bit wave channel, the consoles' triangle */
    {ENGINE_SUB, W_WAVE, W_WAVE, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.003f, 0.40f, 0.70f, 0.20f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.85f, 7000.0f, 0, 0, 0, 0, 0},      /* WAVE WARM */
    {ENGINE_SUB, W_VOICE, W_VOICE, 0.0f, 0.0f, 0.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.020f, 0.50f, 0.80f, 0.30f, 0.0f, 0.3f, 0.0f, 8.0f, 5.0f, 0.0f, 0.45f, 0.85f, 7000.0f, 0, 0, 0, 0, 0},      /* WAVE VOICE */
    {ENGINE_SUB, W_NTRI, W_SQUARE, 0.0f, 12.0f, 0.25f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.002f, 0.20f, 0.40f, 0.15f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 1.00f, 9000.0f, 0, 0, 0, 0, 0},      /* TRI KEYS */
    /* COMPUTER: a home computer's stepped saws through its filter */
    {ENGINE_SUB, W_CSAW, W_CSAW, 10.0f, 0.0f, 0.7f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.003f, 0.30f, 0.70f, 0.20f, 2.0f, 0.25f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.65f, 1800.0f, 0, 0, 0, 0, 0},     /* COMPUTER SAW */
    /* 16-BIT FM: the 16-bit consoles' FM chip */
    {ENGINE_FM, W_SINE, W_SINE, 0.0f, 0.0f, 0.0f, 1.0f, 2.4f, 0.30f, 0.50f, 0.30f, 0.6f, 0.996f,
     0.020f, 0.30f, 0.80f, 0.20f, 0.0f, 0.3f, 0.3f, 0.0f, 0.3f, 0.0f, 0.45f, 0.80f, 6000.0f, 0, 0, 0, 0, 0},      /* FM BRASS */
    {ENGINE_FM, W_SINE, W_SINE, 0.0f, 0.0f, 0.0f, 3.5f, 2.0f, 0.25f, 0.00f, 0.00f, 0.6f, 0.996f,
     0.001f, 0.80f, 0.00f, 0.60f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.90f, 8000.0f, 0, 0, 0, 0, 0},      /* FM BELL */
    {ENGINE_FM, W_SINE, W_SINE, 0.0f, 0.0f, 0.0f, 1.0f, 1.4f, 0.50f, 0.25f, 0.05f, 0.6f, 0.996f,
     0.002f, 1.00f, 0.20f, 0.40f, 0.0f, 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.45f, 0.90f, 5000.0f, 0, 0, 0, 0, 0},      /* FM E.PIANO */
    {ENGINE_SUB, W_SAW, W_SAW, 14.0f, 0.0f, 1.0f, 1, 1, 0.4f, 0.2f, 0, 0.6f, 0.996f,
     0.200f, 0.50f, 0.90f, 0.60f, 0.0f, 0.3f, 0.6f, 0.0f, 0.3f, 0.0f, 0.45f, 0.60f, 3000.0f, 0, 0, 0, 0, 0},      /* FM STRINGS */
};
/* the melody's presets (MELODY page, PRESET): the value is the sound + 1 (0 = as the chords); all of them */
static const uint8_t MEL_PRESET[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
#define NMEL_PRESET ((int)(sizeof MEL_PRESET / sizeof MEL_PRESET[0]))
#define NRHYTHM 12
#define RH_FULL 8                        /* FULL: the chord held and latched, the drums and the bass on */
static const char *const RHYTHM_NAME[NRHYTHM] = {"HELD", "QUARTERS", "EIGHTHS", "OFFBEAT", "PUMP", "SYNCOPATED", "TRESILLO", "HALF-TIME",
                                                 "FULL", "STACCATO", "GALLOP", "STABS"};
/* the order ALGORITHM turns through them */
static const uint8_t RHYTHM_ORDER[NRHYTHM] = {0, RH_FULL, 1, 2, 9, 3, 4, 5, 11, 6, 10, 7};
/* 16 steps: X accent, x soft, - tie, . rest ("" = held while the key is down; "=" = FULL: held until the key again) */
static const struct { const char *s; float gate, swing, atk; } RHYTHM_DEF[NRHYTHM] = {
    {"", 0.0f, 0.0f, 0.0f},
    {"X---x---X---x---", 0.85f, 0.0f, 0.0f},
    {"X-x-X-x-X-x-X-x-", 0.6f, 0.0f, 0.0f},
    {"..X...X...X...X.", 0.55f, 0.0f, 0.0f},
    {"X---X---X---X---", 0.95f, 0.0f, 0.09f},
    {"X..x..x...x..x..", 0.4f, 0.0f, 0.0f},
    {"X--x--x-X--x--x-", 0.7f, 0.0f, 0.0f},
    {"X-------x-----x-", 0.9f, 0.0f, 0.0f},
    {"=", 0.0f, 0.0f, 0.0f},
    {"X.x.X.x.X.x.X.x.", 0.22f, 0.0f, 0.0f},     /* STACCATO: short 8-bit chord hits */
    {"X.xxX.xxX.xxX.xx", 0.35f, 0.0f, 0.0f},     /* GALLOP: a fortress's ride */
    {"X.....X.....X...", 0.3f, 0.0f, 0.0f},      /* STABS: short hits across the bar */
};
/* the sets' scales (the melody's notes): their steps above the key */
enum { SC_AUTO, SC_MAJOR, SC_MINOR, SC_HARM, SC_DORIAN, SC_MIXO, SC_PHRYG, SC_PHRYGDOM, SC_LYDIAN, SC_MAJPENTA, SC_MINPENTA,
       SC_BLUES, NSCALE };
static const struct { const char *name; uint8_t n; int8_t iv[7]; } SCALE_DEF[NSCALE] = {
    {"AUTO", 7, {0, 2, 4, 5, 7, 9, 11}}, {"MAJOR", 7, {0, 2, 4, 5, 7, 9, 11}}, {"MINOR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"HARMONIC MINOR", 7, {0, 2, 3, 5, 7, 8, 11}}, {"DORIAN", 7, {0, 2, 3, 5, 7, 9, 10}}, {"MIXOLYDIAN", 7, {0, 2, 4, 5, 7, 9, 10}},
    {"PHRYGIAN", 7, {0, 1, 3, 5, 7, 8, 10}}, {"PHRYGIAN DOM", 7, {0, 1, 4, 5, 7, 8, 10}}, {"LYDIAN", 7, {0, 2, 4, 6, 7, 9, 11}},
    {"MAJOR PENTA", 5, {0, 2, 4, 7, 9}}, {"MINOR PENTA", 5, {0, 3, 5, 7, 10}}, {"BLUES", 6, {0, 3, 5, 6, 7, 10}},
};
/* the stages the runner runs through (app.c draws them): a set's world on the main page, and one for each page */
enum { ST_GRASS, ST_UNDER, ST_SKY, ST_SPEED, ST_FACTORY, ST_FIELD, ST_BLOCKS, ST_FORTRESS, ST_MEADOW, ST_LAVA, ST_CASTLE,
       ST_BASE, ST_BEACH, ST_CRYSTAL, ST_TIME, ST_WATER, ST_CANDY, ST_SPACE, ST_CITY, ST_RAINBOW, ST_ICE, NSTAGE };
#define NGRID 20
/* the SETS: the chords of a tune, one per BLACK key from the left (up to 8); the white keys play a melody in the scale of the
 * chord that sounds. A chord is its semitones above the set's key + a quality (M maj, V dominant-major, m, D dom7, d dim,
 * s sus4, S sus2, a aug); key 0 = C .. 11 = B, rich the richness it starts with. The progressions are written by ear in the
 * style of famous video game tunes (a chord progression is no one's own), under names of their own; each set is a level.
 * Then the set's own sound, set when it is chosen: the chords' sound, the melody's (0 = as the chords, else the sound + 1),
 * the bass's sound and riff, the chord rhythm, the drum groove, the tempo, the scale, the chip arpeggio (ARP mode, 0 = off,
 * its rate), reverb, delay, crush, chorus (0..100), the melody's vibrato, its stage, and its GUIDE melody (1: a folk tune
 * of its own, KORO in app.c; 0: one made up). */
static const struct {
    const char *name, *by;
    uint8_t key, rich;
    const char *white;
    uint8_t snd, msnd, bsnd, bmot, rhy, dpat, bpm, scale, arp, arate, rev, del, crush, chorus, mvib, stage, guide;
} GRID_DEF[NGRID] = {
    {"PLUMBER OVERWORLD", "SUNNY PLATFORMER", 0, 0, "0M 5M 7D 9m 8M 10M", 0, 3, BASS_TRI, MOTIF_OCTAVES, 9, 0, 150, SC_MAJOR,
     0, ARP_R_16, 10, 0, 0, 0, 0, ST_GRASS, 0},
    {"PIPE UNDERGROUND", "UNDERGROUND", 0, 0, "0m 9m 10M 3M 5m 8M", 1, 12, BASS_TRI, MOTIF_CONTRE, 11, 6, 100, SC_BLUES,
     0, ARP_R_16, 5, 35, 0, 0, 0, ST_UNDER, 0},
    {"SKY ATHLETIC", "SKY LEVEL", 5, 0, "0M 9m 5M 7D 2m 4D", 9, 2, BASS_PULSE, MOTIF_GALOP, 5, 2, 120, SC_MAJPENTA,
     0, ARP_R_16, 25, 15, 0, 20, 0, ST_SKY, 0},
    {"GREEN LOOP ZONE", "SPEED ZONE", 0, 1, "0M 9m 2m 7D 4m 5M", 13, 15, BASS_FM, MOTIF_SYNCOPE, 3, 16, 150, SC_MAJOR,
     0, ARP_R_16, 15, 0, 0, 30, 0, ST_SPEED, 0},
    {"TOXIC PLANT", "FACTORY ZONE", 7, 0, "0m 8M 10M 5m 3M 2m", 12, 4, BASS_ACID, MOTIF_ROULE, 2, 15, 145, SC_DORIAN,
     0, ARP_R_16, 10, 20, 25, 0, 0, ST_FACTORY, 0},
    {"HEROS FIELD", "HERO ADVENTURE", 10, 0, "0M 10M 5M 8M 3M 2m", 2, 1, BASS_TRI, MOTIF_GALOP, 6, 1, 125, SC_MIXO,
     0, ARP_R_16, 20, 0, 0, 0, 30, ST_FIELD, 0},
    {"BLOCK DROP", "BLOCK WORLD", 9, 0, "0m 7M 5m 3M 10M 8M", 2, 1, BASS_TRI, MOTIF_OCTAVES, 1, 8, 150, SC_HARM,
     0, ARP_R_16, 5, 0, 0, 0, 0, ST_BLOCKS, 1},
    {"ROBOT FORTRESS", "ROBOT FORTRESS", 4, 0, "0m 8M 10M 7m 5m 3M", 5, 7, BASS_PULSE, MOTIF_ROULE, 10, 7, 155, SC_MINOR,
     0, ARP_R_16, 10, 10, 15, 0, 0, ST_FORTRESS, 0},
    {"FIRST ROUTE", "FIRST JOURNEY", 2, 0, "0M 5M 7M 4m 9m 2m", 11, 3, BASS_TRI, MOTIF_OCTAVES, 2, 18, 130, SC_MAJOR,
     0, ARP_R_16, 15, 0, 0, 0, 0, ST_MEADOW, 0},
    {"MAGMA RUSH", "LAVA LEVEL", 4, 0, "0m 1M 3M 10M 8M 5m", 1, 4, BASS_PULSE, MOTIF_ROULE, RH_FULL, 4, 160, SC_PHRYGDOM,
     ARP_UP, ARP_R_CHIP, 10, 0, 30, 0, 20, ST_LAVA, 0},
    {"NIGHT CASTLE", "BOSS CASTLE", 2, 0, "0m 10M 8M 7M 5m 3M", 4, 13, BASS_PULSE, MOTIF_GALOP, 4, 3, 140, SC_HARM,
     0, ARP_R_16, 45, 0, 0, 0, 25, ST_CASTLE, 0},
    {"AIR BASE", "SOLDIER BASE", 3, 0, "0m 10M 8M 5m 3M 7M", 13, 14, BASS_FM, MOTIF_SYNCOPE, 5, 17, 120, SC_DORIAN,
     0, ARP_R_16, 15, 0, 0, 0, 0, ST_BASE, 0},
    {"COAST DRIVE", "BEACH RACE", 5, 1, "0M 2m 4m 5M 7D 9m", 15, 11, BASS_PLUCK, MOTIF_SYNCOPE, 3, 5, 140, SC_LYDIAN,
     0, ARP_R_16, 20, 0, 0, 40, 20, ST_BEACH, 0},
    {"CRYSTAL PRELUDE", "CRYSTAL CAVE", 0, 0, "0M 9m 5M 7M 8M 10M", 7, 15, BASS_TRI, MOTIF_TENUE, RH_FULL, 9, 100, SC_LYDIAN,
     ARP_UPDOWN2, ARP_R_16, 50, 30, 0, 0, 0, ST_CRYSTAL, 0},
    {"TIME CORRIDOR", "TIME HALL", 9, 1, "0m 5m 10D 3M 8M 2d 7D", 14, 11, BASS_ROND, MOTIF_SUIT, 11, 19, 115, SC_DORIAN,
     0, ARP_R_16, 30, 25, 0, 0, 0, ST_TIME, 0},
    {"DEEP REEF", "UNDERWATER", 4, 2, "0M 5M 9m 7M", 8, 11, BASS_SUB, MOTIF_TENUE, 7, 11, 90, SC_LYDIAN,
     0, ARP_R_16, 60, 20, 0, 50, 40, ST_WATER, 0},
    {"DREAM GREENS", "CANDY LAND", 7, 0, "0M 5M 7M 9m 2m 7D", 2, 10, BASS_TRI, MOTIF_OCTAVES, 3, 10, 140, SC_MAJPENTA,
     0, ARP_R_16, 15, 0, 0, 0, 0, ST_CANDY, 0},
    {"ALIEN CAVES", "ALIEN PLANET", 4, 0, "0m 8M 5m 1M 3M 10M", 10, 4, BASS_SUB, MOTIF_TENUE, 7, 12, 100, SC_PHRYG,
     0, ARP_R_16, 40, 40, 10, 0, 30, ST_SPACE, 0},
    {"CITY BRAWL", "NEON STREET", 9, 0, "0m 8M 3M 10M", 15, 3, BASS_FM, MOTIF_CONTRE, 4, 13, 128, SC_MINOR,
     0, ARP_R_16, 15, 25, 0, 0, 0, ST_CITY, 0},
    {"STAR ROAD", "RAINBOW RACE", 2, 0, "0M 4m 9m 5M 2m 7M", 8, 10, BASS_PULSE, MOTIF_SYNCOPE, RH_FULL, 14, 150, SC_MAJOR,
     ARP_UP2, ARP_R_16, 30, 20, 0, 20, 0, ST_RAINBOW, 0},
};
