#include "app_buttons.h"

#include <jendefs.h>

#include "ZTimer.h"
#include "device_config.h"
#include "fsl_gint.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void GINTCallback(void);
static void HandleButtonState(Button_t* button, uint32_t dioState);
static inline void ResetButtonsState(Button_t* button);

static uint8_t u16ButtonIdleCycles = 0;

void BUTTONS_Hardware_Init(void) {
    BUTTON_DBG("Init hardware\n");
    gpio_pin_config_t button_config = {
        .pinDirection = kGPIO_DigitalInput,
    };
    uint8_t i = 0;
    for (; i < device_config.u8ButtonsAmount; i++) {
        BUTTON_DBG("Configuring button with pin: %d\n", device_config.psButtons[i]->u32DioPin);
        IOCON_PinMuxSet(IOCON, 0, device_config.psButtons[i]->u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
        GPIO_PinInit(GPIO, 0, device_config.psButtons[i]->u32DioPin, &button_config);
    }
    BUTTON_DBG("Init hardware finished!\n");
    BUTTON_DBG("Configuring GINT...\n");
    GINT_Init(GINT0);
    GINT_SetCtrl(GINT0, kGINT_CombineOr, kGINT_TrigEdge, GINTCallback);
    GINT_ConfigPins(GINT0, kGINT_Port0, 0, device_config.u32ButtonsInterruptMask);
    GINT_EnableCallback(GINT0);
    BUTTON_DBG("GINT configured!\n");
}

void BUTTONS_Timers_Init(void) {
    BUTTON_DBG("Init timers\n");
    ZTIMER_eOpen(&device_config.u8ButtonScanTimerID, BUTTONS_ScanCallback, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    BUTTON_DBG("Init timers finished!\n");
}

void BUTTONS_ScanCallback(void* pvParam) {
    GINT_DisableCallback(GINT0);
    uint8_t i;
    bool_t bAnyBtnPressed = FALSE;
    uint32_t u32DIOState = GPIO_PortRead(GPIO, 0) & device_config.u32ButtonsInterruptMask;
    for (i = 0; i < device_config.u8ButtonsAmount; i++) {
        HandleButtonState(device_config.psButtons[i], u32DIOState);
        bAnyBtnPressed |= device_config.psButtons[i]->bPressed;
    }
    // bool_t bResetBtnPressed = HandleResetButtonPressed(u32DIOState);
    bool_t bResetBtnPressed = FALSE;

    if (bAnyBtnPressed || bResetBtnPressed) {
        u16ButtonIdleCycles = 0;
    } else {
        u16ButtonIdleCycles++;
    }

    if (u16ButtonIdleCycles == BUTTON_IDLE_CYCLES_MAX) {
        u16ButtonIdleCycles = 0;
        BUTTON_DBG("IDLE cycles achieved. Stopping scan...\n");
        GINT_EnableCallback(GINT0);
        for (i = 0; i < device_config.u8ButtonsAmount; i++) {
            // ResetButtonsState(&sButtonsTrackers[i]);
        }
        ZTIMER_eStop(device_config.u8ButtonScanTimerID);
    } else {
        ZTIMER_eStop(device_config.u8ButtonScanTimerID);
        ZTIMER_eStart(device_config.u8ButtonScanTimerID, BUTTON_SCAN_TIME_MSEC);
    }
}

static void GINTCallback(void) {
    BUTTONS_ScanCallback(NULL);
}

static void HandleButtonState(Button_t* button, uint32_t dioState) {
    button->u8Debounce <<= 1;
    button->u8Debounce |= ((dioState & button->u32DioMask) ? 1 : 0);
    button->u8Debounce &= BUTTON_DEBOUNCE_MASK;

    switch (button->u8Debounce) {
        case 0:
            if (!button->bPressed) {
                BUTTON_DBG("Button with pin %d pressed. Endpoint: %d\n", button->u32DioPin, button->u16Endpoint);
                button->bPressed = TRUE;
                button->pfOnPressCallback(button->pvOnPressContext);
            }
            button->u16PressedCycles++;

            break;
        case BUTTON_DEBOUNCE_MASK:
            if (button->bPressed) {
                BUTTON_DBG("Button with pin %d released. Endpoint: %d\n", button->u32DioPin, button->u16Endpoint);
                button->bPressed = FALSE;
                ResetButtonsState(button);
            }
    }
}

static inline void ResetButtonsState(Button_t* button) {
    button->bPressed = FALSE;
    button->u16PressedCycles = 0;
    button->eState = IDLE;
}

// static bool_t HandleResetButtonPressed(uint32_t u32DIOState);

//
// static void ResetButtonsState(tsButtonState* sButtonState) {
//     sButtonState->eState = IDLE;
//     sButtonState->u16Cycles = 0;
// }

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

// static bool_t HandleResetButtonPressed(uint32_t u32DIOState) {
//     uint8_t u8ButtonUp = (u32DIOState & sResetButtonState.u.sResetBtnConfig->u32DioMask) ? 1 : 0;
//     sResetButtonState.u8Debounce <<= 1;
//     sResetButtonState.u8Debounce |= u8ButtonUp;
//     sResetButtonState.u8Debounce &= APP_BTN_DEBOUNCE_MASK;

//     switch (sResetButtonState.u8Debounce) {
//         case 0:
//             sResetButtonState.u16Cycles++;
//             if (!sResetButtonState.bPressed) {
//                 DBG_vPrintf(TRACE_BUTTONS,
//                             "BUTTONS: Reset device combination pressed. Reset "
//                             "mask: %x\n",
//                             sResetButtonState.u.sResetBtnConfig->u32DioMask);
//                 sResetButtonState.bPressed = TRUE;
//             }

//             if (sResetButtonState.u16Cycles == APP_RESET_DEVICE_CYCLES) {
//                 DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: Sending reset device event\n");
//                 // APP_vSendResetDeviceEvent();
//             }
//             break;

//         case APP_BTN_DEBOUNCE_MASK:
//             if (sResetButtonState.bPressed) {
//                 DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: Reset device combination released\n");
//                 sResetButtonState.bPressed = FALSE;
//                 sResetButtonState.u16Cycles = 0;
//             }

//         default:
//             break;
//     }

//     return sResetButtonState.bPressed;
// }
