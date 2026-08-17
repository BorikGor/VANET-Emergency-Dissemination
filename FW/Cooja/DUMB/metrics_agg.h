#ifndef METRICS_AGG_H
#define METRICS_AGG_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Metrics Aggregator (Contiki-NG)
 * -------------------------------------------------------------------------
 * What:
 *   A small, unified metrics/logging helper for BGKP/DUMB/UDP baselines.
 *   - Per-message events (CREATE / RSU RX) are buffered and printed in batches
 *     every METR_BATCH_SEC seconds, with a simple ID-based smear delay.
 *   - Aggregate counters (TX by type, BUF stats, RSU TX) are printed every
 *     METR_AGG_SEC seconds using one line per node/role.
 *   - A final report at METR_FINAL_AT_MS prints the same lines with win=50.
 *
 * Why:
 *   Reduces printf spam and keeps log format consistent across protocols.
 *
 * How:
 *   Call metrics_agg_event(...) at each event site.
 *   Call metrics_agg_poll(now_ms, node_id_u16, is_rsu) periodically
 *   (e.g., in the main process loop) to flush due batches/aggregates.
 * ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* Role-independent event types */
typedef enum {
    METR_EV_MBL_CREATE = 1,    /* Per-message: mobile created DATA */
    METR_EV_RSU_RX     = 2,    /* Per-message: RSU received DATA */

    METR_EV_TX_DATA    = 3,    /* Aggregate: TX counters (by type) */
    METR_EV_TX_ACK     = 4,
    METR_EV_TX_QUERY   = 5,
    METR_EV_TX_EMG     = 6,

    METR_EV_BUF_PUT    = 7,    /* Aggregate: BUF stats */
    METR_EV_BUF_RM     = 8,
    METR_EV_BUF_EVICT  = 9,
    
    METR_EV_EMG_CREATE = 10,   /* Per-message: emergency started (local) */
	METR_EV_EMG_FWD    = 11,
    METR_EV_EMG_RX     = 12    /* Per-message: emergency received */

} metr_event_t;

/* Configure sizes for per-message batching (tunable). */
#ifndef METR_RING_MBL_CREATE
#define METR_RING_MBL_CREATE 32
#endif

#ifndef METR_RING_RSU_RX
#define METR_RING_RSU_RX 128
#endif

#ifndef METR_RING_EMG_CREATE
#define METR_RING_EMG_CREATE 32
#endif

#ifndef METR_RING_EMG_RX
#define METR_RING_EMG_RX 128
#endif

/* Timing configuration (seconds / milliseconds). */
#ifndef METR_BATCH_SEC
#define METR_BATCH_SEC 10u
#endif

#ifndef METR_AGG_SEC
#define METR_AGG_SEC 60u
#endif

/* Final report time, ms since simulation start (19:50 => 1190s). */
#ifndef METR_FINAL_AT_MS
#define METR_FINAL_AT_MS (19u * 60u * 1000u + 50u * 1000u)
#endif

/* Smear: must start within <= 5 seconds. */
#ifndef METR_SMEAR_MAX_MS
#define METR_SMEAR_MAX_MS 5000u
#endif

/* Event API: call this from "any point in code". */
void metrics_agg_event(metr_event_t ev,
                       uint32_t origin_id,
                       uint32_t msg_id,
                       uint32_t time_ms,
                       uint16_t aux_u16);

/* Poll API: call periodically to print due batches/aggregates. */
void metrics_agg_poll(uint32_t now_ms,
                      uint16_t node_id_u16,
                      uint8_t is_rsu);

/* Optional: helper for time in ms from Contiki clock. */
uint32_t metrics_agg_now_ms(void);

/* Aggregate the properties of the received mesage. */
void metrics_agg_rsu_tx(metr_event_t ev);

#ifdef __cplusplus
}
#endif

#endif /* METRICS_AGG_H */
