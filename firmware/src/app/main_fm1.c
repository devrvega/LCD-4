/* SPDX-License-Identifier: GPL-3.0-only
 * Boot, audio ISR and main loop adapted from Felucca's main.c / audio.c, Copyright (C) 2026
 * Leo Kuroshita (@kurogedelic), Hugelton Instruments. */
/* LCD-4 on the FM-1 (from IND-1's, AMB-1's and X0X's): boot (WDT first, boot-loop guard, guards), LCD, input (TIMER5
 * 10 kHz), audio (ALNK0), USB; then the main loop: UI at ~60 frames/s, input polled in between. */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];

/* --------------------------------------------------------------- audio --- */
/* Felucca's 256-frame halves (5.8 ms): the keys are played live, they want the short latency */
#define HALF_FRAMES 256u                    /* I2S half buffer: 5.8 ms at 44.1 kHz */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define FS 44100u
#define REND_FRAMES 256u                    /* en_render's frames per call */
#if FELUCCA_UAC
_Static_assert(REND_FRAMES == UA_HALF, "usb.c sizes the USB audio ring band for this block");
#endif
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

#define DBG_MAGIC 0x44424731u                       /* "DBG1": read with `fm1t memr` */
struct en_dbg {
    uint32_t magic, halves, max_us, nested, in_audio, late, timer_irqs, ui_frames;
    uint32_t last_us, cpu_q8, boots, stage;
    uint32_t crash_seen;                            /* the crash count already reported at a boot */
} en_dbg __attribute__((section(".noinit")));
static uint8_t safe_mode;                           /* two failed boots: no audio, USB on (safe_main) */

static void render_block(int32_t *o)                /* ALNK0: one half, measured */
{
    uint32_t t0 = fm1_ticks(), us, budget = HALF_FRAMES * 1000000u / FS;
    en_dbg.in_audio = 1;
#if FELUCCA_UAC
    uac_render_start();
#endif
    {
        uint32_t k;
        for (k = 0; k < HALF_FRAMES; k += REND_FRAMES) {
            en_render(o + 2u * k, REND_FRAMES, master_q12);
#if FELUCCA_UAC
            uac_tap(o + 2u * k, REND_FRAMES);       /* the USB audio input: the same master output */
#endif
        }
    }
    us = (fm1_ticks() - t0) / FM1_TICKS_PER_US;
    en_dbg.cpu_q8 = (en_dbg.cpu_q8 * 15u + (us * 256u) / budget) / 16u;
    audio_cpu_pct = (en_dbg.cpu_q8 * 100u) >> 8;
    en_dbg.halves++;
    en_dbg.last_us = us;
    if (us > en_dbg.max_us)
        en_dbg.max_us = us;
    en_dbg.in_audio = 0;
}

void fm1_alnk0_irq(void)                            /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p = fm1_audio_pending();
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half();
        render_block(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        if (fm1_audio_free_half() != half) {
            en_dbg.late++;                         /* the DMA moved on while we rendered */
            audio_xruns++;
        }
    }
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}

static void audio_silence(void)                     /* IRQs off (flash erase): the DMA would loop stale audio */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}

/* LCD-4's SysEx (F0 7D 4C 34 cmd .. F7, 7D the non-commercial ID), all of it read-only: 01 asks for the
 * audio load (the reply is F0 7D 4C 34 01, then cpu %, the worst render in us, renders late, renders, the
 * build, the time each part of the engine took, the clock, playing / external / writing, steps, MIDI
 * messages in, each as five 7-bit groups, F7); 03 restarts the worst, the late and the parts' times; 04 s:
 * the screen not drawn for s seconds; 05 the store (each object's length in flash, its last save, what is
 * in it); 07 what plays (notes a track, the pattern, its hits, the tempo, the step, the levels). A
 * development build (./build.sh --bench, -DLCD4_BENCH) also takes 02 k (a heavy song to measure: 0 three
 * tracks of pads and every drum, 1 the first demo, 2 everything at once, 3 three mono leads), 06 p (play,
 * stop) and 08 d (demo song d, saved at once). */
