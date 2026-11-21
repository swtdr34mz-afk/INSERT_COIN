/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records)
 * Copyright (C) 2026 swtdr34mz (CHORD RUN: the sets, the melody, the runner, missions, the loop; INSERT COIN: the chip sets,
 * the drum studio, the 8-bar loop, the pixel screens) */
/* INSERT COIN (CHORD RUN's chiptune sibling, based on BLOP by Obsimo): input, pages, the game sets, the melody, the drums and
 * the DRUM STUDIO, the runner and its missions, the loop, the screen, settings. Part of the unity build (device/unity.c,
 * host/plat_host.c), after the platform: gfx.c and announce.c are in scope. The sound is dsp/engine.c.
 *
 * Controls
 *   black keys      the set's chords (left to right, up to 8). With a rhythm the chord keeps playing (latch): the same
 *                   key again stops it; without one (HELD) it plays while the key is down; FULL: held, latched, with the
 *                   drums. The black keys right of the chords: melody keys, the semitone above the white key to their left
 *   white keys      the melody, in the scale of the chord that played last (its own sound, MELODY page)
 *   SELECT set (its own sounds, tempo, arpeggio and drums come with it)   ALGORITHM chord rhythm   PRESET the chords' sound
 *   K1..K4          the page's four settings, always shown at the bottom of the screen
 *   top row: page buttons (the same button again: back to the main page)
 *                   main  cutoff, glide, note length, drums (level; 0 = no drums)
 *                   FX    delay, crush (the bitcrusher), chorus, reverb
 *                   SEL   chords: key, voicing, bass note (root, 3rd, 5th, pedal), level; SEL again: CHORDS 2: width, spread
 *                   ARP   the arpeggiator: mode, rate (1/4 .. 1/32, triplets, and FAST / CHIP: the 8-bit chord arpeggio,
 *                         30 / 60 notes a second), flow, swing
 *                   ENV   groove: BPM, swing, density, sidechain on/off
 *                   LFO   bass: sound (TRIANGLE, PULSE: the chip ones), level, riff, filter
 *                   EDIT  melody: cutoff, level, space, preset; EDIT again: MELODY 2: glide, octave, vibrato, ASSIST
 *                   GLO   drums: style (20 grooves, 20 user slots), kick, hats, snare; GLO again: the DRUM STUDIO
 *   DRUM STUDIO     the first 16 keys are the 16 steps of a track (a tap: accent, again soft, again off; the hats also
 *                   open and a roll); K1 track, K2 kit, K3 swing, K4 the user slot; PLAY the drums alone (a preview),
 *                   REC clears the track, SAVE keeps the groove in the slot (and plays it from there)
 *   bottom row      HOME  back to the main page (held: the title and the player's progress)
 *                   SAVE  the music heard from outside the room (in the studio: save the groove)
 *                   SEQ   the next character (the unlocked ones)   PLAY  the loop: stop / start again from its start
 *                   REC   the loop: ready, the first chord records up to 8 bars (REC again closes it), then it plays
 *                         round; with a loop kept: ready for a new one; held 1 s: gone
 *   OCT- / OCT+ octave.   HOME + REC held 3 s: back to the first-start settings (a countdown; the progress stays).
 *   Settings save themselves a few seconds after the last change, the progress after a few quiet seconds. */
#include "fastmath.h"
#include "engine.h"
#include "data.h"
#include "patterns.h"

/* ------------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x50525534u            /* "PRU4": 10 sets */
typedef struct {
    uint32_t magic;
    uint8_t grid, key, rich, voicing;
    int8_t octave;
    uint8_t sound, rhythm, page;
    uint8_t cutoff, glide, gate, drums;         /* main */
    uint8_t delay, crush, chorus, reverb;       /* FX */
    uint8_t strum;                              /* chords (with key, voicing, richness) */
    uint8_t bpm, swing, dens, side;             /* groove (density: 50 = as written; side: 0 off, 1 on) */
    uint8_t dpat, dkick, dhat, drim;            /* drums: the style (the grooves, then the user slots), levels */
    uint8_t bsound, blevel, bmotif, bcut;       /* bass */
    uint8_t mglide, moct, mvib, assist;         /* melody 2: glide, octave (2 = as is), vibrato; ASSIST (the good notes light up) */
    uint8_t mcut, mlev, mrev, msound;           /* melody: cutoff, level, space (reverb, then echoes), preset (0 = as the chords) */
    uint8_t chr;                                /* the character that runs */
    uint8_t aflow, carp, cbass, cwidth, cspread, crate;   /* arp flow, arp mode, bass note, width, spread, arp rate */
    uint8_t aswing, clevel;                               /* arp swing, the chords' level */
    uint8_t dtrack, dkit, dswing, dslot;                  /* the drum studio: its track, the groove's kit and swing, the slot */
    uint8_t mfollow;                                      /* the melody follows: 0 SMART (its keys stay, the notes fit the chord),
                                                             1 ROOT (the chord's root on the leftmost key) */
    uint8_t rsv[3];
} settings_t;
static settings_t set;
static uint8_t set_dirty;
static uint32_t dirty_at;             /* when the settings last changed (auto-save)*/

static void song_style(void);
static void groove_load(int style);
static void settings_defaults(void)
{
    memset(&set, 0, sizeof set);
    set.magic = SET_MAGIC;
    set.cutoff = 50; set.glide = 0; set.gate = 50; set.drums = 70;      /* chip drums from the start */
    set.delay = 0; set.crush = 0; set.chorus = 0; set.reverb = 15;  /* a little room */
    set.strum = 0;
    set.aflow = 50; set.carp = 0; set.cbass = 0; set.cwidth = 50; set.cspread = 0; set.crate = ARP_R_16;
    set.aswing = 0; set.clevel = 80;
    set.bpm = 120; set.swing = 0; set.dens = 50; set.side = 1;
    set.dpat = 0; set.dkick = 80; set.dhat = 55; set.drim = 60;
    set.bsound = BASS_TRI; set.blevel = 80; set.bmotif = MOTIF_SUIT; set.bcut = 70;
    set.mcut = 50; set.mlev = 80; set.mrev = 25; set.msound = 0;
    set.mglide = 0; set.moct = 2; set.mvib = 0; set.assist = 0;
    set.rhythm = 1;                                                    /* (a rhythm: the set's own, song_style) */
    set.mfollow = 0;
    set.dtrack = 0; set.dslot = 0;
    song_style();
}

/* a set's own sound (data.h GRID_DEF): chords, melody, bass, tempo, drums, the chip arpeggio; its chord rhythm only when a
 * rhythm plays */
static void song_style(void)
{
    set.key = GRID_DEF[set.grid].key;
    set.rich = GRID_DEF[set.grid].rich;
    set.sound = GRID_DEF[set.grid].snd;
    set.msound = GRID_DEF[set.grid].msnd;
    set.bsound = GRID_DEF[set.grid].bsnd;
    set.bmotif = GRID_DEF[set.grid].bmot;
    set.dpat = GRID_DEF[set.grid].dpat;
    set.bpm = GRID_DEF[set.grid].bpm;
    set.carp = GRID_DEF[set.grid].arp;
    if (set.carp) set.crate = GRID_DEF[set.grid].arate;
    if (set.rhythm) set.rhythm = GRID_DEF[set.grid].rhy;   /* its own rhythm (HELD stays HELD: the chords under the fingers) */
    set.reverb = GRID_DEF[set.grid].rev; set.delay = GRID_DEF[set.grid].del;
    set.crush = GRID_DEF[set.grid].crush; set.chorus = GRID_DEF[set.grid].chorus;
    set.mvib = GRID_DEF[set.grid].mvib;
    set.drums = GRID_DEF[set.grid].dlev ? GRID_DEF[set.grid].dlev : 70;
    set.mlev = GRID_DEF[set.grid].mlev ? GRID_DEF[set.grid].mlev : 80;
    if (GRID_DEF[set.grid].guide) set.assist = 2;     /* its tune lights up (ASSIST: GUIDE) */
    else if (set.assist == 2) set.assist = 0;
    groove_load(set.dpat);
}

static float glide_s(void) { return set.glide < 1u ? 0.0f : (float)set.glide * 0.015f; }
/* the knob, 70 Hz .. 12 kHz; the engine reads it relative to the sound's own cutoff (50 % = as the sound is made) */
static float cutoff_hz(void) { return 70.0f * fm_powf(12000.0f / 70.0f, (float)set.cutoff * 0.01f); }
static float reso_q(void) { return 1.0f; }

