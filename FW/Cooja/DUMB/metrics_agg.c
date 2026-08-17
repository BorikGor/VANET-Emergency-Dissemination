#include "metrics_agg.h"
#include "contiki.h"
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Internal data structures
 * ------------------------------------------------------------------------- */

/* Per-message record: (origin, msg, time). */
typedef struct {
    uint32_t o;
    uint32_t m;
    uint32_t t;
    uint16_t kind;   /* 0=MBL CREATE, 1=EMG CREATE, 2=EMG RX */
    uint8_t  a8;     /* emergency code (for EMG*), else 0 */
    uint8_t  used;
} metr_rec_t;

/* -------------------------------------------------------------------------
 * Global/static state (kept minimal)
 * ------------------------------------------------------------------------- */

/* Mobile CREATE ring. */
static metr_rec_t mbl_create_ring[METR_RING_MBL_CREATE];
static uint16_t   mbl_create_head = 0;
static uint16_t   mbl_create_cnt  = 0;

/* RSU RX ring. */
static metr_rec_t rsu_rx_ring[METR_RING_RSU_RX];
static uint16_t   rsu_rx_head = 0;
static uint16_t   rsu_rx_cnt  = 0;

/* TX counters (windowed). */
static uint32_t tx_tot = 0;
static uint32_t tx_d   = 0;
static uint32_t tx_a   = 0;
static uint32_t tx_q   = 0;
static uint32_t tx_e   = 0;

/* RSU TX counters (windowed). */
static uint32_t rsu_tx_tot = 0;
static uint32_t rsu_tx_a   = 0;
static uint32_t rsu_tx_e   = 0;

/* BUF stats (windowed). */
static uint16_t buf_last = 0;
static uint16_t buf_max  = 0;
static uint32_t buf_put  = 0;
static uint32_t buf_rm   = 0;
static uint32_t buf_ev   = 0;

/* Timers (ms) for periodic batching/aggregates. */
static uint32_t next_batch_ms = 0;
static uint32_t next_agg_ms   = 0;

/* Window start for aggregates (ms). */
static uint32_t agg_win_start_ms = 0;

/* Scheduled smear flush times (ms). */
static uint8_t  batch_flush_pending = 0;
static uint32_t batch_flush_at_ms   = 0;

static uint8_t  agg_flush_pending   = 0;
static uint32_t agg_flush_at_ms     = 0;
static uint16_t agg_flush_win_s     = 60;

/* Final report guard. */
static uint8_t final_done = 0;

/* -------------------------------------------------------------------------
 * metrics_agg_now_ms()
 * Convert Contiki clock_time() to milliseconds.
 * ------------------------------------------------------------------------- */
uint32_t
metrics_agg_now_ms(void)
{
    return (uint32_t)((clock_time() * 1000UL) / CLOCK_SECOND);
}

/* -------------------------------------------------------------------------
 * smear_delay_ms()
 * Compute a simple ID-based smear delay, guaranteed <= 4950 ms by default.
 * Uses: ((origin_id - 1) % 100) * 50
 * ------------------------------------------------------------------------- */
static uint32_t
smear_delay_ms(uint32_t origin_id)
{
    uint32_t id0 = (origin_id > 0) ? (origin_id - 1u) : 0u;
    uint32_t d = (id0 % 100u) * 50u;
    if(d > METR_SMEAR_MAX_MS) d = METR_SMEAR_MAX_MS;
    return d;
}

/* -------------------------------------------------------------------------
 * Per-message kinds (printed labels).
 * ------------------------------------------------------------------------- */
#define METR_KIND_MBL_CREATE 0u
#define METR_KIND_EMG_CREATE 1u
#define METR_KIND_EMG_RX     2u
#define METR_KIND_EMG_FWD    3u
#define METR_KIND_RSU_RX     4u

static const char *
tag_from_kind(uint8_t kind)
{
    switch(kind) {

    case METR_KIND_EMG_CREATE: return "EMG CREATE";
    case METR_KIND_EMG_RX:     return "EMG RX";
	case METR_KIND_EMG_FWD:    return "EMG FWD";
    case METR_KIND_RSU_RX:     return "RSU RX";
    default:                   return "MBL CREATE";
    }
}

static uint8_t
kind_has_a(uint8_t kind)
{
    return (kind == 1 || kind == 2 || kind == 3) ? 1 : 0;
}

