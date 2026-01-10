/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "fsl_common.h"
#include "TimersManager.h"
#include "fsl_gpio.h"
#include "fsl_gint.h"
#include "fsl_iocon.h"
#include "app.h"
#include "board.h"
#include "pin_mux.h"
#include "zb_platform.h"

/* Number of buttons on DK6 board */
#define BOARD_BUTTONS_NUM (1)

static tmrTimerID_t board_button_timer_handle;

static uint8_t board_button_bounce[BOARD_BUTTONS_NUM];
static bool board_button_state[BOARD_BUTTONS_NUM];
static const uint8_t board_button_pins[BOARD_BUTTONS_NUM] = {APP_BOARD_SW0_PIN};

#define APP_BUTTONS_DIO_MASK (1 << APP_BOARD_SW0_PIN)
#define DEMO_GINT0_POL_MASK (0)
#define DEMO_GINT0_ENA_MASK ((APP_BUTTONS_DIO_MASK)&0x3FFFFF)

button_cb app_cb;
static uint8_t app_num_buttons;

static uint32_t read_buttons_pins(void)
{
    uint32_t pin_state_mask = -0xffffffff;
#if (!defined DONGLE)
    {
        uint32_t raw_pin_state_mask;

        /* Read all GPIOs */
        raw_pin_state_mask = GPIO_PortRead(GPIO, APP_BOARD_GPIO_PORT);

        /* Apply mask and inversions */
        pin_state_mask = (raw_pin_state_mask & APP_BUTTONS_DIO_MASK);
    }
#endif

    return pin_state_mask;
}

static void timer_button_scan(void *pvParam)
{
    /*
     * The DIO changed status register is reset here before the scan is performed.
     * This avoids a race condition between finishing a scan and re-enabling the
     * DIO to interrupt on a falling edge.
     */

    uint8_t button_state_mask = 0xff;
    uint8_t i;
    uint32_t buttons_pins_state_mask = read_buttons_pins();

    for (i = 0; i < app_num_buttons; i++)
    {
        uint8_t u8Button = (uint8_t)((buttons_pins_state_mask >> board_button_pins[i]) & 1);

        board_button_bounce[i] <<= 1;
        board_button_bounce[i] |= u8Button;
        button_state_mask &= board_button_bounce[i];

        if (0 == board_button_bounce[i] && !board_button_state[i])
        {
            board_button_state[i] = true;
            /* Call user to inform button has been pressed */
            app_cb(i);
        }
        else if (0xff == board_button_bounce[i] && board_button_state[i] != false)
        {
            board_button_state[i] = false;
        }
    }

    if (0xff == button_state_mask)
    {
        /*
         * all buttons high so set dio to interrupt on change
         */
        GINT_EnableCallback(GINT0);
        TMR_StopTimer(board_button_timer_handle);
    }
    else
    {
        /*
         * one or more buttons is still depressed so continue scanning
         */
        TMR_StartTimer(board_button_timer_handle, gTmrSingleShotTimer_c, 10, timer_button_scan, NULL);
    }
}

static void gint_callback(void)
{
    /* Take action for gint event */;
    uint32_t io_status = read_buttons_pins();

    if (io_status != APP_BUTTONS_DIO_MASK)
    {
        /* disable edge detection until scan complete */
        GINT_DisableCallback(GINT0);
        /* Run timer */
        TMR_StartTimer(board_button_timer_handle, gTmrSingleShotTimer_c, 10, timer_button_scan, NULL);
    }
}

bool zbPlatButtonInit(uint8_t num_buttons, button_cb cb)
{
    gpio_pin_config_t switch_config = {
        kGPIO_DigitalInput,
    };

    if (NULL == cb || num_buttons > BOARD_BUTTONS_NUM)
    {
        return false;
    }

    app_cb          = cb;
    app_num_buttons = num_buttons;

    /* Initialise arrays */
    memset(board_button_bounce, 0xFF, sizeof(board_button_bounce));
    memset(board_button_state, false, sizeof(board_button_state));

    /* Loop through buttons */
    for (uint8_t i = 0; i < num_buttons; i++)
    {
        /* Configure io mux for pull up operation */
        IOCON_PinMuxSet(IOCON, 0, board_button_pins[i], IOCON_FUNC0 | IOCON_MODE_PULLUP | IOCON_DIGITAL_EN);

        /* Initialise GPIO use */
        GPIO_PinInit(GPIO, APP_BOARD_GPIO_PORT, board_button_pins[i], &switch_config);
    }

    /* Initialize GINT0 */
    GINT_Init(GINT0);
    /* Setup GINT0 for edge trigger, "OR" mode */
    GINT_SetCtrl(GINT0, kGINT_CombineOr, kGINT_TrigEdge, gint_callback);
    /* Select pins & polarity for GINT0 */
    GINT_ConfigPins(GINT0, DEMO_GINT0_PORT, DEMO_GINT0_POL_MASK, DEMO_GINT0_ENA_MASK);
    /* Enable callback(s) */
    GINT_EnableCallback(GINT0);

    /* Lower the priority of GINT interrupt as this is calling Free RTOS API */
    NVIC_SetPriority(GINT0_IRQn, 0x80 >> (8 - __NVIC_PRIO_BITS));

    board_button_timer_handle = TMR_AllocateTimer();

    /* If we came out of deep sleep; perform appropriate action as well based
       on button press.*/
    timer_button_scan(NULL);

    uint32_t u32Buttons = read_buttons_pins();

    if (u32Buttons != APP_BUTTONS_DIO_MASK)
    {
        return true;
    }
    return false;
}

uint32_t zbPlatButtonGetState(void)
{
    uint32_t result                  = 0;
    uint32_t buttons_pins_state_mask = read_buttons_pins();

    for (uint8_t i = 0; i < app_num_buttons; i++)
    {
        uint8_t button_state = (uint8_t)((buttons_pins_state_mask >> board_button_pins[i]) & 1);

        /* Fill state mask with button states */
        result |= ((!button_state) << i);
    }

    return result;
}