/* ------------------------------------------------------------------ chords --- */
typedef struct { uint8_t semi; char q; } chord_def_t;
static chord_def_t grid_w[NGRID][8];
static uint8_t grid_n[NGRID];             /* the chords in a set (4..8) */
static const char *const NOTE[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const char *const RICH_NAME[4] = {"TRIAD", "7TH", "9TH", "LUSH"};
static const char *const VOICING_NAME[3] = {"CLOSE", "OPEN", "WIDE"};
static const uint8_t WHITE_IDX[8] = {0, 2, 4, 5, 7, 9, 11, 12}, BLACK_IDX[5] = {1, 3, 6, 8, 10};

static int parse_chords(const char *s, chord_def_t *out, int max)
{
    int n = 0;
    while (*s && n < max) {
        int v = 0;
        while (*s == ' ') s++;
        while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
        out[n].semi = (uint8_t)v;
        out[n].q = *s ? *s++ : 'M';
        n++;
    }
    return n;
}

/* intervals for a quality at a richness; returns the count */
static int quality_ivs(char q, int rich, int8_t *iv)
{
    static const int8_t M[4][6] = {{0,4,7}, {0,4,7,11}, {0,4,7,11,14}, {0,4,7,11,14,21}};
    static const int8_t V[4][6] = {{0,4,7}, {0,4,7,10}, {0,4,7,10,14}, {0,4,7,10,14,21}};
    static const int8_t m[4][6] = {{0,3,7}, {0,3,7,10}, {0,3,7,10,14}, {0,3,7,10,14,17}};
    static const int8_t D[4][6] = {{0,4,7,10}, {0,4,7,10}, {0,4,7,10,14}, {0,4,7,10,14,21}};
    static const int8_t d[4][6] = {{0,3,6}, {0,3,6,10}, {0,3,6,10,14}, {0,3,6,10,14,17}};
    static const int8_t s4[4][6] = {{0,5,7}, {0,5,7,10}, {0,5,7,10,14}, {0,5,7,10,14,21}};
    static const int8_t s2[4][6] = {{0,2,7}, {0,2,7,10}, {0,2,7,10}, {0,2,7,10,21}};
    static const int8_t a[4][6] = {{0,4,8}, {0,4,8,11}, {0,4,8,11,14}, {0,4,8,11,14}};
    static const uint8_t NM[4] = {3, 4, 5, 6}, ND[4] = {4, 4, 5, 6}, NS2[4] = {3, 4, 4, 5}, NA[4] = {3, 4, 5, 5};
    const int8_t *src;
    int n, i;
    rich = clamp(rich, 0, 3);
    switch (q) {
    case 'V': src = V[rich]; n = NM[rich]; break;
    case 'm': src = m[rich]; n = NM[rich]; break;
    case 'D': src = D[rich]; n = ND[rich]; break;
    case 'd': src = d[rich]; n = NM[rich]; break;
    case 's': src = s4[rich]; n = NM[rich]; break;
    case 'S': src = s2[rich]; n = NS2[rich]; break;
    case 'a': src = a[rich]; n = NA[rich]; break;
    default: src = M[rich]; n = NM[rich]; break;
    }
    for (i = 0; i < n; i++) iv[i] = src[i];
    return n;
}

typedef struct { int root; int8_t iv[6]; int n; int bass_off; int oct; int shift; } chord_t;

#define KEY_C4 7                         /* key index of C4 (key 0 is F3) */
/* The keys: the BLACK keys play the set's chords (the first 8, left to right), the WHITE keys play the melody. */
static int8_t kb_black[32], kb_white[32];                      /* key -> chord slot (-1) / white-key number (-1) */
static int8_t kb_wkey[32];                                      /* white-key number -> its key */
static uint8_t kb_ready;
static int last_slot = -1;                                      /* the chord the melody follows (the last one played) */
static uint32_t ld_held;                                        /* the white keys held */
static int ld_cur = -1;                                         /* the white key the melody voice plays (-1: none) */

static void kb_init(void)
{
    int k, i, nb = 0, nw = 0;
    for (k = 0; k < 32; k++) { kb_black[k] = -1; kb_white[k] = -1; }
    for (k = 0; k < NKEYS && k < 32; k++) {
        int pos = (((k - KEY_C4) % 12) + 12) % 12, blk = 0;
        for (i = 0; i < 5; i++) if (BLACK_IDX[i] == pos) blk = 1;
        if (blk) { kb_black[k] = (int8_t)(nb < 8 ? nb : -1); nb++; }
        else { kb_wkey[nw] = (int8_t)k; kb_white[k] = (int8_t)nw++; }
    }
    kb_ready = 1;
}

/* a key -> the set's chord (0..7) it plays; -1 when the key is no chord key or the set has no such chord */
static int key_slot(int key, int *var, int *oct)
{
    int s;
    if (!kb_ready) kb_init();
    *var = 0;
    *oct = 0;
    if (key < 0 || key >= 32) return -1;
    s = kb_black[key];
    return s >= 0 && s < (int)grid_n[set.grid] ? s : -1;
}

/* the notes of a set's chord (its triad), a bit per pitch class */
static uint16_t chord_tones(int slot)
{
    int8_t iv[6];
    int nt, i;
    uint16_t ct = 0;
    const chord_def_t *d = &grid_w[set.grid][slot];
    nt = quality_ivs(d->q, 0, iv);
    for (i = 0; i < nt; i++) ct |= (uint16_t)(1u << ((set.key + d->semi + iv[i]) % 12));
    return ct;
}
/* a minor song: it starts or ends on its minor tonic */
static int song_minor(void)
{
    int n = grid_n[set.grid];
    return (grid_w[set.grid][0].semi == 0 && grid_w[set.grid][0].q == 'm')
        || (grid_w[set.grid][n - 1].semi == 0 && grid_w[set.grid][n - 1].q == 'm');
}
/* the set's own scale (data.h SCALE_*, GRID_DEF scale), a bit per pitch class, from its key */
static uint16_t song_scale(void)
{
    int i, sc = GRID_DEF[set.grid].scale;
    uint16_t m = 0;
    if (sc == SC_AUTO) sc = song_minor() ? SC_MINOR : SC_MAJOR;
    for (i = 0; i < SCALE_DEF[sc].n; i++) m |= (uint16_t)(1u << ((set.key + SCALE_DEF[sc].iv[i]) % 12));
    return m;
}
/* the scale the melody plays over a chord (a bit per pitch class): the set's scale, with the chord's own notes in it (where
 * one is missing, the scale's note a semitone from it gives way to it; else it is added) */
static uint16_t mel_scale(int slot)
{
    uint16_t sc = song_scale(), ct;
    int pc;
    if (slot < 0 || slot >= (int)grid_n[set.grid]) slot = 0;
    ct = chord_tones(slot);
    for (pc = 0; pc < 12; pc++) {
        if (!((ct >> pc) & 1u) || ((sc >> pc) & 1u)) continue;
        if ((sc >> ((pc + 1) % 12)) & 1u && !((ct >> ((pc + 1) % 12)) & 1u)) sc &= (uint16_t)~(1u << ((pc + 1) % 12));
        else if ((sc >> ((pc + 11) % 12)) & 1u && !((ct >> ((pc + 11) % 12)) & 1u)) sc &= (uint16_t)~(1u << ((pc + 11) % 12));
        sc |= (uint16_t)(1u << pc);
    }
    return sc;
}
/* the melody: white key number w -> a MIDI pitch.
 * FOLLOW SMART (the start): each key has its own note in the set's scale, from its tonic (the leftmost key), and that note
 * fits the chord that plays: where it is not in the chord's scale (mel_scale: the set's scale with the chord's own notes in
 * it) it moves one semitone, to the chord's note if one is there. So a chord change never makes a key jump: at most a
 * semitone (a note held over the change moves with it).
 * FOLLOW ROOT: the chord's root on the leftmost key, its scale above it: every chord change moves all the keys. */
static float mel_pitch(int w)
{
    int n = grid_n[set.grid], slot = last_slot, base, deg, p, pc, i, np = 0, root;
    int8_t pcs[12];
    uint16_t sc, ct;
    if (slot < 0 || slot >= n) slot = 0;
    sc = mel_scale(slot);
    ct = chord_tones(slot);
    base = 60 + set.key - (set.key >= 6 ? 12 : 0) + set.octave * 12 + ((int)set.moct - 2) * 12;   /* the set's tonic */
    deg = w < 0 ? 0 : w;
    if (set.mfollow == 1) {
        root = (set.key + grid_w[set.grid][slot].semi) % 12;
        for (i = 0; i < 12; i++) if ((sc >> ((root + i) % 12)) & 1u) pcs[np++] = (int8_t)i;   /* the scale from the root */
        if (np < 1) { pcs[0] = 0; np = 1; }
        p = base + ((root - set.key) % 12 + 12) % 12;
        if (p - base > 6) p -= 12;                                         /* the root nearest the tonic */
        p += pcs[deg % np] + 12 * (deg / np);
    } else {
        uint16_t ss = song_scale();
        for (i = 0; i < 12; i++) if ((ss >> ((set.key + i) % 12)) & 1u) pcs[np++] = (int8_t)i;
        if (np < 1) { pcs[0] = 0; np = 1; }
        p = base + pcs[deg % np] + 12 * (deg / np);
        pc = ((p % 12) + 12) % 12;
        if (!((sc >> pc) & 1u)) {
            int dn = (pc + 11) % 12, up = (pc + 1) % 12, in_dn = (sc >> dn) & 1u, in_up = (sc >> up) & 1u;
            if (in_dn && in_up) p += (((ct >> up) & 1u) && !((ct >> dn) & 1u)) ? 1 : -1;
            else p += in_up ? 1 : -1;
        }
    }
    while (p > 108) p -= 12;
    while (p < 36) p += 12;
    return (float)p;
}

/* The black keys right of the song's chords (and the last three, past 8 chords): melody keys too, each the semitone above the
 * white key to its left, so the notes the white keys leave out (the blue notes, the song's accidentals) are there, where a
 * piano has them: between the two white keys (where those are a semitone apart, it is the right one's note, as E# is F) */
static int mel_black(int k)
{
    if (!kb_ready) kb_init();
    return k > 0 && k < NKEYS && kb_white[k] < 0 && kb_white[k - 1] >= 0 && (kb_black[k] < 0 || kb_black[k] >= (int)grid_n[set.grid]);
}
static int mel_key(int k) { return (k >= 0 && k < NKEYS && kb_white[k] >= 0) || mel_black(k); }
static float key_pitch(int k) { return kb_white[k] >= 0 ? mel_pitch(kb_white[k]) : mel_pitch(kb_white[k - 1]) + 1.0f; }

static chord_t slot_chord(int slot, int var, int oct)
{
    chord_t c;
    const chord_def_t *d;
    int rich = set.rich, i;
    c.root = 0; c.n = 0; c.bass_off = 0; c.oct = oct; c.shift = set.octave * 12;
    if (slot < 0 || slot > 7) return c;
    d = &grid_w[set.grid][slot];
    c.root = (set.key + d->semi) % 12;
    c.n = quality_ivs(d->q, rich, c.iv);
    if (var) {                                                  /* the black key: one step richer (one poorer at LUSH) */
        int8_t v[6];
        int nv = quality_ivs(d->q, rich < 3 ? rich + 1 : 2, v);
        if (nv == c.n && rich < 2) nv = quality_ivs(d->q, rich + 2, v);   /* a 7th chord's next step looks the same: go one more */
        for (i = 0; i < nv; i++) c.iv[i] = v[i];
        c.n = nv;
    }
    return c;
}

static chord_t build_chord(int key)
{
    int var, oct, slot = key_slot(key, &var, &oct);
    return slot_chord(slot, var, oct);
}

static int has(const chord_t *c, int pc)
{
    int i;
    for (i = 0; i < c->n; i++) if (c->iv[i] % 12 == pc) return 1;
    return 0;
}

static void text_cat(char *d, const char *s)
{
    while (*d) d++;
    while (*s) *d++ = *s++;
    *d = 0;
}

static void chord_name(const chord_t *c, char *b)
{
    int m = has(c, 3) && !has(c, 4), M = has(c, 4);
    int sus4 = !m && !M && has(c, 5), sus2 = !m && !M && !has(c, 5) && has(c, 2);
    int dim = m && has(c, 6) && !has(c, 7), aug = M && has(c, 8) && !has(c, 7);
    int maj7 = has(c, 11), min7 = has(c, 10), six = has(c, 9), add9 = has(c, 2) && !sus2;
    int eleven = has(c, 5) && (m || M);
    b[0] = 0;
    text_cat(b, NOTE[c->root]);
    if (dim) { text_cat(b, min7 ? "m7b5" : "dim"); return; }
    if (m) text_cat(b, "m");
    if (aug) text_cat(b, "+");
    if (maj7 || min7) {
        if (maj7) text_cat(b, "maj");
        text_cat(b, six ? "13" : eleven ? "11" : add9 ? "9" : "7");
    } else if (six && add9) text_cat(b, "6/9");
    else if (six) text_cat(b, "6");
    else if (add9) text_cat(b, "add9");
    if (sus4) text_cat(b, "sus4");
    if (sus2) text_cat(b, "sus2");
}

/* the 4-note voicing closest to the centre: the same key always sounds the same */
static void voice_chord(const chord_t *c, float *out)
{
    int pcs[6], np = 0, i, r, base, best_set = 0;
    int center = 64 + c->oct * 7 + c->shift;
    float best = 1e9f;
    for (i = 0; i < c->n; i++) {
        int p = (c->root + c->iv[i]) % 12, k, dup = 0;
        for (k = 0; k < np; k++) if (pcs[k] == p) dup = 1;
        if (!dup) pcs[np++] = p;
    }
    if (np > 4) {                        /* drop the fifth, then the root (the bass plays it) */
        int k, j = 0;
        for (k = 0; k < np; k++) if (pcs[k] != (c->root + 7) % 12) pcs[j++] = pcs[k];
        np = j;
    }
    if (np > 4) {
        int k, j = 0;
        for (k = 0; k < np; k++) if (pcs[k] != c->root) pcs[j++] = pcs[k];
        np = j;
    }
    if (np > 4) np = 4;
    for (r = 0; r < np; r++) {
        for (base = 50 + c->shift; base <= 72 + c->shift; base++) {
            int notes[4], k, cur = base, cnt = 0;
            float mean = 0.0f, cost;
            for (k = 0; k < np; k++) {
                int p = pcs[(r + k) % np];
                while (((cur % 12) + 12) % 12 != p) cur++;
                notes[cnt++] = cur++;
            }
            if (cnt == 3) notes[cnt++] = notes[0] + 12;
            if (notes[0] < 50 + c->shift || notes[cnt - 1] > 84 + c->shift) continue;
            for (k = 0; k < 4; k++) mean += (float)notes[k];
            mean *= 0.25f;
            cost = fm_fabsf(mean - (float)center);
            if (cost < best) {
                best = cost;
                for (k = 0; k < 4; k++) out[k] = (float)notes[k];
                best_set = 1;
            }
        }
    }
    if (!best_set) for (i = 0; i < 4; i++) out[i] = 60.0f + (float)i * 4.0f;
}

static void spread(float *n, int mode)   /* SERRE as is, OUVERT drop 2, LARGE drop 2 + top up an octave */
{
    int i, j;
    if (mode >= 1) n[2] -= 12.0f;
    if (mode >= 2) n[3] += 12.0f;
    for (i = 1; i < 4; i++)              /* sort */
        for (j = i; j > 0 && n[j - 1] > n[j]; j--) { float t = n[j]; n[j] = n[j - 1]; n[j - 1] = t; }
}

/* ------------------------------------------------------------------ engine links --- */
/* the melody voice (white keys): its preset (0 = the chords' sound), cutoff (50 = as the sound is made), level, and SPACE,
 * one knob for a few effects, sent (the melody itself stays as loud): up to 40 % a reverb that grows (a light tail), then
 * echoes (the ping-pong delay, in time) join in and the reverb grows on to big and airy */
static const sound_t *snd_at(int i) { return &SOUNDS[i < NSOUND ? i : 0]; }
static void send_lead(void)
{
    float hz = 70.0f * fm_powf(12000.0f / 70.0f, (float)set.mcut * 0.01f), sp = (float)set.mrev * 0.01f;
    float rev = fm_minf(1.0f, sp * 1.35f), del = sp <= 0.4f ? 0.0f : (sp - 0.4f) * (0.5f / 0.6f);
    eng_lead_sound(snd_at(set.msound ? set.msound - 1 : set.sound));
    eng_lead_params(hz, (float)set.mlev * 0.0125f, rev, del);
    eng_lead_mod((float)set.mglide * 0.015f, (float)set.mvib * 0.01f);    /* glide up to 0.6 s (as GLIDE shows it) */
}
static void send_sound(void) { eng_sound(snd_at(set.sound)); send_lead(); }
static void send_rhythm(void)
{
    rhythm_t r;
    int i, def = set.rhythm;
    const char *s = RHYTHM_DEF[def].s;
    memset(&r, 0, sizeof r);
    r.hold = s[0] == 0;
    r.sus = s[0] == '=';
    r.gate = RHYTHM_DEF[def].gate;
    r.swing = RHYTHM_DEF[def].swing;
    r.atk = RHYTHM_DEF[def].atk;
    if (!r.hold && !r.sus)                                       /* (FULL's "=" is no pattern) */
        for (i = 0; i < 16; i++) {
            if (s[i] == 'X' || s[i] == 'x') {
                int len = 1;
                while (i + len < 16 && s[i + len] == '-') len++;
                r.v[i] = s[i] == 'X' ? 1.0f : 0.62f;
                r.len[i] = (uint8_t)len;
            }
        }
    eng_rhythm(&r);
}
/* ------------------------------------------------------------------ MIDI clock in (USB) --- */
/* Ableton (or any host) sending MIDI clock to Blop: its tempo replaces BPM, and its beats place the rhythm's steps.
 * The 1 kHz tick reads the MIDI in (each clock's time to the ms); the main loop takes the tempo and the beats from it.
 * Start (FA) puts the next clock on beat 1, Continue (FB) and Song Position (F2) where the host is, Stop (FC) lets
 * Blop run on alone at the last tempo, as when the clock goes quiet. */
#define CK_N 49u                                  /* 48 clocks: two beats, for the tempo */
static volatile struct {
    uint32_t t[CK_N], n;                          /* the clocks' times (ms), how many so far */
    int32_t pos;                                  /* clocks since beat 1 (24 a beat), counted while the host plays */
    uint8_t run;                                  /* the host's transport plays (Start / Continue .. Stop) */
    uint32_t beat_ms, beat_seq;                   /* the last beat's time, and a count of beats */
    int32_t beat_pos;
    uint32_t last_ms;
} ck;
static float ck_bpm;                              /* the tempo it gives (0: no clock, BPM leads) */
static uint32_t ck_seen_seq;
static uint8_t ck_placed;                         /* the engine's steps follow the beats */

void app_tick(void)
{
    uint32_t pkt, now;
    int port;
    while (plat_midi_in(&pkt, &port)) {
        uint32_t st = pkt >> 8 & 0xFFu;
        if (port != PORT_USB) continue;
        now = plat_ms();
        switch (st) {
        case 0xF8:
            ck.t[ck.n % CK_N] = now; ck.n++; ck.last_ms = now;
            if (ck.run) {
                ck.pos++;
                if (ck.pos % 24 == 0) { ck.beat_ms = now; ck.beat_pos = ck.pos; ck.beat_seq++; }
            }
            break;
        case 0xFA: ck.run = 1; ck.pos = -1; break;
        case 0xFB: ck.run = 1; break;
        case 0xFC: ck.run = 0; break;
        case 0xF2: ck.pos = (int32_t)((pkt >> 16 & 0x7Fu) | (pkt >> 24 & 0x7Fu) << 7) * 6 - 1; break;
        default: break;
        }
    }
}

static void send_params(void)
{
    eng_params(ck_bpm > 0.0f ? ck_bpm : (float)set.bpm, cutoff_hz(), reso_q(), set.side ? 0.75f : 0.0f, set.drums > 0);
}

static void clock_follow(uint32_t now)
{
    uint32_t f, n, t0, t1, seq, bms;
    int32_t bpos;
    uint8_t run;
    float bpm = 0.0f;
    f = plat_lock();
    n = ck.n; t1 = ck.t[(n + CK_N - 1u) % CK_N]; t0 = ck.t[n % CK_N];
    seq = ck.beat_seq; bms = ck.beat_ms; bpos = ck.beat_pos; run = ck.run;
    plat_unlock(f);
    if (n >= CK_N && now - t1 < 300u && t1 - t0 > 0u) bpm = 120000.0f / (float)(t1 - t0);   /* 48 clocks = 2 beats */
    if (bpm > 0.0f && (bpm < 30.0f || bpm > 300.0f)) bpm = 0.0f;
    if (bpm > 0.0f) {
        float b = ck_bpm > 0.0f ? ck_bpm + 0.25f * (bpm - ck_bpm) : bpm;
        b = (float)(int32_t)(b * 10.0f + 0.5f) * 0.1f;               /* to a tenth */
        if (ck_bpm <= 0.0f) app_say("MIDI SYNC", "TEMPO FROM USB");
        if (b != ck_bpm) { ck_bpm = b; send_params(); }
        if (seq != ck_seen_seq) {
            ck_seen_seq = seq;
            if (run) { eng_sync((int)((bpos / 6) & 15), bms); ck_placed = 1; }
        }
        if (!run && ck_placed) { eng_sync(-1, 0); ck_placed = 0; }   /* the host stopped: free again, its tempo kept */
    } else if (ck_bpm > 0.0f) {
        ck_bpm = 0.0f;
        eng_sync(-1, 0);
        ck_placed = 0;
        send_params();
        app_say("MIDI SYNC", "OFF");
    }
}
static void send_fx(void)
{
    eng_fx((float)set.reverb * 0.01f, (float)set.delay * 0.01f, 0.0f, (float)set.chorus * 0.01f, (float)set.crush * 0.01f);
}
static void send_cfx(void)
{
    eng_chordfx(set.carp, set.crate, (float)set.aflow * 0.01f, (float)set.aswing * 0.01f, (float)set.cwidth * 0.01f,
                set.cspread, (float)set.clevel * (1.0f / 80.0f));
}
static void send_groove(void)
{
    eng_groove((float)set.swing * 0.01f, ((float)set.dens - 50.0f) * 0.02f, (float)set.gate * 0.01f,
               (float)set.strum * 0.01f);
}
/* ------------------------------------------------------------------ the drums and the DRUM STUDIO --- */
/* What plays is the working groove (wk): a style's groove copied in (a built-in one, or a user slot), which the studio
 * edits as it plays. Each step of each track (KICK, SNARE, HATS, PERC) is 0 off, 1 soft, 2 accent; the hats also 3 open
 * and 4 a roll (three hits). SAVE in the studio keeps it in a user slot (the 20 of them in the flash, apart from the
 * settings), and the style is then that slot. */
enum { TR_KICK, TR_SNARE, TR_HAT, TR_PERC, NTRACK };
static const char *const TRACK_NAME[NTRACK] = {"KICK", "SNARE", "HATS", "PERC"};
typedef struct { uint8_t st[NTRACK][16]; uint8_t kit, swing, used, rsv; } groove_t;
#define DRUMS_MAGIC 0x50524431u          /* "PRD1" */
static struct { uint32_t magic; groove_t g[NUSER]; } user;
static uint8_t user_dirty;
static groove_t wk;                      /* the working groove: what plays */
static uint8_t wk_edited;                /* changed in the studio since it was loaded or saved */
static uint8_t preview;                  /* the studio's preview: the drums alone */
static char drum_name_buf[NUSER][12];
static const char *drum_name[NDRUM];     /* STYLE's names: the grooves, then USER 1 .. USER 20 (app_init) */
static const char *slot_name[NUSER];

static uint8_t step_code(char c)
{
    return c == 'X' ? 2 : c == 'x' ? 1 : c == 'o' ? 3 : (c >= '2' && c <= '4') ? 4 : 0;
}
static void groove_load(int style)
{
    int t, i;
    memset(&wk, 0, sizeof wk);
    if (style < NGROOVE) {
        const char *row[NTRACK];
        row[TR_KICK] = GROOVE_DEF[style].k; row[TR_SNARE] = GROOVE_DEF[style].s;
        row[TR_HAT] = GROOVE_DEF[style].h; row[TR_PERC] = GROOVE_DEF[style].r;
        for (t = 0; t < NTRACK; t++)
            for (i = 0; i < 16; i++) {
                uint8_t c = step_code(row[t][i]);
                wk.st[t][i] = t == TR_HAT ? c : c > 2 ? 1 : c;
            }
        wk.kit = GROOVE_DEF[style].kit; wk.swing = GROOVE_DEF[style].swing; wk.used = 1;
    } else if (style < NDRUM && user.g[style - NGROOVE].used) wk = user.g[style - NGROOVE];
    else { wk.kit = 0; wk.swing = 0; }    /* an empty slot: silence, to fill in the studio */
    if (wk.kit >= NKIT) wk.kit = 0;
    if (wk.swing > 100) wk.swing = 0;
    set.dkit = wk.kit; set.dswing = wk.swing;
    wk_edited = 0;
}
static void send_drums(void)
{
    drums_t d;
    int i;
    static const float V[5] = {0.0f, 0.6f, 1.0f, 0.75f, 0.7f};
    for (i = 0; i < 16; i++) {
        uint8_t hc = wk.st[TR_HAT][i];
        d.k[i] = V[wk.st[TR_KICK][i] & 3];
        d.s[i] = V[wk.st[TR_SNARE][i] & 3];
        d.r[i] = V[wk.st[TR_PERC][i] & 3];
        d.h[i] = V[hc < 5 ? hc : 0];
        d.open[i] = hc == 3;
        d.roll[i] = (uint8_t)(hc == 4 ? 3 : 0);
    }
    d.swing = (float)wk.swing * 0.01f;
    d.kit = KIT[wk.kit < NKIT ? wk.kit : 0];
    {   /* the main page's DRUMS is the kit's master level */
        float m = (float)set.drums * 0.01f;
        m = m * (1.6f - 0.6f * m);
        eng_drums(&d, (float)set.dkick * 0.01f * m, (float)set.dhat * 0.01f * m, (float)set.drim * 0.01f * m);
    }
}
static void user_load(void)
{
    if (plat_store_load(OBJ_DRUMS, &user, sizeof user) != (int)sizeof user || user.magic != DRUMS_MAGIC) {
        memset(&user, 0, sizeof user);
        user.magic = DRUMS_MAGIC;
    }
}
/* the studio's SAVE: the working groove into the slot (the style is then that slot) */
static void mission_groove(void);
static void groove_save(void)
{
    char t[24];
    wk.used = 1;
    user.g[set.dslot] = wk;
    user_dirty = 1;
    set.dpat = (uint8_t)(NGROOVE + set.dslot);
    wk_edited = 0;
    t[0] = 0;
    {
        const char *n = slot_name[set.dslot];
        int k = 0;
        while (n[k] && k < 20) { t[k] = n[k]; k++; }
        t[k] = 0;
    }
    app_say("SAVED TO", t);
    mission_groove();
}
/* a key in the studio: its step (the first 16 keys) on the track, round: off, accent, soft (hats: open, a roll) */
static void studio_tap(int step)
{
    uint8_t *v = &wk.st[set.dtrack][step];
    if (set.dtrack == TR_HAT) *v = (uint8_t)(*v == 0 ? 2 : *v == 2 ? 1 : *v == 1 ? 3 : *v == 3 ? 4 : 0);
    else *v = (uint8_t)(*v == 0 ? 2 : *v == 2 ? 1 : 0);
    wk_edited = 1;
    send_drums();
}
static void send_bass(void) { eng_bass(set.bsound, set.bmotif, (float)set.blevel * 0.01f, (float)set.bcut * 0.01f); }

/* ------------------------------------------------------------------ playing --- */
static int cur_idx = -1, held_idx = -1, active;
static char chord_txt[16] = APP_NAME;
static uint32_t hop_at;                                         /* the runner hops (a chord, a melody note) */
static void gm_chord(void);
static void gm_note(int pitch);
static uint32_t chord_at;                                       /* when the chord began (ASSIST's beat) */
static void play(int idx)
{
    chord_t c = build_chord(idx);
    float notes[4], bass;
    uint32_t now = plat_ms();
    if (c.n == 0) return;                                       /* a silent key */
    voice_chord(&c, notes);
    spread(notes, set.voicing);
    {   /* the bass note: the root, the chord's third or fifth (a slash chord, C/E), or the song's tonic all along (a pedal) */
        int bpc = c.root;
        if (set.cbass == 1 && c.n > 1) bpc = (c.root + c.iv[1]) % 12;
        else if (set.cbass == 2 && c.n > 2) bpc = (c.root + c.iv[2]) % 12;
        else if (set.cbass == 3) bpc = set.key;
        bass = (float)(36 + bpc + clamp(c.shift, -12, 12));
        chord_name(&c, chord_txt);
        if (bpc != c.root) { text_cat(chord_txt, "/"); text_cat(chord_txt, NOTE[bpc]); }
    }
    if (cur_idx != idx || !active) chord_at = now;
    cur_idx = idx;
    hop_at = now;
    gm_chord();                                                 /* the hero jumps (at a boss: throws a star) */
    { int v, o, was = last_slot; last_slot = key_slot(idx, &v, &o);
      if (was != last_slot && ld_cur >= 0) eng_lead_bend(key_pitch(ld_cur)); }   /* a held melody note moves with the chord */
    active = 1;
    eng_chord(notes, bass, glide_s());
}

static void stop(void)
{
    active = 0;
    cur_idx = -1;
    eng_stop();
}

static void rechord(void)
{
    int v, o;
    if (!active || cur_idx < 0) return;
    if (key_slot(cur_idx, &v, &o) < 0) stop();                  /* the new set has no chord on this key */
    else play(cur_idx);
}

/* ------------------------------------------------------------------ pages --- */
enum { PG_MAIN, PG_FX, PG_ACC, PG_GROOVE, PG_BASS, PG_MELODY, PG_DRUMS, PG_MELODY2, PG_ACC2, PG_ARP, PG_STUDIO, NPAGE };
static const char *const PAGE_NAME[NPAGE] = {"MAIN", "FX", "CHORDS", "GROOVE", "BASS", "MELODY", "DRUMS", "MELODY 2", "CHORDS 2",
                                             "ARP", "DRUM STUDIO"};
static const uint8_t PAGE_BTN[NPAGE] = {0xFF, B_FX, B_SEL, B_ENV, B_LFO, B_EDIT, B_GLO, B_EDIT, B_SEL, B_ARP, B_SEQ};   /* EDIT / SEL
                                                                                                                 GLO again: page 2 */
static const uint8_t PAGE_RGB[NPAGE][3] = {{255, 214, 40}, {90, 200, 255}, {255, 150, 40}, {255, 80, 80},
                                          {80, 230, 120}, {200, 130, 255}, {255, 110, 180}, {200, 130, 255}, {255, 150, 40},
                                          {60, 230, 230}, {255, 110, 180}};
static const char *const ARP_NAME[NARP] = {"OFF", "UP", "DOWN", "UP-DOWN", "RANDOM", "UP 2 OCT", "DOWN 2 OCT", "UP-DN 2 OCT",
                                           "UP 3 OCT"};
static const char *const BASSNOTE_NAME[4] = {"ROOT", "3RD", "5TH", "PEDAL"};
static const char *const SPREAD_NAME[NSPREAD] = {"DIVERGE", "ALTERNATE", "DIVERGE2", "NOTEVAL"};
static const char *const RATE_NAME[NARP_RATE] = {"1/4", "1/4 T", "1/8", "1/8 T", "1/16", "1/16 T", "1/32", "FAST", "CHIP"};
static const char *const MOCT_NAME[5] = {"-2", "-1", "0", "+1", "+2"};
static const char *const ASSIST_NAME[3] = {"OFF", "CHORD", "GUIDE"};
static const char *const FOLLOW_NAME[2] = {"SMART", "ROOT"};

enum { KD_PCT, KD_OFF, KD_GLIDE, KD_BPM, KD_BIP, KD_LIST };
enum { G_NONE, G_PARAMS, G_FX, G_GROOVE, G_DRUMS, G_BASS, G_CHORD, G_DRUMLVL, G_MELODY, G_CFX, G_STYLE, G_GROOVE_ED };
typedef struct {
    const char *lab;
    uint8_t *v;
    uint8_t lo, hi, kind, grp;
    const char *const *names;
} kdef_t;
static const char *msound_name[NSOUND + 1];    /* "AS CHORDS", then the sounds (app_init) */
/* where a melody preset is in MELODY's list (-1: one for the chords only) */
static int mel_preset_at(int v)
{
    int i;
    for (i = 0; i < NMEL_PRESET; i++)
        if (MEL_PRESET[i] == v) return i;
    return -1;
}
static const char *const CHR_NAME_B[10] = {"VIPER", "COMET", "NOVA", "PULSAR", "ZENITH", "FLARE", "ORBIT", "RAPTOR", "QUASAR", "ION"};
static int chars_unlocked(void);
static const kdef_t KD[NPAGE][4] = {
    {{"CUTOFF", &set.cutoff, 0, 100, KD_PCT, G_PARAMS, 0}, {"GLIDE", &set.glide, 0, 100, KD_GLIDE, G_NONE, 0},
     {"LENGTH", &set.gate, 0, 100, KD_PCT, G_GROOVE, 0}, {"DRUMS", &set.drums, 0, 100, KD_OFF, G_DRUMLVL, 0}},
    {{"DELAY", &set.delay, 0, 100, KD_OFF, G_FX, 0}, {"CRUSH", &set.crush, 0, 100, KD_OFF, G_FX, 0},
     {"CHORUS", &set.chorus, 0, 100, KD_OFF, G_FX, 0}, {"REVERB", &set.reverb, 0, 100, KD_PCT, G_FX, 0}},
    {{"KEY", &set.key, 0, 11, KD_LIST, G_CHORD, NOTE}, {"VOICING", &set.voicing, 0, 2, KD_LIST, G_CHORD, VOICING_NAME},
     {"BASS NOTE", &set.cbass, 0, 3, KD_LIST, G_CHORD, BASSNOTE_NAME}, {"LEVEL", &set.clevel, 0, 100, KD_PCT, G_CFX, 0}},
    {{"BPM", &set.bpm, 70, 160, KD_BPM, G_PARAMS, 0}, {"SWING", &set.swing, 0, 100, KD_OFF, G_GROOVE, 0},
     {"DENSITY", &set.dens, 0, 100, KD_BIP, G_GROOVE, 0}, {"SIDECHAIN", &set.side, 0, 1, KD_LIST, G_PARAMS, ONOFF_NAME}},
    {{"SOUND", &set.bsound, 0, NBASS_SOUND - 1, KD_LIST, G_BASS, BASS_NAME}, {"LEVEL", &set.blevel, 0, 100, KD_PCT, G_BASS, 0},
     {"RIFF", &set.bmotif, 0, NMOTIF - 1, KD_LIST, G_BASS, MOTIF_NAME}, {"FILTER", &set.bcut, 0, 100, KD_PCT, G_BASS, 0}},
    {{"FOLLOW", &set.mfollow, 0, 1, KD_LIST, G_NONE, FOLLOW_NAME}, {"LEVEL", &set.mlev, 0, 100, KD_PCT, G_MELODY, 0},
     {"SPACE", &set.mrev, 0, 100, KD_OFF, G_MELODY, 0}, {"PRESET", &set.msound, 0, NSOUND, KD_LIST, G_MELODY, msound_name}},
    {{"STYLE", &set.dpat, 0, NDRUM - 1, KD_LIST, G_STYLE, drum_name}, {"KICK", &set.dkick, 0, 100, KD_OFF, G_DRUMS, 0},
     {"HATS", &set.dhat, 0, 100, KD_OFF, G_DRUMS, 0}, {"SNARE", &set.drim, 0, 100, KD_OFF, G_DRUMS, 0}},
    {{"GLIDE", &set.mglide, 0, 40, KD_GLIDE, G_MELODY, 0}, {"OCTAVE", &set.moct, 0, 4, KD_LIST, G_MELODY, MOCT_NAME},
     {"VIBRATO", &set.mvib, 0, 100, KD_OFF, G_MELODY, 0}, {"ASSIST", &set.assist, 0, 2, KD_LIST, G_NONE, ASSIST_NAME}},
    {{"WIDTH", &set.cwidth, 0, 100, KD_PCT, G_CFX, 0}, {"SPREAD", &set.cspread, 0, NSPREAD - 1, KD_LIST, G_CFX, SPREAD_NAME},
     {"SHIP", &set.chr, 0, 9, KD_LIST, G_NONE, CHR_NAME_B}, {0, 0, 0, 0, 0, 0, 0}},
    {{"MODE", &set.carp, 0, NARP - 1, KD_LIST, G_CFX, ARP_NAME}, {"RATE", &set.crate, 0, NARP_RATE - 1, KD_LIST, G_CFX, RATE_NAME},
     {"FLOW", &set.aflow, 0, 100, KD_PCT, G_CFX, 0}, {"SWING", &set.aswing, 0, 100, KD_OFF, G_CFX, 0}},
    {{"TRACK", &set.dtrack, 0, NTRACK - 1, KD_LIST, G_NONE, TRACK_NAME}, {"KIT", &set.dkit, 0, NKIT - 1, KD_LIST, G_GROOVE_ED, KIT_NAME},
     {"SWING", &set.dswing, 0, 100, KD_OFF, G_GROOVE_ED, 0}, {"SAVE TO", &set.dslot, 0, NUSER - 1, KD_LIST, G_NONE, slot_name}},
};

static int settings_ok(void)
{
    int p, k;
    if (set.magic != SET_MAGIC || set.grid >= NGRID || set.octave < -2 || set.octave > 2 || set.sound >= NSOUND
        || set.rhythm >= NRHYTHM || set.page >= NPAGE || set.chr >= 10 || set.dpat >= NDRUM)
        return 0;
    for (p = 0; p < NPAGE; p++)
        for (k = 0; k < 4; k++)
            if (KD[p][k].v && (*KD[p][k].v < KD[p][k].lo || *KD[p][k].v > KD[p][k].hi)) return 0;
    return 1;
}

/* ------------------------------------------------------------------ platform contract --- */
static void settings_load(void)
{
    int n = plat_store_load(OBJ_SETTINGS, &set, sizeof set);
    if (n != (int)sizeof set || set.magic != SET_MAGIC || !settings_ok()) settings_defaults();
    else groove_load(set.dpat);              /* the style's groove (a user slot's as kept) */
    if (mel_preset_at(set.msound) < 0) set.msound = GRID_DEF[set.grid].msnd;
    if (set.page == PG_STUDIO) set.page = PG_DRUMS;
}

static void game_init(void);
void app_init(void)
{
    int g;
    msound_name[0] = "AS CHORDS";
    for (g = 0; g < NSOUND; g++) msound_name[g + 1] = SOUND_NAME[g];
    for (g = 0; g < NGROOVE; g++) drum_name[g] = GROOVE_NAME[g];
    for (g = 0; g < NUSER; g++) {               /* USER 1 .. USER 20 */
        char *b = drum_name_buf[g];
        b[0] = 'U'; b[1] = 'S'; b[2] = 'E'; b[3] = 'R'; b[4] = ' ';
        if (g >= 9) { b[5] = (char)('0' + (g + 1) / 10); b[6] = (char)('0' + (g + 1) % 10); b[7] = 0; }
        else { b[5] = (char)('1' + g); b[6] = 0; }
        drum_name[NGROOVE + g] = b;
        slot_name[g] = b;
    }
    user_load();
    for (g = 0; g < NGRID; g++) {
        grid_n[g] = (uint8_t)parse_chords(GRID_DEF[g].white, grid_w[g], 8);
    }
    settings_load();
    game_init();
    eng_init();
    send_sound();
    send_rhythm();
    send_groove();
    send_cfx();
    send_drums();
    send_bass();
    send_fx();
    send_params();
}

void app_render(int32_t *lr, uint32_t n, uint32_t master_q12)
{
    eng_render(lr, n, (float)master_q12 * (1.0f / 4096.0f) * 8300000.0f);
}

void app_panic(void) { eng_panic(); }

/* ------------------------------------------------------------------ UI --- */
static struct {
    char msg[2][24];
    uint32_t msg_until;
    uint32_t keys, btn;
    uint32_t frame_at, home_at;
    uint8_t help, help_drawn, home_long;
    uint32_t reset_at;                   /* HOME + REC held since (0: not) */
    uint8_t reset_shown, reset_done;
    uint8_t bowl;                        /* the music heard from outside the room (SAVE) */
} ui;

static void int_text(int32_t v, char *b)
{
    char t[12];
    int n = 0;
    uint32_t u = (uint32_t)(v < 0 ? -v : v);
    do t[n++] = (char)('0' + u % 10u); while ((u /= 10u) && n < 11);
    if (v < 0) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}

void app_say(const char *a, const char *b)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++) ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++) ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (ann_on()) {
        t[0] = 0;
        text_cat(t, ui.msg[0]);
        text_cat(t, " ");
        text_cat(t, ui.msg[1]);
        ann_say(t, 0, ANN_NOW);
    }
}

