#include "bgkp_port.h"

#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/uart0-arch.h"
#include "net/linkaddr.h"

#include <stdio.h>
#include <string.h>

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)

/*
 * Configuration constants.
 *
 * BGKP_PORT_UART_RX_BUF_SIZE:
 *     Ring buffer size for received UART bytes.
 *
 * BGKP_PORT_LINE_BUF_SIZE:
 *     Maximum accepted line length including final '\0'.
 *
 * BGKP_PORT_HEARTBEAT_DEFAULT_MS:
 *     Default green LED heartbeat period.
 */
#define BGKP_PORT_UART_RX_BUF_SIZE      128
#define BGKP_PORT_LINE_BUF_SIZE         96
#define BGKP_PORT_HEARTBEAT_DEFAULT_MS  1000UL

/*
 * Shared RX ring buffer state.
 *
 * These fields are shared between UART callback context and process
 * context, so they are declared volatile.
 */
static volatile uint8_t rx_buf[BGKP_PORT_UART_RX_BUF_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

/*
 * line_buf and line_len store the currently assembled command line.
 */
static char line_buf[BGKP_PORT_LINE_BUF_SIZE];
static uint8_t line_len;

/*
 * last_was_cr tracks CRLF handling.
 *
 * If a terminal sends '\r' followed by '\n', the second character
 * should not produce an extra empty line.
 */
static uint8_t last_was_cr;

/*
 * owner points to the process that will receive process_poll()
 * notifications from the UART RX callback.
 */
static struct process *owner;

/*
 * line_handler is called whenever a full zero-terminated line is ready.
 */
static bgkp_port_line_handler_t line_handler;

/*
 * Heartbeat state.
 */
static struct etimer heartbeat_timer;
static uint8_t heartbeat_enabled;
static unsigned long heartbeat_period_ms;

/*
 * local_echo controls whether received bytes are echoed back to UART.
 */
static uint8_t local_echo_enabled;

/*
 * bgkp_port_ms_to_ticks
 * What:
 *     Convert milliseconds to Contiki clock ticks.
 *
 * Methods:
 *     - Uses CLOCK_SECOND for conversion.
 *     - Rounds up to avoid returning 0 for small non-zero values.
 *
 * Creates:
 *     ticks
 *
 * Returns:
 *     Number of clock ticks corresponding to the requested delay.
 */
static clock_time_t
bgkp_port_ms_to_ticks(unsigned long ms)
{
    unsigned long ticks;

    ticks = (ms * CLOCK_SECOND + 999UL) / 1000UL;

    if(ticks == 0UL) {
        ticks = 1UL;
    }

    return (clock_time_t)ticks;
}

/*
 * bgkp_port_start_heartbeat_timer
 * What:
 *     Start or restart the heartbeat timer according to current period.
 *
 * Methods:
 *     Converts milliseconds to Contiki ticks and arms etimer.
 *
 * Creates:
 *     No persistent local variables.
 */
static void
bgkp_port_start_heartbeat_timer(void)
{
    etimer_set(&heartbeat_timer, bgkp_port_ms_to_ticks(
        heartbeat_period_ms));
}

/*
 * bgkp_port_uart_try_dequeue_byte
 * What:
 *     Pop one byte from the RX ring buffer.
 *
 * Methods:
 *     Checks for empty buffer and copies one byte to *out.
 *
 * Creates:
 *     No persistent local variables.
 *
 * Returns:
 *     1 if a byte was dequeued, 0 if the buffer was empty.
 */
static int
bgkp_port_uart_try_dequeue_byte(uint8_t *out)
{
    if(rx_tail == rx_head) {
        return 0;
    }

    *out = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % BGKP_PORT_UART_RX_BUF_SIZE;
    return 1;
}

/*
 * bgkp_port_uart_rx_callback
 * What:
 *     Minimal UART RX callback.
 *
 * Methods:
 *     - Store one byte in the ring buffer if space exists.
 *     - Request a process poll on the bound owner process.
 *     - Avoid all heavy work here.
 *
 * Creates:
 *     next_head
 *
 * Returns:
 *     1 to indicate the byte was handled.
 */
static int
bgkp_port_uart_rx_callback(unsigned char c)
{
    uint8_t next_head;

    next_head = (rx_head + 1) % BGKP_PORT_UART_RX_BUF_SIZE;

    if(next_head != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next_head;
    }

    if(owner != NULL) {
        process_poll(owner);
    }

    return 1;
}

/*
 * bgkp_port_on_complete_line
 * What:
 *     Finalize the current line and dispatch it to the application.
 *
 * Methods:
 *     - Zero-terminate line_buf.
 *     - Invoke line_handler if present.
 *     - Reset line assembly state.
 *
 * Creates:
 *     No persistent local variables.
 */
static void
bgkp_port_on_complete_line(void)
{
    line_buf[line_len] = '\0';

    if(line_handler != NULL) {
        line_handler(line_buf);
    }

    line_len = 0;
}

/*
 * bgkp_port_handle_rx_byte
 * What:
 *     Consume one received byte in process context.
 *
 * Methods:
 *     - Optionally echo character back to UART.
 *     - Treat both CR and LF as end-of-line.
 *     - Avoid duplicate empty lines for CRLF sequences.
 *     - Support backspace processing.
 *     - Assemble a zero-terminated line for higher-level parsing.
 *
 * Creates:
 *     No persistent local variables.
 */
static void
bgkp_port_handle_rx_byte(uint8_t c)
{
    if(local_echo_enabled) {
        uart0_write_byte(c);

        if(c == '\r') {
            uart0_write_byte('\n');
        }
    }

    if(c == '\r') {
        last_was_cr = 1;

        if(line_len > 0) {
            bgkp_port_on_complete_line();
        }

        return;
    }

    if(c == '\n') {
        if(last_was_cr) {
            last_was_cr = 0;
            return;
        }

        if(line_len > 0) {
            bgkp_port_on_complete_line();
        }

        return;
    }

    last_was_cr = 0;

    if(c == '\b' || c == 0x7f) {
        if(line_len > 0) {
            line_len--;
        }
        return;
    }

    if(line_len < (BGKP_PORT_LINE_BUF_SIZE - 1)) {
        line_buf[line_len++] = (char)c;
    } else {
        /*
         * Drop the current line if it grows too large.
         * This keeps the port layer simple and safe.
         */
        line_len = 0;
    }
}

/*
 * bgkp_port_init
 * What:
 *     Initialize the BGKP platform abstraction layer.
 *
 * Methods:
 *     - Bind owner process and line handler.
 *     - Reset all parser and buffer state.
 *     - Enable UART RX using a non-NULL callback.
 *     - Set default heartbeat and local echo configuration.
 *
 * Creates:
 *     Internal port-layer state.
 */
void
bgkp_port_init(struct process *owner_process,
               bgkp_port_line_handler_t cb)
{
    owner = owner_process;
    line_handler = cb;

    rx_head = 0;
    rx_tail = 0;

    line_len = 0;
    last_was_cr = 0;

    heartbeat_enabled = 1;
    heartbeat_period_ms = BGKP_PORT_HEARTBEAT_DEFAULT_MS;

    local_echo_enabled = 1;

    leds_off(LEDS_ALL);

    /*
     * Critical for CC13xx/CC26xx:
     * a non-NULL UART callback enables RX.
     */
    uart0_set_callback(bgkp_port_uart_rx_callback);

    bgkp_port_start_heartbeat_timer();
}

/*
 * bgkp_port_handle_event
 * What:
 *     Handle Contiki events owned by the port layer.
 *
 * Methods:
 *     - On PROCESS_EVENT_POLL, drain the RX ring buffer and assemble
 *       complete input lines.
 *     - On heartbeat timer expiration, toggle the green LED and re-arm
 *       the timer when heartbeat is enabled.
 *
 * Creates:
 *     c
 *
 * Returns:
 *     1 if the event was handled by the port layer, 0 otherwise.
 */
int
bgkp_port_handle_event(process_event_t ev, process_data_t data)
{
    uint8_t c;

    if(ev == PROCESS_EVENT_POLL) {
        while(bgkp_port_uart_try_dequeue_byte(&c)) {
            bgkp_port_handle_rx_byte(c);
        }

        return 1;
    }

    if(ev == PROCESS_EVENT_TIMER && data == &heartbeat_timer) {
        if(heartbeat_enabled) {
            leds_toggle(LEDS_GREEN);
            bgkp_port_start_heartbeat_timer();
        }

        return 1;
    }

    return 0;
}

/*
 * bgkp_port_uart_puts
 * What:
 *     Send a zero-terminated string over UART.
 *
 * Methods:
 *     Writes one byte at a time to remain compatible with all builds.
 *
 * Creates:
 *     No persistent local variables.
 */
void
bgkp_port_uart_puts(const char *s)
{
    if(s == NULL) {
        return;
    }

    while(*s != '\0') {
        uart0_write_byte((uint8_t)*s++);
    }
}

/*
 * bgkp_port_uart_write
 * What:
 *     Send a binary buffer over UART.
 *
 * Methods:
 *     Writes one byte at a time.
 *
 * Creates:
 *     p
 *     i
 */
void
bgkp_port_uart_write(const void *buf, size_t len)
{
    const uint8_t *p;
    size_t i;

    if(buf == NULL || len == 0U) {
        return;
    }

    p = (const uint8_t *)buf;

    for(i = 0; i < len; i++) {
        uart0_write_byte(p[i]);
    }
}

/*
 * bgkp_port_local_echo_enable
 * What:
 *     Enable or disable local terminal echo.
 *
 * Methods:
 *     Stores the requested flag.
 */
void
bgkp_port_local_echo_enable(uint8_t en)
{
    local_echo_enabled = (en != 0U);
}

/*
 * bgkp_port_led_red_set
 * What:
 *     Turn the red LED on or off.
 *
 * Methods:
 *     Uses Contiki LED driver.
 */
void
bgkp_port_led_red_set(uint8_t on)
{
    if(on) {
        leds_on(LEDS_RED);
    } else {
        leds_off(LEDS_RED);
    }
}

/*
 * bgkp_port_led_green_set
 * What:
 *     Turn the green LED on or off.
 *
 * Methods:
 *     Uses Contiki LED driver.
 */
void
bgkp_port_led_green_set(uint8_t on)
{
    if(on) {
        leds_on(LEDS_GREEN);
    } else {
        leds_off(LEDS_GREEN);
    }
}

/*
 * bgkp_port_led_red_toggle
 * What:
 *     Toggle the red LED.
 *
 * Methods:
 *     Uses Contiki LED driver.
 */
void
bgkp_port_led_red_toggle(void)
{
    leds_toggle(LEDS_RED);
}

/*
 * bgkp_port_led_green_toggle
 * What:
 *     Toggle the green LED.
 *
 * Methods:
 *     Uses Contiki LED driver.
 */
void
bgkp_port_led_green_toggle(void)
{
    leds_toggle(LEDS_GREEN);
}

/*
 * bgkp_port_heartbeat_enable
 * What:
 *     Enable or disable the port heartbeat.
 *
 * Methods:
 *     - When enabled, restart the heartbeat timer.
 *     - When disabled, stop the timer and turn off the green LED.
 */
void
bgkp_port_heartbeat_enable(uint8_t en)
{
    heartbeat_enabled = (en != 0U);

    if(heartbeat_enabled) {
        bgkp_port_start_heartbeat_timer();
    } else {
        etimer_stop(&heartbeat_timer);
        leds_off(LEDS_GREEN);
    }
}

/*
 * bgkp_port_heartbeat_is_enabled
 * What:
 *     Return whether heartbeat is enabled.
 *
 * Methods:
 *     Returns current internal heartbeat state.
 */
uint8_t
bgkp_port_heartbeat_is_enabled(void)
{
    return heartbeat_enabled;
}

/*
 * bgkp_port_heartbeat_set_period_ms
 * What:
 *     Set the heartbeat period in milliseconds.
 *
 * Methods:
 *     - Ignore a zero period.
 *     - Restart timer immediately if heartbeat is enabled.
 */
void
bgkp_port_heartbeat_set_period_ms(unsigned long period_ms)
{
    if(period_ms == 0UL) {
        return;
    }

    heartbeat_period_ms = period_ms;

    if(heartbeat_enabled) {
        bgkp_port_start_heartbeat_timer();
    }
}

/*
 * bgkp_port_heartbeat_get_period_ms
 * What:
 *     Return the heartbeat period in milliseconds.
 */
unsigned long
bgkp_port_heartbeat_get_period_ms(void)
{
    return heartbeat_period_ms;
}

/*
 * bgkp_port_id_ptr
 * What:
 *     Return a pointer to the device ID bytes.
 *
 * Methods:
 *     Uses linkaddr_node_addr as the platform's current unique ID.
 *
 * Notes:
 *     On this platform Contiki usually populates linkaddr_node_addr
 *     from factory information stored in flash.
 */
const uint8_t *
bgkp_port_id_ptr(void)
{
    return linkaddr_node_addr.u8;
}

/*
 * bgkp_port_id_len
 * What:
 *     Return the number of bytes in the device ID.
 */
uint8_t
bgkp_port_id_len(void)
{
    return LINKADDR_SIZE;
}

/*
 * bgkp_port_print_id
 * What:
 *     Print the current device ID over UART as uppercase hex.
 *
 * Methods:
 *     Uses linkaddr_node_addr byte array.
 *
 * Creates:
 *     i
 */
void
bgkp_port_print_id(void)
{
    uint8_t i;

    printf("ID: ");

    for(i = 0; i < LINKADDR_SIZE; i++) {
        printf("%02X", linkaddr_node_addr.u8[i]);
    }

    printf("\r\n");
}

/*
 * bgkp_port_reboot
 * What:
 *     Perform a full system reset.
 *
 * Methods:
 *     Uses TI driverlib SysCtrlSystemReset().
 *
 * Notes:
 *     This function should not return.
 */
void
bgkp_port_reboot(void)
{
    SysCtrlSystemReset();

    while(1) {
    }
}

