/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4's drums: four kits, eleven parts each, on the same slots in every kit (a pattern written on one
 * kit still makes sense on another): kick, snare, three toms, a rim (or a click), a clap, closed and open
 * hats, a cymbal, and one more. Synthesized the way each machine did it:
 *   808   bridged-T resonators (a sine that rings and dies), the six square oscillators of its cymbal and
 *         hats through band-passes, a clap of noise bursts, the cowbell's two squares;
 *   909   a swept kick with a click, triangles and noise for the snare, swept toms, hats and cymbals of
 *         metal and noise (the real ones were 6-bit samples);
 *   ARC   a games console's sound chip: pulses and a triangle swept in pitch, a 15-bit noise register
 *         (long, or short and metallic), volume in sixteen steps; blips, zaps and a coin;
 *   BOX   a CR-78 and a TR-606: soft kicks, a snare, toms, a conga, claves, the guiro's scrape,
 *         metallic hats, the tambourine and the cowbell.
 * Included once, by lcd4.c. */

const char *const EN_KIT_NAME[EN_NKIT] = {"808", "909", "Arcade", "Box"};

enum { T_KICK, T_SNARE, T_TOM, T_RIM, T_CLAP, T_HAT, T_CYM, T_COWBELL, T_RIDE, T_CLAVES, T_GUIRO, T_TAMB,
       T_CKICK, T_CSNARE, T_CTOM, T_CHAT, T_BLIP, T_ZAP, T_COIN, T_CCRASH };
typedef struct {
    uint8_t type;
    float f;                           /* the pitch (Hz) */
    float dec;                         /* the decay, -60 dB (s) */
    float a, b;                        /* the type's own */
    float pan;                         /* -1 left .. 1 right */
} dpart_t;

