/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4's building blocks: oscillators, filters, envelopes. Included once, by lcd4.c (one -O2 unit).
 * The sine table, the noise, the clipper, the polyBLEP saw and the SVF are AMB-1's and IND-1's. */

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}
static inline float sine_s(float ph)            /* any sign */
{
    return sine(ph - fm_floorf(ph));
}

/* white noise (an LCG: the cheapest there is, and white enough for drums and hiss) */
static uint32_t nz = 22222u;
static inline float white(void)
{
    nz = nz * 1664525u + 1013904223u;
    return (float)(int32_t)nz * (1.0f / 2147483648.0f);
}

/* a soft clipper: the cubic one, flat from |x| = 1.5 (no divide: the FM-1's FPU divides slowly) */
static inline float sat(float x)
{
    if (x > 1.5f)
        return 1.0f;
    if (x < -1.5f)
        return -1.0f;
    return x - 0.148148148f * x * x * x;
}

/* polyBLEP: a saw (-1..1) at phase t (0..1), dt the phase step */
static inline float blep(float t, float dt)     /* the correction at a falling edge at t = 0 */
{
    if (t < dt) {
        float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}
static inline float blep_saw(float t, float dt) { return 2.0f * t - 1.0f - blep(t, dt); }
/* a pulse, high for the last w of the turn (w 0..1; 0.5 a square): two saws w apart; mean 0, 2 peak to peak */
static inline float blep_pulse(float t, float dt, float w)
{
    float t2 = t + w;
    if (t2 >= 1.0f)
        t2 -= 1.0f;
    return blep_saw(t, dt) - blep_saw(t2, dt);
}
static inline float wrap1(float t) { return t >= 1.0f ? t - (float)(int)t : t; }

/* TPT state-variable filter (Zavalishin): coefficients a1, a2, a3, k (k = 2 none .. 0 self-oscillating) */
static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 20.0f, 19000.0f) * (1.0f / EN_SR));
    k = fm_maxf(k, 0.05f);                      /* below 0 it would grow without end */
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}
/* one sample: the low-pass out, the band-pass in *bp (the high-pass is x - k bp - lp) */
static inline float svf(float *s, const float *c, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}
static inline float svf_hp(float *s, const float *c, float x, float *bp)   /* the high-pass out */
{
    float lp = svf(s, c, x, bp);
    return x - c[3] * *bp - lp;
}
/* the same with the band integrator clipped: a resonance that screams and stays bounded (the MS-20's) */
static inline float svf_hot(float *s, const float *c, float x, float *bp)
{
    float v3 = x - s[1], v1 = c[0] * s[0] + c[1] * v3, v2 = s[1] + c[1] * s[0] + c[2] * v3;
    v1 = sat(v1);
    s[0] = 2.0f * v1 - s[0];
    s[1] = 2.0f * v2 - s[1];
    *bp = v1;
    return v2;
}

/* a four-pole ladder, zero-delay feedback (Zavalishin's linear solution), the input clipped: the Moog's,
 * and (with less resonance and a drive) the 303's and the IR3109's. g = tan(pi fc / fs); k 0..4 (4 rings).
 * The pass band is made up partly for the resonance. */
typedef struct {
    float s[4];
} ladder_t;
typedef struct {
    float G, h, kk, comp;              /* g / (1 + g), 1 / (1 + g), k, the make-up */
    float idrive;                      /* 1 / the drive (ladder_drive) */
    float den;                         /* 1 / (1 + k G^4) */
} ladc_t;
static inline void ladder_coefs(ladc_t *c, float fc, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(fc, 20.0f, 18000.0f) * (1.0f / EN_SR)), G = g / (1.0f + g), G2 = G * G;
    c->G = G;
    c->h = 1.0f / (1.0f + g);
    c->kk = k;
    c->den = 1.0f / (1.0f + k * G2 * G2);
    c->comp = 1.0f + 0.55f * k;
    c->idrive = 1.0f;
}
static inline void ladder_drive(ladc_t *c, float drive) { c->idrive = 1.0f / drive; }
/* the same without the clip, for the keys: four voices at a time */
static inline float ladder_lin(ladder_t *f, const ladc_t *c, float x)
{
    float G = c->G, h = c->h, S, y, u, v, G2 = G * G;
    S = ((f->s[0] * G + f->s[1]) * G + f->s[2]) * G * h + f->s[3] * h;
    y = (G2 * G2 * x + S) * c->den;
    u = x - c->kk * y;
    v = (u - f->s[0]) * G;
    u = v + f->s[0];
    f->s[0] = u + v;
    v = (u - f->s[1]) * G;
    u = v + f->s[1];
    f->s[1] = u + v;
    v = (u - f->s[2]) * G;
    u = v + f->s[2];
    f->s[2] = u + v;
    v = (u - f->s[3]) * G;
    u = v + f->s[3];
    f->s[3] = u + v;
    return u * c->comp;
}
static inline float ladder(ladder_t *f, const ladc_t *c, float x, float drive)
{
    float G = c->G, h = c->h, S, y, u, v;
    S = ((f->s[0] * G + f->s[1]) * G + f->s[2]) * G * h + f->s[3] * h;   /* the states' share of the output */
    {
        float G2 = G * G;
        y = (G2 * G2 * x + S) * c->den;
    }
    u = sat((x - c->kk * y) * drive) * c->idrive;
    v = (u - f->s[0]) * G;
    u = v + f->s[0];
    f->s[0] = u + v;
    v = (u - f->s[1]) * G;
    u = v + f->s[1];
    f->s[1] = u + v;
    v = (u - f->s[2]) * G;
    u = v + f->s[2];
    f->s[2] = u + v;
    v = (u - f->s[3]) * G;
    u = v + f->s[3];
    f->s[3] = u + v;
    return u * c->comp;
}

