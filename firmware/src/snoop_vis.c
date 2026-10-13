/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 M. Geisler (makkuro) */
/* SNOOP37: two visualiser styles (ui_vis.c calls mkv_draw for them), drawn with the boot splash's renderer
 * (snoop_splash.c) in its strips:
 *   SNOOP    the doghouse: a roof key presses down and lights in a track's colour when that track plays a
 *              note (the note's pitch class picks the key), the dog's ear and tail go with the drums, the
 *              sun breathes with the master, the mountains are the live spectrum; the floor runs at the tempo
 *   WATERFALL  the oscilloscope as a stack of its past traces going back to the horizon (each older one
 *              further away, dimmer), each hiding what is behind it; in the selected track's colour
 * No reflection here, to stay inside the visualiser's time. */

#define MKV_WN 18                  /* waterfall: traces kept */
#define MKV_WP 32                  /* points a trace */
static int8_t mkv_wf[MKV_WN][MKV_WP];
static uint8_t mkv_wh;             /* the newest trace */
static uint16_t mkv_kf[MK_NK];     /* a key's light, 0..256 */
static uint8_t mkv_kt[MK_NK];      /* the track that lit it */
static uint32_t mkv_wcol = MK_CYAN;

/* the waterfall's traces, oldest first, each over its own dark curtain down to the floor */
static void mkv_traces(void)
{
    int16_t *pt = (int16_t *)mk_fd;                    /* (the mesh's memory: no mesh here) */
    int32_t k, j;
    for (k = 0; k < MKV_WN; k++) {                     /* project: k = 0 the oldest */
        int32_t age = MKV_WN - 1 - k, z = Q(45) * age, a[2];
        const int8_t *w = mkv_wf[(mkv_wh + 1u + (uint32_t)k) % MKV_WN];
        for (j = 0; j < MKV_WP; j++) {
            mk_pw(Q(-150) + j * Q(300) / (MKV_WP - 1), Q(30) + w[j] * Q(45) / 127, z, a);
            pt[2 * (k * MKV_WP + j)] = (int16_t)a[0];
            pt[2 * (k * MKV_WP + j) + 1] = (int16_t)a[1];
        }
        mk_pw(0, 0, z, a);
        pt[2 * MKV_WN * MKV_WP + k] = (int16_t)a[1];
    }
    for (k = 0; k < MKV_WN; k++) {
        int32_t age = MKV_WN - 1 - k, base = pt[2 * MKV_WN * MKV_WP + k], lo = base, lvl = 256 - age * 12;
        const int16_t *p = pt + 2 * k * MKV_WP;
        for (j = 0; j < MKV_WP; j++)
            lo = p[2 * j + 1] < lo ? p[2 * j + 1] : lo;
        if ((lo >> 4) - 4 > mk_y1 || (base >> 4) + 4 < mk_y0)
            continue;
        mk_core = 10 - age / 3;
        mk_glow = 24 - age / 2;
        for (j = 0; j + 1 < MKV_WP; j++) {
            int32_t q[8];
            q[0] = p[2 * j]; q[1] = p[2 * j + 1]; q[2] = p[2 * j + 2]; q[3] = p[2 * j + 3];
            q[4] = p[2 * j + 2]; q[5] = base; q[6] = p[2 * j]; q[7] = base;
            mk_poly(q, 4, 0x03020A, 256);
        }
        for (j = 0; j + 1 < MKV_WP; j++)
            mk_line(p[2 * j], p[2 * j + 1], p[2 * j + 2], p[2 * j + 3], mkv_wcol, lvl, 0);
    }
}

/* one strip of a visualiser frame (mk_render hands it here while mk_vis is set) */
static void mkv_strip(uint16_t *buf, int32_t y0, uint32_t t)
{
    uint32_t i;
    mk_buf = buf;
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = 0;
    mk_y0 = y0;
    mk_y1 = y0 + MK_SH;
    mk_sky(t);
    mk_floor(t);
    if (mk_vis == 1) {
        mk_hills(t);
        if (mk_nv) {
            mk_draw(1);
            mk_dog_lines();
        }
        mk_zz(t, 0);
    } else
        mkv_traces();
    for (i = 0; i < 240u * MK_SH; i++)
        buf[i] = (uint16_t)(buf[i] >> 8 | buf[i] << 8);
}

