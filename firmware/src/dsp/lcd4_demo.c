/* SPDX-License-Identifier: GPL-3.0-only */
/* The demo songs: what LCD-4 plays the first time it starts, and three more (Setup: Demo). Written in C
 * and left to the chords. Included once, by lcd4.c.
 *
 * A drum row is 16 characters, x a hit, X an accented one. A synth line is 16 tokens: "." a rest, a note
 * ("C3", "Eb2", "F#4"), notes joined by "+" for a chord; after it "!" accents, "~" slides into the next,
 * ":N" holds it N sixteenths, "*N" ratchets it (2..4). */
const char *const EN_DEMO_NAME[EN_NDEMO] = {"Pocket", "Arcade", "House", "Lo-fi"};

static void d_drum(en_pat_t *p, int part, const char *row)
{
    int s;
    for (s = 0; s < EN_NSTEP && row[s]; s++)
        if (row[s] == 'x' || row[s] == 'X') {
            p->hit[part] |= (uint16_t)(1u << s);
            if (row[s] == 'X')
                p->acc |= (uint16_t)(1u << s);
        }
}

static int d_note(const char **ps)                /* "C#3" -> 49; 0 if not a note */
{
    static const int8_t L[7] = {9, 11, 0, 2, 4, 5, 7};   /* A B C D E F G */
    const char *s = *ps;
    int n, oct = 0, neg = 0;
    if (*s < 'A' || *s > 'G')
        return 0;
    n = L[*s++ - 'A'];
    if (*s == '#') {
        n++;
        s++;
    } else if (*s == 'b') {
        n--;
        s++;
    }
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9')
        oct = oct * 10 + (*s++ - '0');
    *ps = s;
    return 12 * ((neg ? -oct : oct) + 1) + n;
}

static void d_line(en_pat_t *p, int t, const char *line)
{
    const char *s = line;
    int step = 0;
    while (*s && step < EN_NSTEP) {
        en_step_t *e = &p->syn[t][step];
        while (*s == ' ')
            s++;
        if (!*s)
            break;
        if (*s == '.') {
            s++;
        } else {
            int k = 0, n;
            while ((n = d_note(&s)) != 0) {
                if (k < EN_NNOTE)
                    e->note[k++] = (uint8_t)n;
                if (*s == '+')
                    s++;
                else
                    break;
            }
            e->len = 3;
            for (;;) {
                if (*s == '!') {
                    e->fl |= EN_F_ACC;
                    s++;
                } else if (*s == '~') {
                    e->fl |= EN_F_SLIDE;
                    s++;
                } else if (*s == ':') {
                    int v = 0;
                    s++;
                    while (*s >= '0' && *s <= '9')
                        v = v * 10 + (*s++ - '0');
                    e->len = (uint8_t)clampi(v * 4, 1, 64);
                } else if (*s == '*') {
                    s++;
                    e->fl |= (uint8_t)((clampi(*s++ - '0', 1, 4) - 1) << 4);
                } else {
                    break;
                }
            }
        }
        while (*s && *s != ' ')
            s++;
        step++;
    }
}

static void d_knobs(int t, int snd, int a, int b, int c, int d)
{
    en_cfg.sk[t][snd][0] = (uint8_t)a;
    en_cfg.sk[t][snd][1] = (uint8_t)b;
    en_cfg.sk[t][snd][2] = (uint8_t)c;
    en_cfg.sk[t][snd][3] = (uint8_t)d;
}

static void d_tracks(int kit, int s1, int s2, int s3)
{
    en_cfg.tp[0][TP_SOUND] = (uint8_t)kit;
    en_cfg.tp[1][TP_SOUND] = (uint8_t)s1;
    en_cfg.tp[2][TP_SOUND] = (uint8_t)s2;
    en_cfg.tp[3][TP_SOUND] = (uint8_t)s3;
}