static void dirty(void) { set_dirty = 1; dirty_at = plat_ms(); }

static void wrap_u8(uint8_t *v, int32_t d, int n) { *v = (uint8_t)(((int32_t)*v + d % n + n) % n); }

/* a quick turn moves further: x1 slow, x2, x4 when the detents come close together */
static uint32_t enc_at[NE];
static int32_t accel(int k, int32_t e)
{
    uint32_t now = plat_ms(), dt = now - enc_at[k];
    enc_at[k] = now;
    if (dt < 40u) return e * 4;
    if (dt < 90u) return e * 2;
    return e;
}

/* a knob's value as text, and as 0..1 for its bar */
static float knob_text(const kdef_t *d, char *b)
{
    int v = *d->v;
    float f = (float)(v - d->lo) / (float)(d->hi - d->lo);
    switch (d->kind) {
    case KD_LIST:
        b[0] = 0; text_cat(b, d->names[v]);
        if (d->v == &set.dpat && v >= NGROOVE && !user.g[v - NGROOVE].used) text_cat(b, " -");   /* an empty slot */
        if (d->v == &set.dslot && user.g[v].used) text_cat(b, "*");                          /* a slot kept: SAVE replaces it */
        break;
    case KD_BPM: if (ck_bpm > 0.0f) { int_text((int32_t)(ck_bpm + 0.5f), b); text_cat(b, " SYNC"); } else int_text(v, b); break;
    case KD_GLIDE:
        if (v < 1) { b[0] = 0; text_cat(b, "OFF"); }
        else { int_text(v * 15, b); text_cat(b, " MS"); }
        break;
    case KD_BIP:
        if (v == 50) { b[0] = 0; text_cat(b, "0"); }
        else { if (v > 50) { b[0] = '+'; b[1] = 0; } else b[0] = 0; int_text((v - 50) * 2, b + (v > 50)); text_cat(b, "%"); }
        break;
    case KD_OFF:
        if (v == 0) { b[0] = 0; text_cat(b, "OFF"); break; }
        /* fall through */
    default: int_text(v, b); text_cat(b, "%"); break;
    }
    return f;
}

static uint8_t prev_drums;
static void apply_group(int g)
{
    switch (g) {
    case G_PARAMS: send_params(); break;
    case G_FX: send_fx(); break;
    case G_GROOVE: send_groove(); break;
    case G_DRUMS: send_drums(); break;
    case G_BASS: send_bass(); break;
    case G_MELODY: send_lead(); break;
    case G_CFX: send_cfx(); break;
    case G_CHORD: rechord(); break;
    case G_STYLE: groove_load(set.dpat); send_drums(); send_params(); break;   /* a style: its groove, and its swing */
    case G_GROOVE_ED: wk.kit = set.dkit; wk.swing = set.dswing; wk_edited = 1; send_drums(); break;
    case G_DRUMLVL:
        if ((*KD[PG_MAIN][3].v > 0) != (prev_drums > 0)) send_params();
        send_drums();
        break;
    default: break;
    }
}

static void knob(int k, int32_t e)
{
    const kdef_t *d = &KD[set.page][k];
    int32_t step, n;
    if (!d->v) return;                   /* (an empty place on the page) */
    prev_drums = set.drums;
    step = d->kind == KD_LIST ? e : d->kind == KD_BPM ? accel(EN_K1 + k, e) : accel(EN_K1 + k, e) * 3;
    if (d->v == &set.chr) n = clamp((int32_t)*d->v + step, 0, chars_unlocked() - 1);   /* the ships unlocked */
    else if (d->v == &set.msound) n = MEL_PRESET[clamp(mel_preset_at(set.msound) + step, 0, NMEL_PRESET - 1)];   /* the melody's ones */
    else n = clamp((int32_t)*d->v + step, d->lo, d->hi);
    if (n == *d->v) return;
    *d->v = (uint8_t)n;
    apply_group(d->grp);
    dirty();
}

static void encoder(int k, int32_t e)
{
    switch (k) {
    case EN_SELECT:
        wrap_u8(&set.grid, e, NGRID);
        song_style();                                           /* the song's own sound, tempo and drums */
        app_say("SET", GRID_DEF[set.grid].name);
        send_sound(); send_bass(); send_drums(); send_params(); send_fx(); send_cfx();
        if (set.rhythm) send_rhythm();
        rechord();
        break;
    case EN_ALGO:
        {   /* in RHYTHM_ORDER: HELD, FULL, then the patterns */
            int i, at = 0;
            for (i = 0; i < NRHYTHM; i++) if (RHYTHM_ORDER[i] == set.rhythm) at = i;
            at = ((at + (int)(e % NRHYTHM)) % NRHYTHM + NRHYTHM) % NRHYTHM;
            set.rhythm = RHYTHM_ORDER[at];
        }
        app_say("RHYTHM", set.rhythm ? RHYTHM_NAME[set.rhythm] : "OFF");
        send_rhythm();
        if (!set.rhythm && held_idx < 0 && active) stop();   /* no rhythm, no latch */
        break;
    case EN_PRESET:
        wrap_u8(&set.sound, e, NSOUND);
        app_say(FAMILY_NAME[FAMILY_OF[set.sound]], SOUND_NAME[set.sound]);
        send_sound();
        break;
    case EN_K1: case EN_K2: case EN_K3: case EN_K4: knob(k - EN_K1, e); return;
    default: return;
    }
    dirty();
}

/* ------------------------------------------------------------------ the run (CHORD RUN) --- */
/* While you play (a chord sounds, a melody note, the loop) the little runner runs: its metres add up (kept in the flash, apart
 * from the settings), a run ends after 3 s of quiet (the best one is kept), new characters unlock at so many metres, and
 * there is always a mission (one after the other, a bit harder each round) that gives metres on top. */