/*-------------------------------------------------------------------*
 * ring_push()
 * What: Push a per-message record into a ring buffer.
 * Why: Buffers per-message metric lines and prints in batches.
 * Creates: updates in-place ring slot at *head.
 --------------------------------------------------------------------*/
static void
ring_push(metr_rec_t *ring, uint16_t ring_sz,
          uint16_t *head, uint16_t *cnt,
          uint8_t kind, uint8_t a8, uint32_t o, uint32_t m, uint32_t t)
{
    metr_rec_t *slot = &ring[*head];

    if(slot->used) {
        /* Safety: print before overwrite so we don't lose data. */
        const char *tag = tag_from_kind(slot->kind);

        if(slot->kind == METR_KIND_EMG_RX) {
            printf("METR EMG RX t=%lu o=%lu m=%lu r=%u\n",
                   (unsigned long)slot->t,
                   (unsigned long)slot->o,
                   (unsigned long)slot->m,
                   (unsigned)slot->a8);
        }
        else if(kind_has_a(slot->kind)) {
            printf("METR %s t=%lu o=%lu m=%lu a=%u\n",
                   tag,
                   (unsigned long)slot->t,
                   (unsigned long)slot->o,
                   (unsigned long)slot->m,
                   (unsigned)slot->a8);
        }
        else {
            printf("METR %s t=%lu o=%lu m=%lu\n",
                   tag,
                   (unsigned long)slot->t,
                   (unsigned long)slot->o,
                   (unsigned long)slot->m);
        }
    } else {
        if(*cnt < ring_sz) (*cnt)++;
    }

    slot->o = o;
    slot->m = m;
    slot->t = t;
    slot->kind = kind;
    slot->a8 = a8;
    slot->used = 1;

    (*head)++;
    if(*head >= ring_sz) *head = 0;
}

/*
 * ring_flush_all()
 * What: Flush all used records in ring-order, then clear ring.
 * Why: Periodic batching output for per-message events.
 */
static void
ring_flush_all(metr_rec_t *ring, uint16_t ring_sz,
               uint16_t *head, uint16_t *cnt,
               const char *tag /* kept for compatibility; ignored */)
{
    /* Print in FIFO-ish order: oldest first.
     * Oldest is at head when full; otherwise we scan all used slots.
     */
    for(uint16_t i = 0; i < ring_sz; i++) {
        uint16_t idx = (*head + i) % ring_sz;
        metr_rec_t *r = &ring[idx];
        if(!r->used) continue;

        const char *rtag = tag_from_kind(r->kind);

        if(r->kind == METR_KIND_EMG_RX) {
            printf("METR EMG RX t=%lu o=%lu m=%lu r=%u\n",
                   (unsigned long)r->t,
                   (unsigned long)r->o,
                   (unsigned long)r->m,
                   (unsigned)r->a8);
        }
        else if(kind_has_a(r->kind)) {
            printf("METR %s t=%lu o=%lu m=%lu a=%u\n",
                   rtag,
                   (unsigned long)r->t,
                   (unsigned long)r->o,
                   (unsigned long)r->m,
                   (unsigned)r->a8);
       }
        else {
            printf("METR %s t=%lu o=%lu m=%lu\n",
                   rtag,
                   (unsigned long)r->t,
                   (unsigned long)r->o,
                   (unsigned long)r->m);
        }

        r->used = 0;
    }

    *head = 0;
    *cnt  = 0;
}

/* -------------------------------------------------------------------------
 * metrics_agg_event()
 * Main entry point: call from anywhere to record an event.
 * Stores per-message events in rings, updates aggregate counters.
 *
 * Parameters:
 *   ev        - event type
 *   origin_id - origin identifier
 *   msg_id    - message identifier
 *   time_ms   - timestamp in ms (used for CREATE / RSU RX)
 *   aux_u16   - optional (e.g., carry_count for BUF events)
 * ------------------------------------------------------------------------- */
