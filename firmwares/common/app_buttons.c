#include "app_buttons.h"

#include "ZTimer.h"
#include "device_config.h"
#include "fsl_gint.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void BUTTONS_ScanCallback(void* pvParam);
static void BUTTONS_GINTCallback(void);
static void BUTTONS_ResetState(Button_t* button);
static void HandleButtonState(Button_t* button, uint32_t dioState);
static void BUTTONS_HandleResetState(uint32_t dioState);

static uint8_t u16ButtonIdleCycles = 0;
static ButtonCallbacks_t buttonCallbacks;

void BUTTONS_HW_Init(void) {
    BUTTON_DBG("Init hardware\n");
    gpio_pin_config_t button_config = {
        .pinDirection = kGPIO_DigitalInput,
    };
    for (uint8_t i = 0; i < device_config.u8ButtonsAmount; i++) {
        BUTTON_DBG("Configuring button with pin: %d\n", device_config.psButtons[i]->u32DioPin);
        IOCON_PinMuxSet(IOCON, 0, device_config.psButtons[i]->u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
        GPIO_PinInit(GPIO, 0, device_config.psButtons[i]->u32DioPin, &button_config);
    }
    BUTTON_DBG("Init hardware finished!\n");
    BUTTON_DBG("Configuring GINT...\n");
    GINT_Init(GINT0);
    GINT_SetCtrl(GINT0, kGINT_CombineOr, kGINT_TrigEdge, BUTTONS_GINTCallback);
    GINT_ConfigPins(GINT0, kGINT_Port0, 0, device_config.u32ButtonsInterruptMask);
    GINT_EnableCallback(GINT0);
    BUTTON_DBG("GINT configured!\n");
}

void BUTTONS_SW_Init(const ButtonCallbacks_t* callbacks) {
    BUTTON_DBG("Init timers\n");
    ZTIMER_eOpen(&device_config.u8ButtonScanTimerID, BUTTONS_ScanCallback, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    BUTTON_DBG("Init timers finished!\n");
    BUTTON_DBG("Configuring callbacks...\n");
    if (callbacks) {
        buttonCallbacks = *callbacks;
    }
    BUTTON_DBG("Configuring callbacks done!\n");
}

static void BUTTONS_ScanCallback(void* pvParam) {
    GINT_DisableCallback(GINT0);
    uint8_t i;
    bool_t bAnyBtnPressed = FALSE;

    uint32_t u32DIOState = GPIO_PortRead(GPIO, 0) & device_config.u32ButtonsInterruptMask;
    for (i = 0; i < device_config.u8ButtonsAmount; i++) {
        HandleButtonState(device_config.psButtons[i], u32DIOState);
        bAnyBtnPressed |= device_config.psButtons[i]->bPressed;
    }

    BUTTONS_HandleResetState(u32DIOState);
    if (bAnyBtnPressed || device_config.sResetMaskConfig.bPressed) {
        u16ButtonIdleCycles = 0;
    } else {
        u16ButtonIdleCycles++;
    }

    if (u16ButtonIdleCycles == BUTTONS_IDLE_CYCLES_MAX) {
        u16ButtonIdleCycles = 0;
        BUTTON_DBG("IDLE cycles achieved. Stopping scan...\n");
        GINT_EnableCallback(GINT0);
        for (i = 0; i < device_config.u8ButtonsAmount; i++) {
            BUTTONS_ResetState(device_config.psButtons[i]);
        }
        ZTIMER_eStop(device_config.u8ButtonScanTimerID);
    } else {
        ZTIMER_eStop(device_config.u8ButtonScanTimerID);
        ZTIMER_eStart(device_config.u8ButtonScanTimerID, BUTTONS_SCAN_TIME_MSEC);
    }
}

static void BUTTONS_GINTCallback(void) {
    BUTTONS_ScanCallback(NULL);
}

static void BUTTONS_ResetState(Button_t* button) {
    button->bPressed = FALSE;
    button->u16PressedCycles = 0;
    button->eState = IDLE;
}

static void HandleButtonState(Button_t* button, uint32_t dioState) {
    button->u8Debounce <<= 1;
    button->u8Debounce |= ((dioState & button->u32DioMask) ? 1 : 0);
    button->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    switch (button->u8Debounce) {
        case 0:
            if (!button->bPressed) {
                BUTTON_DBG("Button with pin %d pressed. Endpoint: %d\n", button->u32DioPin, button->u16Endpoint);
                button->bPressed = TRUE;
                if (buttonCallbacks.pfOnPressCallback && button->pvLedConfig) {
                    buttonCallbacks.pfOnPressCallback(button->pvLedConfig);
                }
            }
            button->u16PressedCycles++;

            break;
        case BUTTONS_DEBOUNCE_MASK:
            if (button->bPressed) {
                BUTTON_DBG("Button with pin %d released. Endpoint: %d\n", button->u32DioPin, button->u16Endpoint);
                button->bPressed = FALSE;
                BUTTONS_ResetState(button);
            }
    }
}

static void BUTTONS_HandleResetState(uint32_t dioState) {
    ResetMaskConfig_t* resetMaskConfig = &device_config.sResetMaskConfig;

    resetMaskConfig->u8Debounce <<= 1;
    resetMaskConfig->u8Debounce |= (dioState & resetMaskConfig->u32DioMask) ? 1 : 0;
    resetMaskConfig->u8Debounce &= BUTTONS_DEBOUNCE_MASK;

    switch (resetMaskConfig->u8Debounce) {
        case 0:
            resetMaskConfig->u16PressedCycles++;
            if (!resetMaskConfig->bPressed) {
                BUTTON_DBG("Reset device combination pressed. Reset mask: %x\n", resetMaskConfig->u32DioMask);
                resetMaskConfig->bPressed = TRUE;
            }

            if (resetMaskConfig->u16PressedCycles == BUTTONS_RESET_DEVICE_CYCLES) {
                BUTTON_DBG("Reset device combination pressed. \n");
                if (buttonCallbacks.pfOnResetCallback) {
                    buttonCallbacks.pfOnResetCallback();
                }
            }
            break;

        case BUTTONS_DEBOUNCE_MASK:
            if (resetMaskConfig->bPressed) {
                BUTTON_DBG("Reset device combination released\n");
                resetMaskConfig->bPressed = FALSE;
                resetMaskConfig->u16PressedCycles = 0;
            }
            break;
    }
}

// static bool_t HandleButtonState(tsButtonState* sButtonState, uint32_t u32DIOState) {
//     uint8_t u8ButtonUp = (u32DIOState & sButtonState->u.sEndpointBtnConfig->u32DioMask) ? 1 : 0;
//     sButtonState->u8Debounce <<= 1;
//     sButtonState->u8Debounce |= u8ButtonUp;
//     sButtonState->u8Debounce &= APP_BTN_DEBOUNCE_MASK;

//     switch (sButtonState->u8Debounce) {
//         case 0:
//             if (!sButtonState->bPressed) {
//                 if (sButtonState->u.sEndpointBtnConfig->bHasLed) {
//                     APP_Leds_Blink(sButtonState->u.sEndpointBtnConfig->u32LedMask, 1);
//                 }
//                 DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: DIO for mask %x pressed. Endpoint %d\n",
//                             sButtonState->u.sEndpointBtnConfig->u32DioMask, sButtonState->u.sEndpointBtnConfig->u16Endpoint);
//                 sButtonState->bPressed = TRUE;
//                 // APP_vSendButtonEvent(
//                 //     sButtonState->u.sEndpointBtnConfig->u16Endpoint,
//                 //     BUTTON_TOGGLE_ACTION);
//             }
//             break;

//         case APP_BTN_DEBOUNCE_MASK:
//             if (sButtonState->bPressed) {
//                 DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: DIO for mask %x released. Endpoint %d\n",
//                             sButtonState->u.sEndpointBtnConfig->u32DioMask, sButtonState->u.sEndpointBtnConfig->u16Endpoint);
//                 sButtonState->bPressed = FALSE;
//                 ResetButtonsState(sButtonState);
//             }

//         default:
//             break;
//     }
//     return sButtonState->bPressed;
// }