#ifdef LCD4_BENCH
void lcd4_bench(int k);
#endif
static void dev_put(uint8_t *b, uint32_t v)
{
    uint32_t k;
    for (k = 0; k < 5u; k++)
        b[k] = (uint8_t)((v >> (7u * k)) & 0x7Fu);
}
static uint32_t dev_ticks(void) { return fm1_ticks(); }
static uint32_t ui_pause_until;                     /* SysEx 04: the screen not drawn, to see what it costs the audio */
static void dev_sysex(void)
{
    const uint8_t *p;
    uint32_t n;
    if (!ota_frame_get(&p, &n) || n < 4u || p[0] != 0x7Du || p[1] != 0x4Cu || p[2] != 0x34u)
        return;                                     /* not ours: left for the updater */
    switch (p[3]) {
    case 1: {
        static uint8_t r[6 + 25 + 5 * PR_N + 25 + 1];
        uint32_t k, c0, t0, mhz;
        r[0] = 0xF0;
        r[1] = 0x7D;
        r[2] = 0x4C;
        r[3] = 0x34;
        r[4] = 0x01;
        dev_put(r + 5, (en_dbg.cpu_q8 * 100u) >> 8);
        dev_put(r + 10, en_dbg.max_us);
        dev_put(r + 15, en_dbg.late);
        dev_put(r + 20, en_dbg.halves);
#ifdef LCD4_BENCH
        dev_put(r + 25, 1u);
#else
        dev_put(r + 25, 0u);
#endif
        for (k = 0; k < PR_N; k++)
            dev_put(r + 30 + 5 * k, en_prof[k]);
        c0 = fm1_perf_cycles();                     /* the core's clock: its cycles over 200 us */
        t0 = fm1_ticks();
        while ((uint32_t)(fm1_ticks() - t0) < 200u * FM1_TICKS_PER_US)
            ;
        mhz = (fm1_perf_cycles() - c0) / 200u;
        dev_put(r + 30 + 5 * PR_N, mhz);
        dev_put(r + 35 + 5 * PR_N, en_dbg.halves);
        dev_put(r + 40 + 5 * PR_N, (uint32_t)en_playing | (uint32_t)en_ext << 1 | (uint32_t)en_recording << 2);
        dev_put(r + 45 + 5 * PR_N, en_steps);
        dev_put(r + 50 + 5 * PR_N, midi_rx_count);
        r[55 + 5 * PR_N] = 0xF7;
        ota_frame_done();
        ota_wire_send(r, 56 + 5 * PR_N);
        return;
    }
    case 3: {
        uint32_t k;
        en_dbg.max_us = 0;
        en_dbg.late = 0;
        for (k = 0; k < PR_N; k++)
            en_prof[k] = 0;
        en_time = dev_ticks;
        break;
    }
    case 4:
        ui_pause_until = fm1_ms + 1000u * (n >= 5u ? p[4] : 5u);
        break;
#ifdef LCD4_BENCH
    case 6:                                         /* play (1) or stop (0), as PLAY */
        en_play(n >= 5u ? p[4] : 1);
        break;
#endif
    case 7: {                                       /* what plays: notes counted a track, the pattern, its hits */
        static uint8_t r[6 + 5 * 12 + 1];
        uint32_t k, sum = 0;
        r[0] = 0xF0;
        r[1] = 0x7D;
        r[2] = 0x4C;
        r[3] = 0x34;
        r[4] = 0x07;
        ota_frame_done();
        for (k = 0; k < EN_NTRACK; k++)
            dev_put(r + 5 + 5 * k, en_trk_on[k]);
        dev_put(r + 25, en_pat_now);
        dev_put(r + 30, en_cfg.pat | (uint32_t)en_cfg.chain_n << 8 | (uint32_t)en_cfg.mute << 16);
        for (k = 0; k < EN_NPART; k++)
            sum += en_pat[en_pat_now % EN_NPAT].hit[k];
        dev_put(r + 35, sum);
        dev_put(r + 40, en_pat[en_pat_now % EN_NPAT].syn[0][0].note[0] | (uint32_t)en_pat[en_pat_now % EN_NPAT].len << 8);
        dev_put(r + 45, (uint32_t)en_cfg.par[P_TEMPO] | (uint32_t)en_cfg.par[P_CLOCK] << 16);
        dev_put(r + 50, en_fx_now | (uint32_t)en_step << 8 | (uint32_t)en_tick << 16);
        dev_put(r + 55, (uint32_t)en_cfg.tp[0][TP_LEVEL] | (uint32_t)en_cfg.tp[1][TP_LEVEL] << 8 | (uint32_t)en_cfg.tp[0][TP_SOUND] << 16);
        r[5 + 5 * 12] = 0xF7;
        ota_wire_send(r, 6 + 5 * 12);
        return;
    }
    case 5: {                                       /* the store: each object's length in flash, its last save */
        static uint8_t r[6 + 15 * OBJ_NOBJ + 1];
        static uint8_t tmp[PLAT_STORE_MAX];
        uint32_t k;
        r[0] = 0xF0;
        r[1] = 0x7D;
        r[2] = 0x4C;
        r[3] = 0x34;
        r[4] = 0x05;
        ota_frame_done();
        for (k = 0; k < OBJ_NOBJ; k++) {
            int len = plat_store_load(k, tmp, sizeof tmp);
            uint32_t info = 0;
            if (k == OBJ_PROJ && len == (int)sizeof(project_t)) {
                const project_t *pp = (const project_t *)(const void *)tmp;
                info = (uint32_t)pp->cfg.par[P_TEMPO] | (uint32_t)pp->cfg.chain_n << 10 | (uint32_t)proj_valid(pp) << 20;
            } else if (k >= OBJ_PAT0 && len == (int)sizeof(bank_t)) {
                const bank_t *bb = (const bank_t *)(const void *)tmp;
                uint32_t j, q, hs = 0;
                for (q = 0; q < BANK_PATS; q++) {
                    info |= (uint32_t)en_pat_valid(&bb->pat[q]) << q;
                    for (j = 0; j < EN_NPART; j++)
                        hs += bb->pat[q].hit[j];
                }
                info |= (hs & 0xFFFFu) << 4 | (uint32_t)(bb->magic == BANK_MAGIC) << 20;
            }
            dev_put(r + 5 + 15 * k, (uint32_t)(len + 100));
            dev_put(r + 10 + 15 * k, (uint32_t)(store_rc[k] + 100));
            dev_put(r + 15 + 15 * k, info);
        }
        r[5 + 15 * OBJ_NOBJ] = 0xF7;
        ota_wire_send(r, 6 + 15 * OBJ_NOBJ);
        return;
    }
#ifdef LCD4_BENCH
    case 8:                                         /* a demo song, saved at once */
        en_play(0);
        project_defaults(n >= 5u ? p[4] % EN_NDEMO : 0);
        project_dirty(PD_ALL);
        ota_frame_done();
        project_save();
        return;
#endif
#ifdef LCD4_BENCH
    case 2:
        if (n >= 5u)
            lcd4_bench(p[4]);
        break;
#endif
    default: break;
    }
    ota_frame_done();
}

