#include "app_buttons.h"

#include "ZTimer.h"
#include "fsl_gint.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void BUTTONS_ScanCallback(void* pvParam);
static void BUTTONS_GINTCallback(void);
static void BUTTONS_ResetState(ButtonState_t* buttonState);
static void BUTTONS_HandleButtonState(ButtonWithState_t* buttonWithState, uint32_t dioState);
static void BUTTONS_HandleResetState(uint32_t dioState);

static uint8_t u16ButtonIdleCycles = 0;
static ResetButtonWithState_t sResetButtonState;

uint8_t g_u8ButtonScanTimerID;

void BUTTONS_HW_Init(void) {
    BUTTON_DBG("Init hardware\n");
    gpio_pin_config_t button_config = {
        .pinDirection = kGPIO_DigitalInput,
    };
    for (uint8_t i = 0; i < g_numButtonsPins; i++) {
        BUTTON_DBG("Configuring button with pin: %d\n", g_asButtonsPins[i]);
        IOCON_PinMuxSet(IOCON, 0, g_asButtonsPins[i], IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
        GPIO_PinInit(GPIO, 0, g_asButtonsPins[i], &button_config);
    }
    BUTTON_DBG("Init hardware finished!\n");
    BUTTON_DBG("Configuring GINT...\n");
    GINT_Init(GINT0);
    GINT_SetCtrl(GINT0, kGINT_CombineOr, kGINT_TrigEdge, BUTTONS_GINTCallback);
    GINT_ConfigPins(GINT0, kGINT_Port0, 0, g_u32ButtonsInterruptMask);
    GINT_EnableCallback(GINT0);
    BUTTON_DBG("GINT configured!\n");
}

void BUTTONS_SW_Init(void) {
    BUTTON_DBG("Init timers\n");
    ZTIMER_eOpen(&g_u8ButtonScanTimerID, BUTTONS_ScanCallback, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    BUTTON_DBG("Init timers finished!\n");
    for (uint8_t i = 0; i < g_numButtons; i++) {
        g_asButtonsStates[i].pButton = &g_asButtons[i];
        BUTTONS_ResetState(&g_asButtonsStates[i].sButtonState);
    }
    sResetButtonState.pButton = &g_sResetButton;
    BUTTONS_ResetState(&sResetButtonState.sButtonState);
}

static void BUTTONS_ScanCallback(void* pvParam) {
    GINT_DisableCallback(GINT0);
    uint8_t i;
    bool_t bAnyBtnPressed = FALSE;

    uint32_t u32DIOState = GPIO_PortRead(GPIO, 0) & g_u32ButtonsInterruptMask;
    for (i = 0; i < g_numButtons; i++) {
        BUTTONS_HandleButtonState(&g_asButtonsStates[i], u32DIOState);
        bAnyBtnPressed |= g_asButtonsStates[i].sButtonState.bPressed;
    }

    BUTTONS_HandleResetState(u32DIOState);
    bAnyBtnPressed |= sResetButtonState.sButtonState.bPressed;

    if (bAnyBtnPressed) {
        u16ButtonIdleCycles = 0;
    } else {
        u16ButtonIdleCycles++;
    }

    if (u16ButtonIdleCycles == BUTTONS_IDLE_CYCLES_MAX) {
        u16ButtonIdleCycles = 0;
        BUTTON_DBG("IDLE cycles achieved. Stopping scan...\n");
        GINT_EnableCallback(GINT0);
        for (i = 0; i < g_numButtons; i++) {
            BUTTONS_ResetState(&g_asButtonsStates[i].sButtonState);
        }
        ZTIMER_eStop(g_u8ButtonScanTimerID);
    } else {
        ZTIMER_eStop(g_u8ButtonScanTimerID);
        ZTIMER_eStart(g_u8ButtonScanTimerID, BUTTONS_SCAN_TIME_MSEC);
    }
}

static void BUTTONS_GINTCallback(void) {
    BUTTONS_ScanCallback(NULL);
}

static void BUTTONS_ResetState(ButtonState_t* buttonState) {
    buttonState->eClickState = BTN_CLICK_IDLE;
    buttonState->bPressed = FALSE;
    buttonState->u16PressedCycles = 0;
    buttonState->u8Debounce = BUTTONS_DEBOUNCE_MASK;
}

static void BUTTONS_HandleButtonState(ButtonWithState_t* buttonWithState, uint32_t dioState) {
    const Button_t* button = buttonWithState->pButton;
    ButtonState_t* buttonState = &buttonWithState->sButtonState;

    buttonState->u8Debounce <<= 1;
    buttonState->u8Debounce |= ((dioState & button->u32DioMask) ? 1 : 0);
    buttonState->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    switch (buttonState->u8Debounce) {
        case 0:
            if (!buttonState->bPressed) {
                BUTTON_DBG("Button for endpoint `%d` - PRESSED\n", button->u16Endpoint);
                buttonState->bPressed = TRUE;
                if (g_sButtonsCallbacks.pfOnPressCallback && button->pvLedConfig) {
                    g_sButtonsCallbacks.pfOnPressCallback(button->pvLedConfig);
                }
            }
            buttonState->u16PressedCycles++;

            break;
        case BUTTONS_DEBOUNCE_MASK:
            if (buttonState->bPressed) {
                BUTTON_DBG("Button for endpoint `%d` - RELEASED\n", button->u16Endpoint);
                BUTTONS_ResetState(buttonState);
            }
    }
}

static void BUTTONS_HandleResetState(uint32_t dioState) {
    const ResetButton_t* resetButton = sResetButtonState.pButton;
    ButtonState_t* buttonState = &sResetButtonState.sButtonState;

    buttonState->u8Debounce <<= 1;
    buttonState->u8Debounce |= (dioState & resetButton->u32DioMask) ? 1 : 0;
    buttonState->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    switch (buttonState->u8Debounce) {
        case 0:
            if (!buttonState->bPressed) {
                BUTTON_DBG("Reset device combination pressed. Reset mask: %x\n", resetButton->u32DioMask);
                buttonState->bPressed = TRUE;
                if (g_sButtonsCallbacks.pfOnPressCallback && resetButton->pvLedConfig) {
                    g_sButtonsCallbacks.pfOnPressCallback(resetButton->pvLedConfig);
                }
            }

            buttonState->u16PressedCycles++;
            if (buttonState->u16PressedCycles == BUTTONS_RESET_DEVICE_CYCLES) {
                BUTTON_DBG("Reset device combination pressed. \n");
                if (g_sButtonsCallbacks.pfOnResetCallback) {
                    g_sButtonsCallbacks.pfOnResetCallback();
                }
            }
            break;

        case BUTTONS_DEBOUNCE_MASK:
            if (buttonState->bPressed) {
                BUTTON_DBG("Reset device combination released\n");
                BUTTONS_ResetState(buttonState);
            }
            break;
    }
}