static const uint32_t UNLOCK_M[10] = {0u, 0u, 250u, 700u, 1400u, 2400u, 3600u, 5200u, 7200u, 10000u};
#define PROG_MAGIC 0x31524E43u                  /* "CNR1" */
typedef struct {
    uint32_t magic;
    uint32_t metres, best;                      /* metres run in all, the longest run */
    uint32_t chords, notes;                     /* chords and melody notes played in all */
    uint32_t missions;                          /* missions done: the next one is the one after */
    uint32_t songs;                             /* a bit per song played */
    uint32_t rsv[4];
} prog_t;
static prog_t prog;
static uint8_t prog_dirty;

enum { MS_CHORDS, MS_NOTES, MS_RUN, MS_SONGS, MS_ALL, MS_DRUMS, MS_LOOP, MS_TOTAL, MS_GUIDE, MS_GROOVE, NMS };
static const struct { const char *a, *b; uint16_t base, step, cap; } MISSION[NMS] = {
    {"PLAY ", " CHORDS", 24, 12, 96},
    {"PLAY ", " NOTES", 40, 20, 160},
    {"RUN ", " M AT ONCE", 120, 60, 600},
    {"PLAY ", " SETS", 3, 1, 8},
    {"ALL CHORDS OF A SET", "", 0, 0, 0},
    {"JAM ", " S WITH DRUMS", 45, 15, 180},
    {"RECORD A LOOP (REC)", "", 1, 0, 1},
    {"RUN ", " M", 400, 200, 2000},
    {"HIT ", " LIT KEYS (GUIDE)", 20, 10, 80},
    {"SAVE A GROOVE (STUDIO)", "", 1, 0, 1},
};
static const uint8_t MS_ORDER[NMS] = {MS_CHORDS, MS_NOTES, MS_RUN, MS_GROOVE, MS_SONGS, MS_ALL, MS_DRUMS, MS_LOOP, MS_GUIDE,
                                      MS_TOTAL};

static struct {
    float speed, run_m, carry, scroll;          /* m/s now, this run's metres, a part metre not yet counted, the ground's scroll (px) */
    uint32_t busy_at, note_at, party_at;        /* last played, last melody note, a celebration */
    uint8_t running;                            /* a run is under way */
    uint8_t mtype, mgrid, mslots;               /* the mission, and for EVERY CHORD: the song, the chords played in it */
    uint16_t mtarget;
    float mprog;
    uint32_t mseen;                             /* songs played in this mission (a bit each) */
} run;
static struct { char a[24], b[24]; uint32_t at, until; } toast;

static void show_toast(const char *a, const char *b)
{
    toast.a[0] = 0; text_cat(toast.a, a);
    toast.b[0] = 0; text_cat(toast.b, b);
    toast.at = plat_ms(); toast.until = toast.at + 2600u;
    run.party_at = toast.at;
    app_say(a, b);
}

static int chars_unlocked(void)
{
    int i, n = 0;
    for (i = 0; i < 10; i++) if (prog.metres >= UNLOCK_M[i]) n = i + 1;
    return n;
}

static int grid_full_mask(void) { return (1 << grid_n[set.grid]) - 1; }
static int popcount8(uint32_t v) { int n = 0; while (v) { n += (int)(v & 1u); v >>= 1; } return n; }

static void mission_start(void)
{
    uint32_t k = prog.missions;
    int round = (int)(k / NMS), t;
    run.mtype = MS_ORDER[k % NMS];
    t = MISSION[run.mtype].base + MISSION[run.mtype].step * round;
    if (t > MISSION[run.mtype].cap && MISSION[run.mtype].cap) t = MISSION[run.mtype].cap;
    run.mtarget = (uint16_t)t;
    run.mprog = 0.0f;
    run.mseen = 0;
    run.mgrid = set.grid; run.mslots = 0;
}

/* the mission as text: "PLAY 24 CHORDS", and its progress "5/24" */
static void mission_text(char *b, char *p)
{
    char n[12];
    b[0] = 0; p[0] = 0;
    text_cat(b, MISSION[run.mtype].a);
    if (MISSION[run.mtype].b[0]) { int_text(run.mtarget, n); text_cat(b, n); text_cat(b, MISSION[run.mtype].b); }
    if (run.mtype == MS_ALL) {
        int_text(popcount8(run.mslots), p); text_cat(p, "/"); int_text(grid_n[set.grid], n); text_cat(p, n);
    } else if (run.mtype != MS_LOOP && run.mtype != MS_GROOVE) {
        int_text((int32_t)run.mprog, p); text_cat(p, "/"); int_text(run.mtarget, n); text_cat(p, n);
    }
}

static void add_metres(uint32_t m)
{
    int before = chars_unlocked(), after;
    prog.metres += m;
    prog_dirty = 1;
    after = chars_unlocked();
    if (after > before) {
        char t[24];
        t[0] = 0; text_cat(t, CHR_NAME_B[after - 1]); text_cat(t, " - SEL 2X: SHIP");
        show_toast("NEW SHIP", t);
    }
}

static void mission_check(void)
{
    int done = run.mtype == MS_ALL ? run.mslots != 0 && run.mslots == grid_full_mask() : run.mprog >= (float)run.mtarget;
    if (done) {
        uint32_t bonus = 150u + 50u * (prog.missions / NMS);
        char t[16], n[12];
        prog.missions++;
        prog_dirty = 1;
        int_text((int32_t)bonus, n);
        t[0] = 0; text_cat(t, "+"); text_cat(t, n); text_cat(t, " M");
        show_toast("MISSION DONE", t);
        mission_start();
        add_metres(bonus);
    }
}

static void mission_groove(void) { if (run.mtype == MS_GROOVE) { run.mprog = 1.0f; mission_check(); } }

/* the player played a chord (key) or stopped it (-1), a melody note */
static void game_chord(int key)
{
    int v, o, slot;
    uint32_t now = plat_ms();
    run.busy_at = now;
    if (key < 0) return;
    prog.chords++;
    prog.songs |= 1u << set.grid;
    if (run.mtype == MS_CHORDS) run.mprog += 1.0f;
    if (run.mtype == MS_SONGS && !(run.mseen & (1u << set.grid))) { run.mseen |= 1u << set.grid; run.mprog += 1.0f; }
    if (run.mtype == MS_ALL) {
        if (run.mgrid != set.grid) { run.mgrid = set.grid; run.mslots = 0; }
        slot = key_slot(key, &v, &o);
        if (slot >= 0) run.mslots |= (uint8_t)(1u << slot);
    }
    mission_check();
}
static void game_note(void)
{
    run.note_at = run.busy_at = plat_ms();
    prog.notes++;
    if (run.mtype == MS_NOTES) { run.mprog += 1.0f; mission_check(); }
}

/* ------------------------------------------------------------------ the loop (REC, PLAY) --- */
/* REC: ready; the first chord starts the recording, 4 bars (of the tempo), then it plays round and round (the chord changes,
 * as played), and the melody is yours to play over it. PLAY / STOP: the loop stops, and starts again from its start (the
 * loop stays). REC while recording: the loop closes at the bar; REC with a loop kept: ready for a new one (the old one plays
 * again with PLAY until a chord starts the new recording; REC again: not). REC held 1 s: the loop is gone. */
enum { LP_OFF, LP_READY, LP_REC, LP_PLAY, LP_PAUSE };
#define LP_MAX 192
#define LP_BARS 8
static struct {
    uint8_t st, firing, held_done, has;         /* has: a loop is kept (to play with PLAY) */
    uint16_t n, len;                            /* events, the loop's length in steps (16 a bar) */
    struct { uint16_t step; int8_t key; } ev[LP_MAX];
    uint32_t t0, rec_down;                      /* the recording's / the round's start (ms), REC pressed at */
    int32_t last;                               /* the last step played this round */
} lp;

static float step_ms(void) { return 15000.0f / (ck_bpm > 0.0f ? ck_bpm : (float)set.bpm); }

static void lp_note(int key)
{
    uint32_t now = plat_ms();
    if (lp.st == LP_READY && key >= 0) {
        lp.st = LP_REC; lp.t0 = now; lp.n = 0; lp.len = LP_BARS * 16;
        app_say("RECORDING", "UP TO 8 BARS - REC CLOSES");
    }
    if (lp.st == LP_REC && lp.n < LP_MAX) {
        int32_t st = (int32_t)((float)(now - lp.t0) / step_ms() + 0.5f);
        if (key < 0 && lp.n && lp.ev[lp.n - 1].key >= 0 && st <= (int32_t)lp.ev[lp.n - 1].step)
            st = lp.ev[lp.n - 1].step + 1;      /* a short tap: it still sounds a step */
        if (st < (int32_t)lp.len) { lp.ev[lp.n].step = (uint16_t)st; lp.ev[lp.n].key = (int8_t)key; lp.n++; }
    }
}

static void lp_fire(int i)
{
    lp.firing = 1;
    if (lp.ev[i].key >= 0) { held_idx = -1; play(lp.ev[i].key); }
    else if (active) stop();
    lp.firing = 0;
}

static void lp_close(uint32_t now)
{
    float sm = step_ms();
    uint32_t bars = (uint32_t)((float)(now - lp.t0) / (sm * 16.0f)) + 1u;
    if (bars > LP_BARS) bars = LP_BARS;
    lp.len = (uint16_t)(bars * 16u);
    lp.t0 += (uint32_t)((float)lp.len * sm);    /* the next round starts where the recording's bars end */
    lp.last = -1;
    lp.st = LP_PLAY;
    lp.has = 1;
    app_say("LOOP", "PLAYING - PLAY ON TOP");
    if (run.mtype == MS_LOOP) { run.mprog = 1.0f; mission_check(); }
}

static void lp_tick(uint32_t now)
{
    float sm = step_ms();
    int32_t cur;
    int i;
    if (lp.st == LP_REC && (float)(now - lp.t0) >= (float)lp.len * sm) lp_close(lp.t0 + (uint32_t)((float)lp.len * sm) - 1u);
    if (lp.st != LP_PLAY || (int32_t)(now - lp.t0) < 0) return;
    cur = (int32_t)((float)(now - lp.t0) / sm);
    while (cur >= (int32_t)lp.len) {             /* the round ends: its last events, then the next round */
        for (i = 0; i < lp.n; i++) if ((int32_t)lp.ev[i].step > lp.last) lp_fire(i);
        lp.t0 += (uint32_t)((float)lp.len * sm);
        cur -= lp.len;
        lp.last = -1;
    }
    for (i = 0; i < lp.n; i++)
        if ((int32_t)lp.ev[i].step > lp.last && (int32_t)lp.ev[i].step <= cur) lp_fire(i);
    lp.last = cur;
    run.busy_at = now;
}

static void lp_silence(void) { lp.firing = 1; if (active) stop(); lp.firing = 0; }
static void lp_start(uint32_t now) { lp.st = LP_PLAY; lp.t0 = now; lp.last = -1; app_say("LOOP", "PLAYING - PLAY ON TOP"); }

/* REC: a tap (on release); held 1 s, the loop is gone (lp_hold) */
static void lp_button(int down, uint32_t now)
{
    if (down) { lp.rec_down = now; lp.held_done = 0; return; }
    if (lp.held_done) return;
    switch (lp.st) {
    case LP_PLAY: lp_silence(); /* fall through */
    case LP_OFF: case LP_PAUSE:
        lp.st = LP_READY;
        app_say("REC", lp.has ? "PLAY A CHORD: A NEW LOOP" : "PLAY A CHORD: 8 BARS");
        break;
    case LP_READY: lp.st = lp.has ? LP_PAUSE : LP_OFF; app_say("REC", "CANCELLED"); break;
    default: lp_close(now); break;                /* recording: the loop closes at the bar */
    }
}
/* PLAY / STOP: the loop stops, or starts from its start */
static void lp_play_button(uint32_t now)
{
    switch (lp.st) {
    case LP_PLAY: lp_silence(); lp.st = LP_PAUSE; app_say("LOOP", "STOPPED - PLAY STARTS IT"); break;
    case LP_REC: lp_close(now); break;
    case LP_PAUSE: lp_start(now); break;
    default:
        if (lp.has) lp_start(now);
        else app_say("LOOP", "NONE YET - PRESS REC");
        break;
    }
}
static void lp_hold(uint32_t now)
{
    if (lp.held_done || lp.st == LP_OFF || now - lp.rec_down < 1000u) return;
    lp.held_done = 1;
    if (lp.st == LP_PLAY && active) { lp.firing = 1; stop(); lp.firing = 0; }
    lp.st = LP_OFF; lp.n = 0; lp.has = 0;
    app_say("LOOP", "CLEARED");
}

static void game_init(void)
{
    if (plat_store_load(OBJ_PROGRESS, &prog, sizeof prog) != (int)sizeof prog || prog.magic != PROG_MAGIC) {
        memset(&prog, 0, sizeof prog);
        prog.magic = PROG_MAGIC;
    }
    if (set.chr >= chars_unlocked()) set.chr = 0;
    mission_start();
}

/* the player's own chord (a key down / a stop): the loop records it, the run counts it */
static void user_chord(int key)
{
    lp_note(key);
    game_chord(key);
}

/* once a frame: the runner's speed and metres, the end of a run, the timed missions */
static void gm_advance(float px);
static void gm_chord(void);
static void gm_note(int pitch);
static void game_step(uint32_t now, float dt)
{
    int busy = active || now - run.note_at < 1200u || lp.st == LP_PLAY || lp.st == LP_REC;
    float bpm = ck_bpm > 0.0f ? ck_bpm : (float)set.bpm, vt = busy ? 5.0f * bpm / 120.0f : 0.0f, dm;
    if (busy) run.busy_at = now;
    run.speed += (vt - run.speed) * fm_minf(1.0f, dt * (busy ? 3.0f : 1.4f));
    if (!busy && run.speed < 0.3f) run.speed = 0.0f;
    dm = run.speed * dt;
    gm_advance(dm * 12.0f);                     /* the level scrolls, 12 px a metre (not while a boss fights) */
    if (busy && !run.running) { run.running = 1; run.run_m = 0.0f; }
    if (run.running) {
        run.run_m += dm;
        if (run.mtype == MS_RUN && run.run_m > run.mprog) { run.mprog = run.run_m; mission_check(); }
        if (!busy && now - run.busy_at > 3000u) {       /* the run ends */
            run.running = 0;
            if ((uint32_t)run.run_m > prog.best) {
                char t[16];
                if (run.run_m >= 50.0f) { int_text((int32_t)run.run_m, t); text_cat(t, " M"); show_toast("NEW BEST RUN", t); }
                prog.best = (uint32_t)run.run_m; prog_dirty = 1;
            }
        }
    }
    if (run.mtype == MS_TOTAL && dm > 0.0f) { run.mprog += dm; mission_check(); }
    if (run.mtype == MS_DRUMS && active && set.drums && set.rhythm) { run.mprog += dt; mission_check(); }
    run.carry += dm;
    if (run.carry >= 1.0f) { uint32_t w = (uint32_t)run.carry; run.carry -= (float)w; add_metres(w); }
}

static void button(int k)
{
    char t[8];
    int p;
    if (k == B_EDIT) {                   /* EDIT: MELODY, again: MELODY 2, again: the main page */
        set.page = (uint8_t)(set.page == PG_MELODY ? PG_MELODY2 : set.page == PG_MELODY2 ? PG_MAIN : PG_MELODY);
        dirty();
        return;
    }
    if (k == B_SEL) {                    /* SEL: CHORDS, again: CHORDS 2, again: the main page */
        set.page = (uint8_t)(set.page == PG_ACC ? PG_ACC2 : set.page == PG_ACC2 ? PG_MAIN : PG_ACC);
        dirty();
        return;
    }
    if (k == B_SEQ) {                    /* SEQ: the DRUM STUDIO, again: the main page */
        set.page = (uint8_t)(set.page == PG_STUDIO ? PG_MAIN : PG_STUDIO);
        if (set.page == PG_STUDIO) {
            if (active) { stop(); held_idx = -1; }   /* the keys are the steps now */
            if (!set.drums) { set.drums = 70; send_params(); send_drums(); }
            app_say("DRUM STUDIO", "WHITE KEYS: STEPS");
        }
        dirty();
        return;
    }
    if (set.page == PG_STUDIO && (k == B_SAVE || k == B_PLAY || k == B_REC)) {   /* the studio's own buttons */
        switch (k) {
        case B_SAVE: groove_save(); break;
        case B_PLAY:
            preview ^= 1u;
            eng_preview(preview);
            app_say("PREVIEW", preview ? "THE DRUMS ALONE" : "OFF");
            break;
        default: {                       /* REC: the track cleared */
            int i;
            for (i = 0; i < 16; i++) wk.st[set.dtrack][i] = 0;
            wk_edited = 1;
            send_drums();
            app_say("CLEARED", TRACK_NAME[set.dtrack]);
            break;
        }
        }
        dirty();
        return;
    }
    for (p = 1; p < NPAGE; p++)
        if (PAGE_BTN[p] == k) {          /* a page; the same button again: back to the main page */
            set.page = (uint8_t)(set.page == p ? PG_MAIN : p);
            dirty();
            return;
        }
    switch (k) {
    case B_HOME: ui.home_at = plat_ms(); ui.home_long = 0; return;    /* acts on release (held = help) */
    case B_SAVE:
        ui.bowl ^= 1u;
        eng_muffle(ui.bowl ? 1.0f : 0.0f);
        return;
    case B_REC:
        if ((ui.btn >> B_HOME) & 1u) return;      /* HOME + REC: the reset (input), not the loop */
        lp_button(1, plat_ms());                  /* the loop: it acts on release (held: cleared) */
        return;
    case B_PLAY: lp_play_button(plat_ms()); return;   /* PLAY / STOP: the loop */
    case B_OCTDN:
    case B_OCTUP:
        set.octave = (int8_t)clamp(set.octave + (k == B_OCTUP ? 1 : -1), -2, 2);
        t[0] = set.octave < 0 ? '-' : '+'; t[1] = (char)('0' + (set.octave < 0 ? -set.octave : set.octave)); t[2] = 0;
        app_say("OCTAVE", t);
        rechord();
        break;
    default: return;
    }
    dirty();
}

/* the keys that play a chord in the current set (a bit per key) */
static uint32_t valid_keys(void)
{
    uint32_t m = 0, k;
    int v, o;
    for (k = 0; k < NKEYS; k++)
        if (key_slot((int)k, &v, &o) >= 0) m |= 1u << k;
    return m;
}

/* ------------------------------------------------------------------ the melody guide (ASSIST: GUIDE) --- */
/* Each song gets a melody of its own, made here from the song's number (an original tune that fits, not the record's): two bars
 * of a rhythm (the second often the same, or a breath) and a line of white keys that moves mostly by step; on the strong beats it
 * lands on the chord's notes. The key to play lights up in time, the next one softly; a hit (the lit key, in its time, or just
 * before the next one) gives 2 metres. With a rhythm it follows the rhythm's own steps, without one the tempo from the chord. */