/* ------------------------------------------------------------ timer --- */
static uint8_t usb_due;
void fm1_timer5_irq(void)
{
    static uint32_t sub;
    fm1_timer5_ack();
    en_dbg.timer_irqs++;
    if (en_dbg.in_audio)
        en_dbg.nested++;
    fm1_input_tick();
    /* 2 kHz: all USB SIE traffic lives here. This ISR now nests into the audio render (so the key
     * matrix keeps its rhythm, below); USB shares the MIDI queues with the audio ISR, so a poll that
     * falls inside the render waits for the first tick outside it, as it did before the nesting. */
    if (sub % 5u == 0u) {
        usb_due = 1;
#if FELUCCA_UAC
        /* the USB audio stream cannot wait for the render (one packet per 1 ms frame, a render takes
         * up to ~11.6 ms): it runs nested too. It touches only EP4 (INDEX is set on every access) and
         * the consumer side of its ring, and usb_poll never runs nested, so the two never interleave */
        if (en_dbg.in_audio)
            uac_service();
#endif
    }
    if (usb_due && !en_dbg.in_audio) {
        usb_due = 0;
        usb_poll();
#if EN_TRS
        uart_midi_poll();                           /* the TRS jack: same context, same queue */
#endif
    }
    if (++sub == 10u)
        sub = 0;
    {
        static uint32_t last, acc;
        uint32_t now = fm1_ticks();
        acc += now - last;
        last = now;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
}
extern void isr_timer5(void);

/* ------------------------------------------------------------- boot --- */
#define BOOTGUARD_MAGIC 0x42475244u
struct bootguard_s bootguard __attribute__((section(".noinit")));   /* declared in main.c (ota_commit) */

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(160, 0, 0));
    draw_text_box(0, 8, 240, &FONT_S, "LCD-4 CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(10, 40, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->pc);
    draw_text_box(10, 60, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->emu);
    draw_text_box(10, 84, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->dbg);
    draw_text_box(10, 102, 220, &FONT_S, b, C_WHITE, 0);
    hexs(b, c->rets);
    draw_text_box(10, 120, 220, &FONT_S, b, C_WHITE, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 110, 240, &FONT_S, why, C_WHITE, 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                          /* intentional reset: not a failed boot */
    fm1_enter_uboot();
}

/* SAFE MODE: LCD-4 crashed (or hung) twice while starting. Nothing that makes sound runs: no audio,
 * no engine, no UI; USB is on, so the web installer (LCD-4, or the stock firmware) and M-UPGRADE can
 * reach it. PLAY tries LCD-4 again. The chip's own update mode stays the last resort (fm1_cstart). */
static void safe_main(void)
{
    char b[24];
    uint32_t t0, play;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 18, 240, &FONT_B, "LCD-4 SAFE MODE", C_HI, 1);
    draw_text_box(0, 52, 240, &FONT_S, "IT CRASHED TWICE", C_WHITE, 1);
    draw_text_box(0, 72, 240, &FONT_S, "WHILE STARTING.", C_WHITE, 1);
    draw_text_box(0, 102, 240, &FONT_S, "NO SOUND. USB IS ON:", C_WHITE, 1);
    draw_text_box(0, 122, 240, &FONT_S, "REINSTALL FROM THE", C_WHITE, 1);
    draw_text_box(0, 142, 240, &FONT_S, "WEB INSTALLER.", C_WHITE, 1);
    draw_text_box(0, 172, 240, &FONT_S, "PLAY: TRY AGAIN", C_HI, 1);
    if (fm1_crash.magic == FM1_CRASH_MAGIC) {
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 208, 240, &FONT_S, b, C_GRAY, 1);
    }
    fm1_input_init();
    panel_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 1);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    play = 1u << panel.btn[B_PLAY];
    t0 = fm1_ms;
    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (bootguard.pending && fm1_ms - t0 > 10000u)
            bootguard.pending = 0;                  /* safe mode itself is up: not another failed boot */
        if ((fm1_in.buttons & play) && fm1_ms - t0 > 500u) {
            bootguard.failed = 0;                   /* try LCD-4 again, from scratch */
            bootguard.pending = 0;
            draw_text_box(0, 172, 240, &FONT_S, "STARTING LCD-4...", C_HI, 1);
            usb_detach();
            fm1_delay_ms(30);
            fm1_reboot();
        }
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();                      /* an install commits and resets; else back here */
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
    }
}