void en_demo(int which)
{
    int i;
    for (i = 0; i < EN_NPAT; i++)
        en_pat_clear(&en_pat[i]);
    en_cfg_defaults();
    switch (which) {
    default:
    case 0: {   /* Pocket: an 808, a 303, a Juno's stabs, a chip lead, over Am F C G */
        en_pat_t *a = &en_pat[0], *b = &en_pat[1];
        static const uint8_t CH[4] = {5, 3, 0, 4};
        en_cfg.par[P_TEMPO] = 118;
        en_cfg.par[P_SWING] = 24;
        d_tracks(KIT_808, SND_ACID, SND_POLY, SND_ARCADE);
        d_drum(a, 0, "X.....x...x.....");
        d_drum(a, 1, "....X.......X...");
        d_drum(a, 7, "x.x.x.x.x.x.x.x.");
        d_drum(a, 8, "..............x.");
        d_drum(a, 6, "............x...");
        d_line(a, 0, "C2 C2 C3! . C2 . G2~ C3 C2 . C2! Eb2 . G2 C3~ Bb2");
        d_line(a, 1, ". . C4+E4+G4 . . . C4+E4+G4 . . . C4+E4+G4 . . . . .");
        d_line(a, 2, "E5:2 . G5 . A5:2 . G5 E5 D5:2 . E5:2 . . C5 .");
        d_drum(b, 0, "X.....x...x...x.");
        d_drum(b, 1, "....X.......X...");
        d_drum(b, 7, "xxx.x.xxx.x.x.xx");
        d_drum(b, 8, "......x.......x.");
        d_drum(b, 5, "..x.......x..x..");
        d_drum(b, 10, "...x.......x....");
        d_drum(b, 6, "....x.......x..x");
        d_line(b, 0, "C2 C3! C2 . C2~ Eb2 G2 . C2 C3! . C2 Bb2~ G2 F2 Eb2");
        d_line(b, 1, ". . C4+E4+G4 . . . C4+E4+G4 . . C4+E4+G4 . . C4+E4+G4:2 . . .");
        d_line(b, 2, "G5 . A5 G5 E5 . D5 . E5 . G5 . C6:3 . . .");
        d_knobs(0, SND_ACID, 28, 78, 62, 35);
        d_knobs(1, SND_POLY, 55, 30, 0, 25);
        d_knobs(2, SND_ARCADE, 30, 20, 55, 0);
        en_cfg.tp[1][TP_LEVEL] = 74;
        en_cfg.tp[1][TP_DSEND] = 18;
        en_cfg.tp[2][TP_LEVEL] = 80;
        en_cfg.tp[2][TP_RSEND] = 30;
        en_cfg.tp[3][TP_LEVEL] = 74;
        en_cfg.tp[3][TP_DSEND] = 30;
        en_cfg.tp[3][TP_PAN] = 62;
        en_cfg.chain_n = 4;
        en_cfg.chain[0] = 0;
        en_cfg.chain[1] = 0;
        en_cfg.chain[2] = 1;
        en_cfg.chain[3] = 1;
        en_cfg.chords_n = 4;
        for (i = 0; i < 4; i++)
            en_cfg.chords[i] = CH[i];
        break;
    }
    case 1: {   /* Arcade: the console's kit, a triangle bass, an arp, a pulse lead with vibrato */
        en_pat_t *a = &en_pat[0], *b = &en_pat[1];
        static const uint8_t CH[4] = {0, 5, 3, 4};
        en_cfg.par[P_TEMPO] = 140;
        d_tracks(KIT_ARCADE, SND_ARCADE, SND_ARP, SND_ARCADE);
        d_drum(a, 0, "x.......x.x.....");
        d_drum(a, 1, "....x.......x...");
        d_drum(a, 7, "x.x.x.x.x.x.x.x.");
        d_drum(a, 5, "...............x");
        d_line(a, 0, "C2 . C3 . C2 . C3 . C2 . C3 . C2 C3 G2 E2");
        d_line(a, 1, "C4:4 . . . . . . . C4:4 . . . . . . .");
        d_line(a, 2, "C5:2 . E5 G5 C6:2 . B5 . G5:3 . . E5 . D5 E5");
        d_drum(b, 0, "x.......x.x...x.");
        d_drum(b, 1, "....x.......x.xx");
        d_drum(b, 7, "x.xxx.x.x.xxx.x.");
        d_drum(b, 6, "............x...");
        d_drum(b, 10, "x...............");
        d_line(b, 0, "C2 . C3 . C2 . C3 . C2 C3 . C2 G2 . E2 D2");
        d_line(b, 1, "C4:2 . C4:2 . C4:4 . . . C4:2 . C4:2 . C4*4:4 . . .");
        d_line(b, 2, "E5 G5 C6 . B5 . G5 . A5:2 . G5 E5 G5:4 . . .");
        d_knobs(0, SND_ARCADE, 70, 0, 45, 0);            /* the triangle */
        d_knobs(1, SND_ARP, 0, 40, 30, 25);
        d_knobs(2, SND_ARCADE, 10, 25, 70, 20);          /* 12.5 %, vibrato */
        en_cfg.tp[1][TP_LEVEL] = 92;
        en_cfg.tp[2][TP_LEVEL] = 76;
        en_cfg.tp[2][TP_PAN] = 38;
        en_cfg.tp[2][TP_DSEND] = 25;
        en_cfg.tp[3][TP_LEVEL] = 76;
        en_cfg.tp[3][TP_PAN] = 62;
        en_cfg.tp[3][TP_DSEND] = 22;
        en_cfg.par[P_DTIME] = 1;
        en_cfg.par[P_PUNCH] = 20;
        en_cfg.chain_n = 4;
        en_cfg.chain[0] = 0;
        en_cfg.chain[1] = 0;
        en_cfg.chain[2] = 1;
        en_cfg.chain[3] = 1;
        en_cfg.chords_n = 4;
        for (i = 0; i < 4; i++)
            en_cfg.chords[i] = CH[i];
        break;
    }
    case 2: {   /* House: a 909, a DX bass, the M1 organ, strings, over Dm7 G7 Cmaj7 Am7 */
        en_pat_t *a = &en_pat[0], *b = &en_pat[1];
        static const uint8_t CH[4] = {9, 12, 8, 13};
        en_cfg.par[P_TEMPO] = 124;
        en_cfg.par[P_SWING] = 16;
        d_tracks(KIT_909, SND_FMBASS, SND_ORGAN, SND_STRINGS);
        d_drum(a, 0, "X...x...X...x...");
        d_drum(a, 6, "....x.......x...");
        d_drum(a, 7, "x.x.x.x.x.x.x.x.");
        d_drum(a, 8, "..x...x...x...x.");
        d_line(a, 0, ". . C2 . . C3 C2 . . . C2 . . C3 C2 .");
        d_line(a, 1, ". . . C4+E4+G4+B4 . . C4+E4+G4+B4 . . . . C4+E4+G4+B4 . . C4+E4+G4+B4 .");
        d_line(a, 2, "C4+E4+G4:16 . . . . . . . . . . . . . . .");
        d_drum(b, 0, "X...x...X...x...");
        d_drum(b, 1, "............x..x");
        d_drum(b, 6, "....x.......x...");
        d_drum(b, 7, "x.x.x.x.x.x.x.x.");
        d_drum(b, 8, "..x...x...x...x.");
        d_drum(b, 10, "x.x.x.x.x.x.x.x.");
        d_line(b, 0, ". . C2 . . C3 C2 . . C2 . C2 . C3 Bb1 .");
        d_line(b, 1, ". . . C4+E4+G4+B4 . . C4+E4+G4+B4 . . C4+E4+G4+B4 . . C4+E4+G4+B4 . C4+E4+G4+B4 .");
        d_line(b, 2, "C4+E4+G4:8 . . . . . . . B3+D4+G4:8 . . . . . . .");
        d_knobs(0, SND_FMBASS, 50, 30, 35, 40);
        d_knobs(1, SND_ORGAN, 60, 70, 35, 10);
        d_knobs(2, SND_STRINGS, 50, 75, 55, 60);
        en_cfg.tp[1][TP_LEVEL] = 80;
        en_cfg.tp[2][TP_LEVEL] = 66;
        en_cfg.tp[2][TP_RSEND] = 25;
        en_cfg.tp[2][TP_DSEND] = 12;
        en_cfg.tp[3][TP_LEVEL] = 52;
        en_cfg.tp[3][TP_RSEND] = 40;
        en_cfg.par[P_PUNCH] = 40;
        en_cfg.chain_n = 4;
        en_cfg.chain[0] = 0;
        en_cfg.chain[1] = 0;
        en_cfg.chain[2] = 0;
        en_cfg.chain[3] = 1;
        en_cfg.chords_n = 4;
        for (i = 0; i < 4; i++)
            en_cfg.chords[i] = CH[i];
        break;
    }
    case 3: {   /* Lo-fi: the rhythm box, slow and swung, an 808 sub, a Rhodes-ish piano, bells */
        en_pat_t *a = &en_pat[0], *b = &en_pat[1];
        static const uint8_t CH[4] = {11, 10, 9, 8};
        en_cfg.par[P_TEMPO] = 84;
        en_cfg.par[P_SWING] = 52;
        en_cfg.par[P_SPACE] = 65;
        d_tracks(KIT_BOX, SND_BOOM, SND_EPIANO, SND_BELLS);
        d_drum(a, 0, "x.........x.....");
        d_drum(a, 1, "....x.......x...");
        d_drum(a, 7, "x.x.x.x.x.x.x.xx");
        d_drum(a, 5, "......x.......x.");
        d_line(a, 0, "C2:6 . . . . . . . . . C2:4 . . . G1 .");
        d_line(a, 1, "C4+E4+G4+B4:6 . . . . . . E4+G4+B4:2 . . . . C4+E4+G4:4 . . .");
        d_line(a, 2, ". . . . . . G5 . . . . . E5 . D5 .");
        d_drum(b, 0, "x.......x.x.....");
        d_drum(b, 1, "....x.......x..x");
        d_drum(b, 7, "x.x.x.x.x.x.x.x.");
        d_drum(b, 9, "..x.......x.....");
        d_drum(b, 4, "..............x.");
        d_line(b, 0, "C2:6 . . . . . . . C2:2 . C2 . . . E1~ G1");
        d_line(b, 1, "C4+E4+G4+B4:4 . . . . . E4+G4+B4:2 . . . C4+E4+G4+B4:4 . . . . .");
        d_line(b, 2, "B5 . . G5 . . . . . . E5 . . . . .");
        d_knobs(0, SND_BOOM, 25, 25, 70, 35);
        d_knobs(1, SND_EPIANO, 45, 40, 55, 45);
        d_knobs(2, SND_BELLS, 30, 10, 50, 50);
        en_cfg.tp[1][TP_LEVEL] = 84;
        en_cfg.tp[2][TP_LEVEL] = 76;
        en_cfg.tp[2][TP_RSEND] = 25;
        en_cfg.tp[3][TP_LEVEL] = 78;
        en_cfg.tp[3][TP_RSEND] = 45;
        en_cfg.tp[3][TP_DSEND] = 35;
        en_cfg.tp[3][TP_PAN] = 64;
        en_cfg.par[P_DTIME] = 3;
        en_cfg.par[P_PUNCH] = 25;
        en_cfg.chain_n = 2;
        en_cfg.chain[0] = 0;
        en_cfg.chain[1] = 1;
        en_cfg.chords_n = 4;
        for (i = 0; i < 4; i++)
            en_cfg.chords[i] = CH[i];
        break;
    }
    }
    en_cfg.pat = en_cfg.chain_n ? en_cfg.chain[0] : 0;
}

