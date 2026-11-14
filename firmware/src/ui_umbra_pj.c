/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c before the layers, UMB_PJ builds) THE PROJECTOR'S STREAM (plans/umbra-projector-sync.md).
 * The projector (web/emu ?projector) draws what TOTALITY draws by running the same plates on the same inputs: each
 * layer's context and notes as umb_lay_run hands them over, and what TOTALITY composes the layers with (ui_umbra_tot.c
 * umt_pj_screen), each part only where it changed, with a CRC of a piece of each layer's state after its frame (a
 * projector that drifts is seen to). A projector that comes in (PJ_WATCH, editor_plate.c), or asks for everything again
 * (it lost a message), gets every context and the screen whole, and each layer's whole state, a piece a frame, the layer
 * held still meanwhile (its notes and its time wait, as a hidden layer's do): so what arrives is one moment's (a copy of
 * a moving state never catches up with a plate whose parts move each other, TARN's water: .tools/harness/pj_join.mjs).
 *
 * A frame's events (the main loop's frame: TOTALITY's, or HOME's plates running behind its pages) are made as the layers
 * run, into buf, then sent as messages of UPJ_PIECE bytes at most, each 48 on the wire (a USB transfer), from the main
 * loop's wait (upj_tick): one at a time, into an empty SysEx ring, never while a MIDI note is due (a note waits a
 * transfer at most). No room for a frame: it is lost, and everything goes again (a resync).
 *
 * The wire: F0 7D 46 4C 102 .. F7, the bytes 7-bit packed (groups of a top-bits byte, then up to 7 bytes, as BACKUP_PUT):
 * the message's number (seq, +1 each), flags (bit 0: the frame's last; bit 1 its first: a receiver come in gathers from
 * one), then the frame's bytes, continued. A frame's events:
 *   SCREEN     1, force (the panel sent whole), the screen (upj_scr_t) as a delta: TOTALITY drawn at the frame's end
 *   INIT | li  0x10 | li, the plate's key (umx_key of its name, 2), its state's place in the block (2), the context it
 *              started on (a delta)
 *   FRAME | li 0x20 | li, key (2), place (2), the context its notes get (a delta)
 *   NOTE | li  0x40 | li, a note as handed over (umb_note_t, 10)
 *   POST | li  0x50 | li, the context its frame gets (a delta), a CRC-16 of a piece of its state after it (place 2,
 *              length 2, CRC 2)
 *   SNAP | li  0x30 | li, key (2), place (2), the context it is drawn on (a delta), a piece of its state (place 2,
 *              length 2, the bytes): held; it runs again the frame after the last piece
 * Numbers two bytes, low first. A delta: a mask (bit i: byte i changed), then the bytes that did; a layer's contexts are
 * deltas from the last one sent for it, the screen from the last screen.
 *
 * On the FM-1 the sender is in the Bluetooth segment (BTAPP_FN: the app slot is full); PJ_WATCH turns it on only where
 * that segment is (bt_hw_seg), so a caller asks upj.on alone.
 *
 * The receiver (FELUCCA_WEB: upj_rx) draws nothing of its own (web/emu/felucca_web.c web_frame): a frame's events run its
 * own plates as the sender ran its, and after a SCREEN it draws TOTALITY from what was sent (ui_umbra_tot.c upj_rx_paint) */
#define UPJ_WATCH 101u                                  /* (editor_plate.c) the page: bit 0 watch, bit 1 everything again */
#define UPJ_CMD 102u
#define UPJ_PIECE 34u                                   /* a message's frame bytes at most: + seq, flags: 48 on the wire */
#define UPJ_NFE 8u                                      /* frames made, not all sent yet */
#define UPJ_SNAPN 512u                                  /* a held layer's state sent a frame (web_pj_rate: other), one layer
                                                         * at a time (the others held, waiting their turn) */
