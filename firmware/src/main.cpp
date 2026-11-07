// Standalone contact scanner. No KEY1 output, HID, or media-command mapping.
#include "usb_link.h"
#include <stdio.h>
#include <string.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/iwdg.h>
#include <libopencm3/cm3/systick.h>

static volatile uint32_t ticks;
static constexpr uint16_t pins[] = {GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9};
static uint16_t rawMask;
static bool reporting = true;
static char command[64];
static unsigned used;
static bool overflow;
extern "C" void sys_tick_handler(void) { ++ticks; }

static uint16_t scan() {
    uint8_t directed[6]{};
    for (unsigned drive = 0; drive < 6; ++drive) {
        gpio_clear(GPIOB, pins[drive]);
        gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, pins[drive]);
        for (volatile unsigned settle = 0; settle < 120; ++settle) __asm__("nop");
        for (unsigned sense = 0; sense < 6; ++sense)
            if (sense != drive && !gpio_get(GPIOB, pins[sense])) directed[drive] |= 1u << sense;
        gpio_set(GPIOB, pins[drive]);
        gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, pins[drive]);
    }
    uint16_t result = 0;
    unsigned bit = 0;
    for (unsigned a = 0; a < 6; ++a)
        for (unsigned b = a + 1; b < 6; ++b, ++bit) {
            const bool ab = directed[a] & (1u << b), ba = directed[b] & (1u << a);
            if (ab != ba) return 0xFFFF;
            if (ab) result |= 1u << bit;
        }
    return result;
}
static void handle() {
    char line[96];
    if (!strcmp(command, "PING")) usb_link_send("PONG RENAULT/1\r\n");
    else if (!strcmp(command, "STATE")) {
        snprintf(line, sizeof(line), "STATE mode=scanner raw=%04X txlost=%lu ms=%lu\r\n",
                 rawMask, (unsigned long)usb_link_dropped(), (unsigned long)ticks);
        usb_link_send(line);
    } else if (!strcmp(command, "RAW ON")) { reporting = true; usb_link_send("OK RAW ON\r\n"); }
    else if (!strcmp(command, "RAW OFF")) { reporting = false; usb_link_send("OK RAW OFF\r\n"); }
    else if (!strcmp(command, "HELP")) usb_link_send("PING STATE RAW ON|OFF HELP\r\n");
    else usb_link_send("ERR COMMAND\r\n");
}
extern "C" void app_receive(char ch) {
    if (ch == '\r') return;
    if (ch == '\n') {
        if (!overflow) { command[used] = 0; handle(); }
        used = 0; overflow = false;
    } else if (!overflow) {
        if (used + 1 < sizeof(command)) command[used++] = ch;
        else overflow = true;
    }
}
extern "C" void app_usb_reset(void) { used = 0; overflow = false; }
int main() {
    rcc_clock_setup_pll(&rcc_hse_configs[RCC_CLOCK_HSE8_72MHZ]);
    rcc_periph_clock_enable(RCC_GPIOA); rcc_periph_clock_enable(RCC_GPIOB);
    rcc_periph_clock_enable(RCC_AFIO);
    gpio_primary_remap(AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON, 0);
    for (auto pin : pins) {
        gpio_set(GPIOB, pin);
        gpio_set_mode(GPIOB, GPIO_MODE_INPUT, GPIO_CNF_INPUT_PULL_UPDOWN, pin);
    }
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(72000 - 1); systick_interrupt_enable(); systick_counter_enable();
    gpio_clear(GPIOA, GPIO12);
    gpio_set_mode(GPIOA, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_OPENDRAIN, GPIO12);
    const uint32_t start = ticks; while (uint32_t(ticks - start) < 30) {}
    gpio_set_mode(GPIOA, GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, GPIO11 | GPIO12);
    usb_link_init(); iwdg_set_period_ms(250); iwdg_start();
    uint32_t last = ticks, since = ticks;
    uint16_t candidate = 0xFFFF, reported = 0xFFFF;
    while (true) {
        usb_link_poll();
        const uint32_t now = ticks;
        if (uint32_t(now - last) >= 1) {
            last = now; rawMask = scan();
            if (rawMask != candidate) { candidate = rawMask; since = now; }
            if (reporting && candidate != reported && uint32_t(now - since) >= 20) {
                char line[24]; snprintf(line, sizeof(line), "RAW %04X\r\n", candidate);
                if (usb_link_send(line)) reported = candidate;
            }
        }
        iwdg_reset();
    }
}
