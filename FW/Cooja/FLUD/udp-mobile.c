/* udp-mobile.c */

#include "contiki.h"
//#include "net/ipv6/uip.h"
#include "net/ipv6/simple-udp.h"
#include "sys/etimer.h"
//#include "sys/node-id.h"
#include "dev/serial-line.h"
#include "dev/uart1.h"
#include <stdio.h>
#include <string.h>

#define UDP_PORT      1234
#define SEND_INTERVAL ((CLOCK_SECOND * 5) / 2)   /* 2.5 s in ticks */
#define MAX_HOP 10 // maximum hops allowed for this simulation
#define SEEN_MAX 32 // mitigate data storms by ignoring messages seen to often

#define FLUD_METRIC_LOG_ENABLED 1

#if FLUD_METRIC_LOG_ENABLED
#include "metrics_agg.h"
#endif // FLUD_METRIC_LOG_ENABLED

/* ------------------------------------------------------------------ */
/*  Emergency parameters (mirrors DUMB behaviour)                      */
/*  3.0 m/s^2 = 30 dm/s^2; your DUMB uses 40 by default, keep same.    */
/* ------------------------------------------------------------------ */
#ifndef HB_THR_DMPS2
#define HB_THR_DMPS2 40
#endif
#ifndef EF_TTL_MS
#define EF_TTL_MS 2000u
#endif
#ifndef EF_TX_MIN_MS
#define EF_TX_MIN_MS 1000u
#endif

 typedef struct {
  uint16_t origin;
  uint16_t msg_id;
  uint8_t type;      /* 0=DATA, 1=EMG */
  uint8_t emg_code;  /* valid when type=EMG */
  uint8_t hop;
  uint32_t ts;
 } udp_msg_t;
 
 typedef struct {
  uint16_t origin;
  uint16_t msg_id;
  uint8_t type;
 } seen_msg_t;

static seen_msg_t seen_msg[SEEN_MAX];
static uint8_t seen_count = 0;

static uint8_t node_id8;
static struct simple_udp_connection udp_conn;
static uint16_t local_msg_counter = 0;
static uip_ipaddr_t mcast_addr;

/* ------------------------------------------------------------------ */
/*  Mobility and emergency state (LOC-driven, like DUMB)               */
/* ------------------------------------------------------------------ */
static uint8_t have_loc = 0;
static int32_t loc_x_dm = 0;
static int32_t loc_y_dm = 0;
static int32_t prev_x_dm = 0;
static int32_t prev_y_dm = 0;
static uint32_t last_ts_ms = 0;
static uint16_t v_dmps = 0;
static uint16_t v_prev_dmps = 0;
static uint8_t ef_active = 0;
static uint32_t ef_self_expire_ms = 0;
static uint32_t ef_self_last_tx_ms = 0;

/* ------------------------------------------------------------------ */
/*  uart1_puts_ln()                                                    */
/*  What: Send a command line to the Cooja script via UART1.           */
/* ------------------------------------------------------------------ */
static void
uart1_puts_ln(const char *s)
{
  if(!s) return;
  while(*s) uart1_writeb((uint8_t)*s++);
  uart1_writeb('\n');
}

/* ------------------------------------------------------------------ */
/*  ihyp_dm()                                                          */
/*  What: Cheap hypotenuse approximation in decimeters.                */
/* ------------------------------------------------------------------ */
static uint32_t
ihyp_dm(uint32_t adx, uint32_t ady)
{
  return adx + (ady >> 1);
}

static uint8_t
seen_before(uint16_t origin, uint16_t msg_id, uint8_t type)
{
  for(uint8_t i = 0; i < seen_count; i++) {
    if(seen_msg[i].origin == origin &&
      seen_msg[i].msg_id == msg_id &&
      seen_msg[i].type == type) {
      return 1;  // already seen
    }
  }
  return 0;
}

static void
mark_seen(uint16_t origin, uint16_t msg_id, uint8_t type)
{
  if(seen_count < SEEN_MAX) {
    seen_msg[seen_count].origin = origin;
    seen_msg[seen_count].msg_id = msg_id;
	seen_msg[seen_count].type = type;
    seen_count++;
  } else {
    // simple overwrite oldest (ring-ish)
    for(uint8_t i = 1; i < SEEN_MAX; i++) {
      seen_msg[i-1] = seen_msg[i];
    }
    seen_msg[SEEN_MAX-1].origin = origin;
    seen_msg[SEEN_MAX-1].msg_id = msg_id;
	seen_msg[SEEN_MAX-1].type = type;
  }
}


