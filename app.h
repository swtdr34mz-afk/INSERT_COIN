/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Obsimo (OSR Records)
 * Copyright (C) 2026 swtdr34mz (PIXEL RUN) */
/* PIXEL RUN by swtdr34mz (CHORD RUN's chiptune sibling, based on BLOP by Obsimo): game-style chord progressions on the black
 * keys, a melody on the white ones, chip sounds and drums, a drum studio, a looper and a runner that runs while you play,
 * for the M-VAVE FM-1 (on fm1-base).
 * This header is the app's half of the contract with the platform (device/unity.c, host/plat_host.c). */
#pragma once
#include <stdint.h>
#include "plat.h"

#define APP_NAME "PIXEL RUN"
#define APP_SPLASH_RGB RGB(255, 200, 40)
#define APP_SPLASH                      /* app_splash: the intro's first picture, not the platform's white page */
#define APP_TICK                        /* app_tick: the MIDI in, each clock timed to the ms */
#define APP_BLOCK 256u                   /* frames per audio block: 5.8 ms at 44.1 kHz */

/* the store: the app's settings, the panel calibration (the platform's), the player's progress (metres run, missions:
 * apart from the settings, so a new settings layout never takes it away) and the DRUM STUDIO's 20 user grooves, and their
 * flash offsets (each object two sectors) */
enum { OBJ_SETTINGS, OBJ_PANEL, OBJ_PROGRESS, OBJ_DRUMS, OBJ_NOBJ };
#define APP_STORE_MAP {0xFC000u, 0xDC000u, 0xDE000u, 0xDA000u}

/* backup and restore over USB: the settings, the progress and the user grooves */
#define APP_BACKUP_OBJS {OBJ_SETTINGS, OBJ_PROGRESS, OBJ_DRUMS}
#define APP_BACKUP_MAGIC 0x4E555250u            /* "PRUN" */
#define APP_BACKUP_VERSION 1u
#define APP_BACKUP_WHAT "replaces settings, progress, grooves"
int app_backup_playing(void);
void app_backup_flush(void);
void app_backup_hold(int on);
void app_backup_restored(void);

void app_init(void);
void app_render(int32_t *lr, uint32_t n, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *a, const char *b);
void app_splash(void);
void app_tick(void);

#define APP_HOST_EXPECT
int app_expect(const char *what, int *got);
