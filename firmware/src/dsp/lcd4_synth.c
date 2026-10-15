/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4's sixteen synth sounds and the three tracks that play them. Included once, by lcd4.c.
 *
 * A track has four voices: the mono sounds use one (a new note while the last one's gate is still open is
 * legato: the pitch glides and the envelopes go on, as a 303's slide); the keys use all four, stealing the
 * quietest with a 1.5 ms fade. Each sound is a render function over a chunk, in sub-blocks of 16 samples:
 * pitch, envelopes and filter coefficients are worked out once a sub-block, the audio a sample. */

static const en_sound_t SOUNDS[EN_NSOUND] = {
    {"Acid", "TB-303", {"Cutoff", "Reso", "Env", "Decay"}, {30, 70, 60, 40}, 1},
    {"Mono", "SH-101", {"Cutoff", "Reso", "Sub", "Decay"}, {40, 35, 50, 45}, 1},
    {"Fat", "Minimoog", {"Cutoff", "Emphasis", "Contour", "Decay"}, {35, 30, 50, 45}, 1},
    {"Boom", "808 bass", {"Punch", "Drive", "Decay", "Glide"}, {40, 20, 60, 30}, 1},
    {"FM bass", "DX7", {"Bright", "Grit", "Decay", "Sub"}, {45, 25, 45, 30}, 1},
    {"Reese", "detuned saws", {"Cutoff", "Detune", "Motion", "Drive"}, {45, 50, 40, 30}, 1},
    {"Poly", "Juno-60", {"Cutoff", "Chorus", "Attack", "Release"}, {60, 40, 10, 40}, 4},
    {"Brass", "Prophet-5 / OB-Xa", {"Cutoff", "Env", "Attack", "Detune"}, {35, 60, 25, 40}, 4},
    {"E.Piano", "DX7 E.Piano 1", {"Tine", "Bark", "Decay", "Tremolo"}, {50, 35, 55, 30}, 4},
    {"Organ", "M1 house organ", {"Tone", "Perc", "Chorus", "Release"}, {55, 60, 30, 20}, 4},
    {"Strings", "Solina", {"Bright", "Ensemble", "Attack", "Release"}, {55, 70, 35, 50}, 4},
    {"Bells", "DX / PPG", {"Tone", "Metal", "Decay", "Shimmer"}, {50, 45, 55, 40}, 4},
    {"Sync", "hard-sync lead", {"Sync", "Sweep", "Cutoff", "Glide"}, {30, 50, 70, 20}, 1},
    {"Scream", "MS-20", {"Cutoff", "Peak", "Drive", "Highpass"}, {45, 70, 40, 15}, 1},
    {"Arcade", "NES / Game Boy", {"Wave", "Effect", "Decay", "Crunch"}, {30, 0, 60, 0}, 1},
    {"Arp", "chip arps", {"Mode", "Rate", "Decay", "Tone"}, {0, 0, 35, 30}, 1},
};
const en_sound_t *en_sound(int s) { return &SOUNDS[(unsigned)s % EN_NSOUND]; }

/* the knobs that are switches: their positions and names */
int en_sound_zones(int snd, int k)
{
    if (snd == SND_ARCADE && k == 0)
        return 5;
    if (snd == SND_ARCADE && k == 1)
        return 6;
    if (snd == SND_ARP && (k == 0 || k == 1))
        return 4;
    if (snd == SND_POLY && k == 1)
        return 4;
    return 0;
}
static void put(char *b, const char *s)
{
    int i;
    for (i = 0; i < 9 && s[i]; i++)
        b[i] = s[i];
    b[i] = 0;
}
void en_sound_text(int snd, int k, int v, char *b)
{
    static const char *const WAVE[5] = {"12%", "25%", "50%", "Tri", "Noise"};
    static const char *const EFX[6] = {"None", "Vibrato", "Porta", "Arp", "Blip", "Rise"};
    static const char *const MODE[4] = {"Up", "Melodic", "Down", "Octave"};
    static const char *const RATE[4] = {"1/16", "1/24", "1/32", "1/48"};
    static const char *const CHOR[4] = {"Off", "I", "II", "I+II"};
    int z = en_sound_zones(snd, k), pos = z ? (int)((float)v * 0.01f * ((float)z - 0.01f)) : 0;
    if (snd == SND_ARCADE && k == 0)
        put(b, WAVE[pos]);
    else if (snd == SND_ARCADE && k == 1)
        put(b, EFX[pos]);
    else if (snd == SND_ARP && k == 0)
        put(b, MODE[pos]);
    else if (snd == SND_ARP && k == 1)
        put(b, RATE[pos]);
    else if (snd == SND_POLY && k == 1)
        put(b, CHOR[v < 10 ? 0 : v < 45 ? 1 : v < 80 ? 2 : 3]);
    else {
        char t[4];
        int n = 0;
        do {
            t[n++] = (char)('0' + v % 10);
            v /= 10;
        } while (v && n < 3);
        while (n)
            *b++ = t[--n];
        *b = 0;
    }
}

#define NV 4
#define SUB 16
typedef struct {
    uint8_t on, gate, note, acc;
    int16_t lock[2];                   /* knob A, B for this note (-1: the track's) */
    float pitch, tgt, gl;              /* MIDI pitch now, its target; the glide's share a sub-block */
    float vel;
    float ae, fe, xe;                  /* the amp envelope, the filter's, one more (index, sync, pitch) */
    uint8_t ast, fst;                  /* stages: 0 attack, 1 decay / sustain, 2 release */
    float ph[4];
    ladder_t lad;
    float s1[2], s2[2];
    float z[4];
    int32_t gate_left;                 /* samples until the gate closes; < 0: held (a key) */
    int32_t t;                         /* samples since the note began */
    float fade;                        /* > 0: fading out for a stolen voice */
    struct {
        uint8_t note, acc, live;
        float vel;
        int32_t gate;
        int16_t lock[2];
    } pend;                            /* the note that takes the voice when the fade is done */
    uint8_t pending;
    uint32_t age;
    uint32_t lfsr;
    float nph, nout;
} sv_t;

#define CHN 1024                       /* the chorus line: 23 ms */
typedef struct {
    sv_t v[NV];
    int8_t snd;
    float accs;                        /* the 303's accent sweep */
    float lfo1, lfo2, lfo3, pwm;
    uint8_t held[8], held_snd[8], held_n;   /* the keys held (live), oldest first; and the notes they sound */
    int32_t idle;                      /* samples since the track last sounded (its chorus rings on a while) */
    float last_pitch;
    float *cbuf;
    int cw;
    float chp;                         /* the chorus's high-pass (the Juno's) */
    float out[256];
    uint32_t ages;
    /* the arp: the notes it runs over, where it is, when the next comes */
    uint8_t arp_n, arp_i, arp_on;
    int8_t arp_note[8];
    int32_t arp_left;
    int32_t arp_gate;                  /* samples the arp still runs (< 0: held) */
    float arp_vel;
    uint8_t arp_acc;
    int16_t arp_lock[2];
} st_t;
static st_t trk[EN_NSYN];
static float fade_buf[256];
static int poly_cap = NV;              /* the keys' voices a track may sound (the governor in en_render lowers it) */
static float chorus_mem[EN_NSYN][CHN] EN_POOL;

static float tick_len = 230.0f;        /* samples a tick (24 a step): the sequencer's (lcd4.c) */

static float knob01(const st_t *T, const sv_t *v, int t, int k)   /* knob k, 0..1, the note's lock first */
{
    int x = (k < 2 && v->lock[k] >= 0) ? v->lock[k] : en_cfg.sk[t][(unsigned)T->snd % EN_NSOUND][k];
    return (float)x * 0.01f;
}
static inline float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }
static inline float m2inc(float n) { return fm_minf(mtof(n) * (1.0f / EN_SR), 0.45f); }
static inline float subc(float secs) { return 1.0f - fm_expf(-(float)SUB / (fm_maxf(secs, 0.0005f) * EN_SR)); }