#define UPJ_WIN 256u                                    /* the CRC's piece of a state */
#if FELUCCA_WEB && !UPJ_SMALL
#define UPJ_TXN 16384u                                  /* a frame's events made, not sent yet, at most */
#else
#define UPJ_TXN 1024u                                   /* (the FM-1's; UPJ_SMALL: the emulator built with it, a test) */
#endif
#if FELUCCA_WEB
#define UPJ_RAM
#else
#define UPJ_RAM __attribute__((section(".upj.bss")))    /* (app.ld: after the rest of .bss: nothing there moves) */
#endif
#define UPJ_HOLDN 104u                                  /* a held layer's SNAP without its piece, its context whole, at most */
enum { UPJ_E_SCREEN = 1u, UPJ_E_INIT = 0x10u, UPJ_E_FRAME = 0x20u, UPJ_E_SNAP = 0x30u, UPJ_E_NOTE = 0x40u, UPJ_E_POST = 0x50u };
#define UPJ_BODY 0xFFFFu                                /* a layer's key: BODIES (figures, not a plate) */
typedef struct {                                        /* what TOTALITY composes the layers with, once a frame */
    uint32_t frame;                                     /* ui.frame (GLITCH's tears) */
    uint16_t la[4];                                     /* each layer's light as umb_scene takes it (the fades in) */
    uint16_t lt[2], gt;                                 /* a TIMELINE cell's transition: umv_lt, the panel's gate (dc_gate) */
    uint16_t ge;                                        /* GLITCH's energy (umt.ge) */
    uint16_t gk, lk[3];                                 /* the ground's plate, LAYER 1..3's, by key (0: none; UPJ_BODY) */
    uint8_t o[G_N];                                     /* the picture's own elements now (umq_o: LIGHT, the effects) */
    uint8_t mo, sk;                                     /* MOSAIC from a cell (umv_mo); the strobe (0: none, 1 + its colour) */
    int8_t roll;                                        /* the picture pushed up / down (umt.roll) */
    uint8_t mir;                                        /* MIRROR as the frame drew it (read before the routes moved it) */
    uint16_t dcr, rsv;                                  /* the panel's rolling resend (gfx.c dc_roll: which cells go anyway,
                                                         * a cell whose change the hash missed mended in step) */
} upj_scr_t;
_Static_assert(sizeof(upj_scr_t) == 52u, "the screen: no padding, the same on either side");
_Static_assert(sizeof(umb_lctx_t) == 80u && sizeof(umb_note_t) == 10u, "the plate API's records, as sent");
_Static_assert(UPJ_SNAPN + UPJ_HOLDN * UMB_NLAY + 64u <= UPJ_TXN, "a frame's held layers and the screen fit, whole");
static struct {
    uint8_t on, seq, ovf;                               /* streaming; the next message's number; this frame's did not fit */
    uint8_t full;                                       /* (bits) layer li's context sent whole next; bit 4: the screen */
    uint8_t snap;                                       /* (bits) layers whose state is to go whole (held till it has) */
    uint8_t sk;                                         /* the strobe this frame (umt_fx) */
    uint8_t nfe;                                        /* frames made, waiting to be sent (their ends: fe) */
    uint8_t fst;                                        /* the next message begins a frame */
    uint16_t n, sent, snapn;                            /* buf's bytes made, sent; a held piece's bytes (0: UPJ_SNAPN) */
    uint16_t fe[UPJ_NFE];
    uint16_t cur[UMB_NLAY];                             /* each held layer's state sent so far */
    uint32_t sf[UMB_NLAY];                              /* the frame (+ 1) whose held piece went: held all of it (a byte
                                                         * came round every 256 frames: a frame held for nothing) */
    uint32_t sfr;                                       /* .. the frame (+ 1) a piece of any layer's went in (one a frame) */
    uint32_t wms;                                       /* the last PJ_WATCH (the FM-1: 3 s without one, it stops) */
    uint32_t lost, made;                                /* frames lost (no room); events made */
    umb_lctx_t sh[UMB_NLAY];                            /* each layer's context as last sent */
    upj_scr_t ss;                                       /* .. the screen's */
    uint8_t buf[UPJ_TXN];
} upj UPJ_RAM;
static int upj_wire(const uint8_t *m, uint32_t n);      /* a message out (0: no room now) */