#ifdef LCD4_BENCH
/* a development build's heavy songs, to measure the audio load on the device (main_fm1.c, SysEx 02 k):
 * 0 three tracks of four-note pads and every drum, 1 the first demo, 2 the heaviest keys (strings, the
 * Juno, the piano) and every drum on every step, 3 three mono leads and basses */
void lcd4_bench(int k)
{
    static const uint8_t KEYS[3][3] = {{SND_POLY, SND_POLY, SND_POLY}, {SND_STRINGS, SND_POLY, SND_EPIANO},
                                       {SND_ACID, SND_FAT, SND_SYNC}};
    int t, s, p;
    if (k == 1) {
        en_demo(0);
        en_play(1);
        return;
    }
    en_demo(0);
    en_cfg.chain_n = 0;
    en_cfg.chords_n = 0;
    en_cfg.pat = 0;
    en_pat_clear(&en_pat[0]);
    for (p = 0; p < EN_NPART; p++)
        en_pat[0].hit[p] = k == 2 ? 0xFFFFu : (uint16_t)(0x1111u << (p % 4));
    for (t = 0; t < EN_NSYN; t++) {
        int snd = KEYS[k == 0 ? 0 : k == 2 ? 1 : 2][t];
        en_cfg.tp[t + 1][TP_SOUND] = (uint8_t)snd;
        en_cfg.tp[t + 1][TP_LEVEL] = 60;
        en_cfg.tp[t + 1][TP_DSEND] = 30;
        en_cfg.tp[t + 1][TP_RSEND] = 30;
        for (s = 0; s < EN_NSTEP; s++) {
            en_step_t *e = &en_pat[0].syn[t][s];
            e->note[0] = (uint8_t)(48 + t * 7 + (s % 5));
            if (en_sound(snd)->poly > 1) {
                e->note[1] = (uint8_t)(e->note[0] + 4);
                e->note[2] = (uint8_t)(e->note[0] + 7);
                e->note[3] = (uint8_t)(e->note[0] + 11);
            }
            e->len = 6;
            e->fl = (uint8_t)((s & 3) == 0 ? EN_F_ACC : 0);
        }
    }
    en_cfg.sk[1][SND_POLY][1] = 100;                /* the chorus on, I+II */
    en_cfg.par[P_TEMPO] = 140;
    en_play(1);
}
#endif