/* the amp envelope a sample: attack to 1, decay to s, release (gate shut) */
static inline float env_run(sv_t *v, float a, float d, float s, float r)
{
    if (v->ast == 0) {
        v->ae += (1.3f - v->ae) * a;
        if (v->ae >= 1.0f) {
            v->ae = 1.0f;
            v->ast = 1;
        }
        if (!v->gate)
            v->ast = 2;
    } else if (v->ast == 1) {
        v->ae = s + (v->ae - s) * d;
        if (!v->gate)
            v->ast = 2;
    } else {
        v->ae *= r;
    }
    return v->ae;
}
static inline float ca(float secs) { return 1.0f - fm_expf(-1.0f / (fm_maxf(secs, 0.0003f) * EN_SR)); }

/* ------------------------------------------------------------------- the sounds --- */
/* each: the voice v of track T (number t), n samples into out (mono, added). They work a sub-block at a time,
 * in passes (the oscillators into a buffer, the filter over it, the envelope out of it), so that each loop keeps
 * few values alive: the FM-1's floats share its sixteen registers. */
#undef SUB
#define SUB 32

/* the amp envelope over a block, x times it (and g) added to out: attack to 1, decay to s, release when the gate
 * shuts (gates only change between chunks) */
static void env_mix(sv_t *v, float a, float d, float s, float r, const float *x, float *out, uint32_t m, float g)
{
    float e = v->ae;
    int st = v->ast;
    uint32_t j = 0;
    if (!v->gate && st < 2)
        st = 2;
    while (j < m && st == 0) {
        e += (1.3f - e) * a;
        if (e >= 1.0f) {
            e = 1.0f;
            st = 1;
        }
        out[j] += x[j] * e * g;
        j++;
    }
    if (st == 1) {
        float sd = s * (1.0f - d);                         /* e = s + (e - s) d */
        for (; j < m; j++) {
            e = sd + e * d;
            out[j] += x[j] * e * g;
        }
    } else {
        for (; j < m; j++) {
            e *= r;
            out[j] += x[j] * e * g;
        }
    }
    v->ae = e;
    v->ast = (uint8_t)st;
}

static void r_acid(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), res = knob01(T, v, t, 1), em = (float)en_cfg.sk[t][SND_ACID][2] * 0.01f;
    float dec = (float)en_cfg.sk[t][SND_ACID][3] * 0.01f, base = exp01((int)(cut * 100.0f), 70.0f, 3500.0f);
    float fd = fm_expf(-(float)SUB / ((v->acc ? 0.2f : 0.2f + 2.3f * dec * dec) * EN_SR * 0.25f));
    float ad = t60(3.0f), ar = t60(0.012f), aa = ca(0.003f), k = 3.85f * res, accamt = v->acc ? 1.0f : 0.0f;
    float g = (1.0f + 0.6f * accamt) * v->vel, b[SUB];
    uint32_t j0, j;
    ladc_t c;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, fc, ph = v->ph[0], hp = v->z[0];
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch);
        T->accs += (v->fe * accamt - T->accs) * 0.04f;      /* the accent's sweep: consecutive accents climb */
        fc = base * fm_exp2f(v->fe * (em * 4.2f + accamt * 1.4f) + T->accs * 1.6f * res);
        ladder_coefs(&c, fc, k);
        ladder_drive(&c, 1.6f);
        v->fe *= fd;
        for (j = 0; j < m; j++) {                           /* the saw, less the 303's feedback high-pass */
            float y;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = blep_saw(ph, inc);
            hp += 0.012f * (y - hp);
            b[j] = y - hp;
        }
        v->ph[0] = ph;
        v->z[0] = hp;
        ladder_run_drive(&v->lad, &c, b, m, 1.6f);
        env_mix(v, aa, ad, 0.0f, ar, b, out + j0, m, g);
    }
}

static void r_mono(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), res = knob01(T, v, t, 1), sub = (float)en_cfg.sk[t][SND_MONO][2] * 0.008f;
    float dec = exp01(en_cfg.sk[t][SND_MONO][3], 0.08f, 2.0f), base = exp01((int)(cut * 100.0f), 60.0f, 9000.0f);
    float ad = t60(dec), ar = t60(0.07f), aa = ca(0.002f), fd = fm_expf(-(float)SUB * 6.9f / (dec * EN_SR)), b[SUB];
    uint32_t j0, j;
    ladc_t c;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, pw, ph = v->ph[0], ph1 = v->ph[1];
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch);
        T->lfo1 = wrap1(T->lfo1 + 0.6f * (float)SUB / EN_SR);
        pw = 0.5f + 0.32f * sine(T->lfo1);
        ladder_coefs(&c, base * fm_exp2f(v->fe * 3.0f + (v->acc ? 0.7f : 0.0f)), 3.98f * res);
        ladder_drive(&c, 1.3f);
        v->fe = 0.3f + (v->fe - 0.3f) * fd;
        for (j = 0; j < m; j++) {                           /* saw and pulse (the saw less another), the sub's square */
            float t2, t3;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph1 += inc * 0.5f;
            if (ph1 >= 1.0f)
                ph1 -= 1.0f;
            t2 = ph + pw;
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            t3 = ph1 + 0.5f;
            if (t3 >= 1.0f)
                t3 -= 1.0f;
            b[j] = 0.93f * blep_saw(ph, inc) - 0.38f * blep_saw(t2, inc) +
                   sub * (blep_saw(ph1, inc * 0.5f) - blep_saw(t3, inc * 0.5f));
        }
        v->ph[0] = ph;
        v->ph[1] = ph1;
        ladder_run_drive(&v->lad, &c, b, m, 1.3f);
        env_mix(v, aa, ad, 0.5f, ar, b, out + j0, m, 0.75f * v->vel);
    }
}