/* ------------------------------------------------------------------------------------------- the sender --- */
static void upj_b(uint32_t v)
{
    if (upj.n < UPJ_TXN)
        upj.buf[upj.n++] = (uint8_t)v;
    else
        upj.ovf = 1;
}
static void upj_w(uint32_t v) { upj_b(v); upj_b(v >> 8); }
BTAPP_FN static void upj_ev(uint32_t tag)                        /* an event begins (the bytes sent let go of first) */
{
    uint32_t i, k = upj.sent;
    if (k && upj.n + 512u > UPJ_TXN) {
        for (i = k; i < upj.n; i++)                     /* (down to the start: no memmove in the firmware's libc) */
            upj.buf[i - k] = upj.buf[i];
        upj.n = (uint16_t)(upj.n - k);
        upj.sent = 0;
        for (i = 0; i < upj.nfe; i++)
            upj.fe[i] = (uint16_t)(upj.fe[i] - k);
    }
    upj.made++;
    upj_b(tag);
}
/* n bytes at now as a delta from was (whole: every byte), was made now */
BTAPP_FN static void upj_delta(const void *now, void *was, uint32_t n, uint32_t whole)
{
    const uint8_t *a = (const uint8_t *)now;
    uint8_t *w = (uint8_t *)was;
    uint32_t i, m = upj.n;
    for (i = 0; i < (n + 7u) / 8u; i++)
        upj_b(0);
    for (i = 0; i < n; i++)
        if (whole || a[i] != w[i]) {
            if (m + i / 8u < UPJ_TXN)
                upj.buf[m + i / 8u] |= (uint8_t)(1u << (i & 7u));
            upj_b(a[i]);
            w[i] = a[i];
        }
}
BTAPP_FN static uint32_t upj_crc(const uint8_t *p, uint32_t n)   /* CRC-16 (CCITT, 0xFFFF first) */
{
    uint32_t c = 0xFFFFu, k;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (k = 0; k < 8u; k++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
    }
    return c & 0xFFFFu;
}
static uint32_t upj_place(const void *st) { return (uint32_t)((const uint8_t *)st - (const uint8_t *)&umb); }
BTAPP_FN static void upj_ctx(uint32_t li, const umb_lctx_t *c)  /* layer li's context, as a delta (whole after a resync) */
{
    upj_delta(c, &upj.sh[li], sizeof *c, upj.full >> li & 1u);
    upj.full &= (uint8_t)~(1u << li);
}
BTAPP_FN static void upj_head(uint32_t e, const umb_plate_t *p, const void *st, const umb_lctx_t *c)   /* tag, key, place, context */
{
    upj_ev(e);
    upj_w(umx_key(p->name));
    upj_w(upj_place(st));
    upj_ctx(e & 3u, c);
}
/* (umb_lay_run) layer li's plate p started afresh at st on context c */
BTAPP_FN static void upj_tx_init(uint32_t li, const umb_plate_t *p, const void *st, const umb_lctx_t *c)
{
    if (!upj.on || li >= UMB_NLAY)
        return;
    upj.snap &= (uint8_t)~(1u << li);                   /* (started afresh on both sides: nothing to send) */
    upj_head(UPJ_E_INIT | li, p, st, c);
}
BTAPP_FN static void upj_tx_frame(uint32_t li, const umb_plate_t *p, const void *st, const umb_lctx_t *c)   /* .. its frame: notes next */
{
    if (upj.on && li < UMB_NLAY)
        upj_head(UPJ_E_FRAME | li, p, st, c);
}
BTAPP_FN static void upj_tx_note(uint32_t li, const umb_note_t *n)   /* .. a note handed over */
{
    uint32_t i;
    if (!upj.on || li >= UMB_NLAY)
        return;
    upj_ev(UPJ_E_NOTE | li);
    for (i = 0; i < sizeof *n; i++)
        upj_b(((const uint8_t *)n)[i]);
}
/* .. its frame run, on context c: the context, a CRC of a piece of its state (a piece a frame, round) */
BTAPP_FN static void upj_tx_post(uint32_t li, const umb_plate_t *p, const void *st, const umb_lctx_t *c)
{
    uint32_t sz = p->state, k, n;
    if (!upj.on || li >= UMB_NLAY)
        return;
    upj_ev(UPJ_E_POST | li);
    upj_ctx(li, c);
    k = sz > UPJ_WIN ? ui.frame * UPJ_WIN % ((sz + UPJ_WIN - 1u) / UPJ_WIN * UPJ_WIN) : 0u;
    n = sz - k < UPJ_WIN ? sz - k : UPJ_WIN;
    upj_w(k);
    upj_w(n);
    upj_w(upj_crc((const uint8_t *)st + k, n));
}
/* (umb_lay_run) a frame of layer li's, on context c: 1 while it is held, its state going whole, a piece a frame of one
 * layer at a time (the others held meanwhile, their contexts sent: SNAP without a piece), each as much as the frame has
 * room for, the layers after it and the screen kept room for */