static void splash(void)                            /* an LCD in a dark body: the name, its ghost segments */
{
    static const uint16_t TC[4] = {RGB(255, 122, 40), RGB(56, 196, 232), RGB(246, 204, 40), RGB(236, 84, 160)};
    int32_t i;
    lcd_fill(0, 0, 240, 240, RGB(22, 22, 25));
    cv_begin(240, 120, RGB(22, 22, 25));
    cv_rect(14, 8, 212, 104, RGB(92, 96, 84));
    cv_rect(16, 10, 208, 100, RGB(197, 205, 179));
    {
        int32_t w = text_w(&FONT_L, "LCD-4");
        cv_text((240 - w) / 2 + 2, 26, &FONT_L, "LCD-4", RGB(182, 190, 164));   /* its ghost, a little off */
        cv_text((240 - w) / 2, 24, &FONT_L, "LCD-4", RGB(30, 35, 29));
        w = text_w(&FONT_S, "A pocket groovebox");
        cv_text((240 - w) / 2, 66, &FONT_S, "A pocket groovebox", RGB(104, 112, 92));
        w = text_w(&FONT_XS, "v" EN_VERSION);
        cv_text((240 - w) / 2, 86, &FONT_XS, "v" EN_VERSION, RGB(104, 112, 92));
    }
    cv_blit(0, 40);
    cv_begin(240, 30, RGB(22, 22, 25));
    for (i = 0; i < 4; i++)
        cv_rect(28 + i * 48, 10, 40, 8, TC[i]);
    cv_blit(0, 172);
    lcd_sync();
}