static void r_fat(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), res = knob01(T, v, t, 1), con = (float)en_cfg.sk[t][SND_FAT][2] * 0.01f;
    float dec = exp01(en_cfg.sk[t][SND_FAT][3], 0.1f, 2.5f), base = exp01((int)(cut * 100.0f), 40.0f, 9000.0f);
    float ad = t60(dec), ar = t60(0.09f), aa = ca(0.003f), fdd = fm_expf(-(float)SUB * 6.9f / (dec * EN_SR)), b[SUB];
    uint32_t j0;
    ladc_t c;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float i1, i2, i3;
        v->pitch += (v->tgt - v->pitch) * v->gl;
        i1 = m2inc(v->pitch);
        i2 = i1 * 1.00347f;                                  /* +6 cents */
        i3 = i1 * 0.49913f;                                  /* an octave down, -3 cents */
        if (v->fst == 0) {                                   /* the contour: 5 ms up, then down to 0.35 */
            v->fe += (1.2f - v->fe) * subc(0.005f);
            if (v->fe >= 1.0f)
                v->fe = 1.0f, v->fst = 1;
        } else {
            v->fe = 0.35f + (v->fe - 0.35f) * fdd;
        }
        ladder_coefs(&c, base * fm_exp2f(v->fe * con * 4.5f), 3.95f * res);
        ladder_drive(&c, 2.2f);
        v->ph[0] = saw_run(v->ph[0], i1, b, m, 0.55f);
        v->ph[1] = saw_add(v->ph[1], i2, b, m, 0.55f);
        v->ph[2] = saw_add(v->ph[2], i3, b, m, 0.44f);
        ladder_run_drive(&v->lad, &c, b, m, 2.2f);           /* the mixer, driven hot into the ladder */
        env_mix(v, aa, ad, 0.75f, ar, b, out + j0, m, 0.9f * v->vel);
    }
}

static void r_boom(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float punch = knob01(T, v, t, 0), drv = knob01(T, v, t, 1);
    float dec = exp01(en_cfg.sk[t][SND_BOOM][2], 0.15f, 3.5f), ad = t60(dec), ar = t60(0.03f), aa = ca(0.001f);
    float g = 1.0f + 9.0f * drv * drv, norm = 1.0f / sat(g * 0.8f), b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, ph = v->ph[0], gg = g * 0.8f;
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch + v->xe * punch * 24.0f);
        v->xe *= fm_expf(-(float)SUB / (0.028f * EN_SR));
        for (j = 0; j < m; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            b[j] = sat(sine(ph) * gg);
        }
        v->ph[0] = ph;
        env_mix(v, aa, ad, 0.0f, ar, b, out + j0, m, 0.62f * norm * v->vel);
    }
}

static void r_fmbass(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float br = knob01(T, v, t, 0), grit = knob01(T, v, t, 1);
    float dec = exp01(en_cfg.sk[t][SND_FMBASS][2], 0.1f, 2.0f), sub = (float)en_cfg.sk[t][SND_FMBASS][3] * 0.007f;
    float ad = t60(dec), ar = t60(0.06f), aa = ca(0.001f), fb = grit * 0.95f, xd = fm_expf(-(float)SUB / (0.11f * EN_SR));
    float b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, idx, p0 = v->ph[0], p2 = v->ph[2], z = v->z[0];
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch);
        idx = (0.25f + 3.2f * br * br) * (0.3f + 0.7f * v->xe) * (v->acc ? 1.3f : 1.0f) * 0.16f;
        v->xe *= xd;
        for (j = 0; j < m; j++) {
            float mo;
            p0 += inc;
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p2 += inc * 0.5f;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            mo = sine_s(p0 + fb * z);                       /* the modulator (same ratio), its own feedback */
            z = 0.5f * (z + mo);
            b[j] = sine_s(p0 + idx * mo) + sub * sine(p2);
        }
        v->ph[0] = p0;
        v->ph[2] = p2;
        v->z[0] = z;
        env_mix(v, aa, ad, 0.45f, ar, b, out + j0, m, 0.5f * v->vel);
    }
}

static void r_reese(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), det = knob01(T, v, t, 1) * 0.3f, mot = (float)en_cfg.sk[t][SND_REESE][2] * 0.01f;
    float drv = 1.0f + 5.0f * (float)en_cfg.sk[t][SND_REESE][3] * 0.01f, base = exp01((int)(cut * 100.0f), 60.0f, 7000.0f);
    float aa = ca(0.006f), ar = t60(0.16f), dn = 1.0f / sat(drv * 0.6f), b[SUB];
    uint32_t j0, j;
    ladc_t c;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float i1, i2, i3, dg = drv * 0.3f;
        v->pitch += (v->tgt - v->pitch) * v->gl;
        i3 = m2inc(v->pitch);
        i1 = i3 * fm_exp2f(det * (1.0f / 12.0f));
        i2 = i3 * fm_exp2f(-det * (1.0f / 12.0f));
        i3 *= 0.5f;
        T->lfo1 = wrap1(T->lfo1 + 0.23f * (float)SUB / EN_SR);
        ladder_coefs(&c, base * fm_exp2f(mot * 1.6f * sine(T->lfo1)), 1.3f);
        ladder_drive(&c, 1.2f);
        v->ph[0] = saw_run(v->ph[0], i1, b, m, 1.0f);
        v->ph[1] = saw_add(v->ph[1], i2, b, m, 1.0f);
        v->ph[2] = saw_add(v->ph[2], i3, b, m, 0.5f);
        for (j = 0; j < m; j++)
            b[j] = sat(b[j] * dg) * dn;
        ladder_run_drive(&v->lad, &c, b, m, 1.2f);
        env_mix(v, aa, 1.0f, 1.0f, ar, b, out + j0, m, 0.6f * v->vel);
    }
}

static void r_poly(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0);
    float att = exp01(en_cfg.sk[t][SND_POLY][2], 0.002f, 1.6f), rel = exp01(en_cfg.sk[t][SND_POLY][3], 0.05f, 3.5f);
    float aa = ca(att), ad = t60(1.4f), ar = t60(rel), base = exp01((int)(cut * 100.0f), 140.0f, 14000.0f), b[SUB];
    float inc = m2inc(v->pitch);
    uint32_t j0, j;
    ladc_t c;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float pw = 0.5f + 0.22f * sine(T->pwm), ph = v->ph[0];
        ladder_coefs(&c, base * fm_exp2f(v->ae * 1.2f), 1.0f);   /* the Juno's one envelope moves the filter too */
        for (j = 0; j < m; j++) {                           /* the DCO's saw, and its pulse (the saw less another) */
            float t2;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            t2 = ph + pw;
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            b[j] = 0.95f * blep_saw(ph, inc) - 0.4f * blep_saw(t2, inc);
        }
        v->ph[0] = ph;
        ladder_run(&v->lad, &c, b, m);
        env_mix(v, aa, ad, 0.78f, ar, b, out + j0, m, 0.28f * v->vel);
    }
}