BTAPP_FN static int upj_tx_snap(uint32_t li, const umb_plate_t *p, const void *st, const umb_lctx_t *c)
{
    uint32_t k, n, i, keep = UPJ_HOLDN * (UMB_NLAY - li) + 64u;
    if (!upj.on || li >= UMB_NLAY)
        return 0;
    if (upj.sf[li] == ui.frame + 1u)                   /* (this frame's piece gone, the last too: its other strips held) */
        return 1;
    if (!(upj.snap >> li & 1u))
        return 0;
    upj.sf[li] = ui.frame + 1u;
    k = upj.cur[li] < p->state ? upj.cur[li] : 0u;
    n = upj.sfr == ui.frame + 1u ? 0u : upj.snapn ? upj.snapn : UPJ_SNAPN;
    if (n + keep > UPJ_TXN - upj.n)
        n = UPJ_TXN - upj.n > keep ? UPJ_TXN - upj.n - keep : 0u;
    if (n > p->state - k)
        n = p->state - k;
    if (n)
        upj.sfr = ui.frame + 1u;
    upj_head(UPJ_E_SNAP | li, p, st, c);
    upj_w(k);
    upj_w(n);
    for (i = 0; i < n; i++)
        upj_b(((const uint8_t *)st)[k + i]);
    upj.cur[li] = (uint16_t)(k + n);
    if (k + n >= p->state)                              /* (all of it: the layer runs on from the next frame) */
        upj.snap &= (uint8_t)~(1u << li);
    return 1;
}
/* (ui_umbra_tot.c umb_full) TOTALITY's frame: what it composes the layers with, before they are drawn */
BTAPP_FN static void upj_tx_screen(const upj_scr_t *s, uint32_t force)
{
    if (!upj.on)
        return;
    upj_ev(UPJ_E_SCREEN);
    upj_b(force);
    upj_delta(s, &upj.ss, sizeof *s, upj.full >> 4 & 1u);
    upj.full &= (uint8_t)~16u;
}
BTAPP_FN static void upj_resync(void)                           /* everything again: contexts and screen whole, states held and sent */
{
    upj.full = 31u;
    upj.snap = (1u << UMB_NLAY) - 1u;
    memset(upj.cur, 0, sizeof upj.cur);
    ui.force = 1;
}
/* streaming on or off (bit 0); bit 1, or coming on: everything again (a projector come, or one that lost a message) */
BTAPP_FN static void upj_watch(uint32_t v)
{
    uint32_t was = upj.on;
    upj.on = (uint8_t)(v & 1u);
    upj.wms = fm1_ms;
    if (!upj.on || !was) {
        upj.n = upj.sent = upj.nfe = upj.ovf = 0;
        upj.fst = 1;
    }
    if (upj.on && (!was || (v & 2u)))
        upj_resync();
}
/* the main loop's wait: the frames made, out as messages while there is room (none: later) */
BTAPP_FN static void upj_tick(void)
{
    uint8_t m[48];
#if !FELUCCA_WEB
    if (upj.on && (fm1_ms - upj.wms > 3000u || !usb.config))   /* (the page gone: 3 s without a PJ_WATCH) */
        upj_watch(0);
#endif
    while (upj.nfe) {
        uint32_t k = upj.fe[0] - upj.sent, last, i, j, w = 5;
        if (k > UPJ_PIECE)
            k = UPJ_PIECE;
        last = upj.sent + k == upj.fe[0];
        m[0] = 0xF0u;
        m[1] = 0x7Du;                                   /* (the editor's header, editor.c ED_HDR0..2) */
        m[2] = 0x46u;
        m[3] = 0x4Cu;
        m[4] = UPJ_CMD;
        for (i = 0; i < k + 2u; i += 7u) {
            uint32_t mi = w++;
            m[mi] = 0;
            for (j = 0; j < 7u && i + j < k + 2u; j++) {
                uint32_t b = !(i + j) ? upj.seq : i + j == 1u ? last | (uint32_t)upj.fst << 1 : upj.buf[upj.sent + i + j - 2u];
                m[mi] |= (uint8_t)((b >> 7 & 1u) << j);
                m[w++] = (uint8_t)(b & 0x7Fu);
            }
        }
        m[w++] = 0xF7u;
        if (!upj_wire(m, w))
            break;
        upj.seq++;
        upj.fst = (uint8_t)last;
        upj.sent = (uint16_t)(upj.sent + k);
        if (last) {
            upj.nfe--;
            for (i = 0; i < upj.nfe; i++)
                upj.fe[i] = upj.fe[i + 1u];
        }
    }
    if (!upj.nfe && upj.sent == upj.n)
        upj.n = upj.sent = 0;
}
/* the main loop's frame done: its events, a frame to send (none: nothing ran). One that did not fit is lost, and
 * everything goes again */