void
metrics_agg_event(metr_event_t ev,
                  uint32_t origin_id,
                  uint32_t msg_id,
                  uint32_t time_ms,
                  uint16_t aux_u16)
{
    switch(ev) {
    case METR_EV_MBL_CREATE:
        ring_push(mbl_create_ring, (uint16_t)METR_RING_MBL_CREATE,
                  &mbl_create_head, &mbl_create_cnt,
                  METR_KIND_MBL_CREATE, 0u, origin_id, msg_id, time_ms);

        break;

    case METR_EV_RSU_RX:
        ring_push(rsu_rx_ring,
                  (uint16_t)METR_RING_RSU_RX,
                  &rsu_rx_head, &rsu_rx_cnt,
                  METR_KIND_RSU_RX, 0u, origin_id, msg_id, time_ms);
        break;

    case METR_EV_EMG_CREATE:
        ring_push(mbl_create_ring, (uint16_t)METR_RING_MBL_CREATE,
                  &mbl_create_head, &mbl_create_cnt,
                  METR_KIND_EMG_CREATE, (uint8_t)aux_u16, origin_id, msg_id, time_ms);
        break;

    case METR_EV_EMG_RX:
        ring_push(mbl_create_ring, (uint16_t)METR_RING_MBL_CREATE,
                  &mbl_create_head, &mbl_create_cnt,
                  METR_KIND_EMG_RX, (uint8_t)aux_u16, origin_id, msg_id, time_ms);
        break;

    case METR_EV_EMG_FWD:
        ring_push(mbl_create_ring,
                  (uint16_t)METR_RING_MBL_CREATE,
                  &mbl_create_head, &mbl_create_cnt,
                  METR_KIND_EMG_FWD,
                  (uint8_t)aux_u16,
                  origin_id, msg_id, time_ms);
        break;

    case METR_EV_TX_DATA:
        tx_tot++; tx_d++;
        break;
    case METR_EV_TX_ACK:
        tx_tot++; tx_a++;
        break;
    case METR_EV_TX_QUERY:
        tx_tot++; tx_q++;
        break;
    case METR_EV_TX_EMG:
        tx_tot++; tx_e++;
        break;

    case METR_EV_BUF_PUT:
        buf_put++;
        buf_last = aux_u16;
        if(buf_last > buf_max) buf_max = buf_last;
        break;
    case METR_EV_BUF_RM:
        buf_rm++;
        buf_last = aux_u16;
        if(buf_last > buf_max) buf_max = buf_last;
        break;
    case METR_EV_BUF_EVICT:
        buf_ev++;
        buf_last = aux_u16;
        if(buf_last > buf_max) buf_max = buf_last;
        break;

    default:
        break;
    }
}

/* -------------------------------------------------------------------------
 * rsu_tx_event()
 * Helper: RSU-side TX counters by type.
 * Call from rsu.c before sending ACK / EMG rebroadcast.
 * ------------------------------------------------------------------------- */
static void
rsu_tx_event(metr_event_t ev)
{
    switch(ev) {
    case METR_EV_TX_ACK:
        rsu_tx_tot++; rsu_tx_a++;
        break;
    case METR_EV_TX_EMG:
        rsu_tx_tot++; rsu_tx_e++;
        break;
    default:
        /* Ignore other TX types on RSU for now. */
        break;
    }
}

/* -------------------------------------------------------------------------
 * metrics_agg_poll()
 * Periodic service routine:
 *   - Every 10s: schedule & flush per-message rings (CREATE / RSU RX).
 *   - Every 60s: schedule & flush aggregates (MBL TX/BUF or RSU TX).
 *   - At 19:50: schedule final aggregate flush with win=50 (same format).
 *
 * Call from main loop frequently (e.g., each PROCESS_YIELD cycle).
 * ------------------------------------------------------------------------- */
