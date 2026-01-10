#include "app_buttons.h"

#include "ZTimer.h"
#include "fsl_gint.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void BUTTONS_ScanCallback(void* pvParam);
static void BUTTONS_GINTCallback(void);
static void BUTTONS_ResetState(ButtonState_t* buttonState);
static void BUTTONS_HandleButtonState(ButtonWithState_t* buttonWithState, uint32_t dioState);
static void BUTTONS_HandleResetButtonState(uint32_t dioState);
static void BUTTONS_HandleToogle(ButtonWithState_t* buttonWithState);
static void BUTTONS_HandleMomentaryOnOff(ButtonWithState_t* buttonWithState);
static void BUTTONS_HandleMultistate(ButtonWithState_t* buttonWithState);
static inline void BUTTONS_IncrementCycles(ButtonState_t* buttonState);
static inline void BUTTONS_SendEvent(ButtonEvent_t eButtonEvent, uint8_t u8Endpoint);
static inline ButtonEvent_t BUTTONS_MultiStateToEvent(ButtonClickState_t eClickState);

static uint8_t u8ButtonIdleCycles = 0;
static ResetButtonWithState_t sResetButtonState;
static ButtonMode_t eButtonMode;

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
        g_asButtonsStates->sButtonState.bPressed = FALSE;
        BUTTONS_ResetState(&g_asButtonsStates[i].sButtonState);
    }
    sResetButtonState.pButton = &g_sResetButton;
    sResetButtonState.sButtonState.bPressed = FALSE;
    BUTTONS_ResetState(&sResetButtonState.sButtonState);
}

