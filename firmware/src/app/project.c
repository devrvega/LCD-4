/* SPDX-License-Identifier: GPL-3.0-only */
/* LCD-4 project: the settings in one flash object (OBJ_PROJ), the sixteen patterns in four more
 * (OBJ_PAT0..3, four each), so a change to one pattern rewrites one small object. A project of another
 * format, or none (or IND-1's, which lives at the same place), gives the first demo song. Part of the unity
 * build, after app.h. */
project_t proj;
static bank_t bank_buf;
static uint32_t dirty;

void project_dirty(int what) { dirty |= (uint32_t)what; }
int project_is_dirty(void) { return dirty != 0; }

void project_defaults(int demo)
{
    int t;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    en_demo(demo);                       /* the settings and all sixteen patterns */
    proj.track = 1;
    proj.part = 0;
    for (t = 0; t < EN_NSYN; t++)
        proj.oct[t] = 0;
    proj.oct[0] = -1;                    /* the bass track starts an octave down */
    proj.leds = 1;
}

int proj_valid(const project_t *p)
{
    int t;
    if (p->magic != PROJ_MAGIC || p->format != PROJ_FORMAT || !en_cfg_valid(&p->cfg) || p->track >= EN_NTRACK ||
        p->part >= EN_NPART || p->leds > 2u)
        return 0;
    for (t = 0; t < EN_NSYN; t++)
        if (p->oct[t] < -3 || p->oct[t] > 3)
            return 0;
    return 1;
}

int project_load(void)
{
    int n, b, i;
    memset(&proj, 0, sizeof proj);
    n = plat_store_load(OBJ_PROJ, &proj, sizeof proj);
    if (n != (int)sizeof proj || !proj_valid(&proj)) {
        project_defaults(0);
        dirty = PD_ALL;                  /* the first save writes it all: the patterns go with the settings */
        return -1;
    }
    dirty = 0;
    en_cfg = proj.cfg;
    for (b = 0; b < NBANK; b++) {        /* a bank that is missing or broken: empty patterns */
        n = plat_store_load(OBJ_PAT0 + (uint32_t)b, &bank_buf, sizeof bank_buf);
        if (n == (int)sizeof bank_buf && bank_buf.magic == BANK_MAGIC && bank_buf.format == PROJ_FORMAT &&
            bank_buf.bank == (uint32_t)b) {
            for (i = 0; i < BANK_PATS; i++)
                if (en_pat_valid(&bank_buf.pat[i])) {
                    en_pat[b * BANK_PATS + i] = bank_buf.pat[i];
                } else {
                    en_pat_clear(&en_pat[b * BANK_PATS + i]);
                    dirty |= PD_PAT(b * BANK_PATS);
                }
        } else {
            for (i = 0; i < BANK_PATS; i++)
                en_pat_clear(&en_pat[b * BANK_PATS + i]);
            dirty |= PD_PAT(b * BANK_PATS);   /* written as it is now, empty, at the next save */
        }
    }
    return 0;
}

int project_save(void)
{
    int b, rc = 0;
    if (!dirty)
        return 0;
    if (dirty & ~PD_CFG) {
        for (b = 0; b < NBANK; b++)
            if (dirty & PD_PAT(b * BANK_PATS)) {
                bank_buf.magic = BANK_MAGIC;
                bank_buf.format = PROJ_FORMAT;
                bank_buf.bank = (uint32_t)b;
                memcpy(bank_buf.pat, &en_pat[b * BANK_PATS], sizeof bank_buf.pat);
                if (plat_store_save(OBJ_PAT0 + (uint32_t)b, &bank_buf, sizeof bank_buf))
                    rc = -1;
                else
                    dirty &= ~(uint32_t)PD_PAT(b * BANK_PATS);
            }
    }
    proj.cfg = en_cfg;                   /* the settings last: a project only ever names patterns that were saved */
    if (plat_store_save(OBJ_PROJ, &proj, sizeof proj))
        rc = -1;
    else
        dirty &= ~PD_CFG;
    return rc;
}