void
metrics_agg_poll(uint32_t now_ms,
                 uint16_t node_id_u16,
                 uint8_t is_rsu)
{
    /* Lazy init of timers and window start. */
    if(next_batch_ms == 0) {
        next_batch_ms = now_ms + (METR_BATCH_SEC * 1000u);
    }
    if(next_agg_ms == 0) {
        next_agg_ms = now_ms + (METR_AGG_SEC * 1000u);
        agg_win_start_ms = now_ms;
    }

    /* Final report trigger: at METR_FINAL_AT_MS print current partial window.
     * If periodic resets happen at exact 60s boundaries, then at 19:50
     * the current window length should be 50 seconds since last minute mark.
     */
    if(!final_done && now_ms >= (uint32_t)METR_FINAL_AT_MS) {
        uint32_t elapsed_ms = now_ms - agg_win_start_ms;
        uint16_t win_s = (uint16_t)(elapsed_ms / 1000u);

        /* Clamp to 50 by design requirement. */
        if(win_s > 50u) win_s = 50u;

        agg_flush_win_s = win_s;
        agg_flush_at_ms = now_ms + smear_delay_ms((uint32_t)node_id_u16);
        agg_flush_pending = 1;
        final_done = 1;
    }

    /* 10-second batch schedule for per-message rings. */
    if(now_ms >= next_batch_ms) {
        batch_flush_at_ms = now_ms + smear_delay_ms((uint32_t)node_id_u16);
        batch_flush_pending = 1;
        next_batch_ms += (METR_BATCH_SEC * 1000u);
    }

    /* 60-second aggregate schedule. */
    if(now_ms >= next_agg_ms) {
        agg_flush_win_s = METR_AGG_SEC;
        agg_flush_at_ms = now_ms + smear_delay_ms((uint32_t)node_id_u16);
        agg_flush_pending = 1;

        /* Reset window start for next aggregate period. */
        agg_win_start_ms = now_ms;
        next_agg_ms += (METR_AGG_SEC * 1000u);
    }

    /* Execute due batched per-message flush. */
    if(batch_flush_pending && now_ms >= batch_flush_at_ms) {
        if(is_rsu) {
            ring_flush_all(rsu_rx_ring,
                           (uint16_t)METR_RING_RSU_RX,
                           &rsu_rx_head, &rsu_rx_cnt,
                           "RSU RX");
        } else {
            ring_flush_all(mbl_create_ring,
                           (uint16_t)METR_RING_MBL_CREATE,
                           &mbl_create_head, &mbl_create_cnt,
                           "MBL CREATE");
        }
        batch_flush_pending = 0;
    }

    /* Execute due aggregate flush (same format for regular and final). */
    if(agg_flush_pending && now_ms >= agg_flush_at_ms) {
        if(is_rsu) {
            printf("METR RSU TX t=%lu win=%u id=%u tot=%lu a=%lu e=%lu\n",
                   (unsigned long)now_ms,
                   (unsigned)agg_flush_win_s,
                   (unsigned)node_id_u16,
                   (unsigned long)rsu_tx_tot,
                   (unsigned long)rsu_tx_a,
                   (unsigned long)rsu_tx_e);

            /* Reset RSU TX counters for next window. */
            rsu_tx_tot = 0;
            rsu_tx_a   = 0;
            rsu_tx_e   = 0;
        } else {
            printf("METR MBL TX t=%lu win=%u id=%u tot=%lu d=%lu a=%lu q=%lu e=%lu\n",
                   (unsigned long)now_ms,
                   (unsigned)agg_flush_win_s,
                   (unsigned)node_id_u16,
                   (unsigned long)tx_tot,
                   (unsigned long)tx_d,
                   (unsigned long)tx_a,
                   (unsigned long)tx_q,
                   (unsigned long)tx_e);

            printf("METR MBL BUF t=%lu win=%u id=%u last=%u max=%u put=%lu rm=%lu ev=%lu\n",
                   (unsigned long)now_ms,
                   (unsigned)agg_flush_win_s,
                   (unsigned)node_id_u16,
                   (unsigned)buf_last,
                   (unsigned)buf_max,
                   (unsigned long)buf_put,
                   (unsigned long)buf_rm,
                   (unsigned long)buf_ev);

            /* Reset MBL counters for next window. */
            tx_tot = tx_d = tx_a = tx_q = tx_e = 0;
            buf_max = buf_last;
            buf_put = buf_rm = buf_ev = 0;
        }

        agg_flush_pending = 0;
    }
}

/* -------------------------------------------------------------------------
 * Public hook for RSU TX counters.
 * If you prefer, you can expose this in the header; for simplicity,
 * call metrics_agg_event(METR_EV_TX_ACK/EMG, ...) in rsu.c and route it
 * here by checking is_rsu before calling metrics_agg_event.
 *
 * Simplest: in rsu.c call this local function by copying declaration
 * or move it to header as metrics_agg_rsu_tx(ev).
 * ------------------------------------------------------------------------- */
void
metrics_agg_rsu_tx(metr_event_t ev)
{
    rsu_tx_event(ev);
}