void BUTTONS_SetMode(ButtonMode_t eMode) {
    if (eButtonMode == eMode) {
        BUTTON_DBG("Button mode is already %d. Doing nothing\n", eButtonMode);
        return;
    }

    BUTTON_DBG("Changing button mode %d -> %d\n", eButtonMode, eMode);
    eButtonMode = eMode;
    for (uint8_t i = 0; i < g_numButtons; i++) {
        BUTTONS_ResetState(&g_asButtonsStates[i].sButtonState);
    }
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

    BUTTONS_HandleResetButtonState(u32DIOState);
    bAnyBtnPressed |= sResetButtonState.sButtonState.bPressed;

    if (bAnyBtnPressed) {
        u8ButtonIdleCycles = 0;
    } else {
        u8ButtonIdleCycles++;
    }

    if (u8ButtonIdleCycles == BUTTONS_IDLE_CYCLES_MAX) {
        u8ButtonIdleCycles = 0;
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
    buttonState->u16StateCycles = 0;
    buttonState->u8Debounce = BUTTONS_DEBOUNCE_MASK;
}

static void BUTTONS_HandleButtonState(ButtonWithState_t* buttonWithState, uint32_t dioState) {
    const Button_t* button = buttonWithState->pButton;
    ButtonState_t* buttonState = &buttonWithState->sButtonState;

    buttonState->u8Debounce <<= 1;
    buttonState->u8Debounce |= ((dioState & button->u32DioMask) ? 1 : 0);
    buttonState->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    if (buttonState->u8Debounce == 0 && !buttonState->bPressed) {
        BUTTON_DBG("Button for endpoint `%d` - PRESSED. Calling onPressCallback if available\n", button->u8Endpoint);
        if (g_sButtonsCallbacks.pfOnPressCallback && button->pvLedConfig) {
            g_sButtonsCallbacks.pfOnPressCallback(button->pvLedConfig);
        }
    }

    switch (eButtonMode) {
        case BUTTON_MODE_TOGGLE:
            BUTTONS_HandleToogle(buttonWithState);
            break;

        case BUTTON_MODE_MOMENTARY_ON_OFF:
            BUTTONS_HandleMomentaryOnOff(buttonWithState);
            break;

        case BUTTON_MODE_MULTISTATE_INPUT:
            BUTTONS_HandleMultistate(buttonWithState);
            break;
        default:
            break;
    }
}

static void BUTTONS_HandleResetButtonState(uint32_t dioState) {
    const ResetButton_t* resetButton = sResetButtonState.pButton;
    ButtonState_t* buttonState = &sResetButtonState.sButtonState;

    buttonState->u8Debounce <<= 1;
    buttonState->u8Debounce |= (dioState & resetButton->u32DioMask) ? 1 : 0;
    buttonState->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    switch (buttonState->u8Debounce) {
        case 0:
            BUTTONS_IncrementCycles(buttonState);
            if (!buttonState->bPressed) {
                BUTTON_DBG("Reset device combination pressed. Reset mask: %x\n", resetButton->u32DioMask);
                buttonState->bPressed = TRUE;
                if (g_sButtonsCallbacks.pfOnPressCallback && resetButton->pvLedConfig) {
                    g_sButtonsCallbacks.pfOnPressCallback(resetButton->pvLedConfig);
                }
            }

            if (buttonState->u16StateCycles == BUTTONS_RESET_DEVICE_CYCLES) {
                BUTTON_DBG("Reset device combination pressed. \n");
                if (g_sButtonsCallbacks.pfOnResetCallback) {
                    g_sButtonsCallbacks.pfOnResetCallback();
                }
            }
            break;

        case BUTTONS_DEBOUNCE_MASK:
            if (buttonState->bPressed) {
                buttonState->bPressed = FALSE;
                BUTTON_DBG("Reset device combination released\n");
                BUTTONS_ResetState(buttonState);
            }
            break;
    }
}

static void BUTTONS_HandleToogle(ButtonWithState_t* buttonWithState) {
    const Button_t* button = buttonWithState->pButton;
    ButtonState_t* buttonState = &buttonWithState->sButtonState;

    if (buttonState->u8Debounce == 0 && !buttonState->bPressed) {
        BUTTON_DBG("Button for endpoint `%d` - PRESSED\n", button->u8Endpoint);
        buttonState->bPressed = TRUE;
        BUTTONS_SendEvent(BTN_TOGGLE_EVENT, button->u8Endpoint);
    } else if (buttonState->u8Debounce == BUTTONS_DEBOUNCE_MASK && buttonState->bPressed) {
        BUTTON_DBG("Button for endpoint `%d` - RELEASED\n", button->u8Endpoint);
        buttonState->bPressed = FALSE;
        BUTTONS_ResetState(buttonState);
    }
}

static void BUTTONS_HandleMomentaryOnOff(ButtonWithState_t* buttonWithState) {
    const Button_t* button = buttonWithState->pButton;
    ButtonState_t* buttonState = &buttonWithState->sButtonState;
    if (buttonState->u8Debounce == 0 && !buttonState->bPressed) {
        BUTTON_DBG("Button for endpoint `%d` - PRESSED\n", button->u8Endpoint);
        buttonState->bPressed = TRUE;
        BUTTONS_SendEvent(BTN_MOMENTARY_PRESSED_EVENT, button->u8Endpoint);

    } else if (buttonState->u8Debounce == BUTTONS_DEBOUNCE_MASK && buttonState->bPressed) {
        BUTTON_DBG("Button for endpoint `%d` - RELEASED\n", button->u8Endpoint);
        buttonState->bPressed = FALSE;
        BUTTONS_SendEvent(BTN_MOMENTARY_RELEASED_EVENT, button->u8Endpoint);
        BUTTONS_ResetState(buttonState);
    }
}

static void BUTTONS_HandleMultistate(ButtonWithState_t* buttonWithState) {
    const Button_t* button = buttonWithState->pButton;
    ButtonState_t* buttonState = &buttonWithState->sButtonState;

    if (buttonState->u8Debounce == 0) {
        if (!buttonState->bPressed) {
            BUTTON_DBG("Button for endpoint `%d` - PRESSED\n", button->u8Endpoint);
            buttonState->bPressed = TRUE;
            /* BTN_CLICK_IDLE -> BTN_CLICK_SINGLE -> BTN_CLICK_DOUBLE -> BTN_CLICK_TRIPLE */
            if (buttonState->eClickState < BTN_CLICK_TRIPLE) {
                buttonState->u16StateCycles = 0;
                buttonState->eClickState++;
                BUTTON_DBG("Changed state to %d\n", buttonState->eClickState);
            }
        }
        BUTTONS_IncrementCycles(buttonState);

        /* Long press detection */
        if (buttonState->u16StateCycles == BUTTONS_LONG_PRESS_REGISTER_CYCLES && buttonState->eClickState == BTN_CLICK_SINGLE) {
            buttonState->eClickState = BTN_CLICK_LONG;
            BUTTON_DBG("Long press detected: %d\n", buttonState->eClickState);
            BUTTONS_SendEvent(BTN_LONG_PRESSED_EVENT, button->u8Endpoint);
            return;
        }

        /* Emit press event for 2 and 3 presses */
        if (buttonState->u16StateCycles == BUTTONS_REGISTER_WINDOW_CYCLES && buttonState->eClickState >= BTN_CLICK_DOUBLE &&
            buttonState->eClickState <= BTN_CLICK_TRIPLE) {
            BUTTON_DBG("Multi-click on press detected: %d\n", buttonState->eClickState);
            ButtonEvent_t eButtonEvent = BUTTONS_MultiStateToEvent(buttonState->eClickState);
            if (eButtonEvent != BTN_LONG_EMPTY_EVENT)
                BUTTONS_SendEvent(eButtonEvent, button->u8Endpoint);
            BUTTONS_ResetState(buttonState);
        }

    } else if (buttonState->u8Debounce == BUTTONS_DEBOUNCE_MASK) {
        if (buttonState->bPressed) {
            BUTTON_DBG("Button for endpoint `%d` - RELEASED\n", button->u8Endpoint);
            buttonState->bPressed = FALSE;
        }
        BUTTONS_IncrementCycles(buttonState);

        if (buttonState->eClickState == BTN_CLICK_LONG) {
            BUTTONS_SendEvent(BTN_LONG_RELEASED_EVENT, button->u8Endpoint);
            BUTTON_DBG("Long release detected: %d\n", buttonState->eClickState);
            BUTTONS_ResetState(buttonState);
            return;
        }

        if (buttonState->eClickState >= BTN_CLICK_SINGLE && buttonState->eClickState <= BTN_CLICK_TRIPLE) {
            if (buttonState->u16StateCycles == BUTTONS_REGISTER_WINDOW_CYCLES) {
                BUTTON_DBG("Multi-click on release detected: %d\n", buttonState->eClickState);
                BUTTONS_SendEvent(BUTTONS_MultiStateToEvent(buttonState->eClickState), button->u8Endpoint);
                BUTTONS_ResetState(buttonState);
            }
        }
    }
}

static inline void BUTTONS_IncrementCycles(ButtonState_t* buttonState) {
    if (buttonState->u16StateCycles < UINT16_MAX) {
        buttonState->u16StateCycles++;
    } else {
        buttonState->u16StateCycles = UINT16_MAX;
    }
}

static inline void BUTTONS_SendEvent(ButtonEvent_t eButtonEvent, uint8_t u8Endpoint) {
    if (g_sButtonsCallbacks.pfOnPressEventCallback) {
        g_sButtonsCallbacks.pfOnPressEventCallback(eButtonEvent, u8Endpoint);
    }
}

static inline ButtonEvent_t BUTTONS_MultiStateToEvent(ButtonClickState_t eClickState) {
    if (eClickState == BTN_CLICK_SINGLE)
        return BTN_SINGLE_CLICK_EVENT;
    else if (eClickState == BTN_CLICK_DOUBLE)
        return BTN_DOUBLE_CLICK_EVENT;
    else if (eClickState == BTN_CLICK_TRIPLE)
        return BTN_TRIPLE_CLICK_EVENT;
    return BTN_LONG_EMPTY_EVENT;
}