/* ------------------------------------------------------------------ */
/*  send_emg_update()                                                  */
/*  What: Emit one EMG update packet and log CREATE/TX when requested. */
/* ------------------------------------------------------------------ */
static void
send_emg_update(uint8_t code, uint8_t is_create, uint32_t ts_ms)
{
  udp_msg_t msg;
  msg.origin = (uint16_t)node_id8;
  msg.msg_id = local_msg_counter++;
  msg.type = 1;
  msg.emg_code = code;
  msg.hop = 0;
  msg.ts = ts_ms;

#if FLUD_METRIC_LOG_ENABLED
  if(is_create) {
    metrics_agg_event(METR_EV_EMG_CREATE,
                      (uint32_t)msg.origin,
                      (uint32_t)msg.msg_id,
                      metrics_agg_now_ms(),
                      (uint16_t)code);
  }
  metrics_agg_event(METR_EV_TX_EMG, 0, 0, 0, 0);
#endif

  simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &mcast_addr);
}

/* ------------------------------------------------------------------ */
/* skip_spaces()                                                      */
/* What: Advance pointer over ASCII spaces.                           */
/* How: Simple loop over ' ' and '\t'.                                */
/* Creates: No new variables beyond local pointer.                    */
/* ------------------------------------------------------------------ */
static const char *
skip_spaces(const char *p)
{
    while(p && (*p == ' ' || *p == '\t')) {
        p++;
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* parse_u32_dec()                                                    */
/* What: Parse an unsigned 32-bit decimal integer.                    */
/* How: Manual digit loop; stops at first non-digit.                  */
/* Creates: out (result), endp (next char position).                  */
/* Returns: 1 on success, 0 on failure (no digits).                   */
/* ------------------------------------------------------------------ */
static uint8_t
parse_u32_dec(const char *p, uint32_t *out, const char **endp)
{
    uint32_t v = 0;
    uint8_t any = 0;

    p = skip_spaces(p);
    while(*p >= '0' && *p <= '9') {
        any = 1;
        v = (uint32_t)(v * 10u + (uint32_t)(*p - '0'));
        p++;
    }

    if(!any) {
        return 0;
    }

    if(out) {
        *out = v;
    }
    if(endp) {
        *endp = p;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* parse_i32_dec()                                                    */
/* What: Parse a signed 32-bit decimal integer.                       */
/* How: Optional '-' then parse_u32_dec; applies sign.                */
/* Creates: out (result), endp (next char position).                  */
/* Returns: 1 on success, 0 on failure.                               */
/* ------------------------------------------------------------------ */
static uint8_t
parse_i32_dec(const char *p, int32_t *out, const char **endp)
{
    uint8_t neg = 0;
    uint32_t u = 0;

    p = skip_spaces(p);
    if(*p == '-') {
        neg = 1;
        p++;
    }

    if(!parse_u32_dec(p, &u, &p)) {
        return 0;
    }

    if(out) {
        *out = neg ? -(int32_t)u : (int32_t)u;
    }
    if(endp) {
        *endp = p;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/*  parse_loc_line()                                                   */
/*  What: Parse "LOC id x y ts"; update speed and detect harsh braking.*/
/*  Notes: Trigger + TTL + rate limit, matching DUMB emergency model.  */
/* ------------------------------------------------------------------ */
static void
parse_loc_line(const char *line)
{
    const char *p;
    int32_t xdm, ydm;
    uint32_t ts;
    uint32_t id_u32;

    if(!line) return;
    if(strncmp(line, "LOC", 3) != 0) return;

    p = line + 3;

    if(!parse_u32_dec(p, &id_u32, &p)) return;
    if(!parse_i32_dec(p, &xdm, &p)) return;
    if(!parse_i32_dec(p, &ydm, &p)) return;
    if(!parse_u32_dec(p, &ts, &p)) return;

    (void)id_u32;

  prev_x_dm = loc_x_dm;
  prev_y_dm = loc_y_dm;
  loc_x_dm = xdm;
  loc_y_dm = ydm;

  if(!have_loc) {
    have_loc = 1;
    last_ts_ms = ts;
    return;
  }

  uint32_t dt_ms = (ts >= last_ts_ms) ? (ts - last_ts_ms) : 0;
  if(dt_ms == 0) dt_ms = 1;
  last_ts_ms = ts;

  int32_t dx = loc_x_dm - prev_x_dm;
  int32_t dy = loc_y_dm - prev_y_dm;
  uint32_t adx = (dx >= 0) ? (uint32_t)dx : (uint32_t)(-dx);
  uint32_t ady = (dy >= 0) ? (uint32_t)dy : (uint32_t)(-dy);
  uint32_t dist_dm = ihyp_dm(adx, ady);

  v_prev_dmps = v_dmps;
  v_dmps = (uint16_t)((dist_dm * 1000u) / dt_ms);

  int32_t dv = (int32_t)v_dmps - (int32_t)v_prev_dmps;
  int32_t a_dmps2 = (dv * 1000) / (int32_t)dt_ms;

  uint8_t trigger = (a_dmps2 <= -(int32_t)HB_THR_DMPS2) ? 1 : 0;

  if(trigger) {
    ef_self_expire_ms = (uint32_t)ts + EF_TTL_MS;

    if(!ef_active ||
       ((uint32_t)ts - ef_self_last_tx_ms) >= EF_TX_MIN_MS) {

      uint8_t is_create = ef_active ? 0 : 1;
      send_emg_update(1u, is_create, (uint32_t)ts);
      ef_self_last_tx_ms = (uint32_t)ts;
      ef_active = 1;
    }
  }

  if(ef_active && (uint32_t)ts >= ef_self_expire_ms) {
    ef_active = 0;
  }
}

PROCESS(udp_mobile_process, "UDP Mobile Node");
AUTOSTART_PROCESSES(&udp_mobile_process);

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{

  (void)c; (void)sender_addr; (void)sender_port;
  (void)receiver_addr; (void)receiver_port;

  if(datalen != sizeof(udp_msg_t)) return;

  udp_msg_t rx;
  memcpy(&rx, data, sizeof(rx));
  /* Ignore transmissions with maximum allowed hops */
  if(rx.hop >= MAX_HOP) return;
  
  /* Check if message was seen before and ignore it if it's been seen too often */
  
  if(seen_before(rx.origin, rx.msg_id, rx.type)) {
    return;   // drop duplicate
  }
  mark_seen(rx.origin, rx.msg_id, rx.type);
  
  /* Ignore own transmissions reflected back */
  if(rx.origin == node_id8) return;

#if FLUD_METRIC_LOG_ENABLED
  if(rx.type == 1) {
    metrics_agg_event(METR_EV_EMG_RX,
                      (uint32_t)rx.origin,
                      (uint32_t)rx.msg_id,
                      metrics_agg_now_ms(),
                      (uint16_t)rx.emg_code);
  }
#endif

  /* Relay: increment hop and rebroadcast */
  rx.hop += 1;

#if FLUD_METRIC_LOG_ENABLED
    /* Count each actual radio packet send at the app layer */

  if(rx.type == 1) {
    metrics_agg_event(METR_EV_EMG_FWD,
                     (uint32_t)rx.origin,
                     (uint32_t)rx.msg_id,
                     metrics_agg_now_ms(),
                     (uint16_t)rx.emg_code);
    metrics_agg_event(METR_EV_TX_EMG, 0, 0, 0, 0);
  } else {
    metrics_agg_event(METR_EV_TX_DATA, 0, 0, 0, 0);
 }
#endif // FLUD_METRIC_LOG_ENABLED

  simple_udp_sendto(&udp_conn, &rx, sizeof(rx), &mcast_addr);
}

PROCESS_THREAD(udp_mobile_process, ev, data)
{
  static struct etimer send_timer;
#if FLUD_METRIC_LOG_ENABLED
  static struct etimer poll_timer;
#endif // FLUD_METRIC_LOG_ENABLED
  static struct etimer loc_timer;
  
  PROCESS_BEGIN();

  node_id8  = linkaddr_node_addr.u8[7];
  
  serial_line_init();
  uart1_set_input(serial_line_input_byte);
  simple_udp_register(&udp_conn, UDP_PORT,
                      NULL, UDP_PORT,
                      udp_rx_callback);

  uip_create_linklocal_allnodes_mcast(&mcast_addr);

  etimer_set(&send_timer, SEND_INTERVAL);

#if FLUD_METRIC_LOG_ENABLED
    /* Poll metrics periodically so METR MBL TX lines appear */
    etimer_set(&poll_timer, CLOCK_SECOND);
#endif
  etimer_set(&loc_timer, CLOCK_SECOND);

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == serial_line_event_message && data != NULL) {
      parse_loc_line((const char *)data);
    }

#if FLUD_METRIC_LOG_ENABLED
        if(ev == PROCESS_EVENT_TIMER && data == &poll_timer) {
            metrics_agg_poll(metrics_agg_now_ms(),
                             (uint16_t)node_id8,
                             0);
            etimer_reset(&poll_timer);
        }
#endif

if(ev == PROCESS_EVENT_TIMER && data == &loc_timer) {
    uart1_puts_ln("REQ_LOC");
    etimer_reset(&loc_timer);
}

if(ev == PROCESS_EVENT_TIMER && data == &send_timer) {
    udp_msg_t msg;
    msg.origin = node_id8;
    msg.msg_id = local_msg_counter++;
	msg.type = 0;
    msg.emg_code = 0;

    msg.hop    = 0;
    msg.ts     = (clock_time() * 1000UL / CLOCK_SECOND);

#if FLUD_METRIC_LOG_ENABLED
            {
                metrics_agg_event(METR_EV_MBL_CREATE,
                                  (uint32_t)msg.origin,
                                  (uint32_t)msg.msg_id,
                                  msg.ts,
                                  0);

                metrics_agg_event(METR_EV_TX_DATA, 0, 0, 0, 0);
            }
#endif // FLUD_METRIC_LOG_ENABLED
 
    simple_udp_sendto(&udp_conn, &msg, sizeof(msg), &mcast_addr);
    etimer_reset(&send_timer);
    }
  }

  PROCESS_END();
}
