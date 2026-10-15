/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4 app: the project (everything that is saved) and the app's entry points. Included by the unity
 * build after gfx.c (drawing helpers in scope) and by the host simulator. */
#pragma once
#include <stdint.h>
#include "plat.h"
#include "../dsp/lcd4.h"

#ifndef EN_VERSION                       /* a release build passes its own (tools/build.py --release) */
#define EN_VERSION "DEV"
#endif
#define PROJ_MAGIC 0x3444434Cu           /* "LCD4" (IND-1's, at the same place in flash, is "IND1") */
#define BANK_MAGIC 0x4B42344Cu           /* "L4BK" */
#define PROJ_FORMAT 1u
#define NBANK 4                          /* the patterns, in four flash objects of four */
#define BANK_PATS (EN_NPAT / NBANK)

/* the settings and the player's things (OBJ_PROJ) */
typedef struct {
    uint32_t magic, format;
    en_cfg_t cfg;
    uint8_t track;                       /* the track selected */
    uint8_t part;                        /* the drum part selected */
    int8_t oct[EN_NSYN];                 /* the keyboard's octave, each synth track (-3..3) */
    uint8_t leds;                        /* lights: 0 off, 1 keys and the buttons' glow, 2 keys only */
    uint8_t rsv[24];
} project_t;

/* four patterns (OBJ_PAT0 + bank) */
typedef struct {
    uint32_t magic, format, bank;
    en_pat_t pat[BANK_PATS];
} bank_t;

extern project_t proj;

void project_defaults(int demo);         /* a demo song (0 the first one) and the player's defaults */
int project_load(void);                  /* 0 = loaded; else the first demo is in place */
int project_save(void);                  /* the parts marked dirty: 0 = saved and verified */
void project_dirty(int what);            /* PD_CFG, or a pattern's bank: PD_PAT(p) */
int project_is_dirty(void);
#define PD_CFG 1u
#define PD_PAT(p) (2u << ((p) / BANK_PATS))
#define PD_ALL 0x1Fu

void ui_init(void);
void ui_frame(void);
void ui_input_only(void);
int ui_dirty(void);
void ui_say(const char *a, const char *b);