#define GD_N 128                         /* up to 8 bars */
static struct {
    int8_t w[GD_N];                             /* the white key (number) a note starts on, -1 none (a fixed tune: 0, its note in mn) */
    uint8_t mn[GD_N];                           /* a fixed tune's notes (MIDI, in the set's own key) */
    int16_t grid;                               /* the song it was made for (-1 none yet) */
    int16_t pos, next_pos, hit_pos;             /* the note under way, the next one (steps in the bars), the last hit */
    int8_t key, next_key;                       /* lit, lit softly (-1 none) */
    uint8_t soon;                               /* the next note is a moment away (an early press counts for it) */
    uint8_t len, fixed;                         /* its length in steps, a fixed tune */
    uint32_t hits;                              /* the lit keys hit since the start */
} gd = {{0}, {0}, -1, -1, -1, -1, -1, -1, 0, 32, 0, 0};
/* BLOCK DROP's GUIDE: Korobeiniki, the Russian folk song (1861, public domain) the falling-blocks game made famous, in A minor:
 * (step in 8 bars of 16, MIDI note) */
static const uint8_t KORO[][2] = {
    {0, 76}, {4, 71}, {6, 72}, {8, 74}, {12, 72}, {14, 71}, {16, 69}, {20, 69}, {22, 72}, {24, 76}, {28, 74}, {30, 72},
    {32, 71}, {38, 72}, {40, 74}, {44, 76}, {48, 72}, {52, 69}, {56, 69},
    {66, 74}, {70, 77}, {72, 81}, {76, 79}, {78, 77}, {80, 76}, {86, 72}, {88, 76}, {92, 74}, {94, 72},
    {96, 71}, {100, 71}, {102, 72}, {104, 74}, {108, 76}, {112, 72}, {116, 69}, {120, 69},
};
static const char *const GD_RHY[8] = {          /* one bar: x a note */
    "x...x.x...x.x...", "x.x...x.x...x...", "..x.x...x.x.x...", "x...x...x.x.x.x.",
    "x..x..x...x.x...", "x.x.x...x...x.x.", "...x..x.x...x...", "x...x.x.x.......",   /* (the last: a breath at the end) */
};
static uint32_t gd_rng;
static uint32_t gd_rand(void) { gd_rng ^= gd_rng << 13; gd_rng ^= gd_rng >> 17; gd_rng ^= gd_rng << 5; return gd_rng; }

static void guide_make(int grid)
{
    static const int8_t MV[8] = {-2, -1, -1, 0, 1, 1, 2, -1};
    int i, b, w, ra;
    for (i = 0; i < GD_N; i++) { gd.w[i] = -1; gd.mn[i] = 0; }
    gd.grid = (int16_t)grid;
    gd.fixed = GRID_DEF[grid].guide;
    if (gd.fixed) {                             /* a tune of its own */
        for (i = 0; i < (int)(sizeof KORO / sizeof KORO[0]); i++) { gd.w[KORO[i][0]] = 0; gd.mn[KORO[i][0]] = KORO[i][1]; }
        gd.len = 128;
        return;
    }
    gd.len = 32;
    gd_rng = 0x9E3779B9u ^ ((uint32_t)grid * 0x85EBCA6Bu + 0x1234567u);
    for (i = 0; i < 6; i++) gd_rand();
    w = 7 + (int)(gd_rand() % 3u);              /* around the octave above the tonic */
    ra = (int)(gd_rand() % 7u);
    for (b = 0; b < 2; b++) {
        const char *r = GD_RHY[b == 0 ? ra : (gd_rand() & 1u) ? ra : 7];
        for (i = 0; i < 16; i++) {
            int st = b * 16 + i;
            if (r[i] != 'x') { gd.w[st] = -1; continue; }
            if (st) w += MV[gd_rand() % 8u];
            if (w < 4) w = 5;
            if (w > 12) w = 11;
            gd.w[st] = (int8_t)w;
        }
    }
    gd.grid = (int16_t)grid;
}