BTAPP_FN static void upj_tx_flush(void)
{
    uint32_t e;
    if (!upj.on)
        return;
    e = upj.nfe ? upj.fe[upj.nfe - 1u] : upj.sent;
    if (upj.ovf || (upj.n > e && upj.nfe == UPJ_NFE)) {
        upj.n = (uint16_t)e;
        upj.ovf = 0;
        upj.lost++;
        upj_resync();
    } else if (upj.n > e)
        upj.fe[upj.nfe++] = upj.n;
    upj_tick();
}
#if !FELUCCA_WEB
BTAPP_FN static int upj_due(void)                               /* a MIDI note waiting that goes within a millisecond */
{
    uint32_t due;
    if (mo_w == mo_r)
        return 0;
    due = midi_out_due[mo_r % MQ];
    return !due || (int32_t)(fm1_ticks() + 1000u * FM1_TICKS_PER_US - due) >= 0;
}
/* a message into the SysEx ring, only when it is empty and no note is due (usb.c ep1_tx sends SysEx first): a note
 * waits one transfer at most (usb.c ota_wire_send's packets; umb_mirror.c umr_send's rule, tried on the FM-1) */
BTAPP_FN static int upj_wire(const uint8_t *m, uint32_t n)
{
    uint32_t i = 0, k, cin, pkt;
    if (!*(volatile uint8_t *)&usb.config || sx_busy || so_w != so_r || upj_due())
        return 0;
    sx_busy = 1;
    while (i < n) {
        k = n - i >= 3u ? 3u : n - i;
        cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;
        pkt = cin | (uint32_t)m[i] << 8 | (k > 1u ? (uint32_t)m[i + 1] << 16 : 0u) | (k > 2u ? (uint32_t)m[i + 2] << 24 : 0u);
        sx_out_q[so_w % SXQ] = pkt;
        RING_PUBLISH();
        so_w++;
        i += k;
    }
    sx_busy = 0;
    return 1;
}
#endif

#if FELUCCA_WEB
/* ---------------------------------------------------------------------------------- the emulator's ends --- */
#define UPJ_OUTN 65536u                                 /* the messages made, waiting to be taken (web_pj_out) */
#define UPJ_INN 65536u                                  /* the messages come, waiting to be run (web_pj_in) */
static uint8_t upj_out[UPJ_OUTN], upj_in[UPJ_INN];
static uint32_t upj_out_n;
static int upj_wire(const uint8_t *m, uint32_t n)
{
    if (upj_out_n + n > UPJ_OUTN)
        return 0;
    memcpy(upj_out + upj_out_n, m, n);
    upj_out_n += n;
    return 1;
}