static void r_brass(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), env = knob01(T, v, t, 1);
    float att = exp01(en_cfg.sk[t][SND_BRASS][2], 0.006f, 0.35f), det = (float)en_cfg.sk[t][SND_BRASS][3] * 0.0016f;
    float aa = ca(att * 0.6f), ad = t60(1.0f), ar = t60(0.28f), base = exp01((int)(cut * 100.0f), 120.0f, 9000.0f);
    float fa = subc(att), fd = fm_expf(-(float)SUB * 6.9f / (0.7f * EN_SR)), b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float i1 = m2inc(v->pitch + det - 0.12f * v->xe), i2 = i1 * fm_exp2f(-2.0f * det * (1.0f / 12.0f)), c[4];
        float s0 = v->s1[0], s1 = v->s1[1];
        v->xe *= 0.92f;                                      /* a blat: the pitch comes up into the note */
        if (v->fst == 0) {
            v->fe += (1.25f - v->fe) * fa;
            if (v->fe >= 1.0f)
                v->fe = 1.0f, v->fst = 1;
        } else {
            v->fe = 0.45f + (v->fe - 0.45f) * fd;
            if (!v->gate)
                v->fe *= 0.94f;
        }
        svf_coefs(c, base * fm_exp2f(v->fe * env * 4.2f), 1.1f);
        v->ph[0] = saw_run(v->ph[0], i1, b, m, 0.5f);
        v->ph[1] = saw_add(v->ph[1], i2, b, m, 0.5f);
        for (j = 0; j < m; j++) {                           /* the OB's 12 dB state-variable low-pass */
            float v3 = b[j] - s1, v1 = c[0] * s0 + c[1] * v3, v2 = s1 + c[1] * s0 + c[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            b[j] = v2;
        }
        v->s1[0] = fm_flush(s0);
        v->s1[1] = fm_flush(s1);
        env_mix(v, aa, ad, 0.92f, ar, b, out + j0, m, 0.38f * v->vel);
    }
}

static void r_epiano(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float tine = knob01(T, v, t, 0), bark = knob01(T, v, t, 1);
    float dec = exp01(en_cfg.sk[t][SND_EPIANO][2], 0.6f, 8.0f), ad = t60(dec), ar = t60(0.3f), aa = ca(0.0008f);
    float vb = v->acc ? 1.4f : 1.0f, bd = fm_expf(-(float)SUB / (1.1f * EN_SR)), td = fm_expf(-(float)SUB / (0.045f * EN_SR));
    float inc = m2inc(v->pitch), inc2 = inc * 1.00173f, b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float ib = bark * 1.9f * v->fe * vb * 0.16f, it = (0.3f + tine * 2.6f) * v->xe * vb * 0.16f;
        float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2];
        v->fe *= bd;                                         /* the body's bark dies over a second ... */
        v->xe *= td;                                         /* ... the tine's (ratio 14) in 50 ms */
        for (j = 0; j < m; j++) {
            p0 += inc;
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p1 += inc2;
            if (p1 >= 1.0f)
                p1 -= 1.0f;
            p2 += inc2 * 14.0f;
            if (p2 >= 1.0f)
                p2 -= (float)(int)p2;
            b[j] = sine_s(p0 + ib * sine(p0)) + 0.5f * sine_s(p1 + it * sine(p2));
        }
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->ph[2] = p2;
        env_mix(v, aa, ad, 0.0f, ar, b, out + j0, m, 0.26f * v->vel);
    }
}

static void r_organ(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float tone = knob01(T, v, t, 0), perc = knob01(T, v, t, 1);
    float rel = exp01(en_cfg.sk[t][SND_ORGAN][3], 0.02f, 1.2f), aa = ca(0.002f), ar = t60(rel);
    float w2 = 0.25f + 0.65f * tone, w3 = 0.12f + 0.55f * tone, w4 = 0.5f * tone * tone, pd = fm_expf(-(float)SUB / (0.11f * EN_SR));
    float inc = m2inc(v->pitch), b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float p = perc * v->xe * 1.4f, ph = v->ph[0], g3 = w3 + p, g4 = w4 + p * 0.6f;
        v->xe *= pd;                                         /* the house organ's pluck: 3rd and 4th, short */
        for (j = 0; j < m; j++) {
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            b[j] = sine(ph) + w2 * sine(ph * 2.0f) + g3 * sine(ph * 3.0f) + g4 * sine(ph * 4.0f);
        }
        v->ph[0] = ph;
        env_mix(v, aa, 1.0f, 1.0f, ar, b, out + j0, m, 0.16f * v->vel);
    }
}

static void r_strings(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float br = knob01(T, v, t, 0);
    float att = exp01(en_cfg.sk[t][SND_STRINGS][2], 0.02f, 2.2f), rel = exp01(en_cfg.sk[t][SND_STRINGS][3], 0.1f, 4.5f);
    float aa = ca(att), ar = t60(rel), lp = onepole(exp01((int)(br * 100.0f), 500.0f, 11000.0f));
    float inc = m2inc(v->pitch), b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float z0 = v->z[0], z1 = v->z[1];
        v->ph[0] = saw_run(v->ph[0], inc, b, m, 1.0f);
        v->ph[1] = saw_add(v->ph[1], inc * 2.0f, b, m, 0.4f);
        for (j = 0; j < m; j++) {
            z0 += lp * (b[j] - z0);
            z1 += lp * (z0 - z1);
            b[j] = z1;
        }
        v->z[0] = fm_flush(z0);
        v->z[1] = fm_flush(z1);
        env_mix(v, aa, 1.0f, 1.0f, ar, b, out + j0, m, 0.19f * v->vel);
    }
}

static const float BELL_RATIO[8] = {1.0f, 1.414f, 2.0f, 2.76f, 3.5f, 4.2f, 5.19f, 7.11f};
static void r_bells(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float tone = knob01(T, v, t, 0), metal = knob01(T, v, t, 1);
    float dec = exp01(en_cfg.sk[t][SND_BELLS][2], 0.3f, 9.0f), shim = (float)en_cfg.sk[t][SND_BELLS][3] * 0.006f;
    float ratio = BELL_RATIO[(int)(metal * 7.99f)], ad = t60(dec), ar = t60(fm_minf(dec, 1.2f)), aa = ca(0.0008f);
    float xd = fm_expf(-(float)SUB / (0.35f * dec * EN_SR)), inc = m2inc(v->pitch), b[SUB];
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float idx = (0.15f + 3.5f * tone) * (0.2f + 0.8f * v->xe) * (v->acc ? 1.3f : 1.0f) * 0.16f;
        float p0 = v->ph[0], p1 = v->ph[1], p2 = v->ph[2], im = inc * ratio, i2 = inc * 1.0035f;
        v->xe *= xd;
        for (j = 0; j < m; j++) {
            float mo;
            p0 += inc;
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            p1 += im;
            if (p1 >= 1.0f)
                p1 -= (float)(int)p1;
            p2 += i2;
            if (p2 >= 1.0f)
                p2 -= 1.0f;
            mo = sine(p1) * idx;
            b[j] = sine_s(p0 + mo) + shim * sine_s(p2 + mo);
        }
        v->ph[0] = p0;
        v->ph[1] = p1;
        v->ph[2] = p2;
        env_mix(v, aa, ad, 0.0f, ar, b, out + j0, m, 0.3f * v->vel);
    }
}