/* the melody key that plays MIDI note m now, or the nearest with its pitch class (-1: none) */
static int key_for_pitch(int m)
{
    int k, best = -1, bd = 999;
    for (k = 0; k < NKEYS; k++) {
        int p, d;
        if (!mel_key(k)) continue;
        p = (int)key_pitch(k);
        if (p == m) return k;
        if (((p - m) % 12 + 12) % 12) continue;
        d = p > m ? p - m : m - p;
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}
/* the key a guide note lights: a fixed tune's note where it is now; else on a strong beat (or a bar's first note) the
 * nearest white key on the chord's notes */
static int guide_key(int pos)
{
    int w = gd.w[pos], i;
    if (gd.fixed) return key_for_pitch((int)gd.mn[pos] + (int)set.key - (int)GRID_DEF[set.grid].key);
    static const int8_t D[5] = {0, -1, 1, -2, 2};
    if (pos % 4 == 0 || pos == 0 || pos == 16) {
        uint16_t ct = chord_tones(last_slot);
        for (i = 0; i < 5; i++) {
            int ww = w + D[i];
            if (ww >= 0 && ww < 16 && ((ct >> ((int)mel_pitch(ww) % 12)) & 1u)) { w = ww; break; }
        }
    }
    return kb_wkey[w];
}

static void guide_tick(uint32_t now)
{
    static int last_cur = -1, last_slot_g = -1, last_grid = -1;
    int st = 0, cur, i;
    float fr = 0.0f;
    uint32_t bar = 0;
    if (set.assist != 2 || !active || last_slot < 0) {
        gd.key = gd.next_key = -1; gd.pos = gd.next_pos = -1; gd.soon = 0; last_cur = -1;
        return;
    }
    if (gd.grid != set.grid) guide_make(set.grid);
    if (!eng_pos(&st, &fr, &bar)) {             /* no rhythm: by the tempo, from the chord's start */
        float sm = step_ms(), t = (float)(now - chord_at) / sm;
        uint32_t ti = (uint32_t)t;
        st = (int)(ti % 16u); bar = ti / 16u; fr = t - (float)ti;
    }
    cur = (int)((bar % (uint32_t)(gd.len / 16u)) * 16u) + st;
    if (cur == last_cur && last_slot == last_slot_g && set.grid == last_grid) {   /* the same step: only "soon" moves */
        gd.soon = gd.next_pos == (cur + 1) % gd.len && fr > 0.55f;
        return;
    }
    last_cur = cur; last_slot_g = last_slot; last_grid = set.grid;
    gd.key = gd.next_key = -1; gd.pos = gd.next_pos = -1; gd.soon = 0;
    for (i = 0; i < 4; i++) {                   /* the note under way: the last start, at most 4 steps back */
        int p = (cur - i + gd.len) % gd.len;
        if (gd.w[p] >= 0) { gd.pos = (int16_t)p; break; }
    }
    for (i = 1; i <= 8; i++) {                  /* the next one */
        int p = (cur + i) % gd.len;
        if (gd.w[p] >= 0) { gd.next_pos = (int16_t)p; gd.soon = i == 1 && fr > 0.55f; break; }
    }
    if (gd.pos >= 0) gd.key = (int8_t)guide_key(gd.pos);
    if (gd.next_pos >= 0) gd.next_key = (int8_t)guide_key(gd.next_pos);
}

/* a white key pressed: the lit one (or the next, a moment early) is a hit */
static void guide_press(int k, uint32_t now)
{
    int p = -1;
    if (set.assist != 2 || !active) return;
    if (gd.pos >= 0 && k == gd.key && gd.hit_pos != gd.pos) p = gd.pos;
    else if (gd.soon && gd.next_pos >= 0 && k == gd.next_key && gd.hit_pos != gd.next_pos) p = gd.next_pos;
    if (p < 0) return;
    gd.hit_pos = (int16_t)p;
    gd.hits++;
    hop_at = now;
    add_metres(2u);
    if (run.mtype == MS_GUIDE) { run.mprog += 1.0f; mission_check(); }
}

static uint32_t as_at, as_keys;          /* ASSIST: when its keys were found, and they */
static uint32_t bf_at, bf_keys;          /* the melody's black keys that fit the chord: when they were found, and they */
static int bf_slot = -2, bf_grid = -1;
/* A melody black key fits the chord when its note is one of the chord's, or one of the melody's scale over it (the white
 * keys' notes) with no chord note a semitone away (that rubs: F over E, Db over Bb major's D). The notes outside the scale
 * (Bb over C, D over F minor: colour at best) stay dark: in BLINDING LIGHTS too many lit that did not fit */
static uint32_t black_fit_keys(void)
{
    int k, slot = last_slot;
    uint32_t m = 0;
    uint16_t ct, sc;
    if (slot < 0 || slot >= (int)grid_n[set.grid]) slot = 0;
    ct = chord_tones(slot);
    sc = mel_scale(slot);
    for (k = 0; k < NKEYS; k++) {
        int pc;
        if (!mel_black(k)) continue;
        pc = (((int)key_pitch(k) % 12) + 12) % 12;
        if (((ct >> pc) & 1u) || (((sc >> pc) & 1u) && !((ct >> ((pc + 11) % 12)) & 1u) && !((ct >> ((pc + 1) % 12)) & 1u)))
            m |= 1u << k;
    }
    return m;
}

/* ------------------------------------------------------------------ backup and restore over USB (fm1-base backup.c) --- */
static uint8_t bk_hold;                  /* a backup streams: no saving */
int app_backup_playing(void) { return active || preview || lp.st == LP_PLAY || lp.st == LP_REC; }
void app_backup_flush(void)
{
    if (set_dirty && plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) set_dirty = 0;
    if (prog_dirty) { prog.magic = PROG_MAGIC; if (plat_store_save(OBJ_PROGRESS, &prog, sizeof prog) == 0) prog_dirty = 0; }
    if (user_dirty) { user.magic = DRUMS_MAGIC; if (plat_store_save(OBJ_DRUMS, &user, sizeof user) == 0) user_dirty = 0; }
}
void app_backup_hold(int on) { bk_hold = (uint8_t)(on != 0); }
static void settings_load(void);
void app_backup_restored(void)           /* everything from the flash again, as at power-on */
{
    user_load();
    settings_load();
    game_init();
    send_sound(); send_rhythm(); send_groove(); send_cfx(); send_drums(); send_bass(); send_fx(); send_params();
}
static int as_slot = -1, as_grid = -1;
static void input(void)
{
    uint32_t keys = plat_keys(), btn = plat_buttons(), k, lit_keys = 0, lit_btn = 0, mid_keys = 0, now = plat_ms();
    int32_t e;
    clock_follow(now);
    if (!kb_ready) kb_init();
    if (set.page == PG_STUDIO) {                                /* the studio: the first 16 keys are the steps */
        for (k = 0; k < NKEYS; k++)                             /* the white keys, left to right: steps 1..16 */
            if ((keys >> k & 1u) && !(ui.keys >> k & 1u) && kb_white[k] >= 0 && kb_white[k] < 16) studio_tap(kb_white[k]);
        if (ld_held) { ld_held = 0; ld_cur = -1; eng_lead(-1.0f); }
    } else
    for (k = 0; k < NKEYS; k++) {                               /* the white keys: the melody voice (the last key pressed sounds) */
        uint32_t bit = 1u << k;
        if ((keys & bit) && !(ui.keys & bit) && mel_key((int)k)) {      /* (a white key, or a black one with no chord) */
            ld_held |= bit; ld_cur = (int)k; hop_at = now; eng_lead(key_pitch((int)k));
            game_note();
            gm_note((int)key_pitch((int)k));                     /* a coin appears ahead, high as the note */
            guide_press((int)k, now);
        } else if (!(keys & bit) && (ui.keys & bit) && (ld_held & bit)) {
            ld_held &= ~bit;
            if (ld_cur == (int)k) {
                int j;
                ld_cur = -1;
                for (j = NKEYS - 1; j >= 0; j--) if (ld_held & (1u << j)) { ld_cur = j; break; }
                if (ld_cur >= 0) eng_lead(key_pitch(ld_cur)); else eng_lead(-1.0f);
            }
        }
    }
    for (k = 0; k < NKEYS && set.page != PG_STUDIO; k++) {
        uint32_t bit = 1u << k;
        int kv, ko;
        if (key_slot((int)k, &kv, &ko) < 0) continue;            /* a silent key */
        if ((keys & bit) && !(ui.keys & bit)) {
            /* with a rhythm the chord keeps playing: the same key again stops it */
            if (set.rhythm && active && held_idx < 0 && cur_idx == (int)k) { stop(); user_chord(-1); }
            else { held_idx = (int)k; play((int)k); user_chord((int)k); }
        } else if (!(keys & bit) && (ui.keys & bit) && held_idx == (int)k) {
            held_idx = -1;
            if (!set.rhythm) { stop(); user_chord(-1); }
        }
    }
    for (k = 0; k < NB; k++) {
        uint32_t bit = 1u << k;
        if ((btn & bit) && !(ui.btn & bit)) button((int)k);
    }
    if (preview && set.page != PG_STUDIO) { preview = 0; eng_preview(0); }   /* out of the studio: no preview */
    /* the loop: REC let go (a tap), REC held (cleared), its steps */
    if (set.page != PG_STUDIO) {
        if (!((btn >> B_REC) & 1u) && ((ui.btn >> B_REC) & 1u) && !((btn >> B_HOME) & 1u) && !ui.reset_at) lp_button(0, now);
        if (((btn >> B_REC) & 1u) && !((btn >> B_HOME) & 1u)) lp_hold(now);
    }
    lp_tick(now);
    guide_tick(now);
    /* HOME + REC held 3 s: back to the settings of the first start (a countdown; let go before: nothing) */
    if (((btn >> B_HOME) & 1u) && ((btn >> B_REC) & 1u)) {
        uint32_t held;
        char t[4];
        if (!ui.reset_at) { ui.reset_at = now | 1u; ui.reset_shown = 0; ui.reset_done = 0; }
        lp.held_done = 1;                         /* (REC let go after this: no loop tap) */
        ui.home_long = 1; ui.help = 0;
        held = now - ui.reset_at;
        if (!ui.reset_done && held >= 3000u) {
            uint8_t page = PG_MAIN;
            ui.reset_done = 1;
            if (active) stop();
            settings_defaults();
            set.page = page;
            ui.bowl = 0;
            eng_muffle(0.0f);
            send_sound(); send_rhythm(); send_groove(); send_cfx(); send_drums(); send_bass(); send_fx(); send_params();
            dirty();
            app_say("RESET", "FIRST SETTINGS BACK");
        } else if (!ui.reset_done && ui.reset_shown != 1u + held / 1000u) {
            ui.reset_shown = (uint8_t)(1u + held / 1000u);
            t[0] = (char)('0' + 4 - ui.reset_shown); t[1] = 0;
            app_say("RESET IN", t);
        }
    } else if (ui.reset_at) {
        if (!ui.reset_done) app_say("RESET", "CANCELLED");
        ui.reset_at = 0;
    }
    /* HOME: a short press, back to the main page; held, the help screen while held */
    if ((btn >> B_HOME) & 1u) {
        if (!ui.home_long && now - ui.home_at > 600u) { ui.home_long = 1; ui.help = 1; ui.help_drawn = 0; }
        if (ui.reset_at) ui.help = 0;
    } else if ((ui.btn >> B_HOME) & 1u) {
        if (ui.home_long) { ui.help = 0; ui.help_drawn = 0; }
        else if (set.page != PG_MAIN) { set.page = PG_MAIN; dirty(); }
    }
    for (k = 0; k < NE; k++)
        if ((e = plat_enc((int)k)) != 0)
            encoder((int)k, e);
    ui.keys = keys;
    ui.btn = btn;
    /* auto-save: 4 s after the last change, and never while a chord plays (a flash write stops the sound) */
    if (set_dirty && !bk_hold && !active && lp.st != LP_PLAY && plat_ms() - dirty_at > 4000u) {
        if (plat_store_save(OBJ_SETTINGS, &set, sizeof set) == 0) set_dirty = 0;
        else dirty_at = plat_ms();
    }
    /* the user grooves: at once when nothing plays (a flash write stops the sound for a moment) */
    if (user_dirty && !bk_hold && !active && lp.st != LP_PLAY && lp.st != LP_REC && now - dirty_at > 300u) {
        user.magic = DRUMS_MAGIC;
        if (plat_store_save(OBJ_DRUMS, &user, sizeof user) == 0) user_dirty = 0;
        else dirty_at = now;
    }
    /* the progress too: after 4 s of quiet (no chord, no melody, no loop) */
    if (prog_dirty && !bk_hold && !active && !ld_held && lp.st != LP_PLAY && lp.st != LP_REC && now - run.busy_at > 4000u) {
        prog.magic = PROG_MAGIC;
        if (plat_store_save(OBJ_PROGRESS, &prog, sizeof prog) == 0) prog_dirty = 0;
        else run.busy_at = now;
    }
    /* lights: the keys glow, the one playing is lit; the page's button, the fun ones that are on, the pressed one */
    if (set.page == PG_STUDIO) {                 /* the studio: the track's steps (accents lit, the rest soft), the step playing */
        uint32_t at;
        int ps = eng_last_step(&at, 0);
        for (k = 0; k < 16u; k++) {                             /* on the white keys only */
            uint8_t v = wk.st[set.dtrack][k];
            uint32_t b = 1u << (uint32_t)kb_wkey[k];
            if (v == 2) lit_keys |= b; else if (v) mid_keys |= b;
        }
        if (ps >= 0 && ps < 16 && (preview || active) && now - at < 400u) {
            uint32_t b = 1u << (uint32_t)kb_wkey[ps];
            if (lit_keys & b) lit_keys &= ~b; else lit_keys |= b;
        }
        lit_btn |= 1u << B_SEQ;
        if (preview) lit_btn |= (1u << B_PLAY) | LED_PLAY_GREEN;
        if (wk_edited && (now / 400u) % 2u) lit_btn |= 1u << B_SAVE;
        lit_btn |= btn & ((1u << 14) - 1u);
        {   /* only the 16 white keys glow: the black ones stay dark */
            uint32_t wm = 0;
            for (k = 0; k < 16u; k++) wm |= 1u << (uint32_t)kb_wkey[k];
            plat_lights(lit_btn, lit_keys, 0, wm, 1);
        }
        plat_lights_mid(((1u << 14) - 1u) & ~lit_btn, mid_keys & ~lit_keys);
        return;
    }
    lit_keys = valid_keys();
    for (k = 0; k < NKEYS; k++) if (mel_key((int)k) && ((keys >> k) & 1u)) lit_keys |= 1u << k;   /* a melody key lights while pressed */                                    /* the keys that have a chord glow; a latched chord blinks */
    if (cur_idx >= 0 && held_idx < 0 && (now / 300u) % 2u) lit_keys &= ~(1u << (uint32_t)cur_idx);
    if (set.page != PG_MAIN) lit_btn |= 1u << PAGE_BTN[set.page];
    if (ui.bowl) lit_btn |= 1u << B_SAVE;
    if (lp.st == LP_PLAY || ((lp.st == LP_REC || lp.st == LP_READY) && (now / (lp.st == LP_REC ? 200u : 500u)) % 2u))
        lit_btn |= 1u << B_REC;
    if (lp.st == LP_PLAY) lit_btn |= (1u << B_PLAY) | LED_PLAY_GREEN;   /* the loop plays */
    /* ASSIST: while a chord plays, the white keys on its notes light up, fully on each beat's first half, softer after */
    if (set.assist == 2) {                      /* GUIDE: the key to play, the next one softly */
        if (gd.key >= 0) lit_keys |= 1u << (uint32_t)gd.key;
        if (gd.next_key >= 0 && gd.next_key != gd.key) mid_keys |= 1u << (uint32_t)gd.next_key;
    } else if (set.assist && active && last_slot >= 0) {
        uint16_t ct = chord_tones(last_slot);
        float bpm = ck_bpm > 0.0f ? ck_bpm : (float)set.bpm;
        uint32_t beat = (uint32_t)(60000.0f / bpm), on = beat ? (now - chord_at) % beat < beat / 2u : 1u;
        float bp = eng_beat();                      /* with a rhythm: on the rhythm's own beat */
        if (bp >= 0.0f) on = bp < 0.5f;
        if (now - as_at > 30u || as_slot != last_slot || as_grid != set.grid) {   /* (the notes, ~30 times a second) */
            as_at = now; as_slot = last_slot; as_grid = set.grid; as_keys = 0;
            for (k = 0; k < NKEYS; k++)
                if (kb_white[k] >= 0 && ((ct >> ((int)mel_pitch(kb_white[k]) % 12)) & 1u)) as_keys |= 1u << k;
        }
        if (on) lit_keys |= as_keys; else mid_keys |= as_keys & ~lit_keys;
    }
    /* the melody's black keys (right of the chords): softly lit when their note fits the chord the melody follows, dark when it
     * rubs (better not) */
    if (now - bf_at > 30u || bf_slot != last_slot || bf_grid != set.grid) {
        bf_at = now; bf_slot = last_slot; bf_grid = set.grid;
        bf_keys = black_fit_keys();
    }
    mid_keys |= bf_keys & ~lit_keys;
    /* OCT- / OCT+: lit on the side the octave went, blinking at two octaves */
    if (set.octave) {
        uint32_t ob = 1u << (set.octave < 0 ? B_OCTDN : B_OCTUP);
        if (set.octave == 1 || set.octave == -1 || (now / 300u) % 2u) lit_btn |= ob;
    }
    lit_btn |= btn & ((1u << 14) - 1u);  /* a button lights fully while it is pressed */
    plat_lights(lit_btn, lit_keys, 0, (1u << NKEYS) - 1u, 1);
    /* the 12 buttons and OCT- / OCT+ always softly on (one LED colour each: the active one fully lit, the others mid) */
    plat_lights_mid(((1u << 14) - 1u) & ~lit_btn, mid_keys);
}

void app_input_only(void) { input(); }

/* ------------------------------------------------------------------ screen (240 x 240) --- */
/* The pixel look (below) is drawn in two halves (240 x 120) into the canvas and sent, ~25 times a second. At the start, and while
 * HOME is held: the intro (the title, the credits, the player's progress). */
static inline float clamp01(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }

static struct {
    float title, title_ms;                       /* the intro: shown (0..1), its time (ms) */
    uint32_t start_at;
    uint8_t started;
} sc;

static void scene_init(void) { memset(&sc, 0, sizeof sc); }


/* ---- once a frame: the intro at the start (3 s, then it fades), and while HOME is held */
static void scene_step(uint32_t now, float dt)
{
    uint32_t since;
    float tt;
    if (!sc.started) { sc.started = 1; sc.start_at = now; }     /* the intro counts from the first frame */
    since = now - sc.start_at;
    tt = ui.help ? 1.0f : since < 3000u ? 1.0f : since < 3500u ? 1.0f - (float)(since - 3000u) / 500.0f : 0.0f;
    sc.title_ms = ui.help ? (float)(now - ui.home_at - 600u) : (float)since;
    sc.title += (tt - sc.title) * fm_minf(1.0f, dt * 8.0f);
    if (tt >= 1.0f && since < 3000u) sc.title = 1.0f;
}

/* ---- the info bands, crisp over the picture */
static uint16_t page_col(void) { const uint8_t *c = PAGE_RGB[set.page]; return RGB(c[0], c[1], c[2]); }

/* ===================================================================================================== the shooter ===
 * An early-80s arcade shooter, its own: a black sky with a dotted grid rushing towards us in perspective and drifting dots,
 * a formation of pixel invaders swaying at the top, some diving down at the ship in arcs and firing, a saucer crossing now
 * and then. The ship at the bottom flies by itself: it dodges what comes at it and lines up under the invaders; it fires
 * on every key you play (a chord: a double shot, a melody note: one). A wave shot down, the next comes in new colours: six
 * palettes of the era, turning with the waves and with the time played. One screen for every page.
 * The game lives in its own 240 x 152 space, drawn into the view (screen rows 23..173; in the DRUM STUDIO 112..185,
 * squeezed, the sprites small). (x, y) in screen rows 0..239; g_oy the screen row of the canvas' first row (0 or 120);
 * everything drawn through px_rect, clipped to the canvas and to the rows clip0..clip1. */
static int g_oy, clip0 = 0, clip1 = 240;
static void px_rect(int x, int y, int w, int h, uint16_t c)
{
    int y0 = y < clip0 ? clip0 : y, y1 = y + h > clip1 ? clip1 : y + h;
    if (y0 < g_oy) y0 = g_oy;
    if (y1 > g_oy + 120) y1 = g_oy + 120;
    if (x < 0) { w += x; x = 0; }
    if (x + w > 240) w = 240 - x;
    if (y1 <= y0 || w <= 0) return;
    cv_rect(x, y0 - g_oy, w, y1 - y0, c);
}
#define NSR(x, y, w, h, c) px_rect((x), (y), (w), (h), (c))
#define C_YEL RGB(255, 214, 40)
#define C_ORA RGB(255, 140, 20)
#define C_RED RGB(232, 48, 40)
#define C_CYA RGB(90, 220, 255)
#define C_WHI RGB(248, 248, 248)
#define C_BLK RGB(0, 0, 0)
#define C_NAVY RGB(16, 20, 64)
#define C_GREY RGB(168, 176, 200)
static uint32_t hash32(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }

/* ---- the palettes: the grid, the drifting dots, the four rows of invaders, the ship's accent, the words */
#define NPAL 6
static const struct { const char *name; uint8_t grid[3], dot1[3], dot2[3], row[4][3], acc[3], hud[3]; } PAL[NPAL] = {
    {"RADAR", {40, 110, 255}, {230, 40, 40}, {40, 110, 255}, {{255, 220, 60}, {255, 90, 40}, {255, 90, 40}, {255, 60, 120}},
     {255, 60, 120}, {90, 230, 220}},
    {"VECTOR", {40, 220, 80}, {40, 220, 80}, {255, 255, 255}, {{255, 255, 255}, {230, 60, 230}, {230, 60, 230}, {255, 220, 40}},
     {255, 220, 40}, {80, 255, 120}},
    {"NEON", {230, 40, 200}, {90, 230, 255}, {230, 40, 200}, {{255, 255, 120}, {90, 230, 255}, {90, 230, 255}, {255, 140, 40}},
     {90, 230, 255}, {255, 120, 230}},
    {"AMBER", {255, 150, 0}, {255, 200, 60}, {160, 80, 0}, {{255, 255, 200}, {255, 170, 30}, {255, 170, 30}, {230, 70, 20}},
     {255, 230, 120}, {255, 180, 40}},
    {"ICE", {0, 210, 220}, {255, 255, 255}, {0, 120, 255}, {{255, 255, 255}, {255, 110, 180}, {255, 110, 180}, {255, 150, 40}},
     {255, 110, 180}, {120, 255, 255}},
    {"ULTRA", {150, 60, 255}, {90, 255, 120}, {150, 60, 255}, {{255, 255, 80}, {90, 255, 120}, {90, 255, 120}, {255, 80, 80}},
     {255, 255, 80}, {200, 160, 255}},
};
static uint16_t rgb3(const uint8_t *c) { return RGB(c[0], c[1], c[2]); }

/* ---- the game's state (its own space: 240 x 152) */
#define VH 152
#define FR 4                                     /* the formation: 4 rows of 8 */
#define FC 8
#define SHIP_Y 138
#define GM_SHOT 12
#define GM_EB 6
#define GM_DIVE 3
#define GM_PART 40
static struct {
    uint8_t inited;
    float t, grid, sway;                         /* time, the grid's run, the formation's sway */
    float sx, svx;                               /* the ship: x, its speed */
    uint8_t alive[FR][FC];                       /* 0 shot down, 1 in the formation, 2 diving */
    struct { float x, y, vx, vy, t; uint8_t on, r, c, fired; } dv[GM_DIVE];
    struct { float x, y; uint8_t on; } shot[GM_SHOT];
    struct { float x, y, vy; uint8_t on; } eb[GM_EB];
    struct { float x, y, vx, vy, life; uint16_t c; } part[GM_PART];
    float ufo_x, ufo_t; uint8_t ufo_on;
    float dive_t, hit, busy_t, refill;
    uint32_t score, hi, wave, banner_at;
    uint8_t pal;
} gm;

static void gm_burst(float x, float y, uint16_t c, int n)
{
    int i, k = 0;
    for (i = 0; i < GM_PART && k < n; i++) {
        uint32_t h = hash32((uint32_t)i * 77u + (uint32_t)plat_ms() * 13u + (uint32_t)k);
        if (gm.part[i].life > 0.0f) continue;
        gm.part[i].x = x; gm.part[i].y = y;
        gm.part[i].vx = (float)((int)(h % 160u) - 80);
        gm.part[i].vy = (float)((int)((h >> 8) % 160u) - 80);
        gm.part[i].life = 0.35f + (float)((h >> 16) % 30u) * 0.01f; gm.part[i].c = c;
        k++;
    }
}
static float form_x(int c) { return 120.0f + ((float)c - 3.5f) * 22.0f + gm.sway; }
static float form_y(int r) { return 34.0f + (float)r * 13.0f; }
static void gm_points(uint32_t p)
{
    gm.score += p;
    if (gm.score > gm.hi) { gm.hi = gm.score; prog.rsv[2] = gm.hi; prog_dirty = 1; }
}
static void gm_wave(void)
{
    int r, c;
    for (r = 0; r < FR; r++) for (c = 0; c < FC; c++) gm.alive[r][c] = 1;
    gm.banner_at = plat_ms();
}

/* a key played: the ship fires (a chord two shots side by side, a melody note one) */
static void gm_fire(int two)
{
    int i, n = two ? 2 : 1;
    for (i = 0; i < GM_SHOT && n; i++)
        if (!gm.shot[i].on) {
            gm.shot[i].on = 1;
            gm.shot[i].x = gm.sx + (two ? (n == 2 ? -5.0f : 5.0f) : 0.0f);
            gm.shot[i].y = SHIP_Y - 10.0f;
            n--;
        }
}
static void gm_chord(void) { gm_fire(1); }
static void gm_note(int pitch) { (void)pitch; gm_fire(0); }
static void gm_advance(float px) { gm.grid += px * 0.004f; }   /* the grid runs as you play */

/* once a frame */
static void gm_step(uint32_t now, float dt)
{
    int i, r, c, left = 0, busy = run.speed > 0.5f;
    float threat = 0.0f, tx = 120.0f;
    if (!gm.inited) { gm.inited = 1; gm.sx = 120.0f; gm.hi = prog.rsv[2]; gm_wave(); gm.banner_at = 0; }
    gm.t += dt;
    gm.sway = 34.0f * fm_sin_turns(gm.t * 0.07f);
    if (busy) gm.busy_t += dt;
    gm.pal = (uint8_t)((gm.wave + (uint32_t)(gm.busy_t / 45.0f)) % NPAL);
    for (r = 0; r < FR; r++) for (c = 0; c < FC; c++) left += gm.alive[r][c] != 0;
    if (!left) {                                                        /* a wave down: the next, in new colours */
        if (gm.refill <= 0.0f) {
            gm.refill = 1.6f;
            gm.wave++;
            gm_points(500u);
            app_say("WAVE CLEAR", "+500");
        }
        gm.refill -= dt;
        if (gm.refill <= 0.0f) gm_wave();
    }
    /* divers: one leaves the formation now and then (while you play), swoops down at the ship, fires once */
    gm.dive_t -= dt;
    if (busy && gm.dive_t <= 0.0f) {
        gm.dive_t = 1.6f + (float)(hash32((uint32_t)(gm.t * 100.0f)) % 100u) * 0.02f;
        for (i = 0; i < GM_DIVE; i++)
            if (!gm.dv[i].on) {
                uint32_t h = hash32((uint32_t)(gm.t * 1000.0f) + 7u);
                int k;
                for (k = 0; k < FR * FC; k++) {
                    int rr = (int)((h + (uint32_t)k * 5u) % FR), cc = (int)(((h >> 8) + (uint32_t)k * 3u) % FC);
                    if (gm.alive[rr][cc] == 1) {
                        gm.alive[rr][cc] = 2;
                        gm.dv[i].on = 1; gm.dv[i].r = (uint8_t)rr; gm.dv[i].c = (uint8_t)cc; gm.dv[i].fired = 0;
                        gm.dv[i].x = form_x(cc); gm.dv[i].y = form_y(rr); gm.dv[i].t = 0.0f;
                        gm.dv[i].vx = gm.dv[i].x < 120.0f ? -40.0f : 40.0f;
                        break;
                    }
                }
                break;
            }
    }
    for (i = 0; i < GM_DIVE; i++) {
        if (!gm.dv[i].on) continue;
        gm.dv[i].t += dt;
        if (gm.dv[i].t < 0.5f) { gm.dv[i].vy = -30.0f + gm.dv[i].t * 160.0f; }   /* a loop up, then down */
        else { gm.dv[i].vy = 75.0f; gm.dv[i].vx += ((gm.sx - gm.dv[i].x) * 0.9f - gm.dv[i].vx) * dt * 1.5f; }
        gm.dv[i].x += gm.dv[i].vx * dt; gm.dv[i].y += gm.dv[i].vy * dt;
        if (!gm.dv[i].fired && gm.dv[i].y > 84.0f) {
            int e;
            gm.dv[i].fired = 1;
            for (e = 0; e < GM_EB; e++) if (!gm.eb[e].on) { gm.eb[e].on = 1; gm.eb[e].x = gm.dv[i].x; gm.eb[e].y = gm.dv[i].y + 6.0f; gm.eb[e].vy = 95.0f; break; }
        }
        if (gm.dv[i].y > VH + 12) {                                     /* passed: back into the formation */
            gm.dv[i].on = 0;
            if (gm.alive[gm.dv[i].r][gm.dv[i].c] == 2) gm.alive[gm.dv[i].r][gm.dv[i].c] = 1;
        }
    }
    /* the saucer crosses now and then */
    gm.ufo_t -= dt;
    if (!gm.ufo_on && busy && gm.ufo_t <= 0.0f) { gm.ufo_on = 1; gm.ufo_x = -16.0f; gm.ufo_t = 18.0f; }
    if (gm.ufo_on) { gm.ufo_x += 40.0f * dt; if (gm.ufo_x > 256.0f) gm.ufo_on = 0; }
    /* shots */
    for (i = 0; i < GM_SHOT; i++) {
        int hit = 0;
        if (!gm.shot[i].on) continue;
        gm.shot[i].y -= 300.0f * dt;
        if (gm.shot[i].y < 0.0f) { gm.shot[i].on = 0; continue; }
        for (r = 0; r < FR && !hit; r++)
            for (c = 0; c < FC && !hit; c++)
                if (gm.alive[r][c] == 1 && fm_fabsf(gm.shot[i].x - form_x(c)) < 8.0f && fm_fabsf(gm.shot[i].y - form_y(r)) < 7.0f) {
                    gm.alive[r][c] = 0; hit = 1;
                    gm_burst(form_x(c), form_y(r), rgb3(PAL[gm.pal].row[r]), 7);
                    gm_points(r == 0 ? 30u : r < 3 ? 20u : 10u);
                }
        for (c = 0; c < GM_DIVE && !hit; c++)
            if (gm.dv[c].on && fm_fabsf(gm.shot[i].x - gm.dv[c].x) < 9.0f && fm_fabsf(gm.shot[i].y - gm.dv[c].y) < 8.0f) {
                gm.dv[c].on = 0; gm.alive[gm.dv[c].r][gm.dv[c].c] = 0; hit = 1;
                gm_burst(gm.dv[c].x, gm.dv[c].y, C_WHI, 6);
                gm_burst(gm.dv[c].x, gm.dv[c].y, rgb3(PAL[gm.pal].row[gm.dv[c].r]), 6);
                gm_points(80u);
            }
        if (!hit && gm.ufo_on && fm_fabsf(gm.shot[i].x - gm.ufo_x) < 12.0f && gm.shot[i].y < 12.0f) {
            gm.ufo_on = 0; hit = 1;
            gm_burst(gm.ufo_x, 6.0f, rgb3(PAL[gm.pal].acc), 12);
            gm_points(300u);
            app_say("SAUCER", "+300");
        }
        if (hit) gm.shot[i].on = 0;
    }
    /* the invaders' shots; the ship flashes when one gets through (nothing lost) */
    for (i = 0; i < GM_EB; i++) {
        if (!gm.eb[i].on) continue;
        gm.eb[i].y += gm.eb[i].vy * dt;
        if (gm.eb[i].y > VH) { gm.eb[i].on = 0; continue; }
        if (gm.eb[i].y > SHIP_Y - 30.0f && gm.eb[i].y < SHIP_Y + 8.0f && fm_fabsf(gm.eb[i].x - gm.sx) < 30.0f) {
            float d = 1.0f - fm_fabsf(gm.eb[i].x - gm.sx) / 30.0f;
            if (d > threat) { threat = d; tx = gm.eb[i].x; }
        }
        if (gm.eb[i].y > SHIP_Y - 6.0f && gm.eb[i].y < SHIP_Y + 6.0f && fm_fabsf(gm.eb[i].x - gm.sx) < 9.0f) {
            gm.eb[i].on = 0; gm.hit = 0.4f;
            gm_burst(gm.sx, SHIP_Y, C_WHI, 5);
        }
    }
    for (i = 0; i < GM_DIVE; i++)
        if (gm.dv[i].on && gm.dv[i].y > SHIP_Y - 50.0f && fm_fabsf(gm.dv[i].x - gm.sx) < 26.0f) {
            float d = 1.0f - fm_fabsf(gm.dv[i].x - gm.sx) / 26.0f;
            if (d > threat) { threat = d; tx = gm.dv[i].x; }
        }
    if (gm.hit > 0.0f) gm.hit -= dt;
    /* the ship flies by itself: away from a threat, else under the lowest invader nearest it */
    {
        float target = gm.sx, best = 999.0f, v;
        if (threat > 0.0f) target = tx > gm.sx ? (tx - 40.0f < 16.0f ? tx + 40.0f : tx - 40.0f) : (tx + 40.0f > 224.0f ? tx - 40.0f : tx + 40.0f);
        else {
            for (c = 0; c < FC; c++)
                for (r = FR - 1; r >= 0; r--)
                    if (gm.alive[r][c] == 1) {
                        float d = fm_fabsf(form_x(c) - gm.sx);
                        if (d < best) { best = d; target = form_x(c); }
                        break;
                    }
            if (!busy) target = 120.0f + 30.0f * fm_sin_turns(gm.t * 0.05f);
        }
        if (target < 12.0f) target = 12.0f;
        if (target > 228.0f) target = 228.0f;
        v = (target - gm.sx) * 4.0f;
        if (v > 150.0f) v = 150.0f;
        if (v < -150.0f) v = -150.0f;
        gm.svx += (v - gm.svx) * fm_minf(1.0f, dt * 8.0f);
        gm.sx += gm.svx * dt;
    }
    for (i = 0; i < GM_PART; i++)
        if (gm.part[i].life > 0.0f) {
            gm.part[i].x += gm.part[i].vx * dt; gm.part[i].y += gm.part[i].vy * dt;
            gm.part[i].life -= dt;
        }
    (void)now;
}

/* ---- the sprites: rows of letters, a colour each ('.' none); drawn in blocks of sc x sc */
static uint16_t spr_pal[4];
static uint16_t spr_col(char c)
{
    switch (c) {
    case 'K': return C_BLK;
    case 'W': return C_WHI;
    case 'Y': return C_YEL;
    case 'O': return C_ORA;
    case 'R': return C_RED;
    case '1': return spr_pal[1];
    case '2': return spr_pal[2];
    case '3': return spr_pal[3];
    default: return spr_pal[0];
    }
}
static void spr_draw(const char *const *rows, int w, int h, int x, int y, int sc)
{
    int i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            char c = rows[j][i];
            if (c == '.' || !c) continue;
            NSR(x + i * sc, y + j * sc, sc, sc, spr_col(c));
        }
}
/* the invaders (8 x 7), three kinds, two frames each; '1' their row's colour, '2' white */
static const char *const INV[3][2][7] = {
    {{"2......2", ".2.11.2.", ".111111.", "11K11K11", "11111111", "1.1..1.1", "1......1"},
     {"2......2", ".2.11.2.", ".111111.", "11K11K11", "11111111", ".1.11.1.", "..1..1.."}},
    {{"...11...", ".111111.", "11K11K11", "11111111", ".1.11.1.", "1.1..1.1", "........"},
     {"...11...", ".111111.", "11K11K11", "11111111", ".1.11.1.", ".1....1.", "........"}},
    {{"..1..1..", "...11...", ".111111.", "11K11K11", "11111111", ".1....1.", "........"},
     {"..1..1..", "...11...", ".111111.", "11K11K11", "11111111", "1......1", "........"}},
};
/* the ship (11 x 9); '1' white, '2' the palette's accent, '3' the flame */
static const char *const SHIP[9] = {
    ".....1.....", ".....1.....", "....111....", "....121....", "...11211...", ".1.11111.1.", "11111211111",
    "1.2.111.2.1", "....3.3....",
};
static const char *const UFO[5] = {"....1111....", "..11111111..", ".1K11K11K11.", "111111111111", "..2.2..2.2.."};
/* the ten ships' colours (SEQ: the unlocked ones) */
static const uint8_t SHIP_COL[10][3] = {{248, 248, 248}, {90, 220, 255}, {255, 214, 40}, {255, 110, 180}, {120, 255, 120},
                                        {255, 140, 20}, {200, 140, 255}, {255, 80, 80}, {160, 255, 240}, {255, 255, 160}};

