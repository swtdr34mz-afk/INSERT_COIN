/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records) */
/* OSR-1 sound engine (INSERT COIN: chip waveforms, chip drums, a bitcrusher, a drum preview for the studio):
 * 4 chord voices (sub / FM / plucked string) with strum, a resonant filter, envelopes,
 * a 16-step chord-rhythm sequencer with groove (swing, density, gate), a bass with its own sounds and motifs,
 * a small drum kit (kick, hi-hat, rim) on 16-step patterns with a kit per style, sidechain, tremolo,
 * chorus, ping-pong delay, a light reverb, a muffled 'outside the room' mode and two little sound effects.
 * The main loop talks to it through eng_* calls (a small queue); eng_render runs in the audio interrupt. */
#pragma once
#include <stdint.h>

#define ENG_SR 44100.0f

enum { ENGINE_SUB, ENGINE_FM, ENGINE_PLUCK };
/* the sub engine's waveforms: the classic four, then the chip ones (INSERT COIN): pulses of 12.5 % and 25 % (W_SQUARE is the
 * 50 % one), the 4-bit stepped triangle of the 8-bit consoles, two 32-step 4-bit wavetables (a handheld's wave channel: warm,
 * and a nasal "voice"), a 16-step saw (a home computer's) and a pulse whose width sweeps (PWM: sound_t pwm, pwm_rate) */
enum { W_SAW, W_SQUARE, W_TRI, W_SINE, W_P12, W_P25, W_NTRI, W_WAVE, W_VOICE, W_CSAW, W_PWM };

typedef struct {
    uint8_t engine, w1, w2;
    float det, oct2, mix2;             /* sub: detune (cents), second osc (semitones), its level */
    float ratio, idx, idec, isus, fb;  /* fm: modulator ratio, index, index decay (s), index sustain, feedback */
    float bright, damp;                /* pluck: excitation brightness, loop damping */
    float atk, dec, sus, rel;          /* amp envelope (s, s, 0..1, s) */
    float fenv, fdec;                  /* filter envelope (octaves, s) */
    float chorus, wob, wob_rate;       /* chorus amount, pitch wobble (cents, Hz) */
    float noise, bass, level;          /* noise level, bass level (unused: the bass has its own), output level */
    float cut;                         /* the sound's own cutoff in Hz at CUTOFF 50 %; the knob moves it up / down. 0 = the knob alone */
    float sweep, sweep_t;              /* chip: a pitch drop at each note (semitones, + from above), how long it takes (s) */
    float pwm, pwm_rate;               /* W_PWM: how far the pulse width sweeps 0..1, how fast (Hz) */
    float steps;                       /* chip: the voices' output in so many levels (0 = smooth; 7 = about 4 bits) */
} sound_t;

typedef struct {
    uint8_t hold;                      /* 1: the chord is held (no pattern) */
    uint8_t sus;                       /* 1: FULL: the chord is held as with hold, but the transport (drums, bass) runs */
    float v[16];                       /* step velocity (0 = rest) */
    uint8_t len[16];                   /* steps the note lasts (ties) */
    float gate, swing, atk;
} rhythm_t;

typedef struct {                       /* a drum kit: the Game Boy's sound channels, as its games and trackers played drums */
    float k_f0, k_f1, k_drop, k_len;   /* kick, the wave channel (a 4-bit sine, 32 steps): from Hz, to Hz, how fast it drops (s),
                                          its length (s: the length counter cuts it) */
    float k_vol, k_env, k_click;       /* its 4-bit volume 0..15, its envelope (a step down every n/64 s; 0: none), a noise click */
    float s_clk, s_w7, s_vol, s_env, s_len, s_tone;   /* snare, the noise channel: its clock (Hz), 1: the 7-bit register (metallic,
                                                         tonal) else 15-bit, volume, envelope, length (s), a square body (Hz, 0 none) */
    float h_clk, h_w7, h_vol, h_env, h_len, o_len;    /* hats, the same noise channel (a hat cuts the snare, and back):
                                                         clock, 7-bit, volume, envelope, closed length, open length (s) */
    float p_f, p_duty, p_vol, p_env, p_len;           /* perc, the second pulse channel: Hz, duty (0.125 .. 0.5), volume,
                                                         envelope, length (s) */
    float space;                                      /* the reverb they get (the Game Boy had none: 0) */
} kit_t;

typedef struct {                       /* a drum pattern: velocities per step (0 = rest), and its kit */
    float k[16], h[16], s[16], r[16];  /* kick, hi-hat, snare / clap, perc */
    uint8_t open[16];                  /* the hi-hat rings longer on these steps */
    uint8_t roll[16];                  /* hi-hat rolls: hits in the step (0 / 1 one, 2, 3, 4) */
    float swing;                       /* the groove's own swing 0..1 (on top of GROOVE's) */
    kit_t kit;
} drums_t;

enum { BLIP_PLOP, BLIP_TOC };