static const dpart_t KIT[EN_NKIT][EN_NPART] = {
    {   /* 808 */
        {T_KICK, 50.0f, 0.95f, 1.45f, 0.007f, 0.0f},        /* a: the sweep's start (x), b: its time */
        {T_SNARE, 180.0f, 0.2f, 330.0f, 0.0f, 0.0f},       /* a: the second resonator, b: the sweep (x) */
        {T_TOM, 92.0f, 0.5f, 1.25f, 0.06f, -0.35f},
        {T_TOM, 130.0f, 0.42f, 1.25f, 0.05f, 0.0f},
        {T_TOM, 180.0f, 0.36f, 1.25f, 0.045f, 0.35f},
        {T_RIM, 1667.0f, 0.03f, 455.0f, 0.0f, 0.15f},
        {T_CLAP, 1100.0f, 0.32f, 3.0f, 0.0f, 0.0f},        /* a: the bursts */
        {T_HAT, 7200.0f, 0.06f, 0.0f, 0.0f, 0.25f},        /* a: noise in the metal (0..1) */
        {T_HAT, 7200.0f, 0.5f, 0.0f, 0.0f, 0.25f},
        {T_CYM, 3440.0f, 1.4f, 0.0f, 0.0f, -0.25f},
        {T_COWBELL, 540.0f, 0.4f, 800.0f, 0.0f, -0.15f},
    },
    {   /* 909 */
        {T_KICK, 54.0f, 0.55f, 4.2f, 0.022f, 0.0f},
        {T_SNARE, 190.0f, 0.22f, 345.0f, 1.5f, 0.0f},
        {T_TOM, 98.0f, 0.38f, 1.9f, 0.09f, -0.35f},
        {T_TOM, 135.0f, 0.34f, 1.9f, 0.08f, 0.0f},
        {T_TOM, 190.0f, 0.3f, 1.9f, 0.07f, 0.35f},
        {T_RIM, 1250.0f, 0.035f, 520.0f, 1.0f, 0.15f},
        {T_CLAP, 1250.0f, 0.38f, 4.0f, 0.0f, 0.0f},
        {T_HAT, 8800.0f, 0.07f, 0.55f, 0.0f, 0.25f},
        {T_HAT, 8800.0f, 0.55f, 0.55f, 0.0f, 0.25f},
        {T_CYM, 5200.0f, 1.8f, 0.7f, 0.0f, -0.25f},       /* the crash */
        {T_RIDE, 3200.0f, 1.6f, 0.0f, 0.0f, -0.15f},
    },
    {   /* ARCADE */
        {T_CKICK, 50.0f, 0.22f, 8.0f, 0.04f, 0.0f},         /* a: the sweep's start (x), b: its time */
        {T_CSNARE, 210.0f, 0.2f, 0.0f, 0.0f, 0.0f},
        {T_CTOM, 110.0f, 0.22f, 0.0f, 0.0f, -0.35f},
        {T_CTOM, 165.0f, 0.2f, 0.0f, 0.0f, 0.0f},
        {T_CTOM, 245.0f, 0.18f, 0.0f, 0.0f, 0.35f},
        {T_BLIP, 784.0f, 0.16f, 0.0f, 0.0f, 0.2f},
        {T_ZAP, 2600.0f, 0.3f, 0.0f, 0.0f, -0.2f},
        {T_CHAT, 0.0f, 0.045f, 1.0f, 0.0f, 0.25f},          /* a: 1 short (metallic) */
        {T_CHAT, 0.0f, 0.32f, 0.0f, 0.0f, 0.25f},
        {T_CCRASH, 0.0f, 0.9f, 0.0f, 0.0f, -0.25f},
        {T_COIN, 988.0f, 0.4f, 0.0f, 0.0f, 0.15f},
    },
    {   /* BOX: a CR-78 and a TR-606 */
        {T_KICK, 58.0f, 0.35f, 1.2f, 0.004f, 0.0f},
        {T_SNARE, 235.0f, 0.16f, 410.0f, 0.0f, 0.0f},
        {T_TOM, 120.0f, 0.3f, 1.1f, 0.03f, -0.35f},
        {T_TOM, 175.0f, 0.26f, 1.1f, 0.03f, 0.2f},
        {T_TOM, 330.0f, 0.18f, 1.05f, 0.01f, 0.4f},         /* the conga */
        {T_CLAVES, 2500.0f, 0.06f, 0.0f, 0.0f, -0.3f},
        {T_GUIRO, 2600.0f, 0.2f, 0.0f, 0.0f, 0.3f},
        {T_HAT, 6800.0f, 0.04f, 0.35f, 0.0f, 0.2f},
        {T_HAT, 6800.0f, 0.3f, 0.35f, 0.0f, 0.2f},
        {T_TAMB, 7500.0f, 0.3f, 0.0f, 0.0f, -0.2f},
        {T_COWBELL, 720.0f, 0.22f, 1080.0f, 0.0f, -0.1f},
    },
};
static const char *const PART_SHORT[EN_NKIT][EN_NPART] = {
    {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CY", "CB"},
    {"BD", "SD", "LT", "MT", "HT", "RS", "CP", "CH", "OH", "CR", "RD"},
    {"KIK", "SNR", "TM1", "TM2", "TM3", "BLP", "ZAP", "HAT", "OPN", "CRS", "COIN"},
    {"BD", "SD", "LT", "HT", "CGA", "CLV", "GUI", "CH", "OH", "TAM", "CB"},
};
static const char *const PART_LONG[EN_NKIT][EN_NPART] = {
    {"Bass drum", "Snare", "Low tom", "Mid tom", "High tom", "Rim shot", "Clap", "Closed hat", "Open hat",
     "Cymbal", "Cowbell"},
    {"Bass drum", "Snare", "Low tom", "Mid tom", "High tom", "Rim shot", "Clap", "Closed hat", "Open hat",
     "Crash", "Ride"},
    {"Kick", "Snare", "Low tom", "Mid tom", "High tom", "Blip", "Zap", "Hat", "Open hat", "Crash", "Coin"},
    {"Bass drum", "Snare", "Low tom", "High tom", "Conga", "Claves", "Guiro", "Closed hat", "Open hat",
     "Tambourine", "Cowbell"},
};
const char *en_part_name(int kit, int part) { return PART_SHORT[(unsigned)kit % EN_NKIT][(unsigned)part % EN_NPART]; }
const char *en_part_long(int kit, int part) { return PART_LONG[(unsigned)kit % EN_NKIT][(unsigned)part % EN_NPART]; }

const char *const EN_DRUM_KNOB[4] = {"Tune", "Decay", "Tone", "Level"};
void en_drum_text(int k, int v, char *b)          /* tune in semitones, the rest 0..100 */
{
    int n = 0;
    char t[4];
    if (k == 0) {
        v = (v - 50) * 12 / 50;
        if (v > 0)
            *b++ = '+';
        else if (v < 0) {
            *b++ = '-';
            v = -v;
        }
    }
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 3);
    while (n)
        *b++ = t[--n];
    *b = 0;
}
/* each part's knobs as the kit is born: tune 50 (as built), decay 50 (as built), tone, level */
#define DD(t, l) {50, 50, t, l}
const uint8_t EN_DRUM_DEF[EN_NKIT][EN_NPART][4] = {
    {DD(40, 90), DD(50, 72), DD(30, 70), DD(30, 70), DD(30, 70), DD(50, 60), DD(50, 70), DD(50, 56), DD(50, 52),
     DD(50, 44), DD(50, 52)},
    {DD(55, 90), DD(55, 72), DD(30, 70), DD(30, 70), DD(30, 70), DD(50, 60), DD(50, 70), DD(50, 56), DD(50, 52),
     DD(50, 44), DD(50, 44)},
    {DD(60, 82), DD(50, 66), DD(50, 64), DD(50, 64), DD(50, 64), DD(50, 52), DD(50, 52), DD(50, 50), DD(50, 46),
     DD(50, 42), DD(50, 48)},
    {DD(40, 86), DD(45, 66), DD(30, 66), DD(30, 66), DD(50, 64), DD(50, 58), DD(50, 54), DD(50, 52), DD(50, 48),
     DD(50, 46), DD(50, 50)},
};