static void r_sync(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float sy = knob01(T, v, t, 0), sw = knob01(T, v, t, 1), cut = (float)en_cfg.sk[t][SND_SYNC][2] * 0.01f;
    float aa = ca(0.002f), ad = t60(1.5f), ar = t60(0.1f), xd = fm_expf(-(float)SUB / (0.22f * EN_SR)), b[SUB];
    uint32_t j0, j;
    ladc_t c;
    ladder_coefs(&c, exp01((int)(cut * 100.0f), 300.0f, 15000.0f), 1.0f);
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, ratio, pm = v->ph[0], ps = v->ph[1], is;
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch) * 0.5f;                        /* run at twice the rate, naive, then averaged */
        ratio = 1.0f + 7.0f * fm_clampf(sy + sw * v->xe, 0.0f, 1.0f);
        is = inc * ratio;
        v->xe *= xd;
        for (j = 0; j < m; j++) {
            float y = 0.0f;
            int k;
            for (k = 0; k < 2; k++) {
                pm += inc;
                ps += is;
                if (pm >= 1.0f) {
                    pm -= 1.0f;
                    ps = pm * ratio;                        /* the slave restarts with the master */
                }
                if (ps >= 1.0f)
                    ps -= (float)(int)ps;
                y += ps;
            }
            b[j] = y - 1.0f;
        }
        v->ph[0] = pm;
        v->ph[1] = ps;
        ladder_run(&v->lad, &c, b, m);
        env_mix(v, aa, ad, 0.85f, ar, b, out + j0, m, 0.58f * v->vel);
    }
}

static void r_scream(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float cut = knob01(T, v, t, 0), peak = knob01(T, v, t, 1), drv = (float)en_cfg.sk[t][SND_SCREAM][2] * 0.01f;
    float hp = exp01(en_cfg.sk[t][SND_SCREAM][3], 20.0f, 2500.0f), aa = ca(0.002f), ad = t60(1.2f), ar = t60(0.08f);
    float g = 1.0f + 5.0f * drv, base = exp01((int)(cut * 100.0f), 70.0f, 11000.0f), ch[4], b[SUB];
    float fd = fm_expf(-(float)SUB * 6.9f / (0.45f * EN_SR)), og = 1.6f / (0.8f + g * 0.4f);
    uint32_t j0, j;
    svf_coefs(ch, hp, 1.4f);
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float inc, inc2, c[4], bp, gh = g * 0.5f;
        v->pitch += (v->tgt - v->pitch) * v->gl;
        inc = m2inc(v->pitch);
        inc2 = inc * 1.00463f;
        svf_coefs(c, base * fm_exp2f(v->fe * 2.6f), 2.0f - 1.97f * peak);
        v->fe = 0.25f + (v->fe - 0.25f) * fd;
        v->ph[0] = saw_run(v->ph[0], inc, b, m, 0.6f);
        {
            float p1 = v->ph[1];
            for (j = 0; j < m; j++) {
                float t2;
                p1 += inc2;
                if (p1 >= 1.0f)
                    p1 -= 1.0f;
                t2 = p1 + 0.5f;
                if (t2 >= 1.0f)
                    t2 -= 1.0f;
                b[j] += 0.45f * (blep_saw(p1, inc2) - blep_saw(t2, inc2));
            }
            v->ph[1] = p1;
        }
        for (j = 0; j < m; j++) {                           /* the high-pass, then the screaming low-pass */
            float y = svf_hp(v->s2, ch, b[j], &bp);
            b[j] = svf_hot(v->s1, c, y * gh, &bp) * og;
        }
        env_mix(v, aa, ad, 0.7f, ar, b, out + j0, m, 0.43f * v->vel);
    }
}

/* the arcade: a console's channels; Wave picks the channel, Effect what a tracker would do to it. Its volume
 * moves in sixteen steps, as the console's did, so it keeps its own envelope. */
static void r_arcade(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float wave = knob01(T, v, t, 0), fx = knob01(T, v, t, 1);
    float dk = (float)en_cfg.sk[t][SND_ARCADE][2] * 0.01f, crunch = (float)en_cfg.sk[t][SND_ARCADE][3] * 0.01f;
    float ad = dk > 0.97f ? 1.0f : t60(exp01((int)(dk * 100.0f), 0.05f, 3.0f)), ar = t60(0.03f), aa = ca(0.0005f);
    int w = (int)(wave * 4.99f), e = (int)(fx * 5.99f), hold = 1 + (int)(crunch * 10.0f);
    float pw = w == 0 ? 0.125f : w == 1 ? 0.25f : 0.5f, g = 1.15f * v->vel;
    uint32_t j0, j;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float p, inc, sec = (float)(v->t + (int32_t)j0) * (1.0f / EN_SR);
        v->pitch += (v->tgt - v->pitch) * v->gl;
        p = v->pitch;
        switch (e) {
        case 1: p += sec > 0.15f ? 0.4f * sine(sec * 6.0f) * fm_minf((sec - 0.15f) * 4.0f, 1.0f) : 0.0f; break;  /* vibrato */
        case 3: {                                                                       /* the tracker's arpeggio */
            static const int8_t ARP[2][3] = {{0, 4, 7}, {0, 3, 7}};
            int minor = chord_now_q == CQ_MIN || chord_now_q == CQ_MIN7 || chord_now_q == CQ_DIM;
            p += (float)ARP[minor][((v->t + (int32_t)j0) / 735) % 3];                   /* a frame: 1/60 s */
            break;
        }
        case 4: p += 12.0f * v->xe; v->xe *= 0.7f; break;                                /* blip: down from the octave */
        case 5: p += 7.0f * (1.0f - v->xe) - 7.0f; v->xe *= 0.88f; break;               /* power-up: rising into it */
        default: break;
        }
        inc = m2inc(p);
        for (j = j0; j < j0 + m; j++) {
            float y, a = env_run(v, aa, ad, 0.0f, ar);
            v->ph[0] = wrap1(v->ph[0] + inc);
            if (w < 3) {
                y = 0.5f * blep_pulse(v->ph[0], inc, pw);
            } else if (w == 3) {
                y = tri32(v->ph[0]) * 0.6f;
            } else {                                              /* the noise channel, clocked by the pitch */
                v->nph += inc * 8.0f;
                while (v->nph >= 1.0f) {
                    uint32_t bit = (v->lfsr ^ (v->lfsr >> (wave > 0.92f ? 6 : 1))) & 1u;
                    v->nph -= 1.0f;
                    v->lfsr = (v->lfsr >> 1) | (bit << 14);
                    v->nout = (v->lfsr & 1u) ? 0.45f : -0.45f;
                }
                y = v->nout;
            }
            y *= nes_step(a);
            if (hold > 1) {                                       /* the crunch: a slower, coarser DAC */
                if (++v->z[2] >= (float)hold) {
                    v->z[2] = 0.0f;
                    v->z[3] = fm_floorf(y * 32.0f + 0.5f) * (1.0f / 32.0f);   /* rounded: quiet notes stay */
                }
                y = v->z[3];
            }
            v->z[0] += 0.013f * (y - v->z[0]);                   /* the console's high-pass */
            out[j] += (y - v->z[0]) * g;
        }
    }
}