static inline float onepole(float f) { return 1.0f - fm_expf(-FM_TWO_PI * f * (1.0f / EN_SR)); }
static inline float t60(float secs) { return fm_expf(-6.9f / (fm_maxf(secs, 0.0005f) * EN_SR)); }   /* -60 dB in secs */
static inline float mtof(float n);
static inline float exp01(int v, float lo, float hi)   /* a knob (0..100) on an exponential scale lo..hi */
{
    return lo * fm_exp2f(fm_log2f(hi / lo) * (float)v * 0.01f);
}
static inline float lin01(int v, float lo, float hi) { return lo + (hi - lo) * (float)v * 0.01f; }

/* the ladder over a block, in place: its states and coefficients held in registers for the loop (the FM-1's
 * floats share the sixteen general registers: a loop that keeps less alive runs much faster) */
static void ladder_run(ladder_t *f, const ladc_t *c, float *x, uint32_t m)
{
    float s0 = f->s[0], s1 = f->s[1], s2 = f->s[2], s3 = f->s[3], G = c->G, h = c->h;
    float G2 = G * G, g3h = G2 * G * h, g2h = G2 * h, g1h = G * h, g4 = G2 * G2, den = c->den, k = c->kk, comp = c->comp;
    uint32_t j;
    for (j = 0; j < m; j++) {
        float in = x[j], y = (g4 * in + g3h * s0 + g2h * s1 + g1h * s2 + h * s3) * den, u = in - k * y, v;
        v = (u - s0) * G;
        u = v + s0;
        s0 = u + v;
        v = (u - s1) * G;
        u = v + s1;
        s1 = u + v;
        v = (u - s2) * G;
        u = v + s2;
        s2 = u + v;
        v = (u - s3) * G;
        u = v + s3;
        s3 = u + v;
        x[j] = u * comp;
    }
    f->s[0] = fm_flush(s0);
    f->s[1] = fm_flush(s1);
    f->s[2] = fm_flush(s2);
    f->s[3] = fm_flush(s3);
}
/* ... and driven: the input clipped */
static void ladder_run_drive(ladder_t *f, const ladc_t *c, float *x, uint32_t m, float drive)
{
    float s0 = f->s[0], s1 = f->s[1], s2 = f->s[2], s3 = f->s[3], G = c->G, h = c->h, idr = c->idrive;
    float G2 = G * G, g3h = G2 * G * h, g2h = G2 * h, g1h = G * h, g4 = G2 * G2, den = c->den, k = c->kk, comp = c->comp;
    uint32_t j;
    for (j = 0; j < m; j++) {
        float in = x[j], y = (g4 * in + g3h * s0 + g2h * s1 + g1h * s2 + h * s3) * den, u, v;
        u = sat((in - k * y) * drive) * idr;
        v = (u - s0) * G;
        u = v + s0;
        s0 = u + v;
        v = (u - s1) * G;
        u = v + s1;
        s1 = u + v;
        v = (u - s2) * G;
        u = v + s2;
        s2 = u + v;
        v = (u - s3) * G;
        u = v + s3;
        s3 = u + v;
        x[j] = u * comp;
    }
    f->s[0] = fm_flush(s0);
    f->s[1] = fm_flush(s1);
    f->s[2] = fm_flush(s2);
    f->s[3] = fm_flush(s3);
}
/* a block of saw (polyBLEP) at a steady pitch; returns the phase */
static float saw_run(float ph, float inc, float *x, uint32_t m, float g)
{
    uint32_t j;
    for (j = 0; j < m; j++) {
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        x[j] = g * blep_saw(ph, inc);
    }
    return ph;
}
static float saw_add(float ph, float inc, float *x, uint32_t m, float g)
{
    uint32_t j;
    for (j = 0; j < m; j++) {
        ph += inc;
        if (ph >= 1.0f)
            ph -= 1.0f;
        x[j] += g * blep_saw(ph, inc);
    }
    return ph;
}