/* the kits made as loud as each other (a plain beat at the same level) */
static const float KIT_GAIN[EN_NKIT] = {0.83f, 1.21f, 1.97f, 1.46f};

/* --------------------------------------------------------------- a part's voice --- */
typedef struct {
    uint8_t on, type, kit, part;
    float g, gl, gr;                   /* gain; left, right */
    float amp, ad;                     /* the main envelope and its decay a sample */
    float amp2, ad2;                   /* a second (the snare's second head, the cowbell's tail) */
    float ph, inc, inc_end, pd;        /* the oscillator; its pitch sweeps from inc to inc_end, pd a sample */
    float ph2, inc2;
    float nmp, nd;                     /* the noise's envelope */
    float s1[2], s2[2];                /* filter states */
    float c1[4], c2[4];
    float lp, hp, tone;
    int32_t t;                         /* samples since the hit */
    int32_t gap;                       /* the clap's bursts, the coin's second note, the blip's */
    int bursts;
    uint32_t lfsr;                     /* the console's noise register */
    float nph, ninc, nout;             /* ... clocked at ninc */
    float choke;                       /* 1, falling to 0 when choked */
} dv_t;
static dv_t dv[EN_NPART];

/* the 808's metal: six square oscillators (205.3, 304.4, 369.6, 522.7, 540, 800 Hz), run once a sample
 * for every part that uses them; the 909's and the box's are tuned elsewhere */
static const float METAL_F[EN_NKIT][6] = {
    {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f},
    {263.0f, 400.0f, 421.0f, 474.0f, 587.0f, 845.0f},
    {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f},
    {317.0f, 441.0f, 513.0f, 610.0f, 763.0f, 1056.0f},
};
static float metal_ph[6];
static float metal_buf[256];