/* the frame's state from the music: levels 0..1000 (tracks, master), a note's track and pitch class (or -1),
 * the drums' kick (0..255), 12 spectrum bands (0..1000), the trace (MKV_WP points, -127..127), a beat count */
static void mkv_feed(const int16_t *lvl, const int8_t *note, int32_t kick, const int16_t *band, const int8_t *trace,
                     uint32_t beat_q8, uint32_t *t)
{
    int32_t k, i;
    for (k = 0; k < MK_NK; k++)                        /* the keys: lit by notes, fading */
        mkv_kf[k] = (uint16_t)(mkv_kf[k] * 3u / 4u);
    for (i = 0; i < 4; i++)
        if (note[i] >= 0) {
            mkv_kf[note[i] % MK_NK] = 256;
            mkv_kt[note[i] % MK_NK] = (uint8_t)i;
        }
    for (k = 0; k < MK_NK; k++) {
        uint32_t c = mk_rgb(TE_COL[mkv_kt[k] & 3u]);
        mk_vkcol[k] = mkv_kf[k] > 16u ? mk_scale(c, 96u + mkv_kf[k] * 5u / 8u) : 0u;
        mk_vkdy[k] = (int16_t)(-Q(2) * (int32_t)mkv_kf[k] / 256);
    }
    for (k = 0; k < 12; k++) {                          /* the mountains */
        int32_t v = band[k] * 255 / 1000, z = mk_bandz[k];
        mk_bandz[k] = (uint8_t)(v > z ? z + (v - z) * 3 / 4 : z - (z - v) / 3);
    }
    mk_vear = kick / 3 * mk_sin((int32_t)(beat_q8 * 4u)) / 256;
    mk_vwag = Q(1) + Q(4) * lvl[3] / 1000 * mk_sin((int32_t)(beat_q8 * 2u)) / 256;
    mk_vyaw = 1024 * 6 / 360 * mk_sin((int32_t)(beat_q8 / 4u)) / 256;
    mk_sunr = 38 + 10 * lvl[4] / 1000;
    mkv_wh = (uint8_t)((mkv_wh + 1u) % MKV_WN);
    for (i = 0; i < MKV_WP; i++)
        mkv_wf[mkv_wh][i] = trace[i];
    *t = 6000u + (beat_q8 * 600u >> 8) % 600000u;     /* (the floor moves a cell a beat) */
}

#ifndef MK_HOST
/* ui_vis.c: draw style s if it is ours (after vis_update), else 0 */
static int mkv_draw(uint32_t s)
{
    int16_t b[32], band[12];
    int8_t note[4], tr[MKV_WP];
    uint32_t i, t, i0 = 0;
    if (s != VIS_STYLES - 2u && s != VIS_STYLES - 1u)
        return 0;
    for (i = 0; i < 4u; i++)
        note[i] = (int8_t)(vs.flash[i] == 8u ? vis_note[i] % 12u : -1);
    vis_fft();
    vis_bands(b);
    for (i = 0; i < 12u; i++) {
        int32_t m = 0, j;
        for (j = (int32_t)(i * 32u / 12u); j < (int32_t)((i + 1u) * 32u / 12u); j++)
            m = b[j] > m ? b[j] : m;
        band[i] = (int16_t)m;
    }
    for (i = 1; i + 240u < VIS_FFT; i++)                /* a rising zero crossing: the trace stands still */
        if (vis_l[i - 1u] < 0 && vis_l[i] >= 0) {
            i0 = i;
            break;
        }
    for (i = 0; i < MKV_WP; i++)
        tr[i] = (int8_t)clamp(vis_l[i0 + i * 240u / MKV_WP] * 110 / (vs.scale > 1 ? vs.scale : 1), -127, 127);
    mkv_feed(vs.lvl, note, vs.kick, band, tr, vis_beat_q8(), &t);
    mkv_wcol = mk_rgb(TE_COL[song.sel & 3u]);
    mk_vis = (uint8_t)(s == VIS_STYLES - 2u ? 1 : 2);
    mk_frame(t);
    mk_vis = 0;
    if (vis_name_t || ui.msg_t) {                       /* the style's name a second (or a message) */
        const char *n = ui.msg_t ? ui.msg : VIS_NAME[s];
        cv_begin(240, 20, C_BLACK);
        cv_text(120 - text_w(&FONT_S, n) / 2, 2, &FONT_S, n, C_WHITE);
        cv_blit(0, 0);
    }
    if (vis_name_t)
        vis_name_t--;
    return 1;
}
#endif