enum { BASS_OFF, BASS_SUB, BASS_ROND, BASS_ACID, BASS_REESE, BASS_PLUCK, BASS_FM, BASS_TRI, BASS_PULSE, NBASS_SOUND };   /* TRI: the 8-bit
                                                                                    consoles' stepped triangle, PULSE: a 25 % pulse */
enum { MOTIF_SUIT, MOTIF_TENUE, MOTIF_CONTRE, MOTIF_OCTAVES, MOTIF_UN, MOTIF_GALOP, MOTIF_SYNCOPE, MOTIF_ROULE, NMOTIF };

void eng_init(void);
void eng_sound(const sound_t *p);
void eng_rhythm(const rhythm_t *r);
void eng_chord(const float notes[4], float bass, float glide_s);
void eng_stop(void);
void eng_lead_sound(const sound_t *p);                      /* the melody voice's own sound */
void eng_lead_params(float cut_hz, float level, float reverb, float delay);   /* its cutoff (Hz), level (1 = as the sound),
                                                                               reverb and echo sends 0..1 */
void eng_lead_mod(float glide_s, float vibrato);   /* the melody's glide (s, from note to note while it sounds) and
                                                       vibrato depth 0..1 (half a semitone at most, it fades in) */
void eng_lead(float m);               /* the melody voice: a MIDI pitch = note on, negative = release */
void eng_lead_q(float m);             /* the same, on the next sixteenth of the rhythm (now when none runs) */
void eng_lead_bend(float m);          /* the note that sounds moves to this pitch (its glide), not struck again */
/* an outside MIDI clock's beat: the even step STEP (0..15) began at AT_MS (plat_ms); STEP -1: the clock is gone */
void eng_sync(int step, uint32_t at_ms);
/* side: sidechain amount 0..1; drums_on: the drum kit plays while chords play (its kicks pump the sidechain) */
void eng_params(float bpm, float cut_hz, float q, float side, int drums_on);
/* all 0..1; crush: the bitcrusher on the whole mix (fewer bits, a lower sample rate) */
void eng_fx(float reverb, float delay, float trem, float chorus, float crush);
/* the game's sound effects, made of the music: a hit, a blast, the saucer, a wave cleared, a boss; notes: four pitches of the
 * chord that plays (MIDI, low to high); quant 1: on the next sixteenth (when the rhythm runs); level 0..1 (eng_sfx_level) */
enum { SFX_HIT, SFX_BOOM, SFX_UFO, SFX_CLEAR, SFX_BOSS, SFX_DOWN, NSFX };
void eng_sfx(int type, const float notes[4], int quant);
void eng_sfx_level(float level);
/* 1: the drums play on their own (the DRUM STUDIO's preview), with no chord */
void eng_preview(int on);
/* 0..1: the sound as heard from outside the room (muffled, quieter) */
void eng_muffle(float m);
/* a little sound effect (BLIP_*) */
void eng_blip(int kind);
/* swing 0..1, density -1..1 (fewer / more hits), gate 0..1 (0.5 = as written), strum 0..1 */
void eng_groove(float swing, float density, float gate, float strum);
/* the chords' extras: the arpeggiator (ARP_*: the chord's notes one after another, over 1 to 3 octaves; rate ARP_R_*: its own
 * clock, triplets too, lined up again at each bar; flow 0..1: short notes .. notes that ring on into the next; swing 0..1),
 * the chords' level (1 = as the sound), and the voices across the stereo field (SPREAD_*, width 0..1). With a rhythm (not
 * held) the rhythm's steps say which arp notes play (a rest: none), and how strong. */
enum { ARP_OFF, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM, ARP_UP2, ARP_DOWN2, ARP_UPDOWN2, ARP_UP3, NARP };
enum { ARP_R_4, ARP_R_4T, ARP_R_8, ARP_R_8T, ARP_R_16, ARP_R_16T, ARP_R_32, ARP_R_FAST, ARP_R_CHIP, NARP_RATE };   /* FAST, CHIP: the
                                         8-bit chord arpeggio, 30 and 60 notes a second, whatever the tempo */
enum { SPREAD_DIVERGE, SPREAD_ALTERNATE, SPREAD_DIVERGE2, SPREAD_NOTEVAL, NSPREAD };
void eng_chordfx(int arp, int rate, float flow, float swing, float width, int spread, float level);
void eng_drums(const drums_t *d, float kick, float hat, float snare);   /* snare: the snare and the perc */
void eng_bass(int sound, int motif, float level, float cut);
void eng_panic(void);
void eng_render(int32_t *lr, uint32_t n, float gain);

/* for the screen: the last step played (-1 none), when (ms since start, the engine's clock), and whether a
 * kick fell on it; eng_hit_step: the last step where the chords hit (accent 1, soft 2) */
int eng_last_step(uint32_t *at_ms, int *kick);
int eng_last_hit(uint32_t *at_ms);
float eng_beat(void);                  /* where the rhythm is in its beat, 0..1 (-1: it does not run) */
int eng_pos(int *step, float *frac, uint32_t *bar);   /* the rhythm's place: the step under way (0..15), how far into it
                                                          (0..1), the bars begun; 0 when it does not run */
float eng_level(void);                 /* output level, 0..1, smoothed */