static inline float sq(float ph) { return ph < 0.5f ? 1.0f : -1.0f; }
static inline float nes_step(float a) { return (float)(int)(a * 15.0f + 0.5f) * (1.0f / 15.0f); }   /* 4-bit volume */
static inline float tri32(float ph)            /* the console's triangle: 32 steps */
{
    int k = (int)(ph * 32.0f) & 31;
    return (float)(k < 16 ? k : 31 - k) * (2.0f / 15.0f) - 1.0f;
}
static inline float lfsr_step(dv_t *v, int shortm)
{
    uint32_t b = (v->lfsr ^ (v->lfsr >> (shortm ? 6 : 1))) & 1u;
    v->lfsr = (v->lfsr >> 1) | (b << 14);
    return (v->lfsr & 1u) ? 1.0f : -1.0f;
}

static float dk_tune(int v) { return fm_exp2f((float)(v - 50) * (1.0f / 50.0f)); }        /* +- an octave */
static float dk_decay(int v) { return fm_exp2f((float)(v - 50) * (1.0f / 25.0f)); }       /* x0.25 .. x4 */

/* a hit: part p of the kit at velocity vel (0..1), its tune and decay moved by lock offsets (-50..50) */
static void drum_fire(int kit, int p, float vel, int tune_off, int dec_off)
{
    const dpart_t *d = &KIT[kit][p];
    const uint8_t *k = en_cfg.dk[kit][p];
    dv_t *v = &dv[p];
    float tu = dk_tune(clampi(k[0] + tune_off, 0, 100)), dm = dk_decay(clampi(k[1] + dec_off, 0, 100));
    float tone = (float)k[2] * 0.01f, lvl = (float)k[3] * 0.01f, f = d->f * tu, dec = d->dec * dm;
    float pan = d->pan;
    v->on = 1;
    v->type = d->type;
    v->kit = (uint8_t)kit;
    v->part = (uint8_t)p;
    v->g = lvl * lvl * vel * KIT_GAIN[kit];
    v->gl = pan > 0.0f ? 1.0f - pan * 0.7f : 1.0f;
    v->gr = pan < 0.0f ? 1.0f + pan * 0.7f : 1.0f;
    v->t = 0;
    v->tone = tone;
    v->choke = 1.0f;
    v->amp = 1.0f;
    v->ad = t60(dec);
    v->amp2 = 0.0f;
    v->nmp = 0.0f;
    v->lp = v->hp = 0.0f;
    v->s1[0] = v->s1[1] = v->s2[0] = v->s2[1] = 0.0f;
    switch (d->type) {
    case T_KICK:                                   /* the sine, swept a little (808) or a lot (909); the click */
        v->inc_end = f * (1.0f / EN_SR);
        v->inc = v->inc_end * d->a;
        v->pd = fm_expf(-1.0f / (d->b * EN_SR));
        v->ph = 0.0f;
        v->nmp = 0.25f + tone * 1.1f;
        v->nd = t60(0.006f);
        break;
    case T_SNARE:                                  /* two heads and the snares: tone is how snappy */
        v->inc_end = f * (1.0f / EN_SR);
        v->inc = v->inc_end * (d->b > 0.0f ? d->b : 1.0f);
        v->pd = fm_expf(-1.0f / (0.012f * EN_SR));
        v->inc2 = d->a * tu * (1.0f / EN_SR);
        v->ph = v->ph2 = 0.0f;
        v->ad = t60(0.16f * dm);
        v->amp2 = 0.7f;
        v->ad2 = t60(0.09f * dm);
        v->nmp = 0.35f + tone * 1.0f;
        v->nd = t60(dec);
        v->c1[0] = onepole(kit == KIT_909 ? 900.0f : 1600.0f);       /* the noise's band: a high-pass ... */
        v->c2[0] = onepole(9000.0f);                                 /* ... after a low-pass */
        break;
    case T_TOM:
        v->inc_end = f * (1.0f / EN_SR);
        v->inc = v->inc_end * d->a;
        v->pd = fm_expf(-1.0f / (d->b * EN_SR));
        v->ph = 0.0f;
        v->nmp = 0.08f + tone * 0.3f;
        v->nd = t60(0.03f);
        svf_coefs(v->c1, f * 4.0f, 1.0f);
        break;
    case T_RIM:                                    /* two short rings and a click */
        v->inc = f * (1.0f / EN_SR);
        v->inc2 = d->a * tu * (1.0f / EN_SR);
        v->ph = v->ph2 = 0.0f;
        v->ad2 = t60(dec * 0.7f);
        v->amp2 = 0.8f;
        v->nmp = 0.6f + tone * 0.6f;
        v->nd = t60(0.004f);
        svf_coefs(v->c1, 3000.0f, 1.0f);
        break;
    case T_CLAP:                                   /* noise through a band-pass, in bursts, then the room */
        v->bursts = (int)d->a;
        v->gap = (int32_t)(0.0105f * EN_SR);
        v->nd = t60(0.012f);
        v->ad = t60(dec);
        svf_coefs(v->c1, f * (0.8f + 0.5f * tone), 0.9f);
        break;
    case T_HAT:
    case T_CYM:
    case T_RIDE:                                   /* the metal (and noise), band-passed, high-passed */
        svf_coefs(v->c1, f * (0.75f + 0.5f * tone), d->type == T_CYM ? 1.1f : 0.85f);
        v->c2[0] = onepole(d->type == T_RIDE ? 2600.0f : 5200.0f * (0.8f + 0.4f * tone));
        v->nmp = d->a;
        v->ph = 0.0f;
        v->inc = 3150.0f * tu * (1.0f / EN_SR);     /* the ride's bell */
        v->amp2 = d->type == T_RIDE ? 0.5f + tone * 0.5f : 0.0f;
        v->ad2 = t60(0.6f * dm);
        if (d->type == T_HAT && p == 7 && dv[8].on)
            dv[8].choke = 0.999f;                   /* the closed hat chokes the open one */
        break;
    case T_COWBELL:                                /* two squares, a band-pass; a quick fall, then a tail */
        v->inc = f * (1.0f / EN_SR);
        v->inc2 = d->a * tu * (1.0f / EN_SR);
        v->ph = v->ph2 = 0.0f;
        v->amp2 = 1.0f;
        v->ad2 = t60(0.035f);
        svf_coefs(v->c1, (f + d->a * tu) * 1.1f * (0.8f + 0.4f * tone), 0.7f);
        break;
    case T_CLAVES:
        v->inc = f * (1.0f / EN_SR);
        v->ph = 0.0f;
        break;
    case T_GUIRO:                                  /* noise, band-passed, scraped by the teeth */
        svf_coefs(v->c1, f * (0.7f + 0.6f * tone), 0.6f);
        v->ph = 0.0f;
        v->inc = 42.0f * (1.0f / EN_SR);
        break;
    case T_TAMB:
        svf_coefs(v->c1, f, 0.7f);
        v->ph = 0.0f;
        v->inc = 16.0f * (1.0f / EN_SR);           /* the shake */
        break;
    case T_CKICK:
        v->inc_end = f * (1.0f / EN_SR);
        v->inc = v->inc_end * d->a;
        v->pd = fm_expf(-1.0f / (d->b * EN_SR));
        v->ph = 0.0f;
        break;
    case T_CSNARE:
        v->inc = f * (1.0f / EN_SR) * 1.6f;
        v->inc_end = f * (1.0f / EN_SR) * 0.7f;
        v->pd = fm_expf(-1.0f / (0.03f * EN_SR));
        v->lfsr = 0x4A5Bu;
        v->nph = 0.0f;
        v->ninc = (4000.0f + 9000.0f * tone) * (1.0f / EN_SR);
        break;
    case T_CTOM:
        v->inc = f * 1.6f * (1.0f / EN_SR);
        v->inc_end = f * 0.55f * (1.0f / EN_SR);
        v->pd = fm_expf(-1.0f / (0.06f * EN_SR));
        v->ph = 0.0f;
        break;
    case T_CHAT:
    case T_CCRASH:
        v->lfsr = 0x1u + (uint32_t)p;
        v->nph = 0.0f;
        v->ninc = (d->type == T_CCRASH ? 2500.0f + 6000.0f * tone : 9000.0f + 15000.0f * tone) * tu * (1.0f / EN_SR);
        v->ninc = fm_minf(v->ninc, 1.0f);
        if (d->type == T_CHAT && p == 7 && dv[8].on)
            dv[8].choke = 0.999f;
        break;
    case T_BLIP:
        v->inc = f * (1.0f / EN_SR);
        v->gap = (int32_t)(0.04f * EN_SR);
        v->ph = 0.0f;
        break;
    case T_ZAP:
        v->inc = f * (1.0f / EN_SR);
        v->inc_end = 70.0f * (1.0f / EN_SR);
        v->pd = fm_expf(-1.0f / ((0.03f + 0.09f * tone) * EN_SR));
        v->ph = 0.0f;
        break;
    case T_COIN:
        v->inc = f * (1.0f / EN_SR);
        v->inc2 = f * 1.3348f * (1.0f / EN_SR);   /* a fourth up */
        v->gap = (int32_t)(0.07f * EN_SR);
        v->ph = 0.0f;
        break;
    default: break;
    }
}