/* the arp: the notes, one at a time at Rate, each a short chip note; Mode orders them */
static void r_arp(st_t *T, sv_t *v, int t, uint32_t n, float *out)
{
    float tone = (float)en_cfg.sk[t][SND_ARP][3] * 0.01f, dk = (float)en_cfg.sk[t][SND_ARP][2] * 0.01f;
    float ad = t60(exp01((int)(dk * 100.0f), 0.04f, 1.0f)), ar = t60(0.02f), aa = ca(0.0005f);
    float pw = tone < 0.5f ? 0.5f - tone * 0.7f : 0.5f, sawmix = tone > 0.5f ? (tone - 0.5f) * 2.0f : 0.0f;
    float lp = onepole(2500.0f + 8000.0f * tone), inc = m2inc(v->pitch), b[SUB];
    float g1 = (1.0f - sawmix) * 0.5f, g2 = sawmix * 0.5f;
    uint32_t j0, j;
    (void)T;
    for (j0 = 0; j0 < n; j0 += SUB) {
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;
        float ph = v->ph[0], z = v->z[0];
        for (j = 0; j < m; j++) {
            float t2, s1;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            t2 = ph + pw;
            if (t2 >= 1.0f)
                t2 -= 1.0f;
            s1 = blep_saw(ph, inc);
            z += lp * ((g1 + g2) * s1 - g1 * blep_saw(t2, inc) - z);
            b[j] = z;
        }
        v->ph[0] = ph;
        v->z[0] = z;
        env_mix(v, aa, ad, 0.0f, ar, b, out + j0, m, 1.4f * v->vel);
    }
}

typedef void (*render_fn)(st_t *, sv_t *, int, uint32_t, float *);
static const render_fn RENDER[EN_NSOUND] = {r_acid, r_mono, r_fat, r_boom, r_fmbass, r_reese, r_poly, r_brass,
                                             r_epiano, r_organ, r_strings, r_bells, r_sync, r_scream, r_arcade, r_arp};

/* ----------------------------------------------------------------- the voices --- */
static float glide_of(int t, int snd, int legato)              /* the glide's share a sub-block (1: none) */
{
    float secs = 0.0f;
    switch (snd) {
    case SND_ACID: secs = legato ? 0.045f : 0.0f; break;           /* the 303's slide: a fixed RC */
    case SND_MONO:
    case SND_REESE:
    case SND_SCREAM: secs = legato ? 0.05f : 0.0f; break;
    case SND_FAT: secs = legato ? 0.06f : 0.0f; break;
    case SND_BOOM: secs = legato ? exp01(en_cfg.sk[t][SND_BOOM][3], 0.02f, 0.45f) : 0.0f; break;
    case SND_SYNC:
        secs = en_cfg.sk[t][SND_SYNC][3] > 2 ? exp01(en_cfg.sk[t][SND_SYNC][3], 0.008f, 0.5f) : (legato ? 0.04f : 0.0f);
        break;
    case SND_ARCADE: {
        int e = (int)((float)en_cfg.sk[t][SND_ARCADE][1] * 0.01f * 5.99f);
        secs = e == 2 ? 0.07f : 0.0f;                               /* portamento */
        break;
    }
    default: break;
    }
    return secs > 0.0f ? subc(secs) : 1.0f;
}

static void voice_start(st_t *T, int t, sv_t *v, int note, float vel, int acc, int32_t gate, const int16_t *lock)
{
    int snd = T->snd;
    float from = T->last_pitch;
    v->on = 1;
    v->gate = 1;
    v->note = (uint8_t)note;
    v->acc = (uint8_t)acc;
    v->lock[0] = lock ? lock[0] : -1;
    v->lock[1] = lock ? lock[1] : -1;
    v->vel = vel;
    v->tgt = (float)note;
    v->gl = glide_of(t, snd, 0);
    v->pitch = (v->gl < 1.0f && from > 0.0f) ? from : (float)note;
    v->ast = 0;
    v->fst = 0;
    v->fe = 1.0f;
    v->xe = 1.0f;
    if (snd == SND_FAT || snd == SND_BRASS)
        v->fe = 0.0f;                                         /* their filters have an attack */
    v->gate_left = gate;
    v->t = 0;
    v->fade = 0.0f;
    v->pending = 0;
    v->age = ++T->ages;
    if (!v->lfsr)
        v->lfsr = 0x7FFFu;
    if (v->ae < 0.0001f) {                                   /* from silence: the oscillators from the top */
        v->ph[0] = v->ph[1] = v->ph[2] = v->ph[3] = 0.0f;
        v->lad.s[0] = v->lad.s[1] = v->lad.s[2] = v->lad.s[3] = 0.0f;
        v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
        v->z[0] = v->z[1] = 0.0f;
    }
    T->last_pitch = (float)note;
}

/* a note for track t: the sequencer's (gate in samples) or a key's (gate < 0: until en_note off) */
static void syn_note_on(int t, int note, float vel, int acc, int32_t gate, const int16_t *lock)
{
    st_t *T = &trk[t];
    int i, best = -1;
    if (note < 12 || note > 120)
        return;
    if (SOUNDS[T->snd].poly == 1) {
        sv_t *v = &T->v[0];
        if (T->snd == SND_ARP)
            return;                                          /* the arp has its own way in (arp_start) */
        if (v->on && v->gate && v->ast != 2) {               /* legato: the 303's slide */
            v->tgt = (float)note;
            v->gl = glide_of(t, T->snd, 1);
            v->note = (uint8_t)note;
            v->acc = (uint8_t)acc;
            v->gate_left = gate;
            v->lock[0] = lock ? lock[0] : -1;
            v->lock[1] = lock ? lock[1] : -1;
            if (acc)
                v->fe = 1.0f;
            T->last_pitch = (float)note;
            return;
        }
        voice_start(T, t, v, note, vel, acc, gate, lock);
        return;
    }
    for (i = 0; i < NV; i++)                                 /* the same note: that voice again */
        if (T->v[i].on && T->v[i].note == note && !T->v[i].pending) {
            best = i;
            break;
        }
    if (best < 0) {
        int sounding = 0;
        for (i = 0; i < NV; i++)
            sounding += T->v[i].on && !T->v[i].pending && T->v[i].fade <= 0.0f;
        if (sounding < poly_cap)
            for (i = 0; i < NV; i++)
                if (!T->v[i].on) {
                    best = i;
                    break;
                }
    }
    if (best < 0) {                                          /* steal: a released one, quietest; else the oldest */
        float q = 1e9f;
        for (i = 0; i < NV; i++) {
            sv_t *v = &T->v[i];
            float score = v->ae + (v->gate ? 10.0f : 0.0f) + (float)(v->age) * 1e-6f + (v->pending ? 20.0f : 0.0f);
            if (v->on && score < q) {
                q = score;
                best = i;
            }
        }
    }
    {
        sv_t *v = &T->v[best];
        if (v->on && v->ae > 0.02f && v->note != note) {     /* fade it, then the new note */
            v->fade = 1.0f;
            v->pending = 1;
            v->pend.note = (uint8_t)note;
            v->pend.vel = vel;
            v->pend.acc = (uint8_t)acc;
            v->pend.gate = gate;
            v->pend.lock[0] = lock ? lock[0] : -1;
            v->pend.lock[1] = lock ? lock[1] : -1;
            return;
        }
        voice_start(T, t, v, note, vel, acc, gate, lock);
    }
}