static void fm1_main(void)
{
    int32_t knob = 512 * 16;
    uint32_t last_frame = 0;
    {
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;   /* the expected 1 MiB part, else RAM only */
        irq_restore(f);
        if (flash_ok)
            ota_boot_cleanup();
    }
    lcd_init();
    palette_set(1);
    splash();
    if (en_dbg.magic != DBG_MAGIC) {
        memset(&en_dbg, 0, sizeof en_dbg);
        en_dbg.magic = DBG_MAGIC;
    }
    if (safe_mode)
        safe_main();                                /* never returns */
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != en_dbg.crash_seen) {
        char b[16];                                 /* a crash since the last boot: say so, a moment */
        en_dbg.crash_seen = fm1_crash.count;
        b[0] = 'P';
        b[1] = 'C';
        b[2] = ' ';
        hexs(b + 3, fm1_crash.pc);
        draw_text_box(0, 176, 240, &FONT_S, "RESTARTED AFTER A CRASH", RGB(255, 80, 60), 1);
        draw_text_box(0, 196, 240, &FONT_S, b, C_GRAY, 1);
        fm1_wdt_feed();
        fm1_delay_ms(2500);
        fm1_wdt_feed();
    }
    en_dbg.boots++;
    en_dbg.max_us = 0;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    led_pos_init();
    project_load();                                 /* the first demo when nothing (or another format) is saved */
    en_init();
    en_time = dev_ticks;                            /* the engine times itself: its governor, the load over SysEx */
    audio_init();
    usb_start();
#if EN_TRS
    uart_midi_init();                               /* before TIMER5: it read-modify-writes port H too */
#endif
    /* above ALNK0 (3): the key / encoder / LED matrix is scanned one column per tick and must keep
     * its rhythm through a render (X0X's finding: below the audio, turns lost steps and LEDs flickered) */
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & 3u) == 3u)
        panel_setup();                              /* OCT- + OCT+ held at power-on */
    fm1_delay_ms(400);
    ui_init();

    for (;;) {
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                master_q12 = (k10 * k10) >> 8;           /* 0 .. ~4096 */
            }
        }
        {   /* OCT- + OCT+ held 5 s: update mode (Felucca's, unchanged) */
            static uint32_t t0, shown;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both) {
                if (shown)
                    ui_say("UPDATE MODE", "CANCELLED");
                shown = 0;
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != shown) {
                    char d[4] = {(char)('0' + left), '.', '.', 0};
                    ui_say("UPDATE MODE IN", d);
                    shown = left;
                }
            } else if (fm1_ms - t0 > 5000u) {
                enter_uboot("UBOOT");
            }
        }
        dev_sysex();                                /* LCD-4's own: the audio load, asked over USB */
        ota_service();                              /* M-UPGRADE handshake (other SysEx is dropped) */
        if (usb.ota_req) {
            usb.ota_req = 0;
            en_play(0);
            en_panic();
            if (flash_ok)
                ota_session();                      /* returns only if nothing was committed */
            ui_init();
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
#if EN_CDC
        cdc_task();
#endif
        if (fm1_ms - last_frame >= 16u && (int32_t)(fm1_ms - ui_pause_until) >= 0) {   /* ~60 frames/s at most */
            last_frame = fm1_ms;
            en_dbg.ui_frames++;
            ui_frame();
        } else {
            ui_input_only();
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 4u) {                   /* safe mode failed too: the chip's own update mode */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    p3 |= (bootguard.failed >= 2u) << 8;            /* two failed boots: safe mode (kept past the .bss clear) */
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    safe_mode = (uint8_t)(p3 >> 8);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
