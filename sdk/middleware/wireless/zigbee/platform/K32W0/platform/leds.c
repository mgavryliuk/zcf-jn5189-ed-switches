/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include "app.h"
#include "board.h"
#include "fsl_gpio.h"
#include "pin_mux.h"

/* Number of leds on DK6 board */
#define BOARD_LEDS_NUM (2)

static uint8_t app_num_leds;
static uint8_t board_led_pins[BOARD_LEDS_NUM] = {
    APP_BASE_BOARD_LED1_PIN,
    APP_BASE_BOARD_LED2_PIN,
};

bool zbPlatLedInit(uint8_t num_leds)
{
    gpio_pin_config_t led_config = {
        kGPIO_DigitalOutput,
        0,
    };

    if (num_leds == 0 || num_leds > BOARD_LEDS_NUM)
    {
        return false;
    }

    app_num_leds = num_leds;

    /* Led PIN MUX is set in BOARD_InitHardware */
    for (uint8_t i = 0; i < app_num_leds; i++)
    {
        GPIO_PinInit(GPIO, APP_BOARD_GPIO_PORT, board_led_pins[i], &led_config);
    }
    return true;
}

void zbPlatLedSetState(uint8_t led, uint8_t state)
{
    if (app_num_leds == 0 || led > app_num_leds)
    {
        return;
    }

    GPIO_PinWrite(GPIO, APP_BOARD_GPIO_PORT, board_led_pins[led], !state);
}
