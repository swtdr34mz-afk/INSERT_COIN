/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine (PIXEL RUN: the chip waveforms, drums and bitcrusher). See engine.h. Single precision only (the FM-1's FPU), no double.
 * The render works in chunks of 32 frames: pitches, the filter coefficient and the LFOs are updated per chunk,
 * envelopes, oscillators and effects per sample. */
#include "engine.h"
#include "fastmath.h"
#include "plat.h"

#define SR ENG_SR
#define CUT_MID_HZ 916.5f     /* app.c cutoff_hz() at CUTOFF 50 %: 70 Hz * (12000 / 70) ^ 0.5 */

/* no libc on the device: small copies and clears of our own */
static void zero(void *d, uint32_t n) { uint8_t *p = (uint8_t *)d; while (n--) *p++ = 0; }
static void copy(void *d, const void *s, uint32_t n)
{
    uint8_t *p = (uint8_t *)d;
    const uint8_t *q = (const uint8_t *)s;
    while (n--) *p++ = *q++;
}
#define CHUNK 32u
#define KS_N 2048u                      /* plucked string loop: down to ~22 Hz */
#define CH_N 2048u                      /* chorus line */
#define DL_N 28672u                     /* ping-pong delay, per side: a dotted eighth at 70 BPM (0.64 s) */
#define RV_PRE 1323u                    /* reverb pre-delay: 30 ms */
static const uint32_t RV_LEN[4] = {1427u, 1663u, 1871u, 2053u};
static const uint32_t AP_LEN[4] = {142u, 107u, 379u, 277u};

/* ------------------------------------------------------------------ big buffers (the pool on the device) --- */
static float ks_buf[4][KS_N] PLAT_POOL;
static float lks_buf[KS_N] PLAT_POOL;     /* the melody voice's plucked string */
static float ch_buf[CH_N] PLAT_POOL;
#define LR_WET0 0.6f                    /* the melody's reverb return: SPACE at its least .. at the top (0.6 + 0.6) */
#define LR_WET1 0.6f
#define LR_N 2560u                      /* the melody's reverb: 4 lines of 30..53 ms, modulated, no pre-delay */
static float lr_line[4][LR_N] PLAT_POOL;
static float lr_ap0[113] PLAT_POOL, lr_ap1[337] PLAT_POOL;
static const float LR_LEN[4] = {1327.0f, 1619.0f, 1973.0f, 2347.0f};
static int16_t dl_buf[2][DL_N] PLAT_POOL;
static float rv_pre[RV_PRE] PLAT_POOL;
static float rv_line0[1427] PLAT_POOL, rv_line1[1663] PLAT_POOL, rv_line2[1871] PLAT_POOL, rv_line3[2053] PLAT_POOL;
static float ap_buf0[142] PLAT_POOL, ap_buf1[107] PLAT_POOL, ap_buf2[379] PLAT_POOL, ap_buf3[277] PLAT_POOL;
static float *const rv_line[4] = {rv_line0, rv_line1, rv_line2, rv_line3};
static float *const ap_buf[4] = {ap_buf0, ap_buf1, ap_buf2, ap_buf3};

/* ------------------------------------------------------------------ command queue (main loop -> render) --- */
enum { CMD_SOUND, CMD_RHYTHM, CMD_CHORD, CMD_STOP, CMD_PARAMS, CMD_FX, CMD_GROOVE, CMD_DRUMS, CMD_BASS, CMD_MUFFLE,
       CMD_BLIP, CMD_PANIC, CMD_SYNC, CMD_LEAD, CMD_LSOUND, CMD_LPARAM, CMD_LMOD, CMD_CFX, CMD_PREVIEW };
typedef struct {
    uint8_t t;
    union {
        sound_t s;
        rhythm_t r;
        struct { float notes[4], bass, glide; } c;
        struct { float bpm, cut, q, side; uint8_t drums; } p;
        struct { float rev, del, trem, chorus, crush; } f;
        float m;
        int blip;
        struct { float swing, dens, gate, strum; } g;
        struct { drums_t d; float k, h, r; } d;
        struct { uint8_t sound, motif; float level, cut; } b;
        struct { int step; uint32_t at_ms; } y;   /* step: -1 = the clock is gone */
        struct { float m; } ld;
        sound_t ls;
        struct { float cut, lev, rev, del; } lq;
        struct { float glide, vib; } lm;
        struct { uint8_t arp, rate, spread; float flow, swing, width, level; } cf;
    } u;
} cmd_t;
#define QN 16u
static cmd_t cq[QN];
static volatile uint32_t cq_head, cq_tail;

static void push(const cmd_t *c)
{
    uint32_t h = cq_head, nh = (h + 1u) % QN;
    if (nh == cq_tail)                  /* full (the render is not running): drop */
        return;
    cq[h] = *c;
    __asm__ volatile("" ::: "memory");
    cq_head = nh;
}

/* ------------------------------------------------------------------ state (render side) --- */
typedef struct {
    float m, target, inc;               /* pitch (MIDI, fractional) and glide step per chunk */
    float p1, p2, pc, pm, prev, last;
    uint32_t w;                         /* pluck write position */
} voice_t;

static struct {
    sound_t p;
    rhythm_t rh;
    voice_t v[4];
    float bm, btarget, binc, bp, bs, b2;
    float cut, q, bpm, side;
    uint8_t drums_on, active, running;
    /* groove */
    float swing, dens, gatem, strum, sdel[4], sg[4];
    /* drums */
    drums_t dr;
    float dk, dh, dr_l, k_gain, h_env, h_gain, h_dec, h_lp, h_lp2, r_env, r_gain, r_ph1, r_ph2;
    float s_env, s_tenv, s_gain, s_ph, s_ph2, s_lp, s_hp, s_clapt;   /* snare / clap (s_lp, s_hp: its noise filter's state) */
    float k_hold, k_cl, k_clph;                               /* kick: hold left (samples), its click */
    float h_mph[6];                                           /* the hi-hat's six square waves */
    int s_clapn;
    float h_roll_left, h_roll_len, h_roll_g;                   /* hi-hat rolls: the hits left in the step */
    int h_roll;
    /* bass */
    uint8_t bsound, bmotif, bstage, boct;
    float blevel, bcut, be, bgate, bfenv, bic1, bic2, bg, bk;
    /* chorus, tremolo */
    float chorus, trem, tr_ph;
    /* muffle, blips */
    float muf, muf_t, ml, mr;
    float bl_env, bl_ph, bl_f, bl_df, bl_dec, bl_ph2;
    int bl_kind;
    int step;
    float step_left;
    volatile uint32_t bar;              /* bars begun (for the melody guide) */
    uint8_t clocked;                    /* an outside MIDI clock leads: the steps keep counting even with nothing playing */
    /* envelope */
    float e, vel, atk_s, rel_s;
    int stage;
    float gate_left;
    float fenv, ienv, ie_s;               /* ie_s: the FM index envelope, smoothed (a hit must not jump it: a click) */
    /* filter (left and right: the voices are spread across the stereo field) */
    float ic1, ic2, g, ic1r, ic2r;
    /* the chords' extras: arpeggiator, level, stereo spread */
    uint8_t arp, arate, spread, vmask, arp_v, rr, arp_n;
    int arp_last;
    uint32_t arp_k;
    float vg[4], width, aflow, aswing, clev, arp_left, arp_per, arp_fk, cn[4], rh_cov[16];
    /* transition fade */
    float fade_len, fade_left;
    uint8_t pending, retrig;
    float pend_notes[4], pend_bass;
    /* lfo, noise */
    float wob_ph;
    uint32_t rng;
    /* kick + sidechain */
    float k_ph, k_amp, k_pitch, k_tail, k_last, duck;
    uint8_t duck_atk;
    /* fx */
    float rev, del, ch_ph1, ch_ph2;
    uint32_t ch_w, dl_w, rv_w[4], ap_w[4], pre_w;
    float rv_lp[4], rv_g[4], rv_hpl, rv_hpr, rv_xl, rv_xr, dl_lp;
    float dl_time;
    /* clock for the screen */
    uint32_t samples;
    volatile int last_step;
    volatile uint32_t last_step_ms;
    volatile int last_kick;
    /* MIDI out (USB): the chord's and the bass's notes as they sound, channel 1 */
    uint8_t mv[4], mb, mvel;
    volatile int last_hit;
    volatile uint32_t last_hit_ms;
    float level;
    /* the melody (lead) voice: mono, the sound's own oscillators, envelope and filter */
    float ld_m, ld_ph1, ld_ph2, ld_e, ld_fe, ld_lp, ld_lp2, ld_pm, ld_prev, ld_ie, ld_last;
    uint32_t ld_w;
    float ld_ies;        /* the melody's FM index envelope, smoothed */
    uint8_t ld_exc;      /* a plucked melody: a new note to pluck */
    uint8_t ld_st;       /* 0 off, 1 attack, 2 decay / sustain, 3 release */
    sound_t lp;          /* the melody's sound, cutoff (Hz), level, reverb send */
    float lcut, llev, lrev, ldel;
    float lr_g, lr_in, lr_wet;   /* the melody's reverb: its feedback, its input (the tail as loud, short or long), its return */
    float lglide, lvib, ld_tgt, ld_vph, ld_vamt;   /* the melody's glide (s) and vibrato, its target pitch, the LFO */
    uint32_t lr_w[4], lr_apw0, lr_apw1;
    float lr_lp[4], lr_ph[4], lr_en;
    /* PIXEL RUN: the chip sounds' pitch drop (chords, melody), the PWM's phase, the bitcrusher, the drum preview,
     * the chip drums' noise (two shift registers, each with its clock) */
    float swp, ld_swp, pwm_ph, crush, cr_hold_l, cr_hold_r, cr_left;
    uint8_t preview;
    uint16_t lfsr1, lfsr2;
    float nz1_ph, nz2_ph, nz1, nz2;
} E;

static inline float mtof(float m) { return 440.0f * fm_exp2f((m - 69.0f) * (1.0f / 12.0f)); }
static inline float rnd(void)
{
    uint32_t x = E.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    E.rng = x;
    return (float)(int32_t)x * (1.0f / 2147483648.0f);
}

static inline float blep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
/* a pulse of width d (0..1), band-limited at both edges, without its DC */
static inline float pulse(float ph, float dt, float d)
{
    float s = ph < d ? 1.0f : -1.0f, p2 = ph - d;
    if (p2 < 0.0f) p2 += 1.0f;
    return s + blep(ph, dt) - blep(p2, dt) - (2.0f * d - 1.0f);
}
/* the chip waves: stepped on purpose (their grit is the sound), 4 bits */
static const uint8_t WAVE_T[32] = {8, 11, 13, 14, 15, 15, 14, 13, 12, 11, 10, 10, 9, 9, 8, 8,
                                   7, 7, 6, 6, 5, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 5};     /* warm: a rounded saw-sine */
static const uint8_t VOICE_T[32] = {15, 15, 14, 12, 9, 6, 4, 3, 3, 4, 7, 10, 12, 13, 12, 9,
                                    5, 2, 0, 0, 1, 3, 5, 6, 6, 5, 4, 4, 5, 7, 10, 13};     /* a nasal, vowel-like one */
static inline float tri4(float ph)        /* the 8-bit consoles' triangle: 16 steps up, 16 down */
{
    int k = (int)(ph * 32.0f) & 31;
    return (float)(k < 16 ? k : 31 - k) * (2.0f / 15.0f) - 1.0f;
}
static float g_pw = 0.5f;                 /* W_PWM: the pulse width this chunk */
static inline float osc(int w, float ph, float dt)
{
    switch (w) {
    case W_P12: return pulse(ph, dt, 0.125f);
    case W_P25: return pulse(ph, dt, 0.25f);
    case W_PWM: return pulse(ph, dt, g_pw);
    case W_NTRI: return tri4(ph);
    case W_WAVE: return (float)WAVE_T[(int)(ph * 32.0f) & 31] * (2.0f / 15.0f) - 1.0f;
    case W_VOICE: return (float)VOICE_T[(int)(ph * 32.0f) & 31] * (2.0f / 15.0f) - 1.0f;
    case W_CSAW: return (float)((int)(ph * 16.0f) & 15) * (2.0f / 15.0f) - 1.0f;
    case W_SAW: return 2.0f * ph - 1.0f - blep(ph, dt);
    case W_SQUARE: {
        float s = ph < 0.5f ? 1.0f : -1.0f, p2 = ph + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        return s + blep(ph, dt) - blep(p2, dt);
    }
    case W_TRI: return 1.0f - 4.0f * fm_fabsf(ph - 0.5f);
    default: return fm_sin_turns(ph);
    }
}

/* ------------------------------------------------------------------ main-loop API --- */
void eng_sound(const sound_t *p) { cmd_t c; c.t = CMD_SOUND; c.u.s = *p; push(&c); }
void eng_rhythm(const rhythm_t *r) { cmd_t c; c.t = CMD_RHYTHM; c.u.r = *r; push(&c); }
void eng_chord(const float notes[4], float bass, float glide_s)
{
    cmd_t c;
    c.t = CMD_CHORD;
    copy(c.u.c.notes, notes, sizeof c.u.c.notes);
    c.u.c.bass = bass;
    c.u.c.glide = glide_s;
    push(&c);
}
void eng_stop(void) { cmd_t c; c.t = CMD_STOP; push(&c); }
void eng_lead_sound(const sound_t *p) { cmd_t c; c.t = CMD_LSOUND; c.u.ls = *p; push(&c); }
void eng_lead_params(float cut_hz, float level, float reverb, float delay)
{
    cmd_t c;
    c.t = CMD_LPARAM; c.u.lq.cut = cut_hz; c.u.lq.lev = level; c.u.lq.rev = reverb; c.u.lq.del = delay;
    push(&c);
}
void eng_lead(float m) { cmd_t c; c.t = CMD_LEAD; c.u.ld.m = m; push(&c); }
void eng_chordfx(int arp, int rate, float flow, float swing, float width, int spread, float level)
{
    cmd_t c;
    c.t = CMD_CFX; c.u.cf.arp = (uint8_t)arp; c.u.cf.rate = (uint8_t)rate; c.u.cf.spread = (uint8_t)spread;
    c.u.cf.flow = flow; c.u.cf.swing = swing; c.u.cf.width = width; c.u.cf.level = level;
    push(&c);
}
void eng_lead_mod(float glide_s, float vibrato) { cmd_t c; c.t = CMD_LMOD; c.u.lm.glide = glide_s; c.u.lm.vib = vibrato; push(&c); }
void eng_sync(int step, uint32_t at_ms) { cmd_t c; c.t = CMD_SYNC; c.u.y.step = step; c.u.y.at_ms = at_ms; push(&c); }
void eng_panic(void) { cmd_t c; c.t = CMD_PANIC; push(&c); }
void eng_params(float bpm, float cut_hz, float q, float side, int drums_on)
{
    cmd_t c;
    c.t = CMD_PARAMS;
    c.u.p.bpm = bpm; c.u.p.cut = cut_hz; c.u.p.q = q; c.u.p.side = side; c.u.p.drums = (uint8_t)drums_on;
    push(&c);
}
void eng_fx(float reverb, float delay, float trem, float chorus, float crush)
{
    cmd_t c;
    c.t = CMD_FX; c.u.f.rev = reverb; c.u.f.del = delay; c.u.f.trem = trem; c.u.f.chorus = chorus; c.u.f.crush = crush;
    push(&c);
}
void eng_preview(int on) { cmd_t c; c.t = CMD_PREVIEW; c.u.blip = on; push(&c); }
void eng_muffle(float m) { cmd_t c; c.t = CMD_MUFFLE; c.u.m = m; push(&c); }
void eng_blip(int kind) { cmd_t c; c.t = CMD_BLIP; c.u.blip = kind; push(&c); }
void eng_groove(float swing, float density, float gate, float strum)
{
    cmd_t c;
    c.t = CMD_GROOVE; c.u.g.swing = swing; c.u.g.dens = density; c.u.g.gate = gate; c.u.g.strum = strum;
    push(&c);
}
void eng_drums(const drums_t *d, float kick, float hat, float snare)
{
    cmd_t c;
    c.t = CMD_DRUMS; c.u.d.d = *d; c.u.d.k = kick; c.u.d.h = hat; c.u.d.r = snare;
    push(&c);
}
void eng_bass(int sound, int motif, float level, float cut)
{
    cmd_t c;
    c.t = CMD_BASS; c.u.b.sound = (uint8_t)sound; c.u.b.motif = (uint8_t)motif; c.u.b.level = level; c.u.b.cut = cut;
    push(&c);
}

int eng_last_step(uint32_t *at_ms, int *kick)
{
    if (at_ms) *at_ms = E.last_step_ms;
    if (kick) *kick = E.last_kick;
    return E.last_step;
}
int eng_last_hit(uint32_t *at_ms)
{
    if (at_ms) *at_ms = E.last_hit_ms;
    return E.last_hit;
}
float eng_level(void) { return E.level; }
static float step_len(int i);
int eng_pos(int *step, float *frac, uint32_t *bar)
{
    int cur;
    float len, f;
    if (!E.running || E.last_step < 0) return 0;
    cur = E.last_step;
    len = step_len(cur);
    f = len > 0.0f ? 1.0f - E.step_left / len : 0.0f;
    *step = cur;
    *frac = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
    *bar = E.bar ? E.bar - 1u : 0u;
    return 1;
}
float eng_beat(void)
{
    int cur;
    float len, f;
    if (!E.running) return -1.0f;
    cur = (E.step + 15) & 15;                   /* the step under way (E.step is the next one) */
    len = step_len(cur);
    f = len > 0.0f ? 1.0f - E.step_left / len : 0.0f;
    f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
    return ((float)(cur & 3) + f) * 0.25f;
}

static inline float rndu(void) { return 0.5f + 0.5f * rnd(); }
static uint32_t now_ms(void) { return (uint32_t)((float)E.samples * (1000.0f / SR)); }

/* ------------------------------------------------------------------ render side --- */
static void set_notes(const float *notes, float bass, float glide_chunks)
{
    int i;
    for (i = 0; i < 4; i++) {
        voice_t *v = &E.v[i];
        E.cn[i] = notes[i];                     /* (the chord's notes, for the arpeggiator) */
        v->target = notes[i];
        if (glide_chunks < 1.0f) { v->m = notes[i]; v->inc = 0.0f; }
        else v->inc = (notes[i] - v->m) / glide_chunks;
    }
    E.btarget = bass;
    if (glide_chunks < 1.0f) { E.bm = bass; E.binc = 0.0f; }
    else E.binc = (bass - E.bm) / glide_chunks;
}

static void excite_buf(float *buf, uint32_t w, float m, float bright)
{
    float L = SR / mtof(m), lp = 0.0f, a = 0.06f + 0.42f * bright;   /* a softer pick: less of a hard click */
    uint32_t n, j;
    if (L > (float)(KS_N - 4u)) L = (float)(KS_N - 4u);
    n = (uint32_t)L + 2u;
    for (j = 0; j < n; j++) {
        uint32_t k = (w + KS_N - n + j) & (KS_N - 1u);
        lp += a * (rnd() - lp);
        buf[k] = buf[k] * 0.25f + lp * 1.4f;
    }
}
static void excite(voice_t *v, float bright) { excite_buf(ks_buf[v - E.v], v->w, v->m, bright); }

/* ------------------------------------------------------------------ MIDI out --- */
/* The notes go out as they sound: rhythm, strum, latch, glide (a glide = the new notes, legato). The chord's four
 * voices and the bass, all on channel 1, by USB (the FM-1 has no MIDI out jack). 0xFF: no note on. */
#define MIDI_NONE 0xFFu
static void midi_on(uint8_t n, uint8_t v)
{
    if (plat_midi_out_room(TO_USB) < 8u) return;      /* keep room for the note-offs we owe */
    plat_midi_send(0x09u | 0x90u << 8 | (uint32_t)n << 16 | (uint32_t)v << 24, TO_USB);
}
static void midi_off(uint8_t n) { plat_midi_send(0x08u | 0x80u << 8 | (uint32_t)n << 16 | 64u << 24, TO_USB); }
static uint8_t midi_note(float m)
{
    int n = (int)(m + 0.5f);
    return (uint8_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}
static void midi_voice_on(int i)
{
    uint8_t n = midi_note(E.v[i].target);
    if (E.mv[i] != MIDI_NONE) midi_off(E.mv[i]);
    midi_on(n, E.mvel);
    E.mv[i] = n;
}
static void midi_bass_on(void)
{
    uint8_t n;
    if (E.bsound == BASS_OFF) return;
    n = midi_note(E.btarget + (E.boct ? 12.0f : 0.0f));
    if (E.mb != MIDI_NONE) midi_off(E.mb);
    midi_on(n, E.mvel);
    E.mb = n;
}
static void midi_bass_off(void) { if (E.mb != MIDI_NONE) { midi_off(E.mb); E.mb = MIDI_NONE; } }
static void midi_chord_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        if (E.mv[i] != MIDI_NONE) { midi_off(E.mv[i]); E.mv[i] = MIDI_NONE; }
    if (E.bmotif == MOTIF_SUIT) midi_bass_off();
}
/* the chord changed under held notes (a glide): the new notes, legato: the new ones on before the old ones off (a
 * synthesizer's glide, set to legato, slides between them), the notes in both left alone */
static int midi_in_set(const uint8_t *s, int n, uint8_t x)
{
    int i;
    for (i = 0; i < n; i++) if (s[i] == x) return 1;
    return 0;
}
static void midi_retarget(void)
{
    uint8_t old[5], neu[5];
    int i, n = 0;
    for (i = 0; i < 4; i++) {
        old[i] = E.mv[i];
        neu[i] = E.mv[i] == MIDI_NONE ? MIDI_NONE : midi_note(E.v[i].target);
    }
    old[4] = E.mb;
    neu[4] = E.mb == MIDI_NONE || E.bsound == BASS_OFF ? MIDI_NONE : midi_note(E.btarget + (E.boct ? 12.0f : 0.0f));
    for (i = 0; i < 5; i++)
        if (neu[i] != MIDI_NONE && !midi_in_set(old, 5, neu[i]) && !midi_in_set(neu, i, neu[i])) { midi_on(neu[i], E.mvel); n++; }
    for (i = 0; i < 5; i++)
        if (old[i] != MIDI_NONE && !midi_in_set(neu, 5, old[i]) && !midi_in_set(old, i, old[i])) midi_off(old[i]);
    for (i = 0; i < 4; i++) E.mv[i] = neu[i];
    E.mb = neu[4];
    (void)n;
}

/* the chord held, not played by a rhythm's steps: rhythm OFF (hold), or FULL (sus: held while the transport, the drums
 * and the bass run on) */
#define RH_HELD (E.rh.hold || E.rh.sus)
static void hit(float vel, float gate_s, float atk)
{
    const sound_t *p = &E.p;
    int i;
    if (E.fade_left > 0.0f && E.pending) {
        set_notes(E.pend_notes, E.pend_bass, 0.0f);
        E.pending = 0;
        E.fade_left = 0.0f;
    }
    E.vel = vel;
    E.stage = 1;
    E.gate_left = gate_s;
    if (atk <= 0.0f)
        atk = RH_HELD ? p->atk : fm_minf(p->atk, 0.012f);   /* in a rhythm, attacks stay short */
    E.atk_s = fm_maxf(0.003f * SR, atk * SR);   /* never a step: at least 3 ms */
    E.rel_s = (RH_HELD ? p->rel : fm_minf(p->rel, 0.3f)) * SR;
    E.fenv = 1.0f;
    E.ienv = 1.0f;
    E.swp = p->sweep;                     /* a chip sound's pitch drop, from the top again */
    if (E.bmotif == MOTIF_SUIT || E.bmotif == MOTIF_TENUE) E.bfenv = 1.0f;
    E.mvel = (uint8_t)(30.0f + 97.0f * fm_minf(1.0f, vel));
    for (i = 0; i < 4; i++) {           /* strum: the voices start one after the other, low to high */
        if (!((E.vmask >> i) & 1u)) { E.sdel[i] = 0.0f; continue; }   /* (the arpeggiator: its one voice) */
        E.sdel[i] = E.strum * 0.032f * SR * (float)i;
        if (E.sdel[i] > 0.0f) { /* the voice fades out quickly in its wait (see the voice loop), no hard cut */ }
        else {
            if (p->engine == ENGINE_PLUCK) excite(&E.v[i], p->bright);
            midi_voice_on(i);
        }
    }
    if (E.bmotif == MOTIF_SUIT) midi_bass_on();
    E.last_hit = vel > 0.8f ? 1 : 2;
    E.last_hit_ms = now_ms();
}

static float step_len(int i)
{
    float sw = fm_minf(0.42f, E.rh.swing + E.swing + E.dr.swing * 0.3f), d = 60.0f / E.bpm / 4.0f * SR;
    return (i & 1) ? d * (1.0f - sw) : d * (1.0f + sw);
}

static void start_transport(void)
{
    E.running = 1;
    if (!E.clocked) { E.step = 0; E.step_left = 0.0f; return; }
    {   /* in time with the clock: the step under way, now, if it began less than 30 ms ago; else the next one */
        int prev = (E.step + 15) & 15;
        float in = step_len(prev) - E.step_left;
        if (in < 0.03f * SR) { E.step = prev; E.step_left = -in; }
    }
}

/* the transport runs while chords play a rhythm (with the drums), or the bass a motif */
static int need_run(void) { return E.preview || (E.active && (!E.rh.hold || E.bmotif >= MOTIF_CONTRE || E.arp)); }   /* rhythm OFF: no
                                                                                                         drums */
static void check_run(void)
{
    if (need_run()) { if (!E.running) start_transport(); }
    else E.running = 0;
}

/* bass motifs: x a note, o an octave up, - it lasts one more step */
static const char *const MOTIF_STR[NMOTIF] = {
    "", "", "..x-..x-..x-..x-", "x.o.x.o.x.o.x.o.", "x-------x-------", "x.xxx.xxx.xxx.xx", "x-.x-.x-..x-.x-.",
    "xxxxxxxxxxxxxxxx",
};

static void bass_hit(float gate_s, int oct)
{
    E.bstage = 1;
    E.bgate = gate_s;
    E.boct = (uint8_t)oct;
    E.bfenv = 1.0f;
    if (!E.mvel) E.mvel = 100;
    midi_bass_on();
}

/* the arpeggiator's next note (its clock: the render), period its length in samples: the chord's notes low to high over 1 to 3
 * octaves; the voices take turns, so the last notes ring on as they fade (FLOW: from a click to into the next notes) */
static const float RATE_BEATS[NARP_RATE] = {1.0f, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f,
                                            -0.0333f, -0.01667f};   /* < 0: so many seconds, whatever the tempo (the chip arpeggio) */
static void arp_fire(float period)
{
    int oct = E.arp == ARP_UP3 ? 3 : E.arp >= ARP_UP2 ? 2 : 1, total = 4 * oct, idx, v, i;
    float vel = 1.0f, pitch, tau;
    uint32_t k;
    if (!RH_HELD) {                             /* a rhythm: its steps say which notes play (the step this note falls in) */
        int st = E.step_left < 2.0f ? E.step & 15 : (E.last_step >= 0 ? E.last_step & 15 : 0);
        vel = E.rh_cov[st];
        if (vel <= 0.0f) return;
    }
    k = E.arp_k++;
    switch (E.arp) {
    case ARP_DOWN: case ARP_DOWN2: idx = total - 1 - (int)(k % (uint32_t)total); break;
    case ARP_UPDOWN: case ARP_UPDOWN2: {
        uint32_t per = (uint32_t)(2 * total - 2), q = k % per;
        idx = q < (uint32_t)total ? (int)q : (int)(per - q);
        break;
    }
    case ARP_RANDOM: idx = (int)(rndu() * 3.999f); if (idx == E.arp_last) idx = (idx + 1) & 3; break;
    default: idx = (int)(k % (uint32_t)total); break;
    }
    pitch = E.cn[idx & 3] + 12.0f * (float)(idx >> 2);
    v = (int)(k & 3u);
    for (i = 0; i < 4; i++)                     /* MIDI: the other notes off */
        if (i != v && E.mv[i] != MIDI_NONE) { midi_off(E.mv[i]); E.mv[i] = MIDI_NONE; }
    E.v[v].m = E.v[v].target = pitch; E.v[v].inc = 0.0f;
    E.arp_v = (uint8_t)v; E.arp_last = idx;
    if (E.spread == SPREAD_DIVERGE2) E.rr = (uint8_t)((E.rr + 1u) & 3u);
    E.vmask = (uint8_t)(1u << v);
    hit(vel * ((k & 3u) == 0u ? 1.0f : 0.86f), period * (E.aflow < 0.6f ? 0.2f + 0.8f * E.aflow / 0.6f : 1.05f),
        fm_maxf(0.002f, fm_minf(E.p.atk, 0.006f + 0.1f * E.aflow)));   /* short notes: a quick attack, even a pad's */
    E.vmask = 0x0Fu;
    E.rel_s = E.p.rel * SR * (0.35f + 1.3f * E.aflow);         /* a longer flow: plucks ring longer */
    tau = 0.0012f * SR + E.aflow * E.aflow * 1.6f * period;     /* how long the last notes take to fade */
    E.arp_fk = 1.0f / fm_maxf(1.0f, tau);
}

static void on_step(void)
{
    int i = E.step, kick = 0;
    if (i == 0) E.bar++;
    float sl = step_len(i);
    if (E.drums_on && ((E.active && !E.rh.hold) || E.preview)) {   /* the drums play with a rhythm only (or the studio's preview) */
        if (E.dr.k[i] > 0.0f) {
            E.k_tail = E.k_last; E.k_ph = 0.0f; E.k_amp = 1.0f; E.k_pitch = 1.0f;
            E.k_hold = E.dr.kit.k_hold * SR; E.k_cl = 1.0f; E.k_clph = 0.0f;
            E.k_gain = E.dr.k[i] * E.dk;
            E.duck_atk = 1;
            kick = 1;
        }
        if (E.dr.h[i] > 0.0f) {
            int rl = E.dr.roll[i];
            E.h_env = 1.0f;
            E.h_gain = E.dr.h[i] * E.dh * (0.85f + 0.15f * rndu());
            E.h_dec = fm_expf(-1.0f / (fm_maxf(0.003f, E.dr.open[i] ? E.dr.kit.h_open : E.dr.kit.h_closed) * SR));
            E.h_roll = 0;
            if (rl > 1) {                               /* a roll: the other hits of the step, evenly in it */
                E.h_roll = rl - 1;
                E.h_roll_len = sl / (float)rl;
                E.h_roll_left = E.h_roll_len;
                E.h_roll_g = E.h_gain;
            }
        }
        if (E.dr.s[i] > 0.0f) {
            E.s_env = 1.0f; E.s_tenv = 1.0f; E.s_gain = E.dr.s[i] * E.dr_l; E.s_ph = 0.0f; E.s_ph2 = 0.25f;
            E.s_clapn = E.dr.kit.s_clap > 0.5f ? 2 : 0;     /* a clap: two more bursts, 11 ms apart */
            E.s_clapt = 0.011f * SR;
        }
        if (E.dr.r[i] > 0.0f) { E.r_env = 1.0f; E.r_gain = E.dr.r[i] * E.dr_l; E.r_ph1 = E.r_ph2 = 0.0f; }
    }
    if (E.active && E.arp) {                         /* the arpeggiator in place of the rhythm's hits: lined up at each bar */
        if (i == 0) {
            if (E.arp_per > 0.0f && E.arp_left > E.arp_per - 8.0f) E.arp_n = 1;   /* it just played the bar's first note */
            else { E.arp_left = 0.0f; E.arp_n = 0; }
        }
    } else if (E.active && !RH_HELD) {
        float v = E.rh.v[i];
        int nlen = E.rh.len[i];
        if (v > 0.0f && E.dens < 0.0f && i != 0 && rndu() < -E.dens * 0.75f) v = 0.0f;      /* fewer hits */
        else if (v <= 0.0f && E.dens > 0.0f && (i & 1) && !(E.stage >= 1 && E.stage <= 3)
                 && rndu() < E.dens * 0.5f) { v = 0.45f; nlen = 1; }                        /* ghost hits */
        if (v > 0.0f) {
            float len = 0.0f, g;
            int k;
            for (k = 0; k < nlen; k++)
                len += step_len((i + k) & 15);
            g = fm_minf(len * 1.02f, len * E.rh.gate * E.gatem);
            hit(v * (1.0f - 0.12f * E.swing * 2.0f * rndu()), g, E.rh.atk);
        }
    }
    if (E.active && E.bmotif >= MOTIF_CONTRE) {
        const char *m = MOTIF_STR[E.bmotif];
        if (m[i] == 'x' || m[i] == 'o') {
            int k = 1;
            while (i + k < 16 && m[i + k] == '-') k++;
            bass_hit(sl * (float)k * 0.8f, m[i] == 'o');
        }
    }
    E.last_step = i;
    E.last_step_ms = now_ms();
    E.last_kick = kick;
    if (!need_run())
        E.running = 0;
}

static void chord_cmd(const float *notes, float bass, float glide)
{
    float gc = glide * SR / (float)CHUNK;
    if (E.spread == SPREAD_DIVERGE2 && !E.arp) E.rr = (uint8_t)((E.rr + 1u) & 3u);   /* round robin: each chord moves on */
    if (E.active && E.arp) {                    /* the arpeggiator: the new chord's notes for its next notes, the bass moves on */
        int i;
        for (i = 0; i < 4; i++) E.cn[i] = notes[i];
        E.btarget = bass;
        if (gc < 1.0f) { E.bm = bass; E.binc = 0.0f; } else E.binc = (bass - E.bm) / gc;
        return;
    }
    if (!E.active) {
        E.active = 1;
        set_notes(notes, bass, 0.0f);
        E.fade_left = 0.0f;
        E.pending = 0;
        E.arp_k = 0; E.arp_per = 0.0f; E.arp_left = 0.0f;
        if (RH_HELD && !E.arp) hit(1.0f, 1e9f, 0.0f);
        if (E.bmotif == MOTIF_TENUE) bass_hit(1e9f, 0);
        check_run();
        return;
    }
    if (gc >= 1.0f) {
        set_notes(notes, bass, gc); midi_retarget();
        /* held (rhythm OFF, FULL): no step strikes the chord again, so a struck sound (piano, pluck, FM keys) or one that has
         * faded is struck anew as it glides (it slid on silent: the chord did not play); a pad that holds just glides */
        if (RH_HELD && (E.p.engine != ENGINE_SUB || E.stage == 0 || E.stage == 4 || E.e < 0.05f)) hit(1.0f, 1e9f, 0.0f);
        return;
    }
    if (E.e > 0.002f) {                 /* a quick fade, then the new chord as a fresh hit */
        copy(E.pend_notes, notes, sizeof E.pend_notes);
        E.pend_bass = bass;
        E.pending = 1;
        E.retrig = E.stage >= 1 && E.stage <= 3;
        if (E.fade_left <= 0.0f) E.fade_left = E.fade_len;
    } else {
        set_notes(notes, bass, 0.0f);
        if (RH_HELD) hit(1.0f, 1e9f, 0.0f);   /* (held: the faded chord struck again, as no step will) */
    }
}

static void run_commands(void)
{
    while (cq_tail != cq_head) {
        const cmd_t *c = &cq[cq_tail];
        switch (c->t) {
        case CMD_SOUND: E.p = c->u.s; break;
        case CMD_RHYTHM:
            E.rh = c->u.r;
            {   /* the steps the rhythm covers (a note or its tie) and how strong: the arpeggiator's pattern */
                int i2, j;
                for (i2 = 0; i2 < 16; i2++) E.rh_cov[i2] = 0.0f;
                for (i2 = 0; i2 < 16; i2++)
                    if (E.rh.v[i2] > 0.0f)
                        for (j = 0; j < (E.rh.len[i2] ? E.rh.len[i2] : 1) && i2 + j < 16; j++)
                            E.rh_cov[i2 + j] = E.rh.v[i2] * (j ? 0.85f : 1.0f);
            }
            if (E.active && !E.arp) {
                if (RH_HELD) hit(1.0f, 1e9f, 0.0f);
                else E.gate_left = 0.0f;
            }
            check_run();
            break;
        case CMD_CHORD: chord_cmd(c->u.c.notes, c->u.c.bass, c->u.c.glide); break;
        case CMD_STOP:
            E.active = 0;
            if (E.stage >= 1 && E.stage <= 3) E.gate_left = 0.0f;
            if (E.bstage >= 1 && E.bstage <= 2) E.bgate = 0.0f;
            check_run();
            break;
        case CMD_PARAMS:
            E.bpm = c->u.p.bpm; E.cut = c->u.p.cut; E.q = c->u.p.q;
            E.side = c->u.p.side; E.drums_on = c->u.p.drums;
            check_run();
            E.dl_time = fm_minf(0.75f * 60.0f / E.bpm * SR, (float)(DL_N - 2u));
            break;
        case CMD_FX: E.rev = c->u.f.rev; E.del = c->u.f.del; E.trem = c->u.f.trem; E.chorus = c->u.f.chorus; E.crush = c->u.f.crush; break;
        case CMD_PREVIEW: E.preview = (uint8_t)(c->u.blip != 0); check_run(); break;
        case CMD_MUFFLE: E.muf_t = c->u.m; break;
        case CMD_BLIP:
            E.bl_kind = c->u.blip; E.bl_env = 1.0f; E.bl_ph = E.bl_ph2 = 0.0f;
            if (E.bl_kind == BLIP_PLOP) { E.bl_f = 320.0f; E.bl_df = 1.0006f; E.bl_dec = 0.99972f; }
            else { E.bl_f = 880.0f; E.bl_df = 1.0f; E.bl_dec = 0.99905f; }
            break;
        case CMD_GROOVE:
            E.swing = c->u.g.swing * 0.3f; E.dens = c->u.g.dens;
            E.gatem = 0.2f + 1.6f * c->u.g.gate; E.strum = c->u.g.strum;
            break;
        case CMD_DRUMS: E.dr = c->u.d.d; E.dk = c->u.d.k; E.dh = c->u.d.h; E.dr_l = c->u.d.r; break;
        case CMD_BASS: {
            uint8_t was = E.bmotif;
            if (c->u.b.sound != E.bsound || c->u.b.motif != E.bmotif) midi_bass_off();
            E.bsound = c->u.b.sound; E.bmotif = c->u.b.motif; E.blevel = c->u.b.level;
            E.bcut = 55.0f * fm_exp2f(c->u.b.cut * 6.6f);        /* 55 Hz .. 5.4 kHz */
            if (E.active && E.bmotif == MOTIF_TENUE && was != MOTIF_TENUE) bass_hit(1e9f, 0);
            check_run();
            break;
        }
        case CMD_LSOUND: E.lp = c->u.ls; break;
        case CMD_LPARAM:
            E.lcut = c->u.lq.cut; E.llev = c->u.lq.lev; E.lrev = c->u.lq.rev; E.ldel = c->u.lq.del;
            /* SPACE is a send: the melody itself stays as loud, the reverb is mixed in beside it. Its input is scaled so a
             * long tail holds no more energy than a short one (a long one built up to 8 dB over the melody, and the mix
             * overdrove: with chords and drums, SPACE at the top added 2 dB to everything), the return at the top about
             * as loud as the melody (the whole mix then about 0.3 dB louder) */
            E.lr_g = 0.62f + 0.3f * E.lrev;
            E.lr_in = fm_sqrtf(1.0f - E.lr_g * E.lr_g);
            E.lr_wet = E.lrev > 0.001f ? LR_WET0 + LR_WET1 * E.lrev : 0.0f;
            break;
        case CMD_LEAD:
            if (c->u.ld.m < 0.0f) { if (E.ld_st) E.ld_st = 3; }
            else {
                E.ld_tgt = c->u.ld.m;
                if (!(E.ld_st && E.lglide > 0.005f)) E.ld_m = E.ld_tgt;   /* glide only from a note that still sounds */
                E.ld_st = 1; E.ld_fe = 1.0f; E.ld_ie = 1.0f; E.ld_exc = 1; E.ld_vamt = 0.0f; E.ld_swp = E.lp.sweep;
            }
            break;
        case CMD_LMOD: E.lglide = c->u.lm.glide; E.lvib = c->u.lm.vib; break;
        case CMD_CFX: {
            uint8_t was = E.arp;
            E.arp = c->u.cf.arp < NARP ? c->u.cf.arp : 0; E.arate = c->u.cf.rate < NARP_RATE ? c->u.cf.rate : ARP_R_16;
            E.spread = c->u.cf.spread < NSPREAD ? c->u.cf.spread : 0;
            E.aflow = c->u.cf.flow; E.aswing = c->u.cf.swing; E.width = c->u.cf.width; E.clev = c->u.cf.level;
            if (E.active && was != E.arp) {         /* the arpeggiator on / off while a chord plays */
                if (!E.arp && RH_HELD) hit(1.0f, 1e9f, 0.0f);
                check_run();
            }
            break;
        }
        case CMD_PANIC: E.preview = 0; E.ld_st = 0; E.ld_e = 0.0f; E.active = 0; E.running = 0; E.e = 0.0f; E.stage = 0; midi_chord_off(); midi_bass_off(); break;
        case CMD_SYNC: {    /* a beat of the outside clock: step (even) began at_ms (plat_ms) */
            int st = c->u.y.step;
            float a, len;
            if (st < 0) { E.clocked = 0; break; }
            a = (float)(plat_ms() - c->u.y.at_ms) * (SR / 1000.0f);
            len = step_len(st);
            if (a < 0.0f || a >= len) break;            /* (too late to place) */
            if (E.clocked && E.running && E.step == st) E.step_left = -a;   /* not played yet: now */
            else { E.step = (st + 1) & 15; E.step_left = len - a; }
            E.clocked = 1;
            break;
        }
        }
        __asm__ volatile("" ::: "memory");
        cq_tail = (cq_tail + 1u) % QN;
    }
}

void eng_init(void)
{
    int i;
    zero(&E, sizeof E);
    zero(ks_buf, sizeof ks_buf);
    zero(lks_buf, sizeof lks_buf);
    zero(ch_buf, sizeof ch_buf);
    zero(lr_line, sizeof lr_line);
    zero(lr_ap0, sizeof lr_ap0);
    zero(lr_ap1, sizeof lr_ap1);
    zero(dl_buf, sizeof dl_buf);
    zero(rv_pre, sizeof rv_pre);
    for (i = 0; i < 4; i++) {
        zero(rv_line[i], RV_LEN[i] * sizeof(float));
        zero(ap_buf[i], AP_LEN[i] * sizeof(float));
        /* decay per line for a 2.4 s RT60: 10^(-3 len / (SR T60)) */
        E.rv_g[i] = fm_exp2f(-3.0f * 3.321928f * (float)RV_LEN[i] / (SR * 2.4f));
        E.v[i].m = E.v[i].target = 60.0f;
        E.v[i].p1 = 0.13f * (float)i;
        E.v[i].p2 = 0.37f * (float)i;
    }
    E.bm = E.btarget = 36.0f;
    E.rng = 0x1234567u;
    E.lfsr1 = 1u; E.lfsr2 = 0x4321u;
    E.cut = 3800.0f; E.q = 1.0f; E.bpm = 118.0f;
    E.rh.hold = 1;
    E.fade_len = 0.004f * SR;
    E.last_step = -1;
    E.gatem = 1.0f;
    for (i = 0; i < 4; i++) E.mv[i] = MIDI_NONE;
    E.mb = MIDI_NONE;
    E.dr.kit.k_base = 62.0f; E.dr.kit.k_sweep = 95.0f; E.dr.kit.k_pdec = 0.018f; E.dr.kit.k_dec = 0.052f;
    E.dr.kit.k_drive = 1.6f; E.dr.kit.h_tone = 0.5f; E.dr.kit.h_closed = 0.016f; E.dr.kit.h_open = 0.09f;
    E.dr.kit.r_f1 = 1720.0f; E.dr.kit.r_f2 = 480.0f; E.dr.kit.r_dec = 0.008f; E.dr.kit.r_noise = 0.25f;
    E.bsound = BASS_ROND; E.blevel = 0.6f; E.bcut = 600.0f;
    for (i = 0; i < 4; i++) { E.sg[i] = 1.0f; E.vg[i] = 1.0f; }
    E.vmask = 0x0Fu; E.arate = ARP_R_16; E.width = 0.5f; E.clev = 1.0f; E.aflow = 0.4f; E.arp_fk = 0.02f;
    E.dl_time = 0.75f * 60.0f / 118.0f * SR;
    E.p.engine = ENGINE_SUB; E.p.sus = 1.0f; E.p.atk = 0.01f; E.p.rel = 0.5f; E.p.dec = 0.3f; E.p.level = 1.0f;
    E.p.bass = 0.45f; E.p.mix2 = 0.5f; E.p.ratio = 1.0f; E.p.idx = 1.0f; E.p.idec = 0.4f; E.p.bright = 0.6f;
    E.p.damp = 0.996f; E.p.fdec = 0.3f; E.p.wob_rate = 0.3f;
}

static inline float rd_line(const float *b, uint32_t n, uint32_t w, float d)   /* fractional read d behind w */
{
    float rp = (float)w - d;
    int32_t i0, i1;
    float fr, a, c;
    if (rp < 0.0f) rp += (float)n;           /* d is always < n: one wrap is enough (no loop, no division) */
    if (rp < 0.0f) rp = 0.0f;
    i0 = (int32_t)rp;
    fr = rp - (float)i0;
    if (i0 >= (int32_t)n) i0 -= (int32_t)n;
    i1 = i0 + 1; if (i1 >= (int32_t)n) i1 = 0;
    a = b[i0];
    c = b[i1];
    return a + (c - a) * fr;
}

void eng_render(int32_t *lr, uint32_t n, float gain)
{
    const sound_t *p = &E.p;
    uint32_t c0, s, i;
    float lvl_acc = 0.0f;
    run_commands();
    {
        float dC = fm_expf(-1.0f / (fm_maxf(0.005f, p->dec) * SR * 0.3f));
        float rC = fm_expf(-1.0f / (fm_maxf(0.012f, E.rel_s / SR) * SR * 0.25f));
        float fC = fm_expf(-1.0f / (fm_maxf(0.01f, p->fdec) * SR * 0.33f));
        float iC = fm_expf(-1.0f / (fm_maxf(0.01f, p->idec) * SR * 0.33f));
        const float duckC = 0.99974808f;          /* exp(-1 / (0.09 s)) */
        const float duckA = 0.00565385f;          /* 1 - exp(-1 / (4 ms)) */
        const float tailC = 0.99247253f;          /* exp(-1 / (3 ms)) */
        const kit_t *kt = &E.dr.kit;
        const float kDecay = fm_expf(-1.0f / (fm_maxf(0.01f, kt->k_dec) * SR));    /* the kick's body */
        const float kPdecay = fm_expf(-1.0f / (fm_maxf(0.002f, kt->k_pdec) * SR)); /* its pitch drop */
        const float hpC = 0.12f + 0.43f * kt->h_tone;                                  /* the hat's high-pass, two stages (~0.9 .. 5.5 kHz) */
        const float kClick = 0.9905f;                                                  /* exp(-1 / (2.4 ms)) */
        const float hmul = (1.0f + 1.1f * kt->h_tone) / SR;
        const float hm_inc[6] = {205.3f * hmul, 304.4f * hmul, 369.6f * hmul, 522.7f * hmul, 540.0f * hmul, 800.0f * hmul};
        const float hmet = kt->h_metal;
        const float rDec = fm_expf(-1.0f / (fm_maxf(0.002f, kt->r_dec) * SR));
        const float sDec = fm_expf(-1.0f / (fm_maxf(0.01f, kt->s_dec) * SR));          /* the snare's noise */
        const float sTdec = fm_expf(-1.0f / (fm_maxf(0.01f, kt->s_dec * 0.45f) * SR));   /* its body, shorter */
        const float sBurst = fm_expf(-1.0f / (0.0045f * SR));                            /* a clap's first bursts */
        /* the snare's noise filter (state-variable): a snare high and open, a clap a narrower band around 1 kHz */
        const float s_fc = kt->s_clap > 0.5f ? 850.0f + 900.0f * kt->s_snap : 1400.0f + 4600.0f * kt->s_snap;
        const float s_k = kt->s_clap > 0.5f ? 0.45f : 1.1f;
        const float s_g = fm_tanf(3.14159265f * s_fc / SR);
        const float s_a1 = 1.0f / (1.0f + s_g * (s_g + s_k)), s_a2 = s_g * s_a1, s_a3 = s_g * s_a2;
        const float sAmp = kt->s_clap > 0.5f ? 1.1f : 0.95f;                          /* a clap's bursts lose a bit: louder */
        const float dspace = kt->space;
        const int eng = p->engine;
        const float lvl = p->level * (eng == ENGINE_FM ? 0.16f : eng == ENGINE_PLUCK ? 0.30f : 0.11f) * E.clev;
        const float kdamp = 1.0f / fm_maxf(0.3f, E.q);
        const float ang = E.rev * 1.5707963f;
        const float dry = fm_maxf(0.18f, fm_cosf(ang)), rsend = fm_sinf(ang) * 1.1f;
        const float dsend = E.del * 0.7f, cw = fm_minf(1.3f, p->chorus * 0.8f + E.chorus * 1.1f);
        const float ldC = fm_expf(-1.0f / (fm_maxf(0.005f, E.lp.dec) * SR * 0.3f));
        const float lrC = fm_expf(-1.0f / (fm_maxf(0.02f, E.lp.rel) * SR * 0.25f));
        const float lfC = fm_expf(-1.0f / (fm_maxf(0.01f, E.lp.fdec) * SR * 0.33f));
        const float liC = fm_expf(-1.0f / (fm_maxf(0.01f, E.lp.idec) * SR * 0.33f));   /* the melody's FM index */
        const int leng = E.lp.engine;
        const float lgain = leng == ENGINE_FM ? 0.256f : leng == ENGINE_PLUCK ? 0.7f : 0.176f;
        const float rv_damp = 0.575f;             /* ~6 kHz in the reverb's loop */
        const int chip = kt->chip > 0.5f;           /* the 8-bit console's drums */
        const float nz1_inc = fm_minf(1.0f, (5000.0f + 39000.0f * kt->h_tone) / SR);   /* the hat's noise clock */
        const float nz2_inc = fm_minf(1.0f, (3000.0f + 22000.0f * kt->s_snap) / SR);   /* the snare's */
        const int nz1_short = kt->h_metal > 0.5f;
        const float st_c = p->steps, st_l = E.lp.steps;
        const float swC = fm_expf(-(float)CHUNK / (fm_maxf(0.004f, p->sweep_t) * SR * 0.33f));
        const float lswC = fm_expf(-(float)CHUNK / (fm_maxf(0.004f, E.lp.sweep_t) * SR * 0.33f));
        float pw_c, pw_l;

        for (c0 = 0; c0 < n; c0 += CHUNK) {
            uint32_t cn = n - c0 < CHUNK ? n - c0 : CHUNK;
            float inc1[4], inc2[4], incc[4], incm[4], ksL[4], binc, pgl[4], pgr[4];
            float ldi1 = 0.0f, ldi2 = 0.0f, lda = 0.0f, ldo = 0.0f, lrl = 0.0f, lrr = 0.0f, ldL = 0.0f;   /* the melody voice, per chunk: phase steps, loop, filter */
            float wob = p->wob * 0.01f * fm_sin_turns(E.wob_ph), fc, ch1, ch2;
            E.wob_ph += p->wob_rate * (float)cn / SR;
            if (E.wob_ph >= 1.0f) E.wob_ph -= 1.0f;
            if (E.ld_st) {
                float lb = E.lp.cut > 0.0f ? E.lp.cut * E.lcut * (1.0f / CUT_MID_HZ) : E.lcut, lfc, lm;
                if (E.ld_m != E.ld_tgt) {                       /* the glide: on to the new note */
                    E.ld_m += (E.ld_tgt - E.ld_m) * (1.0f - fm_expf(-(float)cn / (fm_maxf(0.005f, E.lglide) * SR * 0.35f)));
                    if (fm_fabsf(E.ld_tgt - E.ld_m) < 0.002f) E.ld_m = E.ld_tgt;
                }
                E.ld_vph += 5.5f * (float)cn / SR; if (E.ld_vph >= 1.0f) E.ld_vph -= 1.0f;   /* the vibrato, fading in */
                E.ld_vamt = fm_minf(1.0f, E.ld_vamt + (float)cn / (0.4f * SR));
                lm = E.ld_m + E.lvib * 0.5f * E.ld_vamt * fm_sin_turns(E.ld_vph) + E.ld_swp;
                E.ld_swp *= lswC; if (fm_fabsf(E.ld_swp) < 1e-3f) E.ld_swp = 0.0f;
                lfc = fm_minf(SR * 0.45f, lb * fm_exp2f(E.lp.fenv * E.ld_fe + (lm - 60.0f) * (0.6f / 12.0f)));   /* follows the note a bit */
                if (leng == ENGINE_FM) {
                    ldi1 = mtof(lm) / SR;
                    ldi2 = ldi1 * E.lp.ratio;
                } else if (leng == ENGINE_PLUCK) {
                    ldL = fm_minf(SR / mtof(lm), (float)(KS_N - 4u));
                    if (E.ld_exc) excite_buf(lks_buf, E.ld_w, lm, E.lp.bright);
                    lfc = fm_minf(lfc, 2400.0f);
                } else {
                    ldi1 = mtof(lm - E.lp.det * 0.005f) / SR;
                    ldi2 = mtof(lm + E.lp.oct2 + E.lp.det * 0.005f) / SR;
                }
                E.ld_exc = 0;
                lda = fm_minf(0.95f, 6.2831853f * lfc / SR);
            }
            E.pwm_ph += p->pwm_rate * (float)cn / SR; if (E.pwm_ph >= 1.0f) E.pwm_ph -= 1.0f;   /* the PWM's sweep */
            pw_c = 0.5f + 0.42f * p->pwm * fm_sin_turns(E.pwm_ph);
            pw_l = 0.5f + 0.42f * E.lp.pwm * fm_sin_turns(E.pwm_ph * 0.7f + 0.3f);
            if (pw_l >= 1.0f) pw_l -= 0.5f;
            for (i = 0; i < 4; i++) {           /* pitches for this chunk */
                voice_t *v = &E.v[i];
                float m;
                if (v->inc != 0.0f) {
                    v->m += v->inc;
                    if ((v->inc > 0.0f && v->m >= v->target) || (v->inc < 0.0f && v->m <= v->target)) {
                        v->m = v->target; v->inc = 0.0f;
                    }
                }
                m = v->m + wob + E.swp;
                if (eng == ENGINE_SUB) {
                    inc1[i] = mtof(m - p->det * 0.005f) / SR;
                    inc2[i] = mtof(m + p->oct2 + p->det * 0.005f) / SR;
                } else if (eng == ENGINE_FM) {
                    incc[i] = mtof(m) / SR;
                    incm[i] = incc[i] * p->ratio;
                } else {
                    ksL[i] = fm_minf(SR / mtof(m), (float)(KS_N - 4u));
                }
            }
            E.swp *= swC; if (fm_fabsf(E.swp) < 1e-3f) E.swp = 0.0f;
            if (E.binc != 0.0f) {
                E.bm += E.binc;
                if ((E.binc > 0.0f && E.bm >= E.btarget) || (E.binc < 0.0f && E.bm <= E.btarget)) {
                    E.bm = E.btarget; E.binc = 0.0f;
                }
            }
            binc = mtof(E.bm) / SR;
            {   /* the bass's filter, its envelope and the tremolo phase for this chunk */
                int bs = E.bsound;
                float famt = bs == BASS_ACID ? 3.6f : bs == BASS_PLUCK ? 2.6f : bs == BASS_FM ? 0.0f : 0.6f;
                float tau = bs == BASS_ACID ? 0.16f : bs == BASS_PLUCK ? 0.09f : 0.25f;
                float bfc = fm_minf(SR * 0.45f, E.bcut * fm_exp2f(famt * E.bfenv));
                E.bg = fm_tanf(3.14159265f * bfc / SR);
                E.bk = bs == BASS_ACID ? 0.18f : bs == BASS_REESE ? 0.9f : 1.3f;
                E.bfenv *= fm_expf(-(float)cn / (tau * SR));
                if (E.bfenv < 1e-5f) E.bfenv = 0.0f;
                E.bic1 = fm_flush(E.bic1); E.bic2 = fm_flush(E.bic2);
                E.ml = fm_flush(E.ml); E.mr = fm_flush(E.mr);
                E.muf += (E.muf_t - E.muf) * 0.03f;
                E.h_lp = fm_flush(E.h_lp); E.h_lp2 = fm_flush(E.h_lp2);
                E.tr_ph += E.bpm * (2.0f / 60.0f) * (float)cn / SR; if (E.tr_ph >= 1.0f) E.tr_ph -= 1.0f;
            }
            E.ic1 = fm_flush(E.ic1); E.ic2 = fm_flush(E.ic2); E.dl_lp = fm_flush(E.dl_lp);
            E.ic1r = fm_flush(E.ic1r); E.ic2r = fm_flush(E.ic2r);
            E.rv_hpl = fm_flush(E.rv_hpl); E.rv_hpr = fm_flush(E.rv_hpr); E.k_tail = fm_flush(E.k_tail);
            E.duck = fm_flush(E.duck);
            E.ld_lp = fm_flush(E.ld_lp); E.ld_lp2 = fm_flush(E.ld_lp2); E.lr_en = fm_flush(E.lr_en);
            E.ld_prev = fm_flush(E.ld_prev); E.ld_ies = fm_flush(E.ld_ies); E.ie_s = fm_flush(E.ie_s); E.ld_last = fm_flush(E.ld_last); E.ld_ie = fm_flush(E.ld_ie);
            E.s_lp = fm_flush(E.s_lp); E.s_hp = fm_flush(E.s_hp);
            for (i = 0; i < 4; i++) E.lr_lp[i] = fm_flush(E.lr_lp[i]);
            for (i = 0; i < 4; i++) { E.rv_lp[i] = fm_flush(E.rv_lp[i]); E.v[i].prev = fm_flush(E.v[i].prev); E.v[i].last = fm_flush(E.v[i].last); }
            {   /* a sound's own cutoff, moved by the knob (about 3.7 octaves each way); 0: the knob's alone */
                float base = p->cut > 0.0f ? p->cut * E.cut * (1.0f / CUT_MID_HZ) : E.cut;
                fc = fm_minf(SR * 0.45f, base * fm_exp2f(p->fenv * E.fenv * E.vel));
            }
            if (eng == ENGINE_PLUCK) fc = fm_minf(fc, 2400.0f);   /* plucked strings: their own ceiling, never harsh */
            E.g += (fm_tanf(3.14159265f * fc / SR) - E.g) * 0.45f;   /* smoothed: a hit's filter jump must not click */
            {   /* the voices across the stereo field (as a Prophet's voice spread), WIDTH how far; equal power */
                static const float DIV[4] = {-0.25f, 0.25f, -0.75f, 0.75f}, ALT[4] = {-1.0f, 1.0f, -1.0f, 1.0f},
                                   RR[4] = {-1.0f, 1.0f, -0.4f, 0.4f};
                for (i = 0; i < 4; i++) {
                    float pn, a;
                    switch (E.spread) {
                    case SPREAD_ALTERNATE: pn = ALT[i]; break;                       /* left, right, left, right */
                    case SPREAD_DIVERGE2: pn = RR[(i + E.rr) & 3u]; break;           /* round robin: each chord (arp note) moves on */
                    case SPREAD_NOTEVAL: pn = fm_clampf((E.v[i].m - 62.0f) * (1.0f / 14.0f), -1.0f, 1.0f); break;   /* by its note */
                    default: pn = DIV[i]; break;                                     /* further out, voice by voice */
                    }
                    a = (pn * E.width + 1.0f) * 0.125f;                              /* 0 .. 0.25 turns: left .. right */
                    pgl[i] = fm_sin_turns(a + 0.25f) * 1.4142136f;
                    pgr[i] = fm_sin_turns(a) * 1.4142136f;
                }
            }
            /* chorus taps for this chunk: 12 ms and 19 ms, +-2.8 ms */
            E.ch_ph1 += 0.31f * (float)cn / SR; if (E.ch_ph1 >= 1.0f) E.ch_ph1 -= 1.0f;
            E.ch_ph2 += 0.37f * (float)cn / SR; if (E.ch_ph2 >= 1.0f) E.ch_ph2 -= 1.0f;
            ch1 = (0.012f + 0.0028f * fm_sin_turns(E.ch_ph1)) * SR;
            ch2 = (0.019f + 0.0028f * fm_sin_turns(E.ch_ph2)) * SR;

            for (s = c0; s < c0 + cn; s++) {
                float sum = 0.0f, sumr = 0.0f, y, yr, bass, kv = 0.0f, fade = 1.0f, l, r, dh_l, dh_r, nz;
                float susL = p->sus * E.vel;
                /* transport */
                if (E.running || E.clocked) {
                    if (E.step_left <= 0.0f) {
                        if (E.running) on_step();
                        E.step_left += step_len(E.step);
                        E.step = (E.step + 1) & 15;
                    }
                    E.step_left -= 1.0f;
                }
                if (E.arp && E.active && E.running) {   /* the arpeggiator's own clock (triplets too), swung in pairs */
                    E.arp_left -= 1.0f;
                    if (E.arp_left <= 0.0f) {
                        float rb = RATE_BEATS[E.arate], per = rb < 0.0f ? -rb * SR : 60.0f / E.bpm * SR * rb, sw = E.aswing * 0.33f;
                        per *= (E.arp_n & 1u) ? 1.0f - sw : 1.0f + sw;
                        E.arp_n++;
                        arp_fire(per);
                        E.arp_left += per;
                        E.arp_per = per;
                    }
                }
                /* amp envelope */
                switch (E.stage) {
                case 1: E.e += E.vel / E.atk_s; if (E.e >= E.vel) { E.e = E.vel; E.stage = 2; } break;
                case 2: E.e = susL + (E.e - susL) * dC; if (fm_fabsf(E.e - susL) < 1e-4f) E.stage = 3; break;
                case 3: E.e = susL; break;
                case 4: E.e *= rC; if (E.e < 1e-5f) { E.e = 0.0f; E.stage = 0; } break;
                default: break;
                }
                if (E.stage >= 1 && E.stage <= 3) {
                    E.gate_left -= 1.0f;
                    if (E.gate_left <= 0.0f) {
                        E.stage = 4;
                        midi_chord_off();
                        rC = fm_expf(-1.0f / (fm_maxf(0.012f, E.rel_s / SR) * SR * 0.25f));
                    }
                }
                E.fenv *= fC;
                E.ienv *= iC;
                E.ie_s += (E.ienv - E.ie_s) * 0.03f;   /* ~0.7 ms */
                if (E.fenv < 1e-6f) E.fenv = 0.0f;   /* no denormals: they cost a lot */
                if (E.ienv < 1e-6f) E.ienv = 0.0f;
                /* transition fade */
                if (E.fade_left > 0.0f) {
                    fade = E.fade_left / E.fade_len;
                    E.fade_left -= 1.0f;
                    if (E.fade_left <= 0.0f && E.pending) {
                        E.pending = 0;
                        set_notes(E.pend_notes, E.pend_bass, 0.0f);
                        E.e = 0.0f;
                        if (E.retrig) hit(E.vel, fm_maxf(E.gate_left, SR * 0.04f), 0.0f);
                        else { E.stage = 0; midi_chord_off(); }
                        fade = 0.0f;
                        for (i = 0; i < 4; i++) {   /* the new notes' increments right away */
                            float m = E.v[i].m + wob;
                            if (eng == ENGINE_SUB) {
                                inc1[i] = mtof(m - p->det * 0.005f) / SR;
                                inc2[i] = mtof(m + p->oct2 + p->det * 0.005f) / SR;
                            } else if (eng == ENGINE_FM) {
                                incc[i] = mtof(m) / SR; incm[i] = incc[i] * p->ratio;
                            } else {
                                ksL[i] = fm_minf(SR / mtof(m), (float)(KS_N - 4u));
                            }
                        }
                        binc = mtof(E.bm) / SR;
                    }
                }
                /* voices */
                g_pw = pw_c;
                for (i = 0; i < 4; i++) {
                    voice_t *v = &E.v[i];
                    float vo;
                    if (E.sdel[i] > 0.0f) {         /* strum: this voice waits its turn */
                        E.sdel[i] -= 1.0f;
                        if (E.sdel[i] <= 0.0f) {
                            if (eng == ENGINE_PLUCK) excite(v, p->bright);
                            midi_voice_on((int)i);
                        }
                    }
                    if (eng == ENGINE_SUB) {
                        vo = osc(p->w1, v->p1, inc1[i]) + p->mix2 * osc(p->w2, v->p2, inc2[i]);
                        v->p1 += inc1[i]; if (v->p1 >= 1.0f) v->p1 -= 1.0f;
                        v->p2 += inc2[i]; if (v->p2 >= 1.0f) v->p2 -= 1.0f;
                    } else if (eng == ENGINE_FM) {
                        float idx = p->idx * (p->isus + (1.0f - p->isus) * E.ie_s);
                        float mod = fm_sin_turns(v->pm + p->fb * v->prev * 0.15915494f) * idx;
                        v->prev = mod;
                        vo = fm_sin_turns(v->pc + mod * 0.15915494f);
                        v->pc += incc[i]; if (v->pc >= 1.0f) v->pc -= 1.0f;
                        v->pm += incm[i]; if (v->pm >= 1.0f) v->pm -= 1.0f;
                    } else {
                        float *b = ks_buf[i];
                        float x = rd_line(b, KS_N, v->w, ksL[i]);
                        b[v->w] = p->damp * 0.5f * (x + v->last) + 1e-15f;
                        v->last = x;
                        v->w = (v->w + 1u) & (KS_N - 1u);
                        vo = x;
                    }
                    if (E.sg[i] < 1.0f || E.sdel[i] > 0.0f) {
                        if (E.sdel[i] <= 0.0f) E.sg[i] = fm_minf(1.0f, E.sg[i] + 0.004f);
                        else E.sg[i] = fm_maxf(0.0f, E.sg[i] - 0.01f);
                        vo *= E.sg[i];
                    }
                    if (E.arp || E.vg[i] < 1.0f) {   /* the arpeggiator: its voice in (~1 ms), the last ones fade (FLOW) */
                        float t = (!E.arp || (int)i == E.arp_v) ? 1.0f : 0.0f;
                        E.vg[i] += (t - E.vg[i]) * (t > E.vg[i] ? 0.02f : E.arp_fk);
                        if (!E.arp && E.vg[i] > 0.999f) E.vg[i] = 1.0f;
                        vo *= E.vg[i];
                    }
                    sum += vo * pgl[i];
                    sumr += vo * pgr[i];
                }
                nz = p->noise * rnd() * 4.0f;
                sum += nz; sumr += nz;
                /* filter: TPT state-variable lowpass, left and right */
                {
                    float g = E.g, a1 = 1.0f / (1.0f + g * (g + kdamp)), a2 = g * a1, a3 = g * a2;
                    float v3 = sum - E.ic2, v1 = a1 * E.ic1 + a2 * v3, v2 = E.ic2 + a2 * E.ic1 + a3 * v3;
                    E.ic1 = 2.0f * v1 - E.ic1;
                    E.ic2 = 2.0f * v2 - E.ic2;
                    sum = v2;
                    v3 = sumr - E.ic2r; v1 = a1 * E.ic1r + a2 * v3; v2 = E.ic2r + a2 * E.ic1r + a3 * v3;
                    E.ic1r = 2.0f * v1 - E.ic1r;
                    E.ic2r = 2.0f * v2 - E.ic2r;
                    sumr = v2;
                }
                if (st_c > 0.5f) {               /* chip: the voices in so many levels (their grit) */
                    sum = (float)(int32_t)(sum * st_c + (sum >= 0.0f ? 0.5f : -0.5f)) / st_c;
                    sumr = (float)(int32_t)(sumr * st_c + (sumr >= 0.0f ? 0.5f : -0.5f)) / st_c;
                }
                /* bass: its own sound and filter; on SUIT it follows the chords' envelope, otherwise its own */
                bass = 0.0f;
                if (E.bsound != BASS_OFF) {
                    float bi = binc * (E.boct ? 2.0f : 1.0f), o, amp;
                    switch (E.bsound) {
                    case BASS_SUB: o = fm_sin_turns(E.bp) * 0.85f + fm_sin_turns(E.bs) * 0.55f; break;
                    case BASS_ROND: o = (1.0f - 4.0f * fm_fabsf(E.bp - 0.5f)) * 0.55f + fm_sin_turns(E.bs) * 0.8f; break;
                    case BASS_ACID: o = osc(W_SAW, E.bp, bi) * 0.9f; break;
                    case BASS_REESE: o = (osc(W_SAW, E.bp, bi) + osc(W_SAW, E.b2, bi * 1.007f)) * 0.5f; break;
                    case BASS_PLUCK: o = osc(W_SQUARE, E.bp, bi) * 0.45f + fm_sin_turns(E.bs) * 0.6f; break;
                    case BASS_TRI: o = tri4(E.bp) * 1.05f; break;                       /* the 8-bit consoles' bass */
                    case BASS_PULSE: o = pulse(E.bp, bi, 0.25f) * 0.5f + tri4(E.bs) * 0.35f; break;
                    default: o = fm_sin_turns(E.bp + (0.1f + 0.35f * E.bfenv) * fm_sin_turns(E.bp)) * 1.1f; break;
                    }
                    E.bp += bi; if (E.bp >= 1.0f) E.bp -= 1.0f;
                    E.bs += bi * 0.5f; if (E.bs >= 1.0f) E.bs -= 1.0f;
                    E.b2 += bi * 1.007f; if (E.b2 >= 1.0f) E.b2 -= 1.0f;
                    {
                        float g = E.bg, a1 = 1.0f / (1.0f + g * (g + E.bk)), a2 = g * a1, a3 = g * a2;
                        float v3 = o - E.bic2, v1 = a1 * E.bic1 + a2 * v3, v2 = E.bic2 + a2 * E.bic1 + a3 * v3;
                        E.bic1 = 2.0f * v1 - E.bic1;
                        E.bic2 = 2.0f * v2 - E.bic2;
                        o = v2;
                    }
                    if (E.bmotif == MOTIF_SUIT) amp = E.e * fade;
                    else {
                        int perc = E.bsound == BASS_ACID || E.bsound == BASS_PLUCK || E.bsound == BASS_FM;
                        switch (E.bstage) {
                        case 1: E.be += 1.0f / (0.003f * SR); if (E.be >= 1.0f) { E.be = 1.0f; E.bstage = 2; } break;
                        case 2: if (perc) E.be *= 0.99987f; break;               /* ~0.18 s */
                        case 3: E.be *= 0.99943f; if (E.be < 1e-5f) { E.be = 0.0f; E.bstage = 0; } break;   /* ~40 ms */
                        default: break;
                        }
                        if (E.bstage == 1 || E.bstage == 2) {
                            E.bgate -= 1.0f;
                            if (E.bgate <= 0.0f) { E.bstage = 3; midi_bass_off(); }
                        }
                        amp = E.be;
                    }
                    bass = o * amp * E.blevel * 0.75f;
                }
                y = (sum * lvl * E.e * fade) + bass * 0.2f;
                yr = (sumr * lvl * E.e * fade) + bass * 0.2f;
                if (E.trem > 0.001f) { float tg = 1.0f - E.trem * (0.5f + 0.5f * fm_sin_turns(E.tr_ph)); y *= tg; yr *= tg; }
                /* kick: a sine that drops in pitch, held at full level a moment (punch, an 808's boom), driven; and the
                 * beater's click on top */
                if (E.k_amp > 1e-4f) {
                    float f = kt->k_base + kt->k_sweep * E.k_pitch;
                    E.k_ph += f / SR;
                    if (E.k_ph >= 1.0f) E.k_ph -= 1.0f;
                    kv = chip ? tri4(E.k_ph) * E.k_amp * 1.1f            /* chip: the stepped triangle, as it drops */
                              : fm_tanhf(fm_sin_turns(E.k_ph) * E.k_amp * kt->k_drive) / fm_tanhf(kt->k_drive);
                    if (E.k_cl > 1e-3f) {
                        E.k_clph += 3100.0f / SR; if (E.k_clph >= 1.0f) E.k_clph -= 1.0f;
                        kv += (fm_sin_turns(E.k_clph) * 0.55f + rnd() * 0.45f) * E.k_cl * kt->k_click * 0.5f;
                        E.k_cl *= kClick;
                    }
                    if (E.k_hold > 0.0f) E.k_hold -= 1.0f;
                    else E.k_amp *= kDecay;
                    E.k_pitch *= kPdecay;
                }
                E.k_tail *= tailC;
                E.k_last = kv;
                kv = (kv + E.k_tail) * 0.5f * E.k_gain;
                if (chip) {                          /* the chip drums' noise: two shift registers, each on its clock */
                    E.nz1_ph += nz1_inc;
                    if (E.nz1_ph >= 1.0f) {
                        uint16_t b = (uint16_t)((E.lfsr1 ^ (E.lfsr1 >> (nz1_short ? 6 : 1))) & 1u);
                        E.lfsr1 = (uint16_t)((E.lfsr1 >> 1) | (b << 14)); E.nz1_ph -= 1.0f;
                        E.nz1 = (E.lfsr1 & 1u) ? 1.0f : -1.0f;
                    }
                    E.nz2_ph += nz2_inc;
                    if (E.nz2_ph >= 1.0f) {
                        uint16_t b = (uint16_t)((E.lfsr2 ^ (E.lfsr2 >> 1)) & 1u);
                        E.lfsr2 = (uint16_t)((E.lfsr2 >> 1) | (b << 14)); E.nz2_ph -= 1.0f;
                        E.nz2 = (E.lfsr2 & 1u) ? 1.0f : -1.0f;
                    }
                }
                /* hi-hat: high-passed noise; rim: two short tones and a click */
                {
                    float hv = 0.0f, rv = 0.0f;
                    if (E.h_env > 1e-4f) {           /* hi-hat: metal (six square waves) and noise, high-passed twice */
                        float x = 0.0f, hp, hp2;
                        int j;
                        if (chip) x = E.nz1 * 0.8f;        /* chip: the shift register's noise (held between clocks) */
                        else if (hmet > 0.01f) {
                            for (j = 0; j < 6; j++) {
                                E.h_mph[j] += hm_inc[j]; if (E.h_mph[j] >= 1.0f) E.h_mph[j] -= 1.0f;
                                x += E.h_mph[j] < 0.5f ? 1.0f : -1.0f;
                            }
                            x *= 0.5f * hmet;
                        }
                        if (!chip) x += rnd() * (1.0f - hmet);
                        E.h_lp += hpC * (x - E.h_lp);
                        hp = x - E.h_lp;
                        E.h_lp2 += hpC * (hp - E.h_lp2);
                        hp2 = hp - E.h_lp2;
                        hv = hp2 * E.h_env * E.h_gain * 1.1f;
                        E.h_env *= E.h_dec;
                    }
                    if (E.r_env > 1e-4f) {
                        E.r_ph1 += kt->r_f1 / SR; if (E.r_ph1 >= 1.0f) E.r_ph1 -= 1.0f;
                        E.r_ph2 += kt->r_f2 / SR; if (E.r_ph2 >= 1.0f) E.r_ph2 -= 1.0f;
                        if (chip)                         /* chip: two square waves, a blip */
                            rv = ((E.r_ph1 < 0.5f ? 0.4f : -0.4f) + (E.r_ph2 < 0.25f ? 0.3f : -0.1f) + E.nz2 * kt->r_noise * E.r_env)
                                 * E.r_env * E.r_gain * 0.5f;
                        else
                        rv = (fm_sin_turns(E.r_ph1) * 0.55f + fm_sin_turns(E.r_ph2) * 0.45f + rnd() * kt->r_noise * E.r_env)
                             * E.r_env * E.r_gain * 0.55f;
                        E.r_env *= rDec;
                    }
                    if (E.h_roll) {                   /* the roll's next hit */
                        E.h_roll_left -= 1.0f;
                        if (E.h_roll_left <= 0.0f) {
                            E.h_roll--; E.h_roll_left += E.h_roll_len;
                            E.h_env = 1.0f; E.h_gain = E.h_roll_g * (0.7f + 0.2f * rndu());
                        }
                    }
                    if (E.s_env > 1e-4f) {           /* snare: a body (two tones that drop a little) and filtered noise;
                                                        a clap: a band of noise in bursts, then a tail */
                        float nz = chip ? E.nz2 : rnd(), sv, body, nf, v1, v2, v3, fb = kt->s_tone * (1.0f + 0.5f * E.s_tenv) / SR;
                        E.s_ph += fb; if (E.s_ph >= 1.0f) E.s_ph -= 1.0f;
                        E.s_ph2 += fb * 1.62f; if (E.s_ph2 >= 1.0f) E.s_ph2 -= 1.0f;
                        body = chip ? tri4(E.s_ph) * E.s_tenv
                                    : (fm_sin_turns(E.s_ph) * 0.65f + fm_sin_turns(E.s_ph2) * 0.35f) * E.s_tenv;
                        v3 = nz - E.s_hp;
                        v1 = s_a1 * E.s_lp + s_a2 * v3;
                        v2 = E.s_hp + s_a2 * E.s_lp + s_a3 * v3;
                        E.s_lp = 2.0f * v1 - E.s_lp;
                        E.s_hp = 2.0f * v2 - E.s_hp;
                        nf = kt->s_clap > 0.5f ? v1 * 1.5f : (nz - s_k * v1 - v2) * 0.55f + v1 * 0.7f;   /* band / high + band */
                        sv = (body * kt->s_body * 1.3f + nf * (1.1f - 0.45f * kt->s_body)) * E.s_env * E.s_gain * sAmp;
                        E.s_env *= E.s_clapn ? sBurst : sDec;
                        E.s_tenv *= sTdec;
                        if (E.s_clapn) {
                            E.s_clapt -= 1.0f;
                            if (E.s_clapt <= 0.0f) { E.s_clapn--; E.s_env = 1.0f; E.s_clapt = 0.011f * SR; }
                        }
                        rv += sv;
                    }
                    dh_l = hv * 0.85f + rv; dh_r = hv + rv * 0.85f;
                }
                /* sidechain */
                if (E.duck_atk) { E.duck += (1.0f - E.duck) * duckA; if (E.duck > 0.985f) E.duck_atk = 0; }
                else E.duck *= duckC;
                /* chorus */
                ch_buf[E.ch_w] = (y + yr) * 0.5f;
                {
                    float t1 = rd_line(ch_buf, CH_N, E.ch_w, ch1) * cw, t2 = rd_line(ch_buf, CH_N, E.ch_w, ch2) * cw;
                    l = y + 0.98f * t1 + 0.2f * t2;
                    r = yr + 0.2f * t1 + 0.98f * t2;
                }
                E.ch_w = (E.ch_w + 1u) & (CH_N - 1u);
                ldo = 0.0f;
                if (E.ld_st) {                    /* the melody voice (white keys): its own sound, level and reverb send */
                    const sound_t *q = &E.lp;
                    float lv;
                    if (leng == ENGINE_FM) {           /* two operators, as the chords' FM */
                        float idx;
                        E.ld_ies += (E.ld_ie - E.ld_ies) * 0.03f;
                        idx = q->idx * (q->isus + (1.0f - q->isus) * E.ld_ies);
                        float mod = fm_sin_turns(E.ld_pm + q->fb * E.ld_prev * 0.15915494f) * idx;
                        E.ld_prev = mod;
                        lv = fm_sin_turns(E.ld_ph1 + mod * 0.15915494f);
                        E.ld_ph1 += ldi1; if (E.ld_ph1 >= 1.0f) E.ld_ph1 -= 1.0f;
                        E.ld_pm += ldi2; if (E.ld_pm >= 1.0f) E.ld_pm -= 1.0f;
                        E.ld_ie *= liC;
                    } else if (leng == ENGINE_PLUCK) { /* its own plucked string */
                        float x = rd_line(lks_buf, KS_N, E.ld_w, ldL);
                        lks_buf[E.ld_w] = q->damp * 0.5f * (x + E.ld_last) + 1e-15f;
                        E.ld_last = x;
                        E.ld_w = (E.ld_w + 1u) & (KS_N - 1u);
                        lv = x;
                    } else {
                        E.ld_ph1 += ldi1; if (E.ld_ph1 >= 1.0f) E.ld_ph1 -= 1.0f;
                        E.ld_ph2 += ldi2; if (E.ld_ph2 >= 1.0f) E.ld_ph2 -= 1.0f;
                        g_pw = pw_l;
                        lv = osc(q->w1, E.ld_ph1, ldi1) + q->mix2 * osc(q->w2, E.ld_ph2, ldi2);
                    }
                    if (st_l > 0.5f) lv = (float)(int32_t)(lv * st_l + (lv >= 0.0f ? 0.5f : -0.5f)) / st_l;
                    E.ld_lp += lda * (lv - E.ld_lp);
                    E.ld_lp2 += lda * (E.ld_lp - E.ld_lp2);       /* two poles: 12 dB per octave */
                    if (E.ld_st == 1) { E.ld_e += 1.0f / (fm_maxf(0.003f, fm_minf(q->atk, 0.12f)) * SR); if (E.ld_e >= 1.0f) { E.ld_e = 1.0f; E.ld_st = 2; } }
                    else if (E.ld_st == 2) { E.ld_e = q->sus + (E.ld_e - q->sus) * ldC; if (q->sus < 0.001f && E.ld_e < 1e-4f) { E.ld_e = 0.0f; E.ld_st = 0; } }
                    else { E.ld_e *= lrC; if (E.ld_e < 1e-4f) { E.ld_e = 0.0f; E.ld_st = 0; } }
                    E.ld_fe *= lfC;
                    ldo = E.ld_lp2 * E.ld_e * q->level * lgain * E.llev;
                }
                lrl = 0.0f; lrr = 0.0f;
                if (E.lrev > 0.001f || E.lr_en > 1e-6f) {   /* the melody's reverb: none before it, smooth, the knob: a light tail .. big and airy */
                    float g = E.lr_g, x = ldo * E.lr_in + 1e-15f, yv[4], sum, wet = E.lr_wet, o, d;
                    uint32_t k;
                    d = lr_ap0[E.lr_apw0]; o = d - 0.6f * x; lr_ap0[E.lr_apw0] = x + 0.6f * o; x = o;
                    if (++E.lr_apw0 >= 113u) E.lr_apw0 = 0u;
                    d = lr_ap1[E.lr_apw1]; o = d - 0.6f * x; lr_ap1[E.lr_apw1] = x + 0.6f * o; x = o;
                    if (++E.lr_apw1 >= 337u) E.lr_apw1 = 0u;
                    for (k = 0; k < 4u; k++) {
                        float tri;
                        E.lr_ph[k] += (0.21f + 0.07f * (float)k) / SR;
                        if (E.lr_ph[k] >= 1.0f) E.lr_ph[k] -= 1.0f;
                        tri = 4.0f * fm_fabsf(E.lr_ph[k] - 0.5f) - 1.0f;                 /* the delay drifts: no metallic ring */
                        d = rd_line(lr_line[k], LR_N, E.lr_w[k], LR_LEN[k] + 7.0f * tri);
                        E.lr_lp[k] += 0.45f * (d - E.lr_lp[k]);                           /* damping: the tail gets darker as it fades */
                        yv[k] = E.lr_lp[k];
                    }
                    sum = (yv[0] + yv[1] + yv[2] + yv[3]) * 0.5f;
                    for (k = 0; k < 4u; k++) {
                        lr_line[k][E.lr_w[k]] = (yv[k] - sum) * g + x;
                        if (++E.lr_w[k] >= LR_N) E.lr_w[k] = 0u;
                    }
                    lrl = (yv[0] + yv[2]) * 0.5f * wet;
                    lrr = (yv[1] + yv[3]) * 0.5f * wet;
                    E.lr_en += (fm_fabsf(yv[0]) + fm_fabsf(yv[1]) - E.lr_en) * 0.001f;
                }
                /* ping-pong delay (16-bit lines) */
                {
                    uint32_t rp = E.dl_w + DL_N - (uint32_t)E.dl_time;
                    if (rp >= DL_N) rp -= DL_N;
                    float dlo = (float)dl_buf[0][rp] * (1.0f / 32767.0f), dro = (float)dl_buf[1][rp] * (1.0f / 32767.0f);
                    float in = (l + r) * 0.5f * dsend + ldo * E.ldel;   /* the melody's own echo send too */
                    E.dl_lp += 0.36f * (dro - E.dl_lp);          /* ~3.2 kHz in the feedback */
                    dl_buf[0][E.dl_w] = (int16_t)(fm_clampf(in + 0.42f * E.dl_lp, -1.0f, 1.0f) * 32767.0f);
                    dl_buf[1][E.dl_w] = (int16_t)(fm_clampf(dlo, -1.0f, 1.0f) * 32767.0f);
                    if (++E.dl_w >= DL_N) E.dl_w = 0u;
                    dlo *= 0.8f; dro *= 0.8f;
                    /* reverb: pre-delay, 4 allpass diffusers, 4-line feedback network (Householder) */
                    {
                        float x = ((l + r) * 0.5f + (dlo + dro) * 0.5f) * rsend + (dh_l + dh_r) * 0.5f * dspace + 1e-15f, yl[4], sumy = 0.0f, ol, orr;
                        float pre = rv_pre[E.pre_w];
                        uint32_t k;
                        rv_pre[E.pre_w] = x;
                        if (++E.pre_w >= RV_PRE) E.pre_w = 0u;
                        x = pre;
                        for (k = 0; k < 4u; k++) {
                            float *ab = ap_buf[k], d = ab[E.ap_w[k]], o = d - 0.7f * x;
                            ab[E.ap_w[k]] = x + 0.7f * o;
                            if (++E.ap_w[k] >= AP_LEN[k]) E.ap_w[k] = 0u;
                            x = o;
                        }
                        for (k = 0; k < 4u; k++) {
                            float o = rv_line[k][E.rv_w[k]];
                            E.rv_lp[k] += rv_damp * (o - E.rv_lp[k]);
                            yl[k] = E.rv_lp[k];
                            sumy += yl[k];
                        }
                        sumy *= 0.5f;
                        for (k = 0; k < 4u; k++) {
                            rv_line[k][E.rv_w[k]] = (yl[k] - sumy) * E.rv_g[k] + x;
                            if (++E.rv_w[k] >= RV_LEN[k]) E.rv_w[k] = 0u;
                        }
                        ol = (yl[0] + yl[2]) * 0.6f;
                        orr = (yl[1] + yl[3]) * 0.6f;
                        /* ~180 Hz high-pass on the return */
                        E.rv_hpl += 0.0253f * (ol - E.rv_hpl);
                        E.rv_hpr += 0.0253f * (orr - E.rv_hpr);
                        ol -= E.rv_hpl;
                        orr -= E.rv_hpr;
                        {
                            float dk = fm_minf(1.0f, E.duck * 1.1f), sh = dk * dk * (3.0f - 2.0f * dk);
                            float dg = 1.0f - 0.85f * E.side * sh;
                            l = (l * dry + dlo + ol) * dg + kv + dh_l;
                            r = (r * dry + dro + orr) * dg + kv + dh_r;
                        }
                    }
                }
                l += ldo + lrl; r += ldo + lrr;   /* the melody voice and its own reverb */
                if (E.bl_env > 1e-4f) {           /* plop (a rising bubble) / toc (a knock on the glass) */
                    float bv;
                    E.bl_ph += E.bl_f / SR; if (E.bl_ph >= 1.0f) E.bl_ph -= 1.0f;
                    E.bl_ph2 += E.bl_f * 1.73f / SR; if (E.bl_ph2 >= 1.0f) E.bl_ph2 -= 1.0f;
                    if (E.bl_kind == BLIP_PLOP) bv = fm_sin_turns(E.bl_ph);
                    else bv = fm_sin_turns(E.bl_ph) * 0.6f + fm_sin_turns(E.bl_ph2) * 0.3f + rnd() * 0.3f * E.bl_env;
                    bv *= E.bl_env * 0.22f;
                    E.bl_f = fm_minf(E.bl_f * E.bl_df, 6000.0f);   /* (it rose without end: no number left, a stutter) */
                    E.bl_env *= E.bl_dec;
                    l += bv; r += bv;
                }
                if (E.crush > 0.01f) {           /* the bitcrusher: the mix held for a few samples, in fewer levels */
                    E.cr_left -= 1.0f;
                    if (E.cr_left <= 0.0f) {
                        float k = 1.0f - E.crush, lv = 6.0f + 122.0f * k * k;
                        E.cr_hold_l = (float)(int32_t)(l * lv + (l >= 0.0f ? 0.5f : -0.5f)) / lv;
                        E.cr_hold_r = (float)(int32_t)(r * lv + (r >= 0.0f ? 0.5f : -0.5f)) / lv;
                        E.cr_left += 1.0f + (float)(int32_t)(E.crush * 7.0f);
                    }
                    l = E.cr_hold_l; r = E.cr_hold_r;
                }
                if (E.muf > 0.002f) {            /* heard from outside the room */
                    float a = 1.0f - 0.9f * E.muf, g = 1.0f - 0.45f * E.muf;
                    E.ml += a * (l - E.ml); E.mr += a * (r - E.mr);
                    l = E.ml * g; r = E.mr * g;
                }
                lvl_acc += fm_fabsf(l) + fm_fabsf(r);
                l = fm_tanhf(l * 1.1f) * 0.9f;
                r = fm_tanhf(r * 1.1f) * 0.9f;
                lr[2 * s] = (int32_t)(l * gain);
                lr[2 * s + 1] = (int32_t)(r * gain);
                E.samples++;
            }
        }
    }
    E.level += (fm_minf(1.0f, lvl_acc / (float)n * 2.5f) - E.level) * 0.3f;
}