/* ---- the view: the sky, its grid and dots, the invaders, the saucer, shots, the ship, the sparks */
static int view_y(int vt, int vb, float y) { return vt + (int)(y * (float)(vb - vt) / (float)VH); }
static void draw_game(int vt, int vb, uint32_t now)
{
    int oc0 = clip0, oc1 = clip1, i, r, c, sc = vb - vt >= 120 ? 2 : 1, H = vb - vt;
    uint16_t gc = rgb3(PAL[gm.pal].grid), d1 = rgb3(PAL[gm.pal].dot1), d2 = rgb3(PAL[gm.pal].dot2);
    int fr = (int)(gm.t * 2.0f) & 1;
    clip0 = vt; clip1 = vb + 1;
    NSR(0, vt, 240, H + 1, C_BLK);
    /* the grid: dotted lines coming towards us (faster as they come), and dotted lines to the vanishing point */
    for (i = 0; i < 8; i++) {
        float z = (float)i / 8.0f + gm.grid - (float)(int)gm.grid, zz;
        int y, x;
        if (z >= 1.0f) z -= 1.0f;
        zz = z * z;
        y = vt + 10 + (int)(zz * (float)(H - 10));
        for (x = (y * 3) % 6; x < 240; x += 6) NSR(x, y, 2 + (zz > 0.5f), 1, gc);
    }
    for (i = -4; i <= 4; i++) {
        int y;
        for (y = vt + 10; y < vb; y += 5) {
            float k = (float)(y - vt - 10) / (float)(H - 10);
            NSR(120 + (int)((float)i * (12.0f + 30.0f * k)) - (i < 0), y, 2, 2, gc);
        }
    }
    for (i = 0; i < 26; i++) {                                         /* the drifting dots */
        uint32_t h = hash32((uint32_t)i * 2654435761u + 3u);
        int x = (int)(h % 240u), y = vt + (int)(((h >> 9) % (uint32_t)H + (uint32_t)(gm.grid * (float)H * 0.6f) * (1u + (h & 1u))) % (uint32_t)H);
        NSR(x, y, 2, 2, (h >> 3) & 1u ? d1 : d2);
    }
    /* the formation */
    for (r = 0; r < FR; r++) {
        spr_pal[1] = rgb3(PAL[gm.pal].row[r]); spr_pal[2] = C_WHI;
        for (c = 0; c < FC; c++)
            if (gm.alive[r][c] == 1)
                spr_draw(INV[r == 0 ? 0 : r < 3 ? 1 : 2][fr], 8, 7, (int)form_x(c) - 4 * sc, view_y(vt, vb, form_y(r)) - 3 * sc, sc);
    }
    for (i = 0; i < GM_DIVE; i++)
        if (gm.dv[i].on) {
            int rr = gm.dv[i].r;
            spr_pal[1] = rgb3(PAL[gm.pal].row[rr]); spr_pal[2] = C_WHI;
            spr_draw(INV[rr == 0 ? 0 : rr < 3 ? 1 : 2][(int)(gm.t * 6.0f) & 1], 8, 7, (int)gm.dv[i].x - 4 * sc,
                     view_y(vt, vb, gm.dv[i].y) - 3 * sc, sc);
        }
    if (gm.ufo_on) {
        spr_pal[1] = rgb3(PAL[gm.pal].acc); spr_pal[2] = (now / 100u) % 2u ? C_WHI : C_RED;
        spr_draw(UFO, 12, 5, (int)gm.ufo_x - 6 * sc / 2 * 2, view_y(vt, vb, 6.0f), sc);
    }
    for (i = 0; i < GM_SHOT; i++)
        if (gm.shot[i].on) { NSR((int)gm.shot[i].x, view_y(vt, vb, gm.shot[i].y), 2, 6 * sc / 2 + 2, C_WHI);
                             NSR((int)gm.shot[i].x, view_y(vt, vb, gm.shot[i].y) + 2, 2, 2, rgb3(PAL[gm.pal].hud)); }
    for (i = 0; i < GM_EB; i++)
        if (gm.eb[i].on) {
            int y = view_y(vt, vb, gm.eb[i].y), z = (int)(gm.t * 12.0f) & 1;
            NSR((int)gm.eb[i].x - 1 + z, y, 2, 2, C_YEL); NSR((int)gm.eb[i].x - z, y + 2, 2, 2, C_RED); NSR((int)gm.eb[i].x - 1 + z, y + 4, 2, 2, C_YEL);
        }
    {   /* the ship */
        int ch = set.chr < 10 ? set.chr : 0;
        spr_pal[1] = gm.hit > 0.0f && (now / 60u) % 2u ? C_RED : rgb3(SHIP_COL[ch]);
        spr_pal[2] = rgb3(PAL[gm.pal].acc);
        spr_pal[3] = (now / 50u) % 2u ? C_YEL : C_ORA;
        spr_draw(SHIP, 11, 9, (int)gm.sx - 11 * sc / 2, view_y(vt, vb, (float)SHIP_Y) - 4 * sc, sc);
    }
    for (i = 0; i < GM_PART; i++)
        if (gm.part[i].life > 0.0f) NSR((int)gm.part[i].x, view_y(vt, vb, gm.part[i].y), 2, 2, gm.part[i].life > 0.2f ? gm.part[i].c : C_RED);
    if (gm.banner_at && now - gm.banner_at < 1800u && (now / 200u) % 4u) {   /* the wave's number */
        char t[24], n[12];
        int32_t w, y = view_y(vt, vb, 80.0f);
        t[0] = 0; text_cat(t, "WAVE "); int_text((int32_t)(gm.wave + 1), n); text_cat(t, n);
        w = text_w(&FONT_S, t);
        if (y >= g_oy && y + 16 <= g_oy + 120) cv_text((240 - w) / 2, y - g_oy, &FONT_S, t, rgb3(PAL[gm.pal].hud));
    }
    clip0 = oc0; clip1 = oc1;
}

/* a text shortened (from its end) until it fits w pixels */
static void fit_text(char *b, const felucca_font_t *f, int32_t w)
{
    int n = 0;
    while (b[n]) n++;
    while (n > 1 && text_w(f, b) > w) b[--n] = 0;
}
/* a box's frame (canvas rows) */
static void box(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c); cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c); cv_rect(x + w - 1, y, 1, h, c);
}
/* a raised console button: black outline, a light top and left, a dark bottom and right (canvas rows) */
static void bevel(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t face, uint16_t hi, uint16_t lo)
{
    cv_rect(x, y, w, h, C_BLK);
    cv_rect(x + 1, y + 1, w - 2, h - 2, face);
    cv_rect(x + 1, y + 1, w - 2, 1, hi); cv_rect(x + 1, y + 1, 1, h - 2, hi);
    cv_rect(x + 1, y + h - 2, w - 2, 1, lo); cv_rect(x + w - 2, y + 1, 1, h - 2, lo);
}
/* text with a black drop shadow (canvas rows) */
static int32_t stext(int32_t x, int32_t y, const felucca_font_t *f, const char *t, uint16_t c)
{
    cv_text(x + 1, y + 1, f, t, C_BLK);
    return cv_text(x, y, f, t, c);
}

/* ---- the info panel (the second canvas: y 0 = screen row 120): a console's dark panel, the page's tag, four knobs as
 * segment bars */
static void draw_bottom_text(void)
{
    int k;
    int32_t y0 = 189 - 120, tw = text_w(&FONT_T, PAGE_NAME[set.page]) + 10;
    uint16_t pc = page_col();
    char t[40];
    cv_rect(0, y0 - 3, 240, 60, C_BLK);
    cv_rect(0, y0 - 3, 240, 1, rgb3(PAL[gm.pal].grid));
    cv_rect(5, y0, tw + 2, 14, C_BLK);
    cv_rect(6, y0 + 1, tw, 12, pc);
    cv_text(11, y0 + 1, &FONT_T, PAGE_NAME[set.page], C_BLK);
    t[0] = 0;
    if (set.page == PG_STUDIO) {
        text_cat(t, wk_edited ? "EDITED - SAVE KEEPS IT" : drum_name[set.dpat]);
    } else {
        if (set.carp) { text_cat(t, "ARP "); text_cat(t, RATE_NAME[set.crate]); text_cat(t, " "); }
        text_cat(t, set.rhythm ? RHYTHM_NAME[set.rhythm] : "HELD");
        if (set.drums && set.rhythm) text_cat(t, " + DRUMS");
    }
    fit_text(t, &FONT_T, 232 - 16 - tw);
    cv_text(232 - text_w(&FONT_T, t), y0 + 1, &FONT_T, t, C_GREY);
    for (k = 0; k < 4; k++) {
        char v[24];
        const kdef_t *d = &KD[set.page][k];
        int32_t x = 6 + k * 58, i, seg;
        float f;
        const felucca_font_t *vf;
        if (!d->v) continue;                 /* (an empty place) */
        f = knob_text(d, v);
        vf = text_w(&FONT_S, v) <= 56 ? &FONT_S : &FONT_T;
        if (vf == &FONT_T) fit_text(v, &FONT_T, 56);
        cv_text(x, y0 + 16, &FONT_T, d->lab, C_GREY);
        stext(x, y0 + 29 + (vf == &FONT_T ? 2 : 0), vf, v, C_WHI);
        seg = (int)(f * 8.0f + 0.5f);
        for (i = 0; i < 8; i++) cv_rect(x + i * 7, y0 + 46, 6, 3, i < seg ? pc : RGB(40, 40, 48));
    }
}

#include "title_data.h"

/* the face's colour by its row (in its word): yellow, orange, red, with a light band sweeping across at t (ms) */
static uint16_t title_col(int x, int y, uint32_t t)
{
    int y0 = y < TITLE_SPLIT ? 0 : TITLE_SPLIT, y1 = y < TITLE_SPLIT ? TITLE_SPLIT : TITLE_H, k = (y - y0) * 6 / (y1 - y0 > 0 ? y1 - y0 : 1);
    int band = (int)((t / 6u) % 420u) - 90, d = x + (y - y0) - band;
    static const uint16_t G[6] = {RGB(255, 248, 160), RGB(255, 224, 48), RGB(255, 184, 24), RGB(255, 136, 16), RGB(240, 88, 24),
                                  RGB(208, 40, 32)};
    if (d > -6 && d < 6) return C_WHI;
    return G[k < 0 ? 0 : k > 5 ? 5 : k];
}

/* the title's picture at row ty (screen), t ms into the intro */
static void title_draw(int ty, uint32_t t)
{
    int x, y;
    for (y = 0; y < TITLE_H; y++) {
        int sy = ty + y;
        if (sy < g_oy || sy >= g_oy + 120) continue;
        for (x = 0; x < TITLE_W; x++) {
            uint32_t q = (uint32_t)(y * TITLE_W + x), v = (title_px[q >> 1] >> ((q & 1u) << 2)) & 15u;
            if (!v) continue;
            cv_pset(x, sy - g_oy, v == 15u ? title_col(x, y, t) : v == 1u ? C_BLK : RGB(8, 10, 48));
        }
    }
}

/* ---- the intro (and the help: HOME held): the grid rushing in, the title dropping in with a bounce, the ship coming up and
 * firing */
static void px_splash(int bottom)
{
    float ms = sc.title_ms;
    uint32_t now = plat_ms(), t = (uint32_t)(ms < 0.0f ? 0.0f : ms);
    int ty, i;
    char ln[40];
    g_oy = bottom ? 120 : 0;
    clip0 = 0; clip1 = 240;
    gm.grid = (float)now * 0.0006f;
    gm.pal = (uint8_t)((now / 4000u) % NPAL);
    gm.t = (float)now * 0.001f;
    {   /* the sky, the grid and the dots, the ship (draw_game without invaders: none in the formation yet) */
        uint8_t keep[FR][FC];
        float sx = gm.sx;
        for (i = 0; i < FR * FC; i++) { keep[i / FC][i % FC] = gm.alive[i / FC][i % FC]; gm.alive[i / FC][i % FC] = 0; }
        gm.sx = 120.0f + 50.0f * fm_sin_turns((float)now * 0.0003f);
        draw_game(0, 239, now);
        gm.sx = sx;
        for (i = 0; i < FR * FC; i++) gm.alive[i / FC][i % FC] = keep[i / FC][i % FC];
    }
    for (i = 0; i < 3; i++) {                                           /* its shots */
        int y = 200 - (int)(((now + (uint32_t)i * 180u) % 540u) * 200u / 540u);
        NSR(120 + (int)(50.0f * fm_sin_turns((float)now * 0.0003f)), y, 2, 6, C_WHI);
    }
    {   /* the title drops in with a bounce */
        float kk = clamp01((ms - 250.0f) / 700.0f), q;
        kk = 1.0f - (1.0f - kk) * (1.0f - kk) * (1.0f - kk);
        q = 1.0f - kk;
        ty = 24 - (int)(q * 120.0f) + (int)(8.0f * fm_sin_turns(q * 1.5f) * q);
        title_draw(ty & ~1, t);
        if (ms > 900.0f) {                                     /* by swtdr34mz, in the title's type */
            int bx = (240 - BY_W) / 2, by = ty + TITLE_H + 2, xx, yy;
            for (yy = 0; yy < BY_H; yy++) {
                if (by + yy < g_oy || by + yy >= g_oy + 120) continue;
                for (xx = 0; xx < BY_W; xx++)
                    if (title_by[yy * BY_W + xx]) { cv_pset(bx + xx + 1, by + yy + 1 - g_oy, C_BLK); cv_pset(bx + xx, by + yy - g_oy, C_WHI); }
            }
        }
    }
    if (t > 1100u && !ui.help && bottom) {                                      /* the credit */
        cv_rect(0, 226 - 120, 240, 14, C_BLK);
        ln[0] = 0; text_cat(ln, (t / 3000u) % 2u ? "TITLE TYPE: PRESS START 2P" : "BASED ON BLOP BY OBSIMO");
        cv_text((240 - text_w(&FONT_XS, ln)) / 2, 227 - 120, &FONT_XS, ln, C_GREY);
    }
    if (ui.help && t > 600u && bottom) {                             /* the help (HOME held): the player's progress */
        char a[40], n[12];
        cv_rect(6, 166 - 120, 228, 68, C_BLK);
        box(8, 168 - 120, 224, 64, C_YEL);
        a[0] = 0; text_cat(a, "HIGH SCORE "); int_text((int32_t)gm.hi, n); text_cat(a, n);
        cv_text((240 - text_w(&FONT_XS, a)) / 2, 173 - 120, &FONT_XS, a, C_YEL);
        a[0] = 0; text_cat(a, "TOTAL "); int_text((int32_t)prog.metres, n); text_cat(a, n);
        text_cat(a, " M  BEST "); int_text((int32_t)prog.best, n); text_cat(a, n); text_cat(a, " M");
        cv_text((240 - text_w(&FONT_XS, a)) / 2, 188 - 120, &FONT_XS, a, C_WHI);
        a[0] = 0; text_cat(a, "SHIPS "); int_text(chars_unlocked(), n); text_cat(a, n);
        text_cat(a, "/10  MISSIONS "); int_text((int32_t)prog.missions, n); text_cat(a, n);
        cv_text((240 - text_w(&FONT_XS, a)) / 2, 203 - 120, &FONT_XS, a, C_WHI);
        a[0] = 0; text_cat(a, "SETS PLAYED "); int_text(popcount8(prog.songs), n); text_cat(a, n);
        text_cat(a, "/"); int_text(NGRID, n); text_cat(a, n);
        cv_text((240 - text_w(&FONT_XS, a)) / 2, 218 - 120, &FONT_XS, a, C_CYA);
    }
}

