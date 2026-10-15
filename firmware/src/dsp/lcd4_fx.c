/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4's effects: the plate and the delay on sends (AMB-1's and IND-1's), the punch-in effects on the whole
 * mix (a low-pass or high-pass sweep, a crusher), and a bus compressor (Punch). Included once, by lcd4.c. */

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni, as in AMB-1): a predelay, four input
 * diffusers, then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass,
 * an allpass and a delay, feeding the other half. Stereo out from seven taps a side. LCD-4 runs it at half
 * rate (22.05 kHz) with every line in one ring of 16384 (a power of two: a tap is an add and a mask): the
 * write point moves one place a step, each line keeps its own stretch of the ring ahead of it. The paper's
 * lengths are for 29761 Hz; DS() scales them to 22.05 kHz, and 6% smaller, to fit the ring. */
#define DS(n) (((n) * 6964 + 5000) / 10000)            /* 22050 / 29761 x 0.94 */
#define PRE_N 400                                    /* 19 ms */
#define EXC DS(16)
#define RING 16384u
#define RMASK (RING - 1u)
enum { RV_PRE, RV_IN1, RV_IN2, RV_IN3, RV_IN4, RV_APL, RV_D1L, RV_AP2L, RV_D2L, RV_APR, RV_D1R, RV_AP2R, RV_D2R, RV_N };
static const uint16_t RV_LEN[RV_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                      DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
static uint16_t rv_base[RV_N];                       /* each line's stretch of the ring */
static float ring[RING] EN_POOL;
static uint32_t rp;                                  /* the write point: one place back a step */
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.8f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

#define RD(line, k) ring[(rp + rv_base[line] + (uint32_t)(k)) & RMASK]
#define WR(line, x) (ring[(rp + rv_base[line]) & RMASK] = (x))
static inline float ap(int line, float x, float g)
{
    float z = RD(line, RV_LEN[line]), v = x - g * z;
    WR(line, fm_flush(v));
    return z + g * v;
}
static inline float ap_mod(int line, float d, float x, float g)
{
    uint32_t k = (uint32_t)d;
    float f = d - (float)k, z0 = RD(line, k), z = z0 + (RD(line, k + 1u) - z0) * f, v = x - g * z;
    WR(line, fm_flush(v));
    return z + g * v;
}

static inline void plate(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = RD(RV_D2R, DS(3163)), fb_r = RD(RV_D2L, DS(3720));
    WR(RV_PRE, in);
    x = RD(RV_PRE, PRE_N);
    pl_bw += 0.8f * (x - pl_bw);                      /* (the paper's at 44.1 kHz, set again for 22 kHz) */
    x = ap(RV_IN1, pl_bw, 0.75f);
    x = ap(RV_IN2, x, 0.75f);
    x = ap(RV_IN3, x, 0.625f);
    x = ap(RV_IN4, x, 0.625f);
    {
        float s = lfo_s + 0.0002f * lfo_c, c = lfo_c - 0.0002f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = ap_mod(RV_APL, (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    WR(RV_D1L, a);
    b = RD(RV_D1L, DS(4453));
    pl_damp_l += 0.64f * (b - pl_damp_l);
    b = ap(RV_AP2L, pl_damp_l * pl_decay, pl_dd2);
    WR(RV_D2L, b);
    a = ap_mod(RV_APR, (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    WR(RV_D1R, a);
    b = RD(RV_D1R, DS(4217));
    pl_damp_r += 0.64f * (b - pl_damp_r);
    b = ap(RV_AP2R, pl_damp_r * pl_decay, pl_dd2);
    WR(RV_D2R, b);
    *ol = 0.6f * (RD(RV_D1R, DS(266)) + RD(RV_D1R, DS(2974)) - RD(RV_AP2R, DS(1913)) + RD(RV_D2R, DS(1996)) -
                  RD(RV_D1L, DS(1990)) - RD(RV_AP2L, DS(187)) - RD(RV_D2L, DS(1066)));
    *or_ = 0.6f * (RD(RV_D1L, DS(353)) + RD(RV_D1L, DS(3627)) - RD(RV_AP2L, DS(1228)) + RD(RV_D2L, DS(2673)) -
                   RD(RV_D1R, DS(2111)) - RD(RV_AP2R, DS(335)) - RD(RV_D2R, DS(121)));
    rp = (rp - 1u) & RMASK;
}

static void plate_init(void)
{
    uint32_t i, b = 0;
    int k;
    for (k = 0; k < RV_N; k++) {
        rv_base[k] = (uint16_t)b;
        b += RV_LEN[k] + 2u;
    }
    for (i = 0; i < RING; i++)
        ring[i] = 0.0f;
    rp = 0;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------------ the ping-pong delay --- */
/* left then right then left, each repeat darker, its time in sixteenths (Time); at half rate (22 kHz,
 * 16-bit), which the repeats don't miss */
#define DLY_N 16560                             /* 0.75 s at 22.05 kHz: a dotted eighth at 60 */
#define DLY_SR (EN_SR * 0.5f)
static int16_t dly_l[DLY_N] EN_POOL, dly_r[DLY_N] EN_POOL;
static int dly_w;
static float dly_len = 8000.0f, dly_len_t = 8000.0f, dly_lpl, dly_lpr, dly_fb = 0.5f;
static const uint8_t DLY_SIXTEENTHS[5] = {1, 2, 3, 4, 6};

static inline float dread(const int16_t *b, float d)
{
    int k = (int)d, i0 = dly_w - k, i1;
    float f = d - (float)k, a, c;
    if (i0 < 0)
        i0 += DLY_N;
    i1 = i0 - 1;
    if (i1 < 0)
        i1 += DLY_N;
    a = (float)b[i0];
    c = (float)b[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void delay_time(void)
{
    float t = (float)DLY_SIXTEENTHS[clampi(en_cfg.par[P_DTIME], 0, 4)] * 15.0f / (float)clampi(en_cfg.par[P_TEMPO], 40, 300);
    dly_len_t = fm_minf(t * DLY_SR, (float)(DLY_N - 8));
    if (dly_len <= 0.0f)
        dly_len = dly_len_t;
}

/* the sends, a chunk at a time: at half rate, the delay's step and then the plate's (which hears the delay's
 * repeats too), their outputs slid between steps */
static int snd_ph;
static float snd_dacc, snd_racc, snd_prev[4], snd_cur[4];
static void sends_run(const float *ds, const float *rs, float *l, float *r, uint32_t n)
{
    uint32_t j;
    for (j = 0; j < n; j++) {
        float f;
        snd_dacc += ds[j];
        snd_racc += rs[j];
        if (++snd_ph >= 2) {
            float yl, yr, wl, wr;
            int k;
            snd_ph = 0;
            dly_len += (dly_len_t - dly_len) * 0.0005f;
            yl = dread(dly_l, dly_len);
            yr = dread(dly_r, dly_len);
            dly_lpl += 0.5f * (yl - dly_lpl);
            dly_lpr += 0.5f * (yr - dly_lpr);
            if (++dly_w >= DLY_N)
                dly_w = 0;
            dly_l[dly_w] = s16(snd_dacc * 0.5f + dly_fb * dly_lpr);   /* the left hears the send and the right's repeats */
            dly_r[dly_w] = s16(dly_fb * dly_lpl);
            plate((snd_racc * 0.5f + (yl + yr) * 0.2f) * 0.6f, &wl, &wr);
            for (k = 0; k < 4; k++)
                snd_prev[k] = snd_cur[k];
            snd_cur[0] = yl;
            snd_cur[1] = yr;
            snd_cur[2] = wl;
            snd_cur[3] = wr;
            snd_dacc = snd_racc = 0.0f;
        }
        f = (float)snd_ph * 0.5f;
        l[j] += 0.75f * (snd_prev[0] + (snd_cur[0] - snd_prev[0]) * f) + snd_prev[2] + (snd_cur[2] - snd_prev[2]) * f;
        r[j] += 0.75f * (snd_prev[1] + (snd_cur[1] - snd_prev[1]) * f) + snd_prev[3] + (snd_cur[3] - snd_prev[3]) * f;
    }
}

/* ------------------------------------------------------- the punch-in effects --- */
/* the sweeps: a state-variable filter a side, its cutoff gliding down (low-pass) or up (high-pass) over two
 * bars while the effect lasts, and back fast; idle (bypassed) when wide open */
static float fx_lp = 1.0f, fx_hp = 0.0f;               /* 0..1: how far each sweep has gone */
static float fxs_l[2], fxs_r[2], fxh_l[2], fxh_r[2];
static int crush_n;
static float crush_l, crush_r, crush_amt;

static void master_fx(float *l, float *r, uint32_t n, int fx, float bar_secs)
{
    float rate = (float)n / (2.0f * bar_secs * EN_SR);
    uint32_t j;
    if (fx == FX_LOWPASS)
        fx_lp = fm_maxf(fx_lp - rate, 0.0f);
    else
        fx_lp = fm_minf(fx_lp + (float)n / (0.08f * EN_SR), 1.0f);
    if (fx == FX_HIPASS)
        fx_hp = fm_minf(fx_hp + rate, 1.0f);
    else
        fx_hp = fm_maxf(fx_hp - (float)n / (0.08f * EN_SR), 0.0f);
    crush_amt += ((fx == FX_CRUSH ? 1.0f : 0.0f) - crush_amt) * fm_minf((float)n / (0.01f * EN_SR), 1.0f);
    if (fx_lp < 0.999f) {
        float c[4], bp;
        svf_coefs(c, 220.0f * fm_exp2f(6.4f * fx_lp * fx_lp), 0.6f);
        for (j = 0; j < n; j++) {
            l[j] = svf(fxs_l, c, l[j], &bp);
            r[j] = svf(fxs_r, c, r[j], &bp);
        }
    } else {
        fxs_l[0] = fxs_l[1] = fxs_r[0] = fxs_r[1] = 0.0f;
    }
    if (fx_hp > 0.001f) {
        float c[4], bp;
        svf_coefs(c, 30.0f * fm_exp2f(6.6f * fx_hp), 0.7f);
        for (j = 0; j < n; j++) {
            l[j] = svf_hp(fxh_l, c, l[j], &bp);
            r[j] = svf_hp(fxh_r, c, r[j], &bp);
        }
    } else {
        fxh_l[0] = fxh_l[1] = fxh_r[0] = fxh_r[1] = 0.0f;
    }
    if (crush_amt > 0.01f) {                            /* a quarter of the rate, 6 bits */
        float a = crush_amt;
        for (j = 0; j < n; j++) {
            if (++crush_n >= 4) {
                crush_n = 0;
                crush_l = (float)(int)(l[j] * 24.0f) * (1.0f / 24.0f);
                crush_r = (float)(int)(r[j] * 24.0f) * (1.0f / 24.0f);
            }
            l[j] += (crush_l - l[j]) * a;
            r[j] += (crush_r - r[j]) * a;
        }
    }
}

/* Punch: a bus compressor (fast attack, release by the beat), then the make-up; 0 leaves the mix alone */
static float comp_env, comp_g = 1.0f, comp_t = 1.0f;
static void punch(float *l, float *r, uint32_t n)
{
    float amt = (float)clampi(en_cfg.par[P_PUNCH], 0, 100) * 0.01f, thr, iratio, mk, att = 0.25f, rel = 0.9993f;
    uint32_t j;
    if (amt <= 0.0f) {
        comp_g = comp_t = 1.0f;
        return;
    }
    thr = 0.5f - 0.35f * amt;
    iratio = 1.0f / (1.0f + 5.0f * amt);
    mk = 1.0f + 1.1f * amt;
    for (j = 0; j < n; j++) {
        float x = fm_maxf(fm_fabsf(l[j]), fm_fabsf(r[j]));
        comp_env = x > comp_env ? comp_env + (x - comp_env) * att : comp_env * rel;
        if ((j & 15u) == 0u)                            /* the gain wanted, every 16 samples (a divide) */
            comp_t = comp_env > thr ? (thr + (comp_env - thr) * iratio) / comp_env : 1.0f;
        comp_g += (comp_t - comp_g) * 0.01f;
        l[j] *= comp_g * mk;
        r[j] *= comp_g * mk;
    }
}