/* ----------------------------------------------------------------------------------------- the receiver --- */
static struct {
    umb_lctx_t rc[UMB_NLAY];                            /* each layer's context as received */
    upj_scr_t rs;                                       /* .. the screen's */
    const umb_plate_t *rp[UMB_NLAY];                    /* .. its plate here (0: none here), its state's place */
    uint8_t *rst[UMB_NLAY];
    uint8_t rdt[UMB_NLAY];                              /* .. its next draw takes dt (the frame's first strip) */
    uint8_t rsnap;                                      /* (bits) layers whose state has not all come: dark meanwhile */
    uint8_t gpl;                                        /* the ground's number here */
    uint8_t rseq, rgot, rbad, skip;                     /* the message due; any yet; this frame broken; skipping (come in,
                                                         * or one lost) until a frame's first message */
    const uint8_t *q, *qe;                              /* the frame being read */
    uint32_t fn;                                        /* the frame's bytes gathered */
    uint32_t st[8];                                     /* messages, bytes, CRCs right, wrong, gaps, broken, frames run,
                                                         * paints */
    uint8_t fr[UPJ_TXN];                                /* a frame, gathered from its messages */
} upr;
static uint32_t upj_r1(void)
{
    if (upr.q < upr.qe)
        return *upr.q++;
    upr.rbad = 1;
    return 0;
}
static uint32_t upj_r2(void) { uint32_t v = upj_r1(); return v | upj_r1() << 8; }
static void upj_rdelta(void *to, uint32_t n)            /* a delta (upj_delta) onto n bytes at to */
{
    uint8_t *t = (uint8_t *)to;
    const uint8_t *m = upr.q;
    uint32_t i, nm = (n + 7u) / 8u;
    if ((uint32_t)(upr.qe - upr.q) < nm) {
        upr.rbad = 1;
        return;
    }
    upr.q += nm;
    for (i = 0; i < n; i++)
        if (m[i / 8u] >> (i & 7u) & 1u)
            t[i] = (uint8_t)upj_r1();
}
static uint32_t upj_pl(uint32_t key)                   /* the plate whose key it is here (UMB_NONE: none; UMX_BODY: BODIES) */
{
    uint32_t pl;
    if (key == UPJ_BODY)
        return UMX_BODY;
    for (pl = 0; key && pl < UMB_NPLT; pl++)
        if (umb_has(pl) && umx_key(umb_mod(pl)->name) == key)
            return pl;
    return UMB_NONE;
}
static void upj_rx_paint(uint32_t force);               /* (ui_umbra_tot.c) */
static void upj_rx_frame(const uint8_t *q, uint32_t n) /* a frame's events run, then TOTALITY drawn (a SCREEN came) */
{
    uint32_t paint = 0, force = 0, e;
    upr.q = q;
    upr.qe = q + n;
    upr.rbad = 0;
    while (upr.q < upr.qe && !upr.rbad) {
        uint32_t li, t, k, m, i;
        e = upj_r1();
        li = e & 3u;
        t = e & 0xF0u;
        if (e == UPJ_E_SCREEN) {
            force |= upj_r1();
            upj_rdelta(&upr.rs, sizeof upr.rs);
            paint = 1;
        } else if ((e & 0x0Cu) || t > UPJ_E_POST || !t) {
            upr.rbad = 1;
        } else if (t == UPJ_E_NOTE) {
            umb_note_t nt;
            for (i = 0; i < sizeof nt; i++)
                ((uint8_t *)&nt)[i] = (uint8_t)upj_r1();
            if (upr.rp[li] && !upr.rbad)
                upr.rp[li]->note(upr.rst[li], &upr.rc[li], &umb_api, &nt);
        } else if (t == UPJ_E_POST) {
            upj_rdelta(&upr.rc[li], sizeof upr.rc[li]);
            if (upr.rp[li] && !upr.rbad)
                upr.rp[li]->frame(upr.rst[li], &upr.rc[li], &umb_api);
            k = upj_r2();
            m = upj_r2();
            i = upj_r2();
            if (upr.rp[li] && !upr.rbad && k + m <= upr.rp[li]->state && !(upr.rsnap >> li & 1u))
                upr.st[upj_crc(upr.rst[li] + k, m) == i ? 2 : 3]++;
            upr.rdt[li] = 1;
            upr.st[6]++;
        } else {                                        /* INIT, FRAME, SNAP: the plate, its place, a context */
            uint32_t key = upj_r2(), off = upj_r2(), pl = upj_pl(key);
            const umb_plate_t *p = pl < UMB_NPLT ? umb_mod(pl) : 0;
            umb_layer_t *L = &umb_lay[li];
            uint8_t *st = (uint8_t *)&umb + off;
            if (p && ((off & 3u) || off + p->state > UMB_BLK))
                p = 0;
            upj_rdelta(&upr.rc[li], sizeof upr.rc[li]);
            if (upr.rbad)
                break;
            upr.rp[li] = p;                             /* (none here: its events read, nothing run) */
            upr.rst[li] = st;
            if (p) {
                L->pp = p;
                L->st = st;
            }
            if (t == UPJ_E_INIT) {
                upr.rsnap &= (uint8_t)~(1u << li);
                if (p) {
                    memset(st, 0, p->state);
                    p->init(st, &upr.rc[li], &umb_api);
                }
            } else if (t == UPJ_E_SNAP) {               /* a piece of its state, held: drawn on the context sent */
                k = upj_r2();
                m = upj_r2();
                for (i = 0; i < m; i++) {
                    uint32_t b = upj_r1();
                    if (p && k + i < p->state)
                        st[k + i] = (uint8_t)b;
                }
                upr.rsnap = (uint8_t)(p && k + m >= p->state ? upr.rsnap & ~(1u << li) : upr.rsnap | 1u << li);
                upr.rdt[li] = 0;
            }
        }
    }
    if (upr.rbad)
        upr.st[5]++;
    else if (paint)
        upj_rx_paint(force);
}
/* SysEx in (n bytes, any number of messages): each of the stream's gathered into its frame, a frame run when whole */
static void upj_rx_feed(const uint8_t *a, uint32_t n)
{
    uint32_t i = 0, j, k, r;
    uint8_t raw[48];
    upr.st[1] += n;
    while (i < n) {
        if (a[i] != 0xF0u) {
            i++;
            continue;
        }
        for (j = i + 1u; j < n && a[j] != 0xF7u; j++)
            ;
        if (j >= n)
            return;
        if (j - i > 5u && a[i + 1] == 0x7Du && a[i + 2] == 0x46u && a[i + 3] == 0x4Cu && a[i + 4] == UPJ_CMD) {
            const uint8_t *s = a + i + 5u;
            uint32_t ns = j - i - 5u, bad = 0;
            r = 0;
            while (ns && !bad) {                        /* (7-bit groups: the top bits, then up to 7 bytes) */
                uint32_t m = *s++, g;
                ns--;
                g = ns > 7u ? 7u : ns;
                if (!g || r + g > sizeof raw)
                    bad = 1;
                for (k = 0; k < g && !bad; k++)
                    raw[r++] = (uint8_t)(s[k] | (m >> k & 1u) << 7);
                s += g;
                ns -= g;
            }
            upr.st[0]++;
            if (bad || r < 2u) {
                upr.st[5]++;
            } else {
                if (upr.rgot && raw[0] != upr.rseq) {   /* a message lost: its frame skipped, the layers dark until their
                                                         * states come again (the page asks: PJ_WATCH 3) */
                    upr.st[4]++;
                    upr.fn = 0;
                    upr.skip = 1;
                    upr.rsnap = (1u << UMB_NLAY) - 1u;
                }
                upr.rseq = (uint8_t)(raw[0] + 1u);
                upr.rgot = 1;
                if (raw[1] & 2u) {                      /* a frame's first: gathered from here */
                    upr.fn = 0;
                    upr.skip = 0;
                }
                if (!upr.skip && upr.fn + r - 2u <= sizeof upr.fr) {
                    memcpy(upr.fr + upr.fn, raw + 2, r - 2u);
                    upr.fn += r - 2u;
                } else
                    upr.skip = 1;
                if (raw[1] & 1u) {                      /* the frame's last */
                    if (!upr.skip && upr.fn)
                        upj_rx_frame(upr.fr, upr.fn);
                    upr.fn = 0;
                }
            }
        }
        i = j + 1u;
    }
}
#endif
