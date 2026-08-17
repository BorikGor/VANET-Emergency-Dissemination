/* udp-rsu.c */

#include "contiki.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/simple-udp.h"
#include <stdio.h>
#include <string.h>

/* Enable/disable flooding metrics/logging */
#define FLUD_METRIC_LOG_ENABLED 1

#if FLUD_METRIC_LOG_ENABLED
#include "metrics_agg.h"
#endif // FLUD_METRIC_LOG_ENABLED

#define UDP_PORT 1234

typedef struct {
  uint16_t origin;
  uint16_t msg_id;
  uint8_t type;      /* 0=DATA, 1=EMG */
  uint8_t emg_code;  /* valid when type=EMG */
  uint8_t  hop;
  uint32_t ts;
} udp_msg_t;

static struct simple_udp_connection udp_conn;

PROCESS(udp_rsu_process, "UDP RSU");
AUTOSTART_PROCESSES(&udp_rsu_process);

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

  if(datalen == sizeof(udp_msg_t)) {
    udp_msg_t rx;
    memcpy(&rx, data, sizeof(rx));

#if FLUD_METRIC_LOG_ENABLED
    metrics_agg_event(METR_EV_RSU_RX,
                      (uint32_t)rx.origin,
                      (uint32_t)rx.msg_id,
                      metrics_agg_now_ms(),
                      0);
#endif // FLUD_METRIC_LOG_ENABLED

  }
}

PROCESS_THREAD(udp_rsu_process, ev, data)
{

#if FLUD_METRIC_LOG_ENABLED
  static struct etimer poll_timer;
#endif // FLUD_METRIC_LOG_ENABLED

  PROCESS_BEGIN();

  simple_udp_register(&udp_conn, UDP_PORT,
                      NULL, UDP_PORT,
                      udp_rx_callback);

#if FLUD_METRIC_LOG_ENABLED
    /* Poll metrics periodically so METR RSU TX lines appear (tot=0) */
    etimer_set(&poll_timer, CLOCK_SECOND);
#endif // FLUD_METRIC_LOG_ENABLED
  while(1) {
	  
    PROCESS_WAIT_EVENT();

#if FLUD_METRIC_LOG_ENABLED
    if(ev == PROCESS_EVENT_TIMER && data == &poll_timer) {
      metrics_agg_poll(metrics_agg_now_ms(),
                       (uint16_t)linkaddr_node_addr.u8[7],
                       1);
      etimer_reset(&poll_timer);
    }
#endif // FLUD_METRIC_LOG_ENABLED
  }
  PROCESS_END();
}