/* ---- the DRUM STUDIO's grid (the first canvas): the four tracks' 16 steps, the track being edited lit, the step playing */
static void draw_studio(uint32_t now)
{
    int t, i, ps;
    uint32_t at;
    char b[32];
    cv_rect(0, 23, 240, 89, C_BLK);
    ps = eng_last_step(&at, 0);
    if (!(preview || active) || now - at > 400u) ps = -1;
    for (t = 0; t < NTRACK; t++) {
        int32_t y = 27 + t * 18, sel = t == set.dtrack;
        if (sel) cv_rect(0, y - 2, 240, 18, RGB(40, 40, 56));
        cv_text(4, y + 1, &FONT_T, TRACK_NAME[t], sel ? C_YEL : C_GREY);
        for (i = 0; i < 16; i++) {
            int32_t x = 40 + i * 12 + (i / 4) * 2, v = wk.st[t][i];
            uint16_t on = t == TR_KICK ? C_RED : t == TR_SNARE ? C_YEL : t == TR_HAT ? C_CYA : RGB(200, 130, 255);
            cv_rect(x, y, 11, 14, C_BLK);
            if (!v) cv_rect(x + 1, y + 1, 9, 12, i % 4 == 0 ? RGB(44, 52, 112) : RGB(30, 36, 88));
            else if (v == 1) { cv_rect(x + 1, y + 1, 9, 12, RGB(30, 36, 88)); cv_rect(x + 2, y + 5, 7, 7, on); }
            else if (v == 2) { cv_rect(x + 1, y + 1, 9, 12, on); cv_rect(x + 1, y + 1, 9, 2, C_WHI); }
            else if (v == 3) { cv_rect(x + 1, y + 1, 9, 12, RGB(30, 36, 88)); box(x + 2, y + 2, 7, 10, on); }   /* open */
            else { cv_rect(x + 1, y + 1, 9, 12, RGB(30, 36, 88)); cv_rect(x + 2, y + 2, 7, 2, on); cv_rect(x + 2, y + 6, 7, 2, on);
                   cv_rect(x + 2, y + 10, 7, 2, on); }                                                            /* a roll */
            if (i == ps) box(x, y, 11, 14, C_WHI);
        }
    }
    b[0] = 0; text_cat(b, "PLAY:LISTEN  REC:CLEAR  SAVE:KEEP");
    cv_text(4, 98, &FONT_T, b, C_GREY);
}

/* ---- the header (the first canvas): the set (or a message), its key; the loop's bars under it */
static void draw_header(uint32_t now)
{
    char b[24], t[32];
    cv_rect(0, 0, 240, 22, C_BLK);
    if (now < ui.msg_until) {
        int32_t x = cv_text(8, 0, &FONT_XS, ui.msg[0], C_WHI);
        cv_text(x + 6, 0, &FONT_XS, ui.msg[1], C_YEL);
    } else if (set.page == PG_STUDIO) {
        b[0] = 0; text_cat(b, drum_name[set.dpat]); if (wk_edited) text_cat(b, " *");
        cv_text(8, 0, &FONT_XS, "DRUM STUDIO", C_YEL);
        cv_text(8, 10, &FONT_XS, b, C_WHI);
        t[0] = 0; text_cat(t, KIT_NAME[wk.kit]); text_cat(t, " KIT");
        cv_text(232 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_CYA);
    } else {
        cv_text(8, 0, &FONT_XS, GRID_DEF[set.grid].name, C_WHI);
        cv_text(8, 10, &FONT_XS, GRID_DEF[set.grid].by, C_CYA);
        t[0] = 0; text_cat(t, "KEY "); text_cat(t, NOTE[set.key]);
        cv_text(232 - text_w(&FONT_XS, t), 0, &FONT_XS, t, C_YEL);
        t[0] = 0; text_cat(t, SCALE_DEF[GRID_DEF[set.grid].scale].name);
        if (lp.st == LP_OFF && set.assist != 2) cv_text(232 - text_w(&FONT_T, t), 11, &FONT_T, t, C_GREY);
    }
    cv_rect(0, 22, 240, 1, C_YEL);
    if (lp.st == LP_OFF && set.assist == 2 && set.page != PG_STUDIO) {   /* GUIDE: the lit keys hit */
        t[0] = 0; text_cat(t, "GUIDE  HITS "); int_text((int32_t)gd.hits, b); text_cat(t, b);
        cv_text(232 - text_w(&FONT_T, t), 11, &FONT_T, t, C_YEL);
    }
    if (lp.st != LP_OFF) {   /* the loop: its state at the right, under the key; its bars on the line, the one playing filling */
        uint16_t lc = lp.st == LP_REC || lp.st == LP_READY ? C_RED : lp.st == LP_PLAY ? RGB(96, 232, 72) : C_GREY;
        float pos = 0.0f;
        int nb = lp.st == LP_REC || (lp.st == LP_READY && !lp.has) ? LP_BARS : lp.len / 16, bi;
        int32_t sw;
        t[0] = 0;
        if (lp.st == LP_READY) text_cat(t, (now / 500u) % 2u ? "REC READY: PLAY A CHORD" : "");
        else if (lp.st == LP_PAUSE) text_cat(t, "LOOP STOPPED: PRESS PLAY");
        else {
            int32_t el = (int32_t)(now - lp.t0);
            float stp = step_ms();
            pos = el < 0 ? 0.0f : (float)el / (stp * (float)lp.len);
            if (pos > 1.0f) pos = 1.0f;
            text_cat(t, lp.st == LP_REC ? "REC " : "LOOP ");
            int_text(el < 0 ? 1 : 1 + (int32_t)((float)el / (stp * 16.0f)), b); text_cat(t, b);
            text_cat(t, "/"); int_text(lp.st == LP_REC ? LP_BARS : lp.len / 16, b); text_cat(t, b);
        }
        if (t[0]) cv_text(232 - text_w(&FONT_T, t), 11, &FONT_T, t, lc);
        if (nb < 1) nb = 1;
        sw = (224 - (nb - 1) * 2) / nb;
        for (bi = 0; bi < nb; bi++) {
            float f = pos * (float)nb - (float)bi;
            int32_t bx = 8 + bi * (sw + 2);
            f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
            cv_rect(bx, 21, sw, 3, lp.st == LP_PAUSE ? RGB(80, 88, 120) : RGB(24, 28, 72));
            if (f > 0.0f) cv_rect(bx, 21, (int32_t)((float)sw * f), 3, lc);
        }
    }
}

/* text in screen rows, only where it fits this canvas whole */
static int32_t hud_text(int32_t x, int y, const felucca_font_t *f, const char *t, uint16_t c, int h)
{
    if (y < g_oy || y + h > g_oy + 120) return x + text_w(f, t);
    return cv_text(x, y - g_oy, f, t, c);
}
/* six digits, as the arcades counted */
static void score_text(char *b, uint32_t v)
{
    int i;
    for (i = 5; i >= 0; i--) { b[i] = (char)('0' + v % 10u); v /= 10u; }
    b[6] = 0;
}
/* ---- the arcade's words over the view: the score, the high score, the wave; the chord; the mission */
static void draw_hud(int vt, uint32_t now)
{
    char b[24], t[40], m[40], pr[12];
    uint16_t hc = rgb3(PAL[gm.pal].hud);
    int32_t x, w;
    hud_text(6, vt + 1, &FONT_T, "SCORE", hc, 12);
    score_text(b, gm.score); hud_text(42, vt + 1, &FONT_T, b, C_WHI, 12);
    hud_text(98, vt + 1, &FONT_T, "HI", hc, 12);
    score_text(b, gm.hi); hud_text(114, vt + 1, &FONT_T, b, C_WHI, 12);
    t[0] = 0; text_cat(t, "WAVE "); int_text((int32_t)(gm.wave + 1), b); text_cat(t, b);
    hud_text(234 - text_w(&FONT_T, t), vt + 1, &FONT_T, t, C_YEL, 12);
    if (set.page == PG_STUDIO) return;
    if (active) hud_text(6, vt + 12, &FONT_S, chord_txt, C_WHI, 16);
    mission_text(m, pr);
    fit_text(m, &FONT_T, 140);
    w = text_w(&FONT_T, m) + (pr[0] ? text_w(&FONT_T, pr) + 5 : 0);
    x = hud_text(234 - w, vt + 13, &FONT_T, m, C_GREY, 12);
    if (pr[0]) hud_text(x + 5, vt + 13, &FONT_T, pr, C_YEL, 12);
    (void)now;
}

/* ---- the chords (screen rows 174..185): a row of their names, the one playing lit */
static void draw_chords(void)
{
    int n = grid_n[set.grid], cs = -1, cvar = 0, i;
    char b[20];
    int32_t w = 240 / (n > 0 ? n : 1);
    if (174 < g_oy || 186 > g_oy + 120) return;
    cv_rect(0, 174 - g_oy, 240, 12, C_BLK);
    cv_rect(0, 174 - g_oy, 240, 1, rgb3(PAL[gm.pal].grid));
    if (active && cur_idx >= 0) { int o; cs = key_slot(cur_idx, &cvar, &o); }
    for (i = 0; i < n; i++) {
        int sel = i == cs;
        chord_t c = slot_chord(i, 0, 0);
        const char *s;
        int32_t tw;
        chord_name(&c, b);
        s = sel ? chord_txt : b;
        tw = text_w(&FONT_XS, s);
        if (sel) cv_rect(i * w + 1, 175 - g_oy, w - 2, 11, C_YEL);
        cv_text(i * w + (w - tw) / 2, 175 - g_oy, &FONT_XS, s, sel ? C_BLK : C_WHI);
    }
}

/* ---- the screen: bottom = 0: the upper canvas (screen rows 0..119), 1: the lower (120..239) */
static void draw_screen(int bottom)
{
    uint32_t now = plat_ms();
    int vt = set.page == PG_STUDIO ? 112 : 23, vb = set.page == PG_STUDIO ? 185 : 173;
    g_oy = bottom ? 120 : 0;
    clip0 = 0; clip1 = 240;
    draw_game(vt, vb, now);
    if (!bottom) {
        draw_header(now);
        if (set.page == PG_STUDIO) draw_studio(now);
    }
    draw_hud(vt, now);
    if (bottom && set.page != PG_STUDIO) draw_chords();
    if (now < toast.until && !bottom) {   /* a celebration: a new ship, a mission done */
        uint32_t age = now - toast.at;
        int32_t bw = 204, bh = 52, bx = 18, by = 40 + (age < 200u ? (int32_t)((200u - age) / 10u) & ~1 : 0);
        uint16_t bc = (now / 150u) % 2u ? C_YEL : C_RED;
        cv_rect(bx, by, bw, bh, C_BLK);
        cv_rect(bx + 2, by + 2, bw - 4, bh - 4, bc);
        cv_rect(bx + 4, by + 4, bw - 8, bh - 8, C_BLK);
        stext(120 - text_w(&FONT_S, toast.a) / 2, by + 9, &FONT_S, toast.a, bc);
        stext(120 - text_w(&FONT_XS, toast.b) / 2, by + 31, &FONT_XS, toast.b, C_WHI);
    }
}


/* the first screen at boot (the platform's splash, while the FM-1 starts): the intro's very first picture (the title is
 * still above the screen), so the start runs on into the animation without any other screen */
void app_splash(void)
{
    scene_init();
    sc.title = 1.0f; sc.title_ms = 0.0f;
    cv_begin(240, 120, 0);
    px_splash(0);
    cv_blit(0, 0);
    cv_begin(240, 120, 0);
    px_splash(1);
    cv_blit(0, 120);
}

void app_ui_init(void) { scene_init(); }

void app_frame(void)
{
    uint32_t now = plat_ms();
    input();
    if (now - ui.frame_at >= 40u) {
        float dt = ui.frame_at ? (float)(now - ui.frame_at) * 0.001f : 0.04f;
        if (dt > 0.1f) dt = 0.1f;
        ui.frame_at = now;
        scene_step(now, dt);
        game_step(now, dt);
        gm_step(now, dt);
        {
            int over = sc.title < 0.5f;     /* the minimal screen; the intro and the help keep the old picture */
            cv_begin(240, 120, 0);
            if (over) draw_screen(0);
            else px_splash(0);
            cv_blit(0, 0);
            cv_begin(240, 120, 0);
            if (over) { draw_screen(1); draw_bottom_text(); }
            else px_splash(1);
            cv_blit(0, 120);
        }
    }
    ann_tick(now);
}

#ifdef PLAT_HOST
int app_expect(const char *what, int *got)
{
    if (!strcmp(what, "grid")) *got = set.grid;
    else if (!strcmp(what, "key")) *got = set.key;
    else if (!strcmp(what, "lead")) *got = ld_cur >= 0;
    else if (!strcmp(what, "sound")) *got = set.sound;
    else if (!strcmp(what, "rhythm")) *got = set.rhythm;
    else if (!strcmp(what, "chord")) *got = cur_idx;
    else if (!strcmp(what, "active")) *got = active;
    else if (!strcmp(what, "rich")) *got = set.rich;
    else if (!strcmp(what, "octave")) *got = set.octave;
    else if (!strcmp(what, "dirty")) *got = set_dirty;
    else if (!strcmp(what, "bpm")) *got = set.bpm;
    else if (!strcmp(what, "syncbpm")) *got = (int)(ck_bpm + 0.5f);
    else if (!strcmp(what, "side")) *got = set.side;
    else if (!strcmp(what, "page")) *got = set.page;
    else if (!strcmp(what, "drums")) *got = set.drums;
    else if (!strcmp(what, "help")) *got = ui.help;
    else if (!strcmp(what, "bowl")) *got = ui.bowl;
    else if (!strcmp(what, "cutoff")) *got = set.cutoff;
    else if (!strcmp(what, "delay")) *got = set.delay;
    else if (!strcmp(what, "bsound")) *got = set.bsound;
    else if (!strcmp(what, "reverb")) *got = set.reverb;
    else if (!strncmp(what, "mel", 3) && what[3] >= '0' && what[3] <= '9') *got = (int)mel_pitch(atoi(what + 3));   /* a white key's note now */
    else if (!strcmp(what, "black_fit")) *got = (int)black_fit_keys();
    else if (!strncmp(what, "kp", 2) && what[2] >= '0' && what[2] <= '9')    /* a key's melody note now (-1: a chord key) */
        *got = mel_key(atoi(what + 2)) ? (int)key_pitch(atoi(what + 2)) : -1;
    else if (!strcmp(what, "loop")) *got = lp.st;
    else if (!strcmp(what, "assist")) *got = set.assist;
    else if (!strcmp(what, "carp")) *got = set.carp;
    else if (!strcmp(what, "cbass")) *got = set.cbass;
    else if (!strcmp(what, "crate")) *got = set.crate;
    else if (!strcmp(what, "preview")) *got = preview;
    else if (!strcmp(what, "dtrack")) *got = set.dtrack;
    else if (!strcmp(what, "dslot")) *got = set.dslot;
    else if (!strcmp(what, "dkit")) *got = wk.kit;
    else if (!strcmp(what, "edited")) *got = wk_edited;
    else if (!strncmp(what, "step", 4) && what[4] >= '0' && what[4] <= '9') *got = wk.st[set.dtrack][atoi(what + 4) & 15];
    else if (!strncmp(what, "used", 4) && what[4] >= '0' && what[4] <= '9') *got = user.g[atoi(what + 4) % NUSER].used;
    else if (!strcmp(what, "crush")) *got = set.crush;
    else if (!strcmp(what, "mfollow")) *got = set.mfollow;
    else if (!strcmp(what, "assist_keys")) *got = (int)as_keys;
    else if (!strcmp(what, "guide_key")) *got = gd.key;
    else if (!strcmp(what, "guide_next")) *got = gd.next_key;
    else if (!strcmp(what, "guide_hits")) *got = (int)gd.hits;
    else if (!strcmp(what, "guide_tap")) {      /* (a test's finger on the lit key: 1 = a hit) */
        uint32_t h = gd.hits;
        if (gd.key >= 0) guide_press(gd.key, plat_ms());
        *got = (int)(gd.hits - h);
    }
    else if (!strcmp(what, "loopn")) *got = lp.n;
    else if (!strcmp(what, "metres")) *got = (int)prog.metres;
    else if (!strcmp(what, "metres_h")) *got = (int)(prog.metres / 100u);   /* (hundreds: the run's own metres vary) */
    else if (!strcmp(what, "best_h")) *got = (int)(prog.best / 100u);
    else if (!strcmp(what, "running")) *got = run.running;
    else if (!strcmp(what, "unlocked")) *got = chars_unlocked();
    else if (!strcmp(what, "missions")) *got = (int)prog.missions;
    else if (!strcmp(what, "mission")) *got = run.mtype;
    else if (!strcmp(what, "character")) *got = set.chr;
    else if (!strcmp(what, "dpat")) *got = set.dpat;
    else if (!strcmp(what, "msound")) *got = set.msound;
    else if (!strcmp(what, "mrev")) *got = set.mrev;
    else if (!strcmp(what, "glide")) *got = set.glide;
    else return 0;
    return 1;
}
#endif
