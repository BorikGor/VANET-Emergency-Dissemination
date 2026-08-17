#ifndef BGKP_PORT_H_
#define BGKP_PORT_H_

#include "contiki.h"
#include <stddef.h>
#include <stdint.h>

/*
 * bgkp_port_line_handler_t
 * What:
 *     Callback type for complete received UART lines.
 *
 * Methods:
 *     The port layer assembles characters into a zero-terminated line
 *     and passes the resulting pointer to this callback.
 *
 * Notes:
 *     The callback is executed from process context, not from UART
 *     callback context.
 */
typedef void (*bgkp_port_line_handler_t)(const char *line);

/*
 * bgkp_port_init
 * What:
 *     Initialize the BGKP port layer.
 *
 * Methods:
 *     - Bind a Contiki process that will receive poll requests.
 *     - Enable UART RX through a non-NULL UART callback.
 *     - Reset internal RX buffers and command line state.
 *     - Start heartbeat with default configuration.
 *
 * Creates:
 *     Internal ring buffer state, heartbeat timer and parser state.
 */
void bgkp_port_init(struct process *owner_process,
                    bgkp_port_line_handler_t line_handler);

/*
 * bgkp_port_handle_event
 * What:
 *     Handle Contiki events owned by the port layer.
 *
 * Methods:
 *     - Drain UART RX bytes on PROCESS_EVENT_POLL.
 *     - Process heartbeat timer events.
 *
 * Returns:
 *     1 if the event was consumed by the port layer.
 *     0 if the event does not belong to the port layer.
 */
int bgkp_port_handle_event(process_event_t ev, process_data_t data);

/*
 * bgkp_port_uart_puts
 * What:
 *     Send a zero-terminated ASCII string over UART.
 *
 * Methods:
 *     Uses the UART TX driver from process context.
 */
void bgkp_port_uart_puts(const char *s);

/*
 * bgkp_port_uart_write
 * What:
 *     Send a binary buffer over UART.
 *
 * Methods:
 *     Uses the UART TX driver from process context.
 */
void bgkp_port_uart_write(const void *buf, size_t len);

/*
 * bgkp_port_local_echo_enable
 * What:
 *     Enable or disable local UART echo for received characters.
 *
 * Methods:
 *     When enabled, every received character is echoed back while the
 *     line is being assembled.
 */
void bgkp_port_local_echo_enable(uint8_t en);

/*
 * bgkp_port_led_red_set
 * What:
 *     Turn the red LED on or off.
 */
void bgkp_port_led_red_set(uint8_t on);

/*
 * bgkp_port_led_green_set
 * What:
 *     Turn the green LED on or off.
 */
void bgkp_port_led_green_set(uint8_t on);

/*
 * bgkp_port_led_red_toggle
 * What:
 *     Toggle the red LED state.
 */
void bgkp_port_led_red_toggle(void);

/*
 * bgkp_port_led_green_toggle
 * What:
 *     Toggle the green LED state.
 */
void bgkp_port_led_green_toggle(void);

/*
 * bgkp_port_heartbeat_enable
 * What:
 *     Enable or disable the port-layer heartbeat.
 *
 * Methods:
 *     The port layer toggles the green LED periodically when the
 *     heartbeat is enabled.
 */
void bgkp_port_heartbeat_enable(uint8_t en);

/*
 * bgkp_port_heartbeat_is_enabled
 * What:
 *     Return whether heartbeat is currently enabled.
 */
uint8_t bgkp_port_heartbeat_is_enabled(void);

/*
 * bgkp_port_heartbeat_set_period_ms
 * What:
 *     Set heartbeat period in milliseconds.
 *
 * Methods:
 *     Values <= 0 are ignored by the caller and should be validated
 *     before calling this function.
 */
void bgkp_port_heartbeat_set_period_ms(unsigned long period_ms);

/*
 * bgkp_port_heartbeat_get_period_ms
 * What:
 *     Get heartbeat period in milliseconds.
 */
unsigned long bgkp_port_heartbeat_get_period_ms(void);

/*
 * bgkp_port_id_ptr
 * What:
 *     Return a pointer to the device ID bytes.
 *
 * Methods:
 *     Uses linkaddr_node_addr as the unique EUI-64 style device ID.
 *
 * Notes:
 *     The returned pointer remains valid for the whole program life.
 */
const uint8_t *bgkp_port_id_ptr(void);

/*
 * bgkp_port_id_len
 * What:
 *     Return the number of bytes in the device ID.
 */
uint8_t bgkp_port_id_len(void);

/*
 * bgkp_port_print_id
 * What:
 *     Print the device ID as uppercase hexadecimal over UART.
 */
void bgkp_port_print_id(void);

/*
 * bgkp_port_reboot
 * What:
 *     Reboot the MCU.
 *
 * Methods:
 *     Performs a full system reset through TI driverlib.
 */
void bgkp_port_reboot(void);

#endif /* BGKP_PORT_H_ */
