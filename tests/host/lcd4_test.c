/* SPDX-License-Identifier: GPL-3.0-only */
/* The instrument on the host: the settings and the patterns (defaults, the demos, validation), the
 * sequencer (tempo, length, swing, ratchets, chains, chords, the punch-in effects, MIDI clock), live
 * writing (notes, chords, lengths, slides, drum hits, no double hits), and the render (every sound and every
 * kit's every part sounds, at its knobs' extremes too; no NaN; bounded). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "lcd4.h"

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_on[16], midi_clock;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d1;
    if ((st & 0xF0u) == 0x90u && d2)
        midi_on[st & 15u]++;
    if (st == 0xF8u)
        midi_clock++;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static long frames;
static int bad;
static void render(uint32_t n, uint32_t gain)       /* n frames, in blocks of at most 256 */
{
    while (n) {
        uint32_t k = n > 256u ? 256u : n, i;
        en_render(buf, k, gain);
        for (i = 0; i < 2 * k; i++) {
            float x = (float)buf[i] / 8388608.0f;
            if (x != x || x > 1.01f || x < -1.01f)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
        frames += (long)k;
        n -= k;
    }
}
static void run(float secs) { render((uint32_t)(secs * 44100.0f), 2048); }
static void meter_reset(void)
{
    peak = sum2 = 0.0f;
    frames = 0;
    bad = 0;
}
static float rms_db(void) { return 10.0f * log10f(sum2 / (float)(2 * (frames ? frames : 1)) + 1e-12f); }

static void fresh(void)                               /* empty patterns, the default settings, stopped */
{
    int i;
    en_cfg_defaults();
    for (i = 0; i < EN_NPAT; i++)
        en_pat_clear(&en_pat[i]);
    en_init();
    run(0.05f);
}
#define STEP_FRAMES(bpm) ((uint32_t)(44100.0f * 60.0f / ((float)(bpm) * 4.0f)))

static void t_settings(void)
{
    int d;
    en_cfg_defaults();
    CHECK(en_cfg_valid(&en_cfg), "the defaults are valid");
    for (d = 0; d < EN_NDEMO; d++) {
        int p;
        en_demo(d);
        CHECK(en_cfg_valid(&en_cfg), "demo %d's settings are valid", d);
        for (p = 0; p < EN_NPAT; p++)
            CHECK(en_pat_valid(&en_pat[p]), "demo %d pattern %d is valid", d, p);
    }
    en_cfg.par[P_TEMPO] = 999;
    CHECK(!en_cfg_valid(&en_cfg), "a tempo out of range is caught");
    en_demo(0);
    en_pat[3].syn[1][2].len = 0;
    CHECK(!en_pat_valid(&en_pat[3]), "a zero length step is caught");
    en_pat[3].syn[1][2].len = 4;
    en_pat[3].fx[0] = 99;
    CHECK(!en_pat_valid(&en_pat[3]), "an unknown effect is caught");
}

static void t_demos(void)
{
    int d;
    for (d = 0; d < EN_NDEMO; d++) {
        en_demo(d);
        en_init();
        en_play(1);
        meter_reset();
        run(8.0f);
        CHECK(!bad, "demo %d: no NaN, bounded", d);
        CHECK(rms_db() > -30.0f, "demo %d sounds (%.1f dB RMS)", d, (double)rms_db());
        CHECK(peak < 0.995f, "demo %d stays under full scale at the middle of MASTER (peak %.3f)", d, (double)peak);
        en_play(0);
        run(0.2f);
    }
}

static void t_tempo_length(void)
{
    uint32_t s0;
    fresh();
    en_cfg.par[P_TEMPO] = 120;
    en_play(1);
    run(0.001f);
    s0 = en_steps;
    render(STEP_FRAMES(120) * 32u, 2048);
    CHECK(en_steps - s0 == 32u || en_steps - s0 == 31u, "32 sixteenths at 120 in their time (%u)", en_steps - s0);
    en_pat[0].len = 6;                               /* six steps: the playhead wraps at 6 */
    run(0.6f);
    {
        int seen = 0, i;
        for (i = 0; i < 40; i++) {
            render(STEP_FRAMES(120) / 2u, 2048);
            seen |= 1 << en_step;
        }
        CHECK(seen == 0x3F, "a six step pattern plays steps 0..5 (%x)", seen);
    }
    en_play(0);
}

static void t_drums_kits(void)
{
    int k, p;
    for (k = 0; k < EN_NKIT; k++)
        for (p = 0; p < EN_NPART; p++) {
            int v;
            for (v = 0; v < 3; v++) {               /* each part at its knobs' middle and extremes */
                fresh();
                en_cfg.tp[0][TP_SOUND] = (uint8_t)k;
                en_cfg.dk[k][p][0] = en_cfg.dk[k][p][1] = en_cfg.dk[k][p][2] = (uint8_t)(v * 50);
                en_cfg.dk[k][p][3] = 100;
                meter_reset();
                en_hit(p, v == 2);
                run(0.4f);
                CHECK(!bad && peak > 0.01f, "kit %s part %s (knobs %d) sounds (peak %.4f)", EN_KIT_NAME[k],
                      en_part_name(k, p), v * 50, (double)peak);
            }
        }
}

static void t_sounds(void)
{
    int s, corner;
    for (s = 0; s < EN_NSOUND; s++)
        for (corner = 0; corner < 4; corner++) {     /* the defaults, all 0, all 100, alternating */
            int k, i;
            fresh();
            en_cfg.tp[1][TP_SOUND] = (uint8_t)s;
            for (k = 0; k < 4; k++)
                en_cfg.sk[0][s][k] = corner == 0 ? en_sound(s)->def[k] : corner == 1 ? 0 : corner == 2 ? 100 : (k & 1) * 100;
            for (i = 0; i < 16; i++) {               /* a line, a chord on every fourth step */
                en_step_t *e = &en_pat[0].syn[0][i];
                e->note[0] = (uint8_t)(36 + (i * 7) % 36);
                if ((i & 3) == 0) {
                    e->note[1] = (uint8_t)(e->note[0] + 4);
                    e->note[2] = (uint8_t)(e->note[0] + 7);
                }
                e->len = (uint8_t)(2 + (i % 5) * 3);
                e->fl = (uint8_t)((i % 3 == 0 ? EN_F_ACC : 0) | (i % 4 == 1 ? EN_F_SLIDE : 0));
            }
            en_play(1);
            meter_reset();
            run(2.5f);
            CHECK(!bad, "%s, knobs %d: no NaN, bounded", en_sound(s)->name, corner);
            CHECK(peak > 0.003f, "%s, knobs %d: it sounds (peak %.4f)", en_sound(s)->name, corner, (double)peak);
            en_play(0);
            run(0.3f);
            if (corner == 0) {                       /* and quiet again (only the tails of the sends) */
                meter_reset();
                run(6.0f);
                meter_reset();
                run(0.5f);
                CHECK(peak < 0.003f, "%s dies away when stopped (peak %.4f)", en_sound(s)->name, (double)peak);
            }
        }
}

static void t_chain_chords(void)
{
    int seen[3] = {0, 0, 0}, i;
    fresh();
    en_cfg.chain_n = 3;
    en_cfg.chain[0] = 2;
    en_cfg.chain[1] = 5;
    en_cfg.chain[2] = 5;
    en_cfg.chords_n = 2;
    en_cfg.chords[0] = 5;                            /* Am */
    en_cfg.chords[1] = 4;                            /* G */
    en_cfg.par[P_TEMPO] = 240;
    en_play(1);
    run(0.01f);
    CHECK(en_pat_now == 2, "the chain starts on its first pattern (%d)", en_pat_now);
    CHECK(en_chord_now == 5, "the chords start on their first (%d)", en_chord_now);
    for (i = 0; i < 60; i++) {
        render(STEP_FRAMES(240) * 4u, 2048);
        if (en_pat_now == 2)
            seen[0]++;
        if (en_pat_now == 5)
            seen[1]++;
        if (en_chord_now == 4)
            seen[2]++;
    }
    CHECK(seen[0] > 10 && seen[1] > 2 * seen[0] - 8, "the chain plays 2, 5, 5 (%d, %d)", seen[0], seen[1]);
    CHECK(seen[2] > 10, "the chords move on with the patterns (%d)", seen[2]);
    /* following the chords: C4 in Am is A3; E (the third) is bent to C */
    {
        int n;
        en_cfg.tp[1][TP_FOLLOW] = 1;
        while (en_chord_now != 5)
            render(256, 2048);
        n = en_transpose(1, 60);
        CHECK(n == 57, "C4 follows Am to A3 (%d)", n);
        n = en_transpose(1, 64);
        CHECK(n == 60, "E4, Am's third: C4 (%d)", n);
        en_cfg.tp[1][TP_FOLLOW] = 0;
        n = en_transpose(1, 64);
        CHECK(n == 64, "not following: as written (%d)", n);
        en_cfg.par[P_KEY] = 2;
        CHECK(en_transpose(1, 60) == 62, "Key D moves it up a tone");
        en_cfg.par[P_KEY] = 9;
        CHECK(en_transpose(1, 60) == 57, "Key A moves it down a minor third");
    }
    en_play(0);
    {   /* the chord names and tones */
        char b[10];
        int pc[4], k;
        en_chord_name(9, 0, b);
        CHECK(!strcmp(b, "Dm7"), "chord 10 is Dm7 (%s)", b);
        en_chord_name(9, 2, b);
        CHECK(!strcmp(b, "Em7"), "in D, Em7 (%s)", b);
        k = en_chord_tones(4, 0, pc);
        CHECK(k == 3 && pc[0] == 7 && pc[1] == 11 && pc[2] == 2, "G is G B D");
    }
}

static void t_pattern_switch(void)
{
    uint32_t t;
    fresh();
    en_cfg.par[P_TEMPO] = 200;
    en_play(1);
    run(0.01f);
    en_pattern(7);                                   /* while playing: at the end of this pattern */
    render(STEP_FRAMES(200) * 4u, 2048);
    CHECK(en_pat_now == 0, "the next pattern waits for the end of this one (%d)", en_pat_now);
    for (t = 0; t < 20 && en_pat_now != 7; t++)
        render(STEP_FRAMES(200) * 2u, 2048);
    CHECK(en_pat_now == 7, "... then it plays (%d)", en_pat_now);
    en_play(0);
    run(0.01f);
    en_pattern(3);                                   /* stopped: at once */
    run(0.01f);
    CHECK(en_pat_now == 3, "stopped, a pattern is taken at once (%d)", en_pat_now);
}

static void t_ratchet_swing(void)
{
    uint8_t c0;
    uint32_t s;
    fresh();
    en_cfg.tp[1][TP_SOUND] = SND_ARCADE;
    en_pat[0].syn[0][0].note[0] = 60;
    en_pat[0].syn[0][0].fl = 3u << 4;                /* x4 */
    en_cfg.par[P_TEMPO] = 120;
    en_play(1);
    c0 = en_trk_on[1];
    render(STEP_FRAMES(120) - 100u, 2048);
    CHECK((uint8_t)(en_trk_on[1] - c0) == 4, "a x4 ratchet plays four notes in its step (%d)", (uint8_t)(en_trk_on[1] - c0));
    en_play(0);
    run(0.1f);
    /* swing: an off-beat comes late */
    fresh();
    en_cfg.tp[1][TP_SOUND] = SND_ARCADE;
    en_pat[0].syn[0][1].note[0] = 60;
    en_cfg.par[P_SWING] = 100;
    en_cfg.par[P_TEMPO] = 120;
    en_play(1);
    c0 = en_trk_on[1];
    render(STEP_FRAMES(120) + STEP_FRAMES(120) / 3u, 2048);
    CHECK(en_trk_on[1] == c0, "with full swing, step 2 has not played a third of the way into it");
    render(STEP_FRAMES(120) / 3u, 2048);
    CHECK(en_trk_on[1] != c0, "... and it has two thirds in");
    s = en_steps;
    (void)s;
    en_play(0);
}

static void t_fx(void)
{
    int i, seen = 0;
    fresh();
    en_cfg.par[P_TEMPO] = 200;
    en_play(1);
    render(STEP_FRAMES(200) * 5u + 100u, 2048);      /* into step 5 */
    en_fx(FX_RPT16);
    for (i = 0; i < 12; i++) {
        render(STEP_FRAMES(200), 2048);
        seen |= 1 << en_step;
    }
    CHECK(seen == (1 << 6) || seen == (1 << 5), "Repeat 1/16 holds one step (%x)", seen);
    en_fx(FX_REVERSE);
    {
        int a, b;
        render(STEP_FRAMES(200) * 2u, 2048);
        a = en_step;
        render(STEP_FRAMES(200), 2048);
        b = en_step;
        CHECK((a - b + 16) % 16 == 1, "Reverse plays backward (%d then %d)", a, b);
    }
    en_fx(FX_OCTUP);
    render(STEP_FRAMES(200), 2048);
    CHECK(en_transpose(1, 60) == 72, "Octave up");
    en_fx(0);
    render(STEP_FRAMES(200), 2048);
    CHECK(en_fx_now == 0 && en_transpose(1, 60) == 60, "the effect let go: as written");
    /* written: an effect held while writing goes onto the steps */
    en_rec(1);
    en_fx(FX_HALF);
    render(STEP_FRAMES(200) * 3u, 2048);
    en_fx(0);
    en_rec(0);
    render(STEP_FRAMES(200), 2048);
    {
        int n = 0;
        for (i = 0; i < 16; i++)
            n += en_pat[0].fx[i] == FX_HALF;
        CHECK(n >= 2 && n <= 4, "an effect held while writing is written on its steps (%d)", n);
    }
    en_play(0);
    run(0.2f);
    for (i = FX_RPT16; i <= FX_ECHO; i++) {          /* every effect: bounded, no NaN */
        fresh();
        en_demo(0);
        en_init();
        en_play(1);
        en_fx(i);
        meter_reset();
        run(2.0f);
        CHECK(!bad && peak > 0.01f, "effect %s plays and stays bounded (peak %.3f)", EN_FX_NAME[i], (double)peak);
        en_fx(0);
        en_play(0);
    }
}

static void t_live_write(void)
{
    const uint32_t SF = STEP_FRAMES(100);
    int s, hits = 0;
    uint8_t c0;
    fresh();
    en_cfg.par[P_TEMPO] = 100;
    en_cfg.tp[2][TP_SOUND] = SND_POLY;
    en_rec(1);
    en_play(1);
    render(SF * 3u + SF / 10u, 2048);                /* a tenth into step 3: the note goes on step 3 */
    en_note(2, 64, 1, 0);
    en_note(2, 67, 1, 1);                            /* and a second key with it: a chord, accented */
    render(SF * 2u, 2048);                           /* held two steps */
    en_note(2, 64, 0, 0);
    en_note(2, 67, 0, 0);
    render(SF * 2u + SF * 7u / 10u, 2048);           /* seven tenths into step 7: the next step (8) */
    en_note(2, 72, 1, 0);
    render(SF / 2u, 2048);
    en_note(2, 72, 0, 0);
    render(SF, 2048);
    {
        const en_step_t *e = &en_pat[0].syn[1][3], *f = &en_pat[0].syn[1][8];
        CHECK(e->note[0] == 64 && e->note[1] == 67, "the chord written on step 3 (%d %d)", e->note[0], e->note[1]);
        CHECK(e->len >= 7 && e->len <= 9, "... two steps long (%d quarter steps)", e->len);
        CHECK(e->fl & EN_F_ACC, "... accented");
        CHECK(f->note[0] == 72 && f->len <= 3, "the late note went on the next step, short (%d, %d)", f->note[0], f->len);
        CHECK(en_rec_count == 3, "three notes written (%u)", en_rec_count);
    }
    /* the drums: a hit near a step's end goes on the next, and does not play twice */
    for (s = 0; s < 16; s++)
        en_pat[0].hit[0] = 0;
    while (en_step != 10)
        render(64, 2048);
    render(SF * 8u / 10u, 2048);
    c0 = en_part_on[1];
    en_hit(1, 0);
    render(SF, 2048);
    hits = (uint8_t)(en_part_on[1] - c0);
    CHECK(en_pat[0].hit[1] >> 11 & 1u, "the late snare went on step 11 (%x)", en_pat[0].hit[1]);
    CHECK(hits == 1, "... and did not play again on it (%d)", hits);
    en_rec(0);
    en_play(0);
    run(0.1f);
    {   /* a slide: a key over a key while writing (the 303's way) */
        fresh();
        en_cfg.tp[1][TP_SOUND] = SND_ACID;
        en_rec(1);
        en_play(1);
        render(SF / 10u, 2048);
        en_note(1, 36, 1, 0);
        render(SF * 2u, 2048);
        en_note(1, 43, 1, 0);
        render(SF, 2048);
        en_note(1, 36, 0, 0);
        en_note(1, 43, 0, 0);
        render(SF, 2048);
        CHECK(en_pat[0].syn[0][0].fl & EN_F_SLIDE, "the first note slides into the second (%x)", en_pat[0].syn[0][0].fl);
        en_rec(0);
        en_play(0);
    }
}

static void t_midi_clock(void)
{
    int i;
    uint32_t s0;
    fresh();
    en_clock(EN_CLK_START);
    render(64, 2048);
    CHECK(en_playing && en_ext, "MIDI start plays, on the external clock");
    s0 = en_steps;
    for (i = 0; i < 6 * 16; i++) {                   /* 16 steps of clock, at 150 BPM (a pulse every 735 frames) */
        en_clock(EN_CLK_TICK);
        render(735, 2048);
    }
    CHECK(en_steps - s0 >= 15 && en_steps - s0 <= 17, "96 pulses are 16 steps (%u)", en_steps - s0);
    en_clock(EN_CLK_STOP);
    render(64, 2048);
    CHECK(!en_playing, "MIDI stop stops");
    for (i = 0; i < 300; i++)
        render(256, 2048);
    CHECK(!en_ext, "the clock gone: our own again");
    en_cfg.par[P_MIDI] = 1;                          /* MIDI out: our clock */
    midi_clock = 0;
    en_play(1);
    run(1.0f);
    CHECK(midi_clock > 40 && midi_clock < 56, "MIDI clock out, 24 a beat at 120 (%u in a second)", midi_clock);
    en_play(0);
}

static void t_mutes(void)
{
    fresh();
    en_demo(0);
    en_init();
    en_cfg.mute = 0x0F;
    en_play(1);
    run(4.0f);                                       /* the delay's and the plate's tails die away */
    meter_reset();
    run(1.0f);
    CHECK(peak < 0.02f, "all four muted: (nearly) silent (%.4f)", (double)peak);
    en_cfg.mute = 0x0E;
    meter_reset();
    run(1.0f);
    CHECK(peak > 0.05f, "the drums alone sound (%.4f)", (double)peak);
    en_play(0);
}

int main(void)
{
    t_settings();
    t_demos();
    t_tempo_length();
    t_pattern_switch();
    t_chain_chords();
    t_ratchet_swing();
    t_fx();
    t_live_write();
    t_midi_clock();
    t_mutes();
    t_drums_kits();
    t_sounds();
    printf("%s (%d failures)\n", fails ? "LCD-4 ENGINE TEST FAILED" : "LCD-4 engine test passed", fails);
    return fails != 0;
}
