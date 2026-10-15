/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument: a four-track pocket groovebox, after the little synths that live in a calculator's
 * body and an LCD game's screen. One drum track, three synth tracks; 16-step patterns that hold all four;
 * chains of patterns; punch-in effects; chords that the synth tracks can follow.
 *
 *   DRUMS   four kits of eleven parts each, synthesized as the machines did it: an 808, a 909, an arcade
 *           kit (pulse kicks, noise-channel snares, blips and zaps) and a rhythm box (a CR-78 and a 606).
 *   SYNTHS  sixteen sounds, each a model of its original's circuit, not a sample:
 *             basses  Acid (TB-303), Mono (SH-101), Fat (Minimoog), Boom (an 808 sub), FM bass (DX7), Reese
 *             keys    Poly (Juno, its chorus), Brass (Prophet / OB), E.Piano (DX7), Organ (M1's house
 *                     organ), Strings (Solina, its ensemble), Bells (DX / PPG)
 *             leads   Sync, Scream (MS-20), Arcade (NES / Game Boy / PO-20 chip sounds), Arp (chip arps)
 *           any track can load any sound; mono sounds glide and slide, the keys are four voices.
 *   FX      sixteen punch-in effects (held, or written into a pattern's steps); a ping-pong delay and a
 *           plate (Dattorro's) on sends; a bus compressor (Punch).
 *
 * Threads: en_render() runs in the audio interrupt. The patterns and the settings (en_cfg, en_pat) are
 * shared: the main loop writes them, the render reads them (a byte or a word at a time; a step edited while
 * it plays sounds as one or the other). Everything else is a command, drained at the start of a block; the
 * state marked (UI) is written by the render and only read elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NSTEP 16                    /* a pattern: 16 sixteenths (fewer with Length) */
#define EN_NTRACK 4                    /* the drums, then three synth tracks */
#define EN_NSYN 3
#define EN_NPAT 16
#define EN_CHAIN_MAX 64
#define EN_NPART 11                    /* a kit's parts: the black keys */
#define EN_NKIT 4
#define EN_NSOUND 16
#define EN_NFX 16
#define EN_NCHORD 16
#define EN_NNOTE 4                     /* notes in a synth step (a chord) */
#define EN_NOLOCK 0xFFu

extern const char *const EN_NOTE_NAME[12];

/* ---------------------------------------------------------------- the sounds --- */
enum { KIT_808, KIT_909, KIT_ARCADE, KIT_BOX };
extern const char *const EN_KIT_NAME[EN_NKIT];
const char *en_part_name(int kit, int part);       /* "BD", "SD", ... (short, for the screen) */
const char *en_part_long(int kit, int part);       /* "Bass drum", ... */

enum { SND_ACID, SND_MONO, SND_FAT, SND_BOOM, SND_FMBASS, SND_REESE, SND_POLY, SND_BRASS, SND_EPIANO,
       SND_ORGAN, SND_STRINGS, SND_BELLS, SND_SYNC, SND_SCREAM, SND_ARCADE, SND_ARP };
typedef struct {
    const char *name;                  /* "Acid" */
    const char *after;                 /* what it models: "TB-303" */
    const char *knob[4];               /* its four knobs (A and B can be locked on a step) */
    uint8_t def[4];
    uint8_t poly;                      /* voices: 1 mono (glides, slides), 4 the keys */
} en_sound_t;
const en_sound_t *en_sound(int s);
extern const char *const EN_DRUM_KNOB[4];          /* the parts' knobs: Tune, Decay, Tone, Level */
void en_sound_text(int snd, int k, int v, char *buf);   /* a sound's knob as the screen shows it (10 bytes) */
int en_sound_zones(int snd, int k);                /* positions, for a knob that is a switch (0: continuous) */
void en_drum_text(int k, int v, char *buf);
extern const uint8_t EN_DRUM_DEF[EN_NKIT][EN_NPART][4];

/* the punch-in effects (1..16; 0 none), on the white keys with FX */
enum { FX_NONE, FX_RPT16, FX_RPT8, FX_RPT4, FX_STUTTER, FX_REVERSE, FX_HALF, FX_GLITCH, FX_FILL, FX_OCTUP,
       FX_OCTDN, FX_SOLODRUM, FX_SOLOSYN, FX_LOWPASS, FX_HIPASS, FX_CRUSH, FX_ECHO };
extern const char *const EN_FX_NAME[EN_NFX + 1];

/* the chords (on the white keys with ARP): in C, moved by Key */
typedef struct {
    const char *name;                  /* in C: "Dm7" */
    uint8_t root;                      /* pitch class */
    uint8_t q;                         /* CQ_* */
} en_chord_t;
enum { CQ_MAJ, CQ_MIN, CQ_DIM, CQ_SUS4, CQ_DOM7, CQ_MAJ7, CQ_MIN7 };
extern const en_chord_t EN_CHORD[EN_NCHORD];
void en_chord_name(int c, int key, char *buf);     /* "F#m7" (8 bytes) */

/* ------------------------------------------------------------ the settings --- */
/* global parameters */
enum {
    P_TEMPO, P_SWING, P_KEY,                       /* the pattern page */
    P_DTIME, P_DFB, P_SPACE, P_PUNCH,              /* the FX page: the delay's time and feedback, the plate, the bus */
    P_TUNE, P_MIDI, P_CLOCK,                       /* setup */
    P_NPARAMS
};
/* per track */
enum { TP_SOUND, TP_LEVEL, TP_PAN, TP_DSEND, TP_RSEND, TP_FOLLOW, TP_N };
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
const en_param_t *en_tparam_info(int i);
void en_param_text(int i, int v, char *buf);       /* the value as the screen shows it (10 bytes) */
void en_tparam_text(int t, int i, int v, char *buf);

typedef struct {
    int16_t par[P_NPARAMS];
    uint8_t tp[EN_NTRACK][TP_N];
    uint8_t sk[EN_NSYN][EN_NSOUND][4];             /* every synth track's knobs, for every sound (0..100) */
    uint8_t dk[EN_NKIT][EN_NPART][4];              /* every kit's parts: tune, decay, tone, level (0..100) */
    uint8_t chain[EN_CHAIN_MAX], chain_n;          /* the pattern chain (chain_n 0: the pattern alone) */
    uint8_t chords[EN_CHAIN_MAX], chords_n;        /* the chord chain, a chord a pattern (0: none) */
    uint8_t pat;                                   /* the pattern selected */
    uint8_t mute;                                  /* bit t: track t muted */
    uint8_t rsv[6];
} en_cfg_t;
extern en_cfg_t en_cfg;

/* ------------------------------------------------------------ the patterns --- */
enum { EN_F_ACC = 1, EN_F_SLIDE = 2 };             /* a step's flags; bits 4-5: the ratchet (x1..x4) */
#define EN_RATCHET(fl) ((((fl) >> 4) & 3u) + 1u)
typedef struct {
    uint8_t note[EN_NNOTE];            /* MIDI notes, ascending, 0 = none; note[0] == 0: the step is empty */
    uint8_t len;                       /* the gate, in quarter steps (1..64) */
    uint8_t fl;
    uint8_t lock[2];                   /* knob A and B on this step (0..100), EN_NOLOCK: the knob's own */
} en_step_t;
typedef struct {
    uint16_t hit[EN_NPART];            /* the drums: bit s = step s */
    uint16_t acc;                      /* the drums' accents */
    uint8_t dlock[2][EN_NSTEP];        /* the drums' tune and decay on a step: 50 is none, EN_NOLOCK none */
    uint8_t fx[EN_NSTEP];              /* effects written on steps (FX_*) */
    uint8_t len;                       /* steps in the pattern, 1..16 */
    uint8_t rsv[3];
    en_step_t syn[EN_NSYN][EN_NSTEP];
} en_pat_t;
extern en_pat_t en_pat[EN_NPAT];

void en_cfg_defaults(void);
void en_pat_clear(en_pat_t *p);
int en_cfg_valid(const en_cfg_t *c);
int en_pat_valid(const en_pat_t *p);

/* ---------------------------------------------------------------- commands --- */
/* (main loop -> render) */
void en_init(void);                    /* the settings and patterns are the caller's: en_cfg_defaults() */
void en_play(int on);                  /* run from the chain's top (or the pattern) / stop */
void en_pattern(int p);                /* play pattern p next: at the end of this one, at once if stopped */
void en_chain_restart(void);           /* the chain (just edited) from its top, at the end of this pattern */
void en_note(int trk, int note, int on, int acc);   /* a synth track's key (recorded while writing) */
void en_hit(int part, int acc);        /* a drum part (recorded while writing) */
void en_rec(int on);                   /* write: keys, knobs A and B and effects go into the pattern */
void en_fx(int fx);                    /* the punch-in effect held (0: none) */
void en_knob(int trk, int k);          /* knob A or B (k 0, 1) of a track just turned: written while writing */
void en_panic(void);
/* MIDI clock in: ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);
/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* ------------------------------------------------------ state for the screen --- */
extern volatile uint8_t en_playing, en_ext, en_recording;
extern volatile uint8_t en_step;                   /* the step playing (after any effect) */
extern volatile uint8_t en_tick;                   /* 0..23 in the step */
extern volatile uint8_t en_pat_now, en_pat_next;   /* the pattern playing; the one that follows */
extern volatile uint8_t en_chain_pos;              /* where in the chain */
extern volatile uint8_t en_chord_now;              /* the chord playing (0xFF none) */
extern volatile uint8_t en_fx_now;                 /* the effect sounding (held or written) */
extern volatile uint32_t en_steps;                 /* steps played since start (for the scenes, the tests) */
extern volatile uint8_t en_trk_on[EN_NTRACK];      /* counts each track's notes (the scenes move on a change) */
extern volatile uint8_t en_trk_note[EN_NTRACK];    /* the last note played (drums: the part) */
extern volatile uint8_t en_trk_acc[EN_NTRACK];     /* ... and whether accented */
extern volatile uint8_t en_gates;                  /* bit t: a note of track t is held */
extern volatile uint8_t en_part_on[EN_NPART];      /* counts each drum part's hits */
extern volatile float en_level[EN_NTRACK];         /* each track's loudness now, 0..1 (meters) */
extern volatile uint32_t en_rec_count;             /* notes written live (tests) */
/* where the render's time goes (the device's SysEx reports it): a clock the app may give, the time spent in
 * each part since it was last cleared, in its ticks */
enum { PR_SEQ, PR_DRUMS, PR_TRK1, PR_TRK2, PR_TRK3, PR_SENDS, PR_MASTER, PR_OTHER, PR_N };
extern uint32_t (*en_time)(void);
extern volatile uint32_t en_prof[PR_N];

/* the pieces of the instrument the tests and the screen use */
int en_transpose(int trk, int note);   /* a written note as it sounds now (key, chord, effects) */
int en_chord_tones(int c, int key, int *pc);   /* chord c's pitch classes (3 or 4) */
void en_demo(int which);               /* fills the patterns and settings with a demo song (0..EN_NDEMO-1) */
#define EN_NDEMO 4
extern const char *const EN_DEMO_NAME[EN_NDEMO];