static void syn_note_off(int t, int note)                    /* a key let go */
{
    st_t *T = &trk[t];
    int i;
    if (T->snd == SND_ARP) {
        if (T->arp_gate < 0 && !T->held_n)
            T->arp_gate = 0;
        return;
    }
    if (SOUNDS[T->snd].poly == 1) {                          /* mono: back to the last key still held */
        sv_t *v = &T->v[0];
        if (v->on && v->gate_left < 0 && v->note == note) {
            if (T->held_n) {
                int back = T->held_snd[T->held_n - 1];
                v->tgt = (float)back;
                v->gl = glide_of(t, T->snd, 1);
                v->note = (uint8_t)back;
                T->last_pitch = (float)back;
            } else {
                v->gate = 0;
            }
        }
        return;
    }
    for (i = 0; i < NV; i++) {
        sv_t *v = &T->v[i];
        if (v->on && v->gate_left < 0 && v->note == note)
            v->gate = 0;
        if (v->pending && v->pend.note == note && v->pend.gate < 0)
            v->pend.gate = 1;
    }
}

/* ------------------------------------------------------------------- the arp --- */
static void arp_start(int t, const int *notes, int nn, float vel, int acc, int32_t gate, const int16_t *lock)
{
    st_t *T = &trk[t];
    int i, k = 0, mode = (int)((float)en_cfg.sk[t][SND_ARP][0] * 0.01f * 3.99f);
    int tones[8], nt = 0;
    if (nn >= 2) {
        for (i = 0; i < nn && nt < 4; i++)
            tones[nt++] = notes[i];
    } else {                                                 /* one note: the chord playing, or its triad in C */
        int pc[4], c, base = notes[0];
        if (chord_now >= 0) {
            c = en_chord_tones(chord_now, 0, pc);
            for (i = 0; i < c; i++) {
                int nnote = base - ((base - pc[i]) % 12 + 12) % 12;   /* each tone at or below the note ... */
                tones[nt++] = nnote < base ? nnote + 12 : nnote;     /* ... up into its octave */
            }
            tones[0] = base;
        } else {
            int r = ((base - (int)key_off()) % 12 + 12) % 12, third = (r == 2 || r == 4 || r == 9 || r == 11) ? 3 : 4;
            tones[nt++] = base;
            tones[nt++] = base + third;
            tones[nt++] = base + (r == 11 ? 6 : 7);
        }
        for (i = 1; i < nt; i++) {                          /* in order */
            int x = tones[i], j = i;
            while (j > 0 && tones[j - 1] > x) {
                tones[j] = tones[j - 1];
                j--;
            }
            tones[j] = x;
        }
    }
    /* the run: up (two octaves), melodic (leapfrog), down, octaves */
    switch (mode) {
    case 0:
        for (i = 0; i < nt; i++)
            T->arp_note[k++] = (int8_t)(tones[i] - 60);
        for (i = 0; i < nt && k < 8; i++)
            T->arp_note[k++] = (int8_t)(tones[i] + 12 - 60);
        break;
    case 1: {
        static const uint8_t LEAP[8] = {0, 2, 1, 3, 2, 4, 3, 5};
        for (i = 0; i < 8; i++) {
            int x = LEAP[i];
            T->arp_note[k++] = (int8_t)(tones[x % nt] + 12 * (x / nt) - 60);
        }
        break;
    }
    case 2:
        for (i = nt - 1; i >= 0; i--)
            T->arp_note[k++] = (int8_t)(tones[i] + 12 - 60);
        for (i = nt - 1; i >= 0 && k < 8; i--)
            T->arp_note[k++] = (int8_t)(tones[i] - 60);
        break;
    default:
        for (i = 0; i < nt && k < 8; i++) {
            T->arp_note[k++] = (int8_t)(tones[i] - 60);
            T->arp_note[k++] = (int8_t)(tones[i] + 12 - 60);
        }
        break;
    }
    T->arp_n = (uint8_t)k;
    T->arp_i = 0;
    T->arp_on = 1;
    T->arp_left = 0;
    T->arp_gate = gate;
    T->arp_vel = vel;
    T->arp_acc = (uint8_t)acc;
    T->arp_lock[0] = lock ? lock[0] : -1;
    T->arp_lock[1] = lock ? lock[1] : -1;
}

static void arp_run(int t, uint32_t n)                       /* the next note, when its time comes (in this chunk) */
{
    st_t *T = &trk[t];
    static const uint8_t RATE_TICKS[4] = {24, 16, 12, 8};
    sv_t *v = &T->v[0];
    if (!T->arp_on)
        return;
    if (T->arp_gate == 0) {
        T->arp_on = 0;
        v->gate = 0;
        return;
    }
    if (T->arp_left <= 0) {
        int r = (int)((float)(T->arp_lock[1] >= 0 ? T->arp_lock[1] : en_cfg.sk[t][SND_ARP][1]) * 0.01f * 3.99f);
        int note = 60 + T->arp_note[T->arp_i % T->arp_n];
        T->arp_i++;
        voice_start(T, t, v, note, T->arp_vel, T->arp_acc && T->arp_i == 1, -1, T->arp_lock);
        T->arp_left += (int32_t)(tick_len * (float)RATE_TICKS[r]);
    }
    T->arp_left -= (int32_t)n;
    if (T->arp_gate > 0) {
        T->arp_gate -= (int32_t)n;
        if (T->arp_gate <= 0)
            T->arp_gate = 0;
    }
}

/* ----------------------------------------------------------------- the chorus --- */
/* the Juno's (I: 0.5 Hz, II: 0.83 Hz, both: a fast shimmer), the Solina's ensemble (three taps, a slow and a
 * fast wobble 120 degrees apart), an organ's rotor-ish chorus; out in stereo */
enum { CH_OFF, CH_JUNO1, CH_JUNO2, CH_JUNO12, CH_ENSEMBLE, CH_ROTOR };
static inline float ctap(const float *b, int w, float d)
{
    int k = (int)d, i0 = w - k, i1;
    float f = d - (float)k;
    if (i0 < 0)
        i0 += CHN;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += CHN;
    return b[i0] + (b[i1] - b[i0]) * f;
}
static float ch_tri(float ph) { return fm_fabsf(ph * 4.0f - 2.0f) - 1.0f; }   /* the Juno's LFO: a triangle */
static void chorus_run(st_t *T, int mode, float mix, const float *in, float *l, float *r, uint32_t n, float gl, float gr)
{
    float r1, d0, dep, r2 = 0.0f, dep2 = 0.0f;
    uint32_t j0, j;
    switch (mode) {
    case CH_JUNO1: r1 = 0.513f; d0 = 160.0f; dep = 75.0f; break;
    case CH_JUNO2: r1 = 0.863f; d0 = 160.0f; dep = 95.0f; break;
    case CH_JUNO12: r1 = 9.75f; d0 = 150.0f; dep = 14.0f; break;
    case CH_ENSEMBLE: r1 = 0.63f; d0 = 220.0f; dep = 110.0f; r2 = 6.1f; dep2 = 12.0f; break;
    default: r1 = 5.8f; d0 = 120.0f; dep = 40.0f; break;
    }
    for (j0 = 0; j0 < n; j0 += SUB) {                        /* the delays worked out a sub-block's ends apart, */
        uint32_t m = n - j0 < SUB ? n - j0 : SUB;             /* and slid between them */
        float a3 = T->lfo3, a2 = T->lfo2, e3, e2, da[3], db[3], st[3], im = 1.0f / (float)m;
        int k, taps = mode == CH_ENSEMBLE ? 3 : 2;
        e3 = wrap1(a3 + r1 * (float)m * (1.0f / EN_SR));
        e2 = wrap1(a2 + (r2 > 0.0f ? r2 : 0.5f) * (float)m * (1.0f / EN_SR));
        if (mode == CH_ENSEMBLE) {
            for (k = 0; k < 3; k++) {
                float o = (float)k * 0.3333f;
                da[k] = d0 + dep * sine(a3 + o) + dep2 * sine(a2 + o);
                db[k] = d0 + dep * sine(e3 + o) + dep2 * sine(e2 + o);
            }
        } else {
            da[0] = d0 + dep * ch_tri(a3);
            db[0] = d0 + dep * ch_tri(e3);
            da[1] = 2.0f * d0 - da[0];
            db[1] = 2.0f * d0 - db[0];
        }
        for (k = 0; k < taps; k++)
            st[k] = (db[k] - da[k]) * im;
        for (j = j0; j < j0 + m; j++) {
            float x = in[j], wl, wr;
            T->chp += 0.004f * (x - T->chp);                 /* the chorus hears no bass */
            T->cbuf[T->cw] = x - T->chp;
            if (taps == 3) {
                float c = ctap(T->cbuf, T->cw, da[2]);
                wl = ctap(T->cbuf, T->cw, da[0]) + 0.5f * c;
                wr = ctap(T->cbuf, T->cw, da[1]) + 0.5f * c;
                da[2] += st[2];
            } else {
                wl = ctap(T->cbuf, T->cw, da[0]);
                wr = ctap(T->cbuf, T->cw, da[1]);
            }
            da[0] += st[0];
            da[1] += st[1];
            if (++T->cw >= CHN)
                T->cw = 0;
            l[j] += (x + mix * wl) * gl;
            r[j] += (x + mix * wr) * gr;
        }
        T->lfo3 = e3;
        T->lfo2 = e2;
    }
}

