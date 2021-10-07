/*
 * Copyright (c) 2020 Raspmelon Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "pico.h"
#include "pico/error.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/multicore.h"
#include "pico/sync.h"
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/vreg.h"
#include "hardware/interp.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

#include "bsp/board.h"
#include "tusb.h"
#include "hid/game_input.h"

#define VGA_MODE vga_mode_640x480_60

#include "sprite.h"
#include "sprite_dma.h"
#include "physics/aabb.h"
#include "sprite/scanline_rendering.h"

#include "content/Fonts/SaikyoBlack.h"

#include "hardware/structs/vreg_and_chip_reset.h"

#define LED_PIN 25

static uint64_t render_micros_core0 = 0;
static uint64_t render_micros_core1 = 0;

#define DEBUG_STR_MAX_LEN 150
static int debug_str_length = 0;
static char debug_str[DEBUG_STR_MAX_LEN];

void __time_critical_func(render_scanline)(struct scanvideo_scanline_buffer *dest, int *dma_channels, size_t dma_channels_count)
{
    uint64_t start_us = time_us_64();

    uint16_t l = scanvideo_scanline_number(dest->scanline_id);
    uint16_t *colour_buf = raw_scanline_prepare(dest, VGA_MODE.width);

    // Begin DMA fill of the background.
    const uint16_t bgcol = PICO_SCANVIDEO_PIXEL_FROM_RGB8(0x40, 0xc0, 0xff);
    sprite_fill16_dma(colour_buf, bgcol, 0, VGA_MODE.width, dma_channels[0]);

    // todo: draw stuff

    // Draw debug string
    #ifdef DEBUG
        if (debug_str_length > 0) {
            dma_channel_wait_for_finish_blocking(dma_channels[0]);
            sprite_string_dma(colour_buf, 10, 10, &debug_str[0], debug_str_length, &SaikyoBlack, l, VGA_MODE.width, dma_channels, dma_channels_count);
            wait_for_dmas(dma_channels, dma_channels_count);
        }
    #endif

    // Wait for all DMA jobs to complete
    wait_for_dmas(dma_channels, dma_channels_count);

    // Update counters of total rendering time
    uint64_t elapsed_us = time_us_64() - start_us;
    if (get_core_num() == 0) {
        render_micros_core0 += elapsed_us;
    } else {
        render_micros_core1 += elapsed_us;
    }

    // Draw a bar indicating rendering cost (1 pixel per microsecond)
    #ifdef DEBUG
        const uint16_t cost_col = PICO_SCANVIDEO_PIXEL_FROM_RGB8(0xDD, 0x10, 0x10);
        sprite_fill16_dma(colour_buf, cost_col, 0, (uint)elapsed_us, dma_channels[0]);
        dma_channel_wait_for_finish_blocking(dma_channels[0]);
    #endif

    raw_scanline_finish(dest);
}

void __time_critical_func(async_update_logic)(uint32_t frame_number)
{
    tuh_task();
    hid_task();
}

static mutex_t hid_input_lock;
static bool hid_jump_pressed = false;
static bool hid_left_pressed = false;
static bool hid_right_pressed = false;
static bool hid_cheat_level_up = false;
void process_kbd_report(hid_keyboard_report_t const *p_new_report)
{
    mutex_enter_blocking(&hid_input_lock);
    hid_jump_pressed = false;
    hid_left_pressed = false;
    hid_right_pressed = false;

    for (size_t i = 0; i < 6; i++) {
        switch (p_new_report->keycode[i]) {
            case HID_KEY_ARROW_UP:
            case HID_KEY_W:
                hid_jump_pressed = true;
                break;
            case HID_KEY_ARROW_LEFT:
            case HID_KEY_A:
                hid_left_pressed = true;
                break;
            case HID_KEY_ARROW_RIGHT:
            case HID_KEY_D:
                hid_right_pressed = true;
                break;
            case HID_KEY_F11:
                hid_cheat_level_up = true;
                break;
        }
    }
    mutex_exit(&hid_input_lock);
}

void __time_critical_func(frame_update_logic)(uint32_t frame_number)
{
    // Toggle LED
    gpio_put(LED_PIN, (frame_number & 8) == 8);

    // Read player input from keyboard
    mutex_enter_blocking(&hid_input_lock);
    //todo: read HID
    mutex_exit(&hid_input_lock);

    uint64_t render_micros = render_micros_core0 + render_micros_core1;
    float balance = (float)render_micros_core0 / (float)render_micros;
    render_micros_core0 = 0;
    render_micros_core1 = 0;

#if DEBUG
    debug_str_length = snprintf(
        debug_str,
        DEBUG_STR_MAX_LEN,
        "RUS:%u RBAL:%.2f",
        (unsigned int)render_micros,
        balance
    );
#endif
}

int main(void)
{
    // Set PSU into PWM mode for reduced ripple (and reduced efficiency)
    gpio_set_dir(23, true);
    gpio_pull_up(23);

    // Setup LED to blink
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // Overclock
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(250000, true);

    // Setup serial communication
    stdio_init_all();

    // Initialise USB system
    board_init();
    tusb_init();

    // Load a level
    //todo: initialise city
    
    // Initialise video system
    mutex_init(&hid_input_lock);
    init_scanline_rendering(&VGA_MODE);

    // Enter the infinite video loop
    render_loop();
}

