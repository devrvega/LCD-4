/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4's scenes: one little LCD game a sound (and a kit), in the manner of the old handheld LCD games and
 * of the pocket synths whose screens took after them. Everything is drawn on a grid of "LCD pixels" (2 x 2
 * screen pixels): the scenery is printed (a soft tint, always there), the moving parts are segments, black
 * when lit and faintly visible when not, as on a real LCD, so every pose a character can take is there
 * behind it. The music lights them: a drum part, a note, its pitch, an accent, the beat. All the art is
 * LCD-4's own. Part of ui.c. */

#define SCX 4                          /* the scene's corner on the screen */
#define SCY 27
#define AW 116                         /* its size, in LCD pixels */
#define AH 46

typedef struct {
    uint32_t ms, frame;
    int playing, step;
    uint32_t steps;
    int drum, snd;                     /* the kit, or the sound */
    uint32_t ago[EN_NPART];            /* ms since each drum part played */
    uint32_t nago, ncount;             /* the track's last note: ms since, how many so far */
    int note, acc, gate;
    float lvl;
    float k[4];                        /* the sound's knobs, 0..1 */
    int chord;
} scn_t;

static uint16_t print_c = RGB(160, 168, 140);
static inline uint16_t col(int on) { return on ? L_INK : L_GHOST; }
static void ap(int x, int y, uint16_t c) { cv_rect(SCX + 2 * x, SCY + 2 * y, 2, 2, c); }
static void abox(int x, int y, int w, int h, int on) { cv_rect(SCX + 2 * x, SCY + 2 * y, 2 * w, 2 * h, col(on)); }
static void pbox(int x, int y, int w, int h) { cv_rect(SCX + 2 * x, SCY + 2 * y, 2 * w, 2 * h, print_c); }
static void cline(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, guard = 400;
    while (guard--) {
        int e2 = 2 * err;
        ap(x0, y0, c);
        if (x0 == x1 && y0 == y1)
            break;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}
static void aline(int x0, int y0, int x1, int y1, int on) { cline(x0, y0, x1, y1, col(on)); }
static void pline(int x0, int y0, int x1, int y1) { cline(x0, y0, x1, y1, print_c); }
static void alimb(int x0, int y0, int x1, int y1, int on)   /* an arm or a leg: two thick, a hand at its end */
{
    uint16_t c = col(on);
    cline(x0, y0, x1, y1, c);
    cline(x0 + 1, y0, x1 + 1, y1, c);
    cv_rect(SCX + 2 * (x1 - 1), SCY + 2 * (y1 - 1), 6, 6, c);
}
static void cring(int cx, int cy, int r, uint16_t c)
{
    int x = r, y = 0, e = 1 - r;
    while (x >= y) {
        ap(cx + x, cy + y, c);
        ap(cx - x, cy + y, c);
        ap(cx + x, cy - y, c);
        ap(cx - x, cy - y, c);
        ap(cx + y, cy + x, c);
        ap(cx - y, cy + x, c);
        ap(cx + y, cy - x, c);
        ap(cx - y, cy - x, c);
        y++;
        if (e < 0) {
            e += 2 * y + 1;
        } else {
            x--;
            e += 2 * (y - x) + 1;
        }
    }
}
static void aring(int cx, int cy, int r, int on) { cring(cx, cy, r, col(on)); }
static void cdisc(int cx, int cy, int r, uint16_t c)
{
    int y;
    for (y = -r; y <= r; y++) {
        int w = 0;
        while ((w + 1) * (w + 1) + y * y <= r * r + r)
            w++;
        cv_rect(SCX + 2 * (cx - w), SCY + 2 * (cy + y), 2 * (2 * w + 1), 2, c);
    }
}
static void adisc(int cx, int cy, int r, int on) { cdisc(cx, cy, r, col(on)); }
/* an arc: the part of a ring between turns a0 and a1 (0 = right, 0.25 = down) */
static void aarc(int cx, int cy, int r, float a0, float a1, int on)
{
    int i, n = r * 4 + 4;
    uint16_t c = col(on);
    for (i = 0; i <= n; i++) {
        float a = (a0 + (a1 - a0) * (float)i / (float)n) * FM_TWO_PI;
        ap(cx + (int)fm_floorf(fm_cosf(a) * (float)r + 0.5f), cy + (int)fm_floorf(fm_sinf(a) * (float)r + 0.5f), c);
    }
}
static void cart(int x, int y, const char *const *a, uint16_t c, int mirror)
{
    int r, i;
    for (r = 0; a[r]; r++) {
        int w = 0;
        while (a[r][w])
            w++;
        for (i = 0; i < w; i++)
            if (a[r][i] == '#')
                ap(mirror ? x + w - 1 - i : x + i, y + r, c);
    }
}
static void art(int x, int y, const char *const *a, int on) { cart(x, y, a, col(on), 0); }
static void artm(int x, int y, const char *const *a, int on) { cart(x, y, a, col(on), 1); }
static void part(int x, int y, const char *const *a) { cart(x, y, a, print_c, 0); }
static int hot(uint32_t ago, uint32_t ms) { return ago < ms; }
static int band_of(uint32_t ago, uint32_t ms, int n)   /* 0..n-1: which stage of a motion ms long, n when over */
{
    return ago >= ms ? n : (int)(ago * (uint32_t)n / ms);
}
static void digits(int x, int y, uint32_t v, int n)    /* a counter, in segments (screen size: 7 x 13) */
{
    int i;
    for (i = n - 1; i >= 0; i--) {
        seg7(SCX + 2 * x + i * 9, SCY + 2 * y, (int)(v % 10u), L_INK, L_GHOST);
        v /= 10u;
    }
}

/* ------------------------------------------------------------- the people --- */
/* Pip, who lives in the LCD: a round head with two eyes, a body, two legs; arms are limbs drawn per pose */
static const char *const PIP_HEAD[] = {"..####..", ".######.", "##.##.##", "########", ".######.", "..####..", 0};
static const char *const PIP_BLINK[] = {"..####..", ".######.", "########", "########", ".######.", "..####..", 0};
static const char *const PIP_BODY[] = {"...##...", ".######.", "########", "########", ".######.", ".######.", 0};
static const char *const PIP_LEGS[] = {"..#..#..", "..#..#..", "..#..#..", ".##..##.", 0};
static const char *const PIP_SIT[] = {"########", "......##", "......#.", ".....##.", 0};
static const char *const MINI[] = {"..##..", ".####.", "..##..", ".####.", "#.##.#", "..##..", ".#..#.", ".#..#.", 0};
static const char *const MINI_JUMP[] = {"..##..", ".####.", "#.##.#", ".####.", "..##..", "..##..", ".#..#.", "#....#", 0};

static void pip(int x, int y, int on, int blink)       /* standing; the head's top-left at x, y (16 tall) */
{
    art(x, y, blink ? PIP_BLINK : PIP_HEAD, on);
    art(x, y + 6, PIP_BODY, on);
    art(x, y + 12, PIP_LEGS, on);
}
static void pip_sit(int x, int y, int on, int blink, int right)
{
    art(x, y, blink ? PIP_BLINK : PIP_HEAD, on);
    art(x, y + 6, PIP_BODY, on);
    if (right)
        art(x + 2, y + 12, PIP_SIT, on);
    else
        artm(x - 2, y + 12, PIP_SIT, on);
}
static int blinks(const scn_t *s) { return (s->frame % 90u) < 4u; }

/* -------------------------------------------------------------- the props --- */
static const char *const NOTE_S[] = {"..##", "..#.", "..#.", "###.", "###.", 0};
static const char *const STAR[] = {".#.", "###", ".#.", 0};
static const char *const SPARK[] = {"#...#", ".#.#.", "..#..", ".#.#.", "#...#", 0};
static const char *const BURST[] = {"#..#..#", ".#.#.#.", "..###..", "###.###", "..###..", ".#.#.#.", "#..#..#", 0};
static const char *const PUFF[] = {".##..", "####.", "#####", ".###.", 0};
static const char *const BIGPUFF[] = {"..###...", ".#####..", "########", "#######.", ".#####..", 0};
static const char *const COIN[] = {".##.", "#..#", "#.##", "#..#", ".##.", 0};
static const char *const BIRD[] = {"#...#", ".#.#.", "..#..", 0};
static const char *const FISH[] = {".##..#", "####.#", "######", "####.#", ".##..#", 0};
static const char *const HEART[] = {".#.#.", "#####", ".###.", "..#..", 0};

/* ================================================================ the kits === */

/* 808: Pip at the kit. Left arm: the hats or the snare; right arm: the toms, the cymbal or the cowbell; the
 * pedal on the kick; both hands up to clap. */
static void sc_808(const scn_t *s)
{
    const uint32_t H = 120;
    int ch = hot(s->ago[7], H), oh = hot(s->ago[8], 260), hat = ch || oh, sn = hot(s->ago[1], H), rs = hot(s->ago[5], H);
    int t1 = hot(s->ago[4], H) || hot(s->ago[3], H), t2 = hot(s->ago[2], H), cy = hot(s->ago[9], 220), cb = hot(s->ago[10], H);
    int cp = hot(s->ago[6], 140), bd = hot(s->ago[0], H);
    int lrest = !hat && !sn && !rs && !cp, rrest = !t1 && !t2 && !cy && !cb && !cp;
    print_c = RGB(150, 160, 150);
    pbox(0, 44, AW, 2);                                   /* the riser */
    pline(33, 22, 33, 43);                                /* the hats' stand */
    pbox(38, 27, 11, 2);                                  /* the snare */
    pline(43, 29, 43, 43);
    pbox(64, 22, 9, 3);                                   /* the rack tom, the floor tom */
    pbox(76, 29, 11, 5);
    pline(79, 34, 79, 43);
    pline(85, 34, 85, 43);
    pline(96, 16, 96, 43);                                /* the cymbal's stand */
    cring(56, 36, 7, print_c);                            /* the kick */
    cring(56, 36, 3, print_c);
    abox(27, 21, 13, 1, ch && !oh);                       /* the hats: closed, or open */
    abox(27, 18, 13, 1, oh);
    abox(27, 22, 13, 1, 1);
    aline(88, 15, 104, 17, !cy);                          /* the cymbal, still or ringing */
    aline(88, 18, 104, 13, cy);
    art(66, 16, (const char *const[]){".###.", "#####", "#####", 0}, cb);   /* the cowbell */
    aarc(56, 36, 10, 0.62f, 0.88f, bd);                   /* the kick's boom */
    aarc(56, 36, 12, 0.64f, 0.86f, bd && s->ago[0] < 60);
    aline(56, 36, 56, 41, !bd);                           /* the beater: back, or in */
    aline(56, 36, 53, 32, bd);
    art(40, 21, SPARK, sn);                               /* the snare's crack, the rim's tick */
    art(36, 26, (const char *const[]){"#", "#", 0}, rs);
    aline(64, 20, 72, 20, t1);                            /* the toms' thud */
    aline(77, 27, 86, 27, t2);
    art(52, 9, PIP_HEAD, 1);                              /* Pip, behind the kit */
    art(52, 15, PIP_BODY, 1);
    alimb(53, 17, 38, 20, hat && !cp);                    /* the left arm */
    alimb(53, 17, 44, 25, (sn || rs) && !cp && !hat);
    alimb(53, 17, 48, 24, lrest);
    alimb(58, 17, 67, 20, t1 && !cp);                     /* the right arm */
    alimb(58, 17, 80, 26, t2 && !cp && !t1);
    alimb(58, 17, 88, 16, cy && !cp && !t1 && !t2);
    alimb(58, 17, 67, 13, cb && !cp && !t1 && !t2 && !cy);
    alimb(58, 17, 63, 24, rrest);
    alimb(53, 17, 50, 6, cp);                             /* the clap: both up */
    alimb(58, 17, 61, 6, cp);
    art(53, 0, (const char *const[]){"#.#.#", ".#.#.", 0}, cp);
}

/* 909: a press shop. A belt carries boxes one place a beat; the piston comes down on the kick, the hammer on
 * the snare (and the clap); the steam valve puffs on the hats, the gears turn on the toms, the beacon flashes
 * on the crash and the ride. */
static const char *const BOX8[] = {"########", "#......#", "#.####.#", "#......#", "########", 0};
static void sc_909(const scn_t *s)
{
    const uint32_t H = 120;
    int bd = hot(s->ago[0], H), sn = hot(s->ago[1], H) || hot(s->ago[6], H), rs = hot(s->ago[5], 90);
    int ch = hot(s->ago[7], 100), oh = hot(s->ago[8], 260), cr = hot(s->ago[9], 300) || hot(s->ago[10], 240);
    int g1 = hot(s->ago[2], 160), g2 = hot(s->ago[3], 160), g3 = hot(s->ago[4], 160), i;
    int shift = s->playing ? (int)(s->steps / 4u) : 0;
    print_c = RGB(156, 158, 146);
    pbox(4, 40, 108, 2);                                  /* the belt */
    for (i = 0; i < 7; i++)
        cring(8 + i * 16, 43, 2, print_c);
    for (i = 0; i < 6; i++) {                             /* the boxes, moving along */
        int lit = ((i + 6 - shift % 6) % 3) == 0;
        art(6 + i * 18, 35, BOX8, lit);
    }
    pbox(28, 2, 2, 8);                                    /* the piston's housing */
    pbox(22, 0, 14, 3);
    abox(25, 10, 8, 3, !bd);                              /* the piston: up, or down */
    abox(28, 13, 2, 4, bd);
    abox(25, 17, 8, 3, bd);
    art(23, 29, SPARK, bd || rs);
    art(31, 29, SPARK, rs);
    pbox(66, 6, 2, 10);                                   /* the hammer's post */
    aline(67, 8, 80, 4, !sn);                             /* the hammer: raised, or struck */
    abox(78, 2, 4, 4, !sn);
    aline(67, 8, 74, 24, sn);
    abox(72, 24, 5, 4, sn);
    art(70, 29, (const char *const[]){"#.#.#", 0}, sn);
    pbox(100, 8, 4, 10);                                  /* the steam valve */
    art(97, 2, PUFF, ch);
    art(104, 0, BIGPUFF, oh);
    for (i = 0; i < 3; i++) {                             /* three gears */
        int on = i == 0 ? g1 : i == 1 ? g2 : g3, cx = 46 + i * 9, cy = 6 + (i & 1) * 5, f = (s->frame / 3u + (uint32_t)i) & 1u;
        cring(cx, cy, 4, print_c);
        aline(cx - 3, cy, cx + 3, cy, on && f);
        aline(cx, cy - 3, cx, cy + 3, on && f);
        aline(cx - 2, cy - 2, cx + 2, cy + 2, on && !f);
        aline(cx - 2, cy + 2, cx + 2, cy - 2, on && !f);
    }
    abox(56, 24, 4, 3, 1);                                /* the beacon, its light */
    aline(52, 23, 49, 20, cr);
    aline(58, 22, 58, 18, cr);
    aline(64, 23, 67, 20, cr);
}

/* ARCADE: a shooting gallery from space. The invaders march a step at a time; the kick fires the cannon, the
 * snare blows one up, the hats are the stars, the toms light the shields, the crash sends the saucer over,
 * the blip, the zap and the coin are what they say. */
static const char *const ALIEN_A[] = {"..#..#..", "...##...", ".######.", "##.##.##", "########", "#.#..#.#", 0};
static const char *const ALIEN_B[] = {"..#..#..", "#..##..#", "#.####.#", "###..###", ".######.", ".#....#.", 0};
static const char *const CANNON[] = {"....#....", "...###...", ".#######.", "#########", 0};
static const char *const SAUCER[] = {"...####...", ".########.", "##.#..#.##", ".########.", 0};
static void sc_arcade_kit(const scn_t *s)
{
    int march = s->playing ? (int)(s->steps / 2u) % 8 : 0, dx = march < 4 ? march : 8 - march;
    int frame = (int)(s->steps & 1u), r, c, i, shot = band_of(s->ago[0], 160, 4), boom = hot(s->ago[1], 160);
    int hit_c = (int)(s->steps % 4u), hit_r = (int)((s->steps / 4u) & 1u);
    print_c = RGB(150, 158, 160);
    pbox(0, 45, AW, 1);
    for (r = 0; r < 2; r++)                               /* the invaders, marching */
        for (c = 0; c < 4; c++) {
            int x = 22 + c * 18 + dx * 2, y = 8 + r * 9;
            int gone = boom && r == hit_r && c == hit_c;
            art(x, y, frame ? ALIEN_A : ALIEN_B, 0);     /* the other pose, a ghost; this one lit */
            art(x, y, frame ? ALIEN_B : ALIEN_A, !gone);
            if (gone)
                art(x, y - 1, BURST, 1);
        }
    art(52, 41, CANNON, 1);                               /* the cannon, its shot climbing */
    for (i = 0; i < 4; i++)
        abox(56, 36 - i * 7, 1, 4, shot == i);
    for (i = 0; i < 3; i++) {                             /* the shields */
        int on = hot(s->ago[2 + i], 160);
        art(14 + i * 38, 32, (const char *const[]){".######.", "########", "##....##", 0}, on);
    }
    for (i = 0; i < 6; i++) {                             /* the stars */
        static const uint8_t SX[6] = {6, 30, 50, 78, 96, 110}, SY[6] = {2, 4, 1, 5, 2, 25};
        int on = (hot(s->ago[7], 90) && (i & 1) == (int)(s->ncount & 1u)) || (hot(s->ago[8], 240) && (i % 3) == 0);
        art(SX[i], SY[i], STAR, on);
    }
    art(100 - band_of(s->ago[9], 900, 6) * 16, 0, SAUCER, hot(s->ago[9], 900));
    aline(4, 30, 112, 30, hot(s->ago[6], 90));             /* the zap */
    art(4, 37, (const char *const[]){"##.##", "#.#.#", 0}, hot(s->ago[5], 120));   /* the blip */
    art(106, 38, COIN, hot(s->ago[10], 300));
    art(105, 31, STAR, hot(s->ago[10], 150));
}

/* BOX: a beach. Pip plays the congas; the kick rolls the waves in, the hats shake the maracas, the claves
 * click, the guiro scrapes, the tambourine jingles, the cowbell wakes the bird in the palm. */
static void sc_box(const scn_t *s)
{
    int wave = s->playing ? (int)((s->steps / 4u) % 3u) : 0, bd = hot(s->ago[0], 150), i;
    int sn = hot(s->ago[1], 120), tom = hot(s->ago[2], 120) || hot(s->ago[3], 120) || hot(s->ago[4], 120);
    int mar = hot(s->ago[7], 110) || hot(s->ago[8], 200), clv = hot(s->ago[5], 110), gui = hot(s->ago[6], 200);
    int tam = hot(s->ago[9], 220), cb = hot(s->ago[10], 300), sway = s->playing ? (int)((s->steps / 4u) & 1u) : 0;
    print_c = RGB(176, 168, 132);
    pbox(0, 44, AW, 2);                                   /* the sand */
    cring(98, 8, 6, print_c);                             /* the sun */
    for (i = 0; i < 3; i++)                               /* the sea: a wave comes in on the kick */
        aarc(10 + i * 14, 40, 5, 0.5f, 1.0f, (i == wave) && (bd || !s->playing));
    pline(16, 44, 22, 14);                                /* the palm */
    pline(17, 44, 23, 14);
    aline(22, 14, 12, 18, !sway);
    aline(22, 14, 32, 17, !sway);
    aline(22, 14, 14, 10, sway);
    aline(22, 14, 31, 11, sway);
    art(12, 6, BIRD, cb);
    art(18, 2, NOTE_S, cb);
    abox(46, 33, 8, 11, 1);                               /* the congas */
    abox(58, 31, 8, 13, 1);
    pip(54, 12, 1, blinks(s));
    alimb(55, 19, 49, 31, sn || tom);                      /* the hands on the heads */
    alimb(55, 19, 47, 23, !(sn || tom) && !mar);
    alimb(61, 19, 62, 29, tom);
    alimb(61, 19, 68, 24, !tom && !clv && !gui);
    alimb(55, 19, 44, 13, mar);                            /* a maraca, shaken */
    adisc(43, 11, 2, mar);
    art(70, 18, (const char *const[]){"#...", ".#..", "..#.", "...#", 0}, clv);   /* the claves */
    alimb(61, 19, 72, 15, gui || clv);
    abox(74, 13, 8, 2, gui);                              /* the guiro, scraped */
    art(74, 10, (const char *const[]){"#.#.#.#.", 0}, gui && (s->frame & 2u));
    aring(90, 30, 5, tam);                                 /* the tambourine */
    art(87, 23, (const char *const[]){"#.#.#.#", 0}, tam);
}

/* ============================================================== the synths === */

/* ACID: the lab. Bubbles climb out of a flask on every note (its flask by the note); an accent boils one over;
 * a slide lights the tube between them; Cutoff is how full they are. */
static const char *const FLASK[] = {"..##..", "..##..", "..##..", ".#..#.", "#....#", "#....#", "######", 0};
static void sc_acid(const scn_t *s)
{
    int f = s->note % 3, b = band_of(s->nago, 240, 3), lvl = (int)(s->k[0] * 2.99f), i;
    print_c = RGB(150, 166, 150);
    pbox(0, 38, AW, 2);                                   /* the bench */
    pbox(4, 40, 2, 6);
    pbox(108, 40, 2, 6);
    for (i = 0; i < 3; i++) {
        int x = 44 + i * 22, j;
        part(x, 23, (const char *const[]){"...##...", "...##...", "...##...", "..#..#..", ".#....#.", "#......#",
                                          "#......#", "#......#", "#......#", "########", 0});
        for (j = 0; j < 3; j++)                           /* the liquid */
            abox(x + 1, 31 - j * 2, 6, 1, j <= lvl);
        for (j = 0; j < 3; j++)                           /* the bubbles, climbing */
            adisc(x + 3 + (j & 1), 19 - j * 5, 1, s->playing && i == f && b == j);
        art(x - 1, 2, BIGPUFF, i == 2 && s->acc && hot(s->nago, 300));
    }
    aline(52, 26, 66, 26, s->gate);                       /* the tube */
    aline(74, 26, 88, 26, s->gate);
    pip(14, 22, 1, blinks(s));                            /* Pip, pouring */
    alimb(20, 29, 32, 24, hot(s->nago, 150));
    alimb(20, 29, 26, 34, !hot(s->nago, 150));
    art(30, 17, FLASK, hot(s->nago, 150));
    art(26, 30, FLASK, !hot(s->nago, 150));
}

/* MONO: on stage with a keytar. The hand runs up the neck with the pitch; an accent is a jump; the lights
 * sweep with the beat; the crowd bobs. */
static void sc_mono(const scn_t *s)
{
    int jump = s->acc && hot(s->nago, 220), pos = (s->note % 12) / 3, beat = s->playing ? (int)((s->steps / 4u) % 3u) : -1;
    int y = jump ? 7 : 12, i, bob = hot(s->nago, 120);
    print_c = RGB(156, 150, 170);
    pbox(0, 36, AW, 2);                                   /* the stage */
    for (i = 0; i < 3; i++) {                             /* the lights */
        pbox(14 + i * 40, 0, 6, 2);
        aline(16 + i * 40, 2, 10 + i * 40 + (i - 1) * 6, 34, beat == i);
        aline(18 + i * 40, 2, 24 + i * 40 + (i - 1) * 6, 34, beat == i);
    }
    pip(54, 7, jump, 0);                                  /* Pip, in the air or on the boards */
    pip(54, 12, !jump, blinks(s));
    aline(48, y + 14, 72, y + 6, 1);                       /* the keytar */
    aline(48, y + 15, 72, y + 7, 1);
    abox(46, y + 13, 6, 4, 1);
    for (i = 0; i < 4; i++)                               /* the hand on the neck */
        abox(58 + i * 4, y + 10 - i, 2, 2, s->playing && i == pos && hot(s->nago, 400));
    for (i = 0; i < 6; i++)                               /* the crowd */
        adisc(10 + i * 19, (i & 1) == (int)(s->ncount & 1u) && bob ? 40 : 42, 3, 1);
}

/* FAT: the strongman. Down on the floor, at the chest with a note, over the head with an accent. */
static void sc_fat(const scn_t *s)
{
    int up = s->acc && hot(s->nago, 400), mid = !up && s->gate, low = !up && !mid, i;
    print_c = RGB(170, 160, 140);
    pbox(20, 42, 76, 4);                                  /* the platform */
    pip(54, 22, 1, blinks(s));
    for (i = 0; i < 3; i++) {                             /* the bar: three places */
        int y = i == 0 ? 37 : i == 1 ? 27 : 15, on = i == 0 ? low : i == 1 ? mid : up;
        abox(36, y, 44, 1, on);
        abox(32, y - 3, 4, 7, on);
        abox(80, y - 3, 4, 7, on);
    }
    alimb(55, 29, 50, 36, low);                           /* the arms */
    alimb(61, 29, 66, 36, low);
    alimb(55, 29, 50, 27, mid);
    alimb(61, 29, 66, 27, mid);
    alimb(55, 29, 50, 15, up);
    alimb(61, 29, 66, 15, up);
    art(46, 8, (const char *const[]){"#...#...#", ".#..#..#.", 0}, up);
    art(64, 18, (const char *const[]){".#", "##", 0}, s->gate && s->nago > 500);   /* a drop of sweat */
}

/* BOOM: the submarine. Every note sends a sonar ping; long notes, bubbles; the fish swim by on the beat. */
static const char *const SUB[] = {"..........##........", ".........####.......", "..###############...",
                                  ".##################.", "####################", ".##################.",
                                  "..################..", 0};
static void sc_boom(const scn_t *s)
{
    int ping = band_of(s->nago, 600, 3), low = s->note < 40, i, fx = s->playing ? 10 - (int)(s->steps % 11u) : 5;
    print_c = RGB(140, 160, 172);
    for (i = 0; i < 12; i++)                              /* the surface, the floor */
        aarc(4 + i * 10, 2, 4, 0.0f, 0.5f, 0);
    pbox(0, 44, AW, 2);
    for (i = 0; i < 3; i++) {                             /* the weed */
        int sw = s->playing && ((s->steps / 4u + (uint32_t)i) & 1u);
        aline(80 + i * 10, 44, 80 + i * 10 + (sw ? 2 : -2), 36, 1);
    }
    art(18, low ? 24 : 16, SUB, 1);                       /* the sub: deep or shallow */
    art(18, low ? 16 : 24, SUB, 0);
    for (i = 0; i < 3; i++)                               /* the pings */
        aarc(40, (low ? 27 : 19), 6 + i * 6, -0.15f, 0.15f, s->playing && ping == i);
    for (i = 0; i < 3; i++)                               /* the bubbles */
        adisc(30 - i, (low ? 20 : 12) - i * 4, 1, s->gate && (int)((s->frame / 4u) % 3u) == i);
    for (i = 0; i < 3; i++)
        art(60 + i * 16, 8 + (i & 1) * 22, FISH, s->playing && (fx % 3) == i);
}

/* FM BASS: the pinball table. A flipper on every note (the left for low notes), the ball one place along, a
 * bumper for an accent, the score counting. */
static void sc_fmbass(const scn_t *s)
{
    int left = s->note < 48, hit = hot(s->nago, 140), pos = (int)(s->ncount % 8u), i;
    static const int8_t BX[8] = {58, 74, 88, 96, 82, 62, 42, 30}, BY[8] = {38, 30, 20, 10, 6, 12, 18, 28};
    print_c = RGB(164, 156, 168);
    pline(10, 46, 10, 4);                                 /* the table */
    pline(10, 4, 106, 4);
    pline(106, 4, 106, 46);
    aline(32, 40, 46, 44, !(hit && left));                /* the flippers: down, up */
    aline(32, 40, 46, 36, hit && left);
    aline(84, 40, 70, 44, !(hit && !left));
    aline(84, 40, 70, 36, hit && !left);
    for (i = 0; i < 8; i++)
        adisc(BX[i], BY[i], 2, s->playing && i == pos);
    for (i = 0; i < 3; i++) {                             /* the bumpers */
        int on = s->acc && hot(s->nago, 200) && (int)(s->ncount % 3u) == i;
        aring(40 + i * 18, 22 + (i & 1) * 6, 4, 1);
        adisc(40 + i * 18, 22 + (i & 1) * 6, 2, on);
    }
    digits(76, 6, (s->ncount * 10u) % 10000u, 4);
}

/* REESE: the night drive. The stripes run past with the steps; the car's lane is the note's; the headlights
 * on every note. */
static const char *const CAR[] = {"..##########..", ".#..........#.", "##############", "#.##......##.#",
                                  "##############", "##..........##", 0};
static void sc_reese(const scn_t *s)
{
    int lane = s->note % 3, ph = s->playing ? (int)(s->steps % 3u) : 0, i;
    print_c = RGB(148, 156, 176);
    pline(2, 46, 52, 14);                                  /* the road */
    pline(114, 46, 64, 14);
    pline(0, 14, 115, 14);
    for (i = 0; i < 8; i++)                                /* the town */
        pbox(4 + i * 14, 14 - 3 - (i * 7) % 6, 8, 3 + (i * 7) % 6);
    cring(100, 4, 3, print_c);
    for (i = 0; i < 3; i++) {                              /* the stripes */
        int y = 18 + i * 9, w = 1 + i;
        abox(58 - w / 2, y + ph * 3 - 2, w, 2, 1);
        abox(58 - w / 2, y + ((ph + 1) % 3) * 3 - 2, w, 2, 0);
    }
    for (i = 0; i < 3; i++) {                              /* the car, in its lane */
        int x = 24 + i * 27;
        art(x, 38, CAR, i == lane);
        aline(x + 3, 37, x - 2, 22, i == lane && hot(s->nago, 220));
        aline(x + 10, 37, x + 15, 22, i == lane && hot(s->nago, 220));
    }
}

/* POLY: the sunset. The grid rolls toward you with the steps; the sun's rays are the voices sounding; the
 * palms sway on the beat. */
static void sc_poly(const scn_t *s)
{
    int ph = s->playing ? 2 - (int)(s->steps % 3u) : 0, rays = (int)(fm_clampf(s->lvl * 2.5f, 0.0f, 1.0f) * 6.0f + 0.5f), i;
    int sway = s->playing && ((s->steps / 4u) & 1u);
    print_c = RGB(186, 160, 140);
    cdisc(58, 22, 11, print_c);                            /* the sun, its bars cut */
    for (i = 0; i < 4; i++)
        cv_rect(SCX + 2 * 46, SCY + 2 * (24 + i * 2), 2 * 25, 2, L_BG);
    pbox(0, 30, AW, 1);                                    /* the horizon */
    for (i = 0; i < 6; i++) {                              /* the rays */
        float a = (0.55f + 0.08f * (float)i) * FM_TWO_PI;
        aline(58 + (int)(fm_cosf(a) * 13.0f), 22 + (int)(fm_sinf(a) * 13.0f), 58 + (int)(fm_cosf(a) * 19.0f),
              22 + (int)(fm_sinf(a) * 19.0f), i < rays && s->gate);
    }
    for (i = 0; i < 7; i++)                                /* the grid's lines toward you */
        pline(58 + (i - 3) * 6, 31, 58 + (i - 3) * 24, 46);
    for (i = 0; i < 6; i++) {                              /* the grid's rows, rolling toward you */
        static const uint8_t RY[6] = {32, 34, 36, 39, 42, 45};
        abox(0, RY[i], AW, 1, (i % 3) == (s->playing ? ph : 0));
    }
    pline(10, 46, 14, 26);                                 /* the palms */
    pline(104, 46, 100, 26);
    aline(14, 26, 4, 30, !sway);
    aline(14, 26, 24, 29, !sway);
    aline(14, 26, 6, 22, sway);
    aline(14, 26, 22, 22, sway);
    aline(100, 26, 90, 29, sway);
    aline(100, 26, 110, 30, sway);
    aline(100, 26, 92, 22, !sway);
    aline(100, 26, 108, 22, !sway);
}

/* BRASS: the trumpeter. The sound waves leave the bell on every note; notes fly out; an accent puffs the
 * cheeks. */
static void sc_brass(const scn_t *s)
{
    int w = band_of(s->nago, 450, 3), n = band_of(s->nago, 900, 4), puff = s->acc && hot(s->nago, 250), i;
    print_c = RGB(176, 166, 136);
    pbox(0, 44, AW, 2);
    pip(18, 26, 1, blinks(s));
    adisc(26, 29, 2, puff);                                /* the cheek */
    aline(26, 31, 44, 30, 1);                              /* the horn and its bell */
    aline(26, 32, 44, 31, 1);
    art(44, 26, (const char *const[]){"..#", ".##", "###", "###", "###", ".##", "..#", 0}, 1);
    alimb(24, 33, 34, 32, 1);
    for (i = 0; i < 3; i++)
        aarc(48, 30, 4 + i * 5, -0.12f, 0.12f, s->playing && w == i);
    for (i = 0; i < 4; i++)
        art(56 + i * 14, 18 - (i & 1) * 8, NOTE_S, s->playing && n == i);
}

/* E.PIANO: the lounge. Pip's hands move up the keys with the pitch; the notes rise from the piano; the lamp
 * glows with the tremolo. */
static void sc_epiano(const scn_t *s)
{
    int pos = (s->note % 24) / 6, n = band_of(s->nago, 700, 3), lamp = s->k[3] > 0.05f, i;
    print_c = RGB(170, 158, 146);
    pbox(0, 44, AW, 2);
    pbox(50, 16, 52, 18);                                  /* the piano */
    cv_rect(SCX + 2 * 52, SCY + 2 * 18, 2 * 48, 2 * 14, L_BG);
    pbox(50, 34, 4, 10);
    pbox(98, 34, 4, 10);
    for (i = 0; i < 12; i++)                               /* its keys */
        abox(51 + i * 4, 33, 3, 1, 1);
    pip_sit(34, 17, 1, blinks(s), 1);
    for (i = 0; i < 4; i++)                                /* the hands, along the keys */
        alimb(40, 24, 54 + i * 11, 31, s->playing ? i == pos && hot(s->nago, 600) : i == 0);
    for (i = 0; i < 3; i++)
        art(66 + i * 10, 11 - i * 5, NOTE_S, s->playing && n == i);
    pline(92, 16, 92, 8);                                  /* the lamp */
    art(88, 4, (const char *const[]){"..###..", ".#####.", "#######", 0}, lamp);
    art(87, 8, (const char *const[]){"#.....#", ".#...#.", 0}, lamp && ((s->frame / 6u) & 1u));
}

/* ORGAN: the carousel. The horses rise and fall with the notes; the lights chase round the roof with the steps;
 * the flag turns on the beat. */
static const char *const HORSE[] = {".....##", "#..####", "#######", ".######", ".#....#", 0};
static void sc_organ(const scn_t *s)
{
    int i, chase = s->playing ? (int)(s->steps % 8u) : -1, up = (int)(s->ncount & 1u), flag = s->playing && ((s->steps / 4u) & 1u);
    print_c = RGB(176, 154, 160);
    pline(58, 0, 18, 12);                                   /* the roof */
    pline(58, 0, 98, 12);
    pbox(18, 12, 81, 2);
    pbox(14, 42, 89, 3);                                    /* the floor */
    pline(58, 0, 58, 42);
    for (i = 0; i < 8; i++)                                 /* the lights */
        abox(20 + i * 10, 14, 2, 2, i == chase);
    aline(58, 0, 64, 1, flag);                              /* the flag */
    aline(58, 0, 52, 1, !flag);
    for (i = 0; i < 3; i++) {                               /* the horses */
        int x = 26 + i * 28, hi = ((i & 1) == up) && hot(s->nago, 400);
        pline(x + 3, 16, x + 3, 41);
        art(x, 22, HORSE, hi);
        art(x, 30, HORSE, !hi);
    }
}

/* STRINGS: the conductor. The baton beats the bar (down, in, out, up); the bows move with the notes. */
static void sc_strings(const scn_t *s)
{
    int beat = s->playing ? (int)((s->steps / 4u) % 4u) : 0, bow = (int)(s->ncount & 1u), i;
    static const int8_t TX[4] = {90, 80, 102, 92}, TY[4] = {28, 20, 20, 8};
    print_c = RGB(160, 162, 150);
    pbox(84, 40, 24, 6);                                    /* the podium */
    pip(88, 24, 1, blinks(s));
    for (i = 0; i < 4; i++)
        alimb(89, 31, TX[i], TY[i], i == beat);
    pline(72, 30, 72, 44);                                  /* the stand */
    pbox(68, 26, 9, 4);
    for (i = 0; i < 3; i++) {                               /* the players and their bows */
        int x = 10 + i * 20;
        adisc(x + 3, 20, 3, 1);
        abox(x, 24, 7, 10, 1);
        aline(x + 6, 26, x + 12, 20, hot(s->nago, 600) && !bow);
        aline(x + 6, 26, x + 12, 32, hot(s->nago, 600) && bow);
        art(x - 2, 6, NOTE_S, s->playing && hot(s->nago, 300) && (int)(s->ncount % 3u) == i);
    }
}

/* BELLS: the tower. The note picks the bell (low, middle, high); it swings and rings; Pip pulls the rope; an
 * accent startles the birds. */
static const char *const BELL[] = {"..##..", ".####.", ".####.", "######", "######", "..##..", 0};
static void sc_bells(const scn_t *s)
{
    int b = s->note < 60 ? 0 : s->note < 72 ? 1 : 2, ring = hot(s->nago, 500), sw = (int)(s->ncount & 1u), i;
    print_c = RGB(166, 158, 150);
    pline(36, 46, 44, 2);                                   /* the tower */
    pline(80, 46, 72, 2);
    pline(44, 2, 72, 2);
    for (i = 0; i < 3; i++) {
        int y = 6 + i * 11, on = s->playing && ring && i == 2 - b;
        art(53, y, BELL, on && sw);
        art(57, y, BELL, on && !sw);
        art(55, y, BELL, !on);
        aarc(58, y + 3, 9, 0.85f, 1.15f, on);
        aarc(58, y + 3, 9, 0.35f, 0.65f, on);
    }
    pip(88, 28, 1, blinks(s));                              /* Pip on the rope */
    pline(92, 0, 92, 26);
    alimb(89, 35, 91, 24, hot(s->nago, 200));
    alimb(89, 35, 91, 30, !hot(s->nago, 200));
    for (i = 0; i < 3; i++)
        art(4 + i * 10, 4 + (i & 1) * 6, BIRD, s->acc && hot(s->nago, 700));
}

/* SYNC: the rocket. Its height is the note's; the flame burns while the note does; smoke at the pad. */
static const char *const ROCKET[] = {"..#..", ".###.", ".###.", "#####", ".###.", ".###.", "#####", "#.#.#", 0};
static void sc_sync(const scn_t *s)
{
    int alt = s->playing ? (s->note % 24) / 6 : 0, i;
    print_c = RGB(156, 160, 176);
    pbox(0, 44, AW, 2);
    pbox(34, 8, 2, 36);                                     /* the tower */
    for (i = 0; i < 6; i++)
        pline(34, 10 + i * 6, 38, 14 + i * 6);
    for (i = 0; i < 4; i++) {                               /* the rocket, at its four heights */
        int y = 34 - i * 10, on = i == alt;
        art(56, y, ROCKET, on);
        art(57, y + 8, (const char *const[]){"#.#", ".#.", 0}, on && s->gate);
    }
    for (i = 0; i < 3; i++)                                 /* the smoke */
        art(44 + i * 12, 38, PUFF, hot(s->nago, 200 + 100 * (uint32_t)i) && s->playing);
    for (i = 0; i < 5; i++)
        art(78 + (i * 13) % 34, 2 + (i * 9) % 24, STAR, ((s->frame / 8u + (uint32_t)i) % 5u) == 0u);
}

/* SCREAM: the monster and the city. It roars on every note; an accent shakes the buildings; the helicopter
 * crosses with the steps. */
static const char *const MONSTER[] = {"....####....", "...######...", "..########..", "..########..",
                                      ".##########.", "############", "############", ".##########.",
                                      "..########..", "..###..###..", "..###..###..", ".####..####.", 0};
static const char *const JAW_SHUT[] = {"######", "######", 0};
static const char *const JAW_OPEN[] = {"######", "......", "######", 0};
static const char *const HELI[] = {"#########", "....#....", ".######..", "########.", ".######..", 0};
static void sc_scream(const scn_t *s)
{
    int roar = hot(s->nago, 220), shake = s->acc && hot(s->nago, 300), w = band_of(s->nago, 450, 3), i;
    int hx = s->playing ? (int)(s->steps % 4u) : 0;
    print_c = RGB(170, 160, 150);
    pbox(0, 44, AW, 2);
    for (i = 0; i < 3; i++) {                               /* the city */
        int x = 4 + i * 14, h = 18 + (i * 7) % 12;
        pbox(x, 44 - h, 10, h);
        art(x + 2, 44 - h + 2, (const char *const[]){"#.#.#", ".....", "#.#.#", 0}, shake);
        aline(x + 3, 44 - h - 2, x + 6, 44 - h - 4, shake);
    }
    art(82, 22, MONSTER, 1);                                /* the monster, its jaw */
    art(76, 26, JAW_SHUT, !roar);
    art(76, 26, JAW_OPEN, roar);
    abox(86, 24, 2, 1, 0);
    for (i = 0; i < 3; i++)
        aarc(76, 27, 5 + i * 6, 0.38f, 0.62f, s->playing && w == i);
    for (i = 0; i < 4; i++)
        art(10 + i * 20, 2, HELI, i == hx && s->playing);
}

/* ARCADE: the platform game. The hero stands on the platform of its note; it jumps on every note, a coin pops
 * out of the block; the critter walks with the steps. */
static void sc_arcade(const scn_t *s)
{
    int pos = s->note % 5, jump = hot(s->nago, 160), crit = s->playing ? (int)(s->steps % 6u) : 0, i;
    static const int8_t PX[5] = {8, 30, 52, 74, 96}, PY[5] = {38, 30, 22, 30, 38};
    print_c = RGB(160, 150, 170);
    pbox(0, 46 - 1, AW, 1);
    for (i = 0; i < 5; i++) {
        pbox(PX[i] - 2, PY[i] + 8, 12, 2);                  /* the platforms */
        art(PX[i], PY[i], MINI, s->playing ? (i == pos && !jump) : i == 0);
        art(PX[i], PY[i] - 4, MINI_JUMP, s->playing && i == pos && jump);
        abox(PX[i] + 1, PY[i] - 14, 6, 4, 1);               /* the blocks, their coins */
        art(PX[i] + 2, PY[i] - 20, COIN, s->playing && i == pos && hot(s->nago, 300));
    }
    for (i = 0; i < 6; i++)                                 /* the critter */
        art(6 + i * 18, 42, (const char *const[]){".##.", "####", "#..#", 0}, i == crit);
}

/* ARP: the stairs. The climber is on the stair of the note (the arp runs up and down them); at the top, the
 * flag and a star. */
static void sc_arp(const scn_t *s)
{
    int st = s->note % 8, i, top = st == 7 && hot(s->nago, 300);
    print_c = RGB(168, 160, 150);
    for (i = 0; i < 8; i++) {                               /* the stairs, the climber on one */
        int x = 8 + i * 12, y = 40 - i * 5;
        pbox(x, y + 8, 12, 2);
        pbox(x + 10, y + 8, 2, 46 - y - 8);
        art(x + 3, y, MINI, s->playing ? i == st && hot(s->nago, 600) : i == 0);
    }
    pline(104, 4, 104, 14);                                 /* the flag */
    art(105, 4, (const char *const[]){"####", "###.", "##..", 0}, top);
    art(98, 0, STAR, top);
    art(108, 0, STAR, top && (s->frame & 4u));
}

/* ---------------------------------------------------------------- dispatch --- */
static uint32_t scene_hash(const scn_t *s)
{
    uint32_t h = 2166136261u, i;
    h = hash(h, (uint32_t)s->drum | (uint32_t)s->snd << 1 | (uint32_t)s->playing << 8 | (uint32_t)s->gate << 9 |
                    (uint32_t)s->acc << 10 | (uint32_t)s->note << 12 | (uint32_t)(s->chord + 1) << 20);
    h = hash(h, s->steps | (uint32_t)blinks(s) << 31);
    h = hash(h, s->ncount);
    for (i = 0; i < EN_NPART; i++)                          /* the motions that last: their stage */
        h = hash(h, s->ago[i] < 1000u ? s->ago[i] / 60u : 99u);
    h = hash(h, s->nago < 1200u ? s->nago / 50u : 99u);
    h = hash(h, (uint32_t)(s->k[0] * 3.0f) | (uint32_t)(s->k[3] > 0.05f) << 4 | (uint32_t)(s->lvl * 8.0f) << 8);
    h = hash(h, (s->frame / 2u));                          /* the slow things (blinks, smoke) */
    return h;
}

static void scene_draw(const scn_t *s)
{
    if (s->drum) {
        switch (s->snd) {
        case KIT_808: sc_808(s); break;
        case KIT_909: sc_909(s); break;
        case KIT_ARCADE: sc_arcade_kit(s); break;
        default: sc_box(s); break;
        }
        return;
    }
    switch (s->snd) {
    case SND_ACID: sc_acid(s); break;
    case SND_MONO: sc_mono(s); break;
    case SND_FAT: sc_fat(s); break;
    case SND_BOOM: sc_boom(s); break;
    case SND_FMBASS: sc_fmbass(s); break;
    case SND_REESE: sc_reese(s); break;
    case SND_POLY: sc_poly(s); break;
    case SND_BRASS: sc_brass(s); break;
    case SND_EPIANO: sc_epiano(s); break;
    case SND_ORGAN: sc_organ(s); break;
    case SND_STRINGS: sc_strings(s); break;
    case SND_BELLS: sc_bells(s); break;
    case SND_SYNC: sc_sync(s); break;
    case SND_SCREAM: sc_scream(s); break;
    case SND_ARCADE: sc_arcade(s); break;
    default: sc_arp(s); break;
    }
}