/* the metal for this chunk, if a part wants it */
static void metal_run(int kit, uint32_t n)
{
    float inc[6];
    uint32_t j;
    int i;
    for (i = 0; i < 6; i++)
        inc[i] = METAL_F[kit][i] * (1.0f / EN_SR);
    for (j = 0; j < n; j++) {
        float s = 0.0f;
        for (i = 0; i < 6; i++) {
            float p = metal_ph[i] + inc[i];
            if (p >= 1.0f)
                p -= 1.0f;
            metal_ph[i] = p;
            s += p < 0.5f ? 1.0f : -1.0f;
        }
        metal_buf[j] = s * (1.0f / 6.0f);
    }
}

static void dv_run(dv_t *v, uint32_t n, float *outl, float *outr)
{
    uint32_t j;
    float a = v->amp, g = v->g, gl = v->gl * g, gr = v->gr * g, bp, y;
    switch (v->type) {
    case T_KICK: {
        float inc = v->inc, ie = v->inc_end, pd = v->pd, ph = v->ph, nm = v->nmp, drive = v->kit == KIT_909 ? 1.8f : 1.1f;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a;
            if (nm > 0.001f) {                     /* the click: a pulse and a breath of noise */
                y += nm * (0.6f + 0.4f * white());
                nm *= v->nd;
            }
            y = sat(y * drive) * (1.0f / drive) * 1.25f;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case T_SNARE: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, ph2 = v->ph2, a2 = v->amp2, nm = v->nmp, lp = v->lp, hl = v->hp;
        float ch = v->c1[0], cl = v->c2[0];
        for (j = 0; j < n; j++) {
            float nzv = white(), hp;
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            if (v->kit == KIT_909)                  /* the 909's heads are triangles */
                y = (fm_fabsf(ph * 4.0f - 2.0f) - 1.0f) * a + (fm_fabsf(ph2 * 4.0f - 2.0f) - 1.0f) * a2;
            else
                y = sine(ph) * a + sine(ph2) * a2;
            lp += cl * (nzv - lp);                  /* the snares: low-passed, then the lows out */
            hl += ch * (lp - hl);
            hp = (lp - hl) * 1.4f;
            y = y * 0.8f + hp * nm;
            a *= v->ad;
            a2 *= v->ad2;
            nm *= v->nd;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        v->nmp = nm;
        v->lp = lp;
        v->hp = hl;
        break;
    }
    case T_TOM: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, nm = v->nmp;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a;
            if (nm > 0.001f) {
                y += svf(v->s1, v->c1, white(), &bp) * nm;
                nm *= v->nd;
            }
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->nmp = nm;
        break;
    }
    case T_RIM: {
        float ph = v->ph, ph2 = v->ph2, a2 = v->amp2, nm = v->nmp;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            y = sine(ph) * a + sine(ph2) * a2 * 0.6f;
            if (nm > 0.001f) {
                svf(v->s1, v->c1, white(), &bp);
                y += bp * nm;
                nm *= v->nd;
            }
            y = sat(y * 1.5f) * 0.8f;
            a *= v->ad;
            a2 *= v->ad2;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        v->nmp = nm;
        break;
    }
    case T_CLAP: {
        int32_t t = v->t;
        float e = v->nmp, tail = v->amp;
        for (j = 0; j < n; j++, t++) {
            int burst = (int)(t / v->gap);
            if (burst < v->bursts) {
                if (t % v->gap == 0)
                    e = 1.0f;
                e *= v->nd;
                y = e;
            } else {
                if (burst == v->bursts && t % v->gap == 0)
                    e = 0.0f;
                y = tail * 0.75f;
                tail *= v->ad;
            }
            svf(v->s1, v->c1, white(), &bp);
            y *= bp * 2.2f;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->nmp = e;
        a = tail;
        break;
    }
    case T_HAT:
    case T_CYM:
    case T_RIDE: {
        float nmix = v->nmp, ph = v->ph, a2 = v->amp2, ch = v->choke, hl = v->hp, k2 = v->c2[0];
        for (j = 0; j < n; j++) {
            float x = metal_buf[j] * (1.0f - nmix) + white() * nmix;
            svf(v->s1, v->c1, x, &bp);
            hl += k2 * (bp - hl);                   /* the band, high-passed */
            y = (bp - hl) * a * 1.9f;
            if (a2 > 0.001f) {                      /* the ride's bell */
                ph += v->inc;
                if (ph >= 1.0f)
                    ph -= 1.0f;
                y += (sine(ph) + 0.6f * sine(ph * 2.0f + 0.13f)) * a2 * 0.25f;
                a2 *= v->ad2;
            }
            a *= v->ad;
            if (ch < 1.0f) {
                a *= 0.995f;
                a2 *= 0.995f;
            }
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        v->amp2 = a2;
        v->hp = hl;
        break;
    }
    case T_COWBELL: {
        float ph = v->ph, ph2 = v->ph2, a2 = v->amp2;
        for (j = 0; j < n; j++) {
            float x;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            ph2 += v->inc2;
            if (ph2 >= 1.0f)
                ph2 -= 1.0f;
            x = (sq(ph) + sq(ph2)) * 0.5f;
            svf(v->s1, v->c1, x, &bp);
            y = bp * (0.35f * a + 0.65f * a2) * 1.5f;
            a *= v->ad;
            a2 *= v->ad2;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        v->ph2 = ph2;
        v->amp2 = a2;
        break;
    }
    case T_CLAVES: {
        float ph = v->ph;
        for (j = 0; j < n; j++) {
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = sine(ph) * a;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        break;
    }
    case T_GUIRO: {
        float ph = v->ph;
        for (j = 0; j < n; j++) {
            float tooth;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            tooth = ph < 0.35f ? 1.0f - ph * 2.5f : 0.1f;
            svf(v->s1, v->c1, white(), &bp);
            y = bp * tooth * a * 1.8f;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        break;
    }
    case T_TAMB: {
        float ph = v->ph;
        for (j = 0; j < n; j++) {
            float x = metal_buf[j] * 0.5f + white() * 0.5f, hp, shake;
            ph += v->inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            shake = 0.6f + 0.4f * sine(ph);
            hp = svf_hp(v->s1, v->c1, x, &bp);
            y = (hp * 0.6f + bp) * a * shake;
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        break;
    }
    case T_CKICK:
    case T_CTOM: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, tone = v->tone;
        int tri = v->type == T_CTOM || tone < 0.5f;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = (tri ? tri32(ph) : (ph < 0.5f ? 0.8f : -0.8f)) * nes_step(a);
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    case T_CSNARE: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph, out = v->nout, nph = v->nph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            nph += v->ninc;
            while (nph >= 1.0f) {
                nph -= 1.0f;
                out = lfsr_step(v, 0);
            }
            y = (out * 0.75f + (ph < 0.5f ? 0.35f : -0.35f) * (a > 0.3f ? 1.0f : 0.0f)) * nes_step(a);
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        v->nout = out;
        v->nph = nph;
        break;
    }
    case T_CHAT:
    case T_CCRASH: {
        float out = v->nout, nph = v->nph, ch = v->choke;
        int shortm = v->type == T_CHAT && KIT[v->kit][v->part].a > 0.5f;
        for (j = 0; j < n; j++) {
            nph += v->ninc;
            while (nph >= 1.0f) {
                nph -= 1.0f;
                out = lfsr_step(v, shortm);
            }
            y = out * nes_step(a) * 0.6f;
            v->hp += 0.3f * (y - v->hp);           /* the hiss, high-passed a little */
            y -= v->hp;
            a *= v->ad;
            if (ch < 1.0f)
                a *= 0.995f;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->nout = out;
        v->nph = nph;
        break;
    }
    case T_BLIP:
    case T_COIN: {
        float ph = v->ph;
        int32_t t = v->t;
        for (j = 0; j < n; j++, t++) {
            float inc = t < v->gap ? v->inc : (v->type == T_COIN ? v->inc2 : v->inc * 2.0f);
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = (ph < 0.5f ? 0.5f : -0.5f) * nes_step(a);
            if (v->type == T_COIN && t < v->gap)
                a = 1.0f;
            else
                a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->ph = ph;
        break;
    }
    case T_ZAP: {
        float inc = v->inc, ie = v->inc_end, ph = v->ph;
        for (j = 0; j < n; j++) {
            inc = ie + (inc - ie) * v->pd;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            y = (ph < 0.25f ? 0.6f : -0.2f) * nes_step(a);
            a *= v->ad;
            outl[j] += y * gl;
            outr[j] += y * gr;
        }
        v->inc = inc;
        v->ph = ph;
        break;
    }
    default: a = 0.0f; break;
    }
    v->amp = a;
    v->t += (int32_t)n;
    if (a < 0.001f && v->amp2 < 0.001f && ((v->type != T_SNARE && v->type != T_RIM) || v->nmp < 0.001f) &&
        (v->type != T_CLAP || v->t > v->gap * (v->bursts + 1)))
        v->on = 0;
}

static int drums_run(uint32_t n, float *outl, float *outr)   /* 0 if none sounded (outl, outr untouched) */
{
    int p, kit = en_cfg.tp[0][TP_SOUND] % EN_NKIT, metal = 0, any = 0;
    uint32_t j;
    for (p = 0; p < EN_NPART; p++) {
        any |= dv[p].on;
        if (dv[p].on && (dv[p].type == T_HAT || dv[p].type == T_CYM || dv[p].type == T_RIDE || dv[p].type == T_TAMB))
            metal = 1;
    }
    if (!any)
        return 0;
    for (j = 0; j < n; j++)
        outl[j] = outr[j] = 0.0f;
    if (metal)
        metal_run(kit, n);
    for (p = 0; p < EN_NPART; p++)
        if (dv[p].on)
            dv_run(&dv[p], n, outl, outr);
    return 1;
}

static void drums_quiet(void)
{
    int p;
    for (p = 0; p < EN_NPART; p++)
        dv[p].on = 0;
}