/* the track's sound changed: everything it was playing stops */
static void trk_sound(int t, int snd)
{
    st_t *T = &trk[t];
    int i;
    T->snd = (int8_t)snd;
    for (i = 0; i < NV; i++) {
        T->v[i].on = 0;
        T->v[i].pending = 0;
        T->v[i].ae = 0.0f;
    }
    T->arp_on = 0;
    T->accs = 0.0f;
}

static void trk_quiet(int t)
{
    st_t *T = &trk[t];
    int i;
    for (i = 0; i < NV; i++) {
        T->v[i].gate = 0;
        T->v[i].pending = 0;
        if (T->v[i].gate_left < 0)
            T->v[i].gate_left = 0;
    }
    T->arp_on = 0;
}

/* a track's chunk: its voices into T->out (mono), then the chorus or the pan into l, r; 0 if it was silent
 * (and l, r untouched) */
static int trk_run(int t, uint32_t n, float *l, float *r, float gain, float pan)
{
    st_t *T = &trk[t];
    int i, snd = en_cfg.tp[t + 1][TP_SOUND] % EN_NSOUND, mode = CH_OFF, any = 0;
    uint32_t j;
    float gl = gain * (pan > 0.0f ? 1.0f - pan : 1.0f), gr = gain * (pan < 0.0f ? 1.0f + pan : 1.0f), mix = 0.0f;
    if (snd != T->snd)
        trk_sound(t, snd);
    if (snd == SND_ARP)
        arp_run(t, n);
    for (i = 0; i < NV; i++)
        any |= T->v[i].on;
    if (any || T->arp_on)
        T->idle = 0;
    else if ((T->idle += (int32_t)n) > 4 * CHN) {     /* silent, and its chorus has died away */
        en_level[t + 1] *= 0.97f;
        T->idle = 4 * CHN + 1;
        return 0;
    }
    for (j = 0; j < n; j++)
        T->out[j] = 0.0f;
    for (i = 0; i < NV; i++) {
        sv_t *v = &T->v[i];
        if (!v->on)
            continue;
        if (v->fade > 0.0f) {                                    /* stolen: faded out over 1.5 ms, then the new */
            for (j = 0; j < n; j++)
                fade_buf[j] = 0.0f;
            RENDER[snd](T, v, t, n, fade_buf);
            for (j = 0; j < n; j++) {
                float g = v->fade - (float)j * (1.0f / 64.0f);
                T->out[j] += fade_buf[j] * (g > 0.0f ? g : 0.0f);
            }
            v->fade -= (float)n * (1.0f / 64.0f);
            if (v->fade <= 0.0f) {
                v->fade = 0.0f;
                v->ae = 0.0f;
                if (v->pending)
                    voice_start(T, t, v, v->pend.note, v->pend.vel, v->pend.acc, v->pend.gate, v->pend.lock);
                else
                    v->on = 0;
            }
            continue;
        }
        RENDER[snd](T, v, t, n, T->out);
        v->t += (int32_t)n;
        if (v->gate_left > 0) {
            v->gate_left -= (int32_t)n;
            if (v->gate_left <= 0) {
                v->gate_left = 0;
                v->gate = 0;
            }
        }
        if (v->ast >= 1 && v->ae < 0.0003f && v->t > 256)
            v->on = 0;
    }
    switch (snd) {                                               /* the keys' chorus */
    case SND_POLY: {
        int c = en_cfg.sk[t][SND_POLY][1];
        mode = c < 10 ? CH_OFF : c < 45 ? CH_JUNO1 : c < 80 ? CH_JUNO2 : CH_JUNO12;
        mix = 0.9f;
        break;
    }
    case SND_STRINGS:
        mode = en_cfg.sk[t][SND_STRINGS][1] < 5 ? CH_OFF : CH_ENSEMBLE;
        mix = (float)en_cfg.sk[t][SND_STRINGS][1] * 0.012f;
        break;
    case SND_ORGAN:
        mode = en_cfg.sk[t][SND_ORGAN][2] < 5 ? CH_OFF : CH_ROTOR;
        mix = (float)en_cfg.sk[t][SND_ORGAN][2] * 0.01f;
        break;
    default: break;
    }
    if (mode != CH_OFF) {
        chorus_run(T, mode, mix, T->out, l, r, n, gl * 0.85f, gr * 0.85f);
    } else if (snd == SND_EPIANO && en_cfg.sk[t][SND_EPIANO][3] > 2) {   /* the suitcase's stereo tremolo */
        float d = (float)en_cfg.sk[t][SND_EPIANO][3] * 0.009f;
        for (j = 0; j < n; j++) {
            float s;
            T->lfo1 = wrap1(T->lfo1 + 4.6f * (1.0f / EN_SR));
            s = sine(T->lfo1) * d;
            l[j] += T->out[j] * gl * (1.0f - s);
            r[j] += T->out[j] * gr * (1.0f + s);
        }
    } else {
        for (j = 0; j < n; j++) {
            l[j] += T->out[j] * gl;
            r[j] += T->out[j] * gr;
        }
    }
    {
        float pk = 0.0f;
        for (j = 0; j < n; j += 4)
            pk = fm_maxf(pk, fm_fabsf(T->out[j]));
        en_level[t + 1] = fm_maxf(pk * gain * 1.5f, en_level[t + 1] * 0.97f);
    }
    T->pwm = wrap1(T->pwm + 0.5f * (float)n / EN_SR);
    return 1;
}
