#include "app_buttons.h"

#include <jendefs.h>

#include "ZTimer.h"
#include "app_leds.h"
#include "dbg.h"
#include "device_config.h"
#include "device_definitions.h"
#include "fsl_gint.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

#define GINT0_POL_MASK (0)
#define GINT0_ENA_MASK (BTN_CTRL_MASK)

// static void APP_GINT_cb(void);
// static bool_t HandleButtonState(tsButtonState* sButtonState, uint32_t u32DIOState);
// static void ResetButtonsState(tsButtonState* sButtonState);
// static bool_t HandleResetButtonPressed(uint32_t u32DIOState);

// uint8_t u8TimerButtonScan;
// static uint16_t u16ButtonIdleCycles = 0;
// static tsButtonState sResetButtonState = {
//     .u.sResetBtnConfig = &sDeviceConfig.sResetButtonConfig,
//     .bPressed = FALSE,
//     .u16Cycles = 0,
//     .u8Debounce = APP_BTN_DEBOUNCE_MASK,
// };
// static tsButtonState sButtonsTrackers[MAX_BUTTONS_COUNT];

void APP_Buttons_Init(void) {
    // DBG_vPrintf(TRACE_BUTTONS, "Buttons_Init\n");

    // gpio_pin_config_t button_config = {
    //     .pinDirection = kGPIO_DigitalInput,
    // };
    // uint8_t i = 0;
    // for (; i < sDeviceConfig.u8EndpointButtonsCount; i++) {
    //     DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: Configuring Button pin: %d for LED %d\n", sDeviceConfig.psEndpointButtons[i].u32DioPin,
    //                 sDeviceConfig.psEndpointButtons[i].u32LedPin);
    //     IOCON_PinMuxSet(IOCON, 0, sDeviceConfig.psEndpointButtons[i].u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);

    //     GPIO_PinInit(GPIO, 0, sDeviceConfig.psEndpointButtons[i].u32DioPin, &button_config);
    // }

    // DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: Configuring GINT\n");
    // GINT_Init(GINT0);
    // GINT_SetCtrl(GINT0, kGINT_CombineOr, kGINT_TrigEdge, APP_GINT_cb);
    // GINT_ConfigPins(GINT0, kGINT_Port0, GINT0_POL_MASK, GINT0_ENA_MASK);
    // GINT_EnableCallback(GINT0);
    // DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: GINT configured. Buttons_Init done\n");
}

// void APP_Buttons_cbTimerScan(void* pvParam) {
//     GINT_DisableCallback(GINT0);

//     bool_t bAnyBtnPressed = FALSE;
//     uint32_t u32DIOState = GPIO_PortRead(GPIO, 0) & sDeviceConfig.sControlMasks.u32DioMask;

//     uint8_t i;
//     for (i = 0; i < sDeviceConfig.u8EndpointButtonsCount; i++) {
//         bAnyBtnPressed |= HandleButtonState(&sButtonsTrackers[i], u32DIOState);
//     }

//     bool_t bResetBtnPressed = HandleResetButtonPressed(u32DIOState);

//     if (bAnyBtnPressed || bResetBtnPressed) {
//         u16ButtonIdleCycles = 0;
//     } else {
//         u16ButtonIdleCycles++;
// #ifdef TRACE_BUTTONS
//         if (u16ButtonIdleCycles % 50 == 0)
//             DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: IDLE cycle %d\n", u16ButtonIdleCycles);
// #endif
//     }

//     if (u16ButtonIdleCycles == APP_BTN_IDLE_CYCLES_MAX) {
//         u16ButtonIdleCycles = 0;
//         DBG_vPrintf(TRACE_BUTTONS, "BUTTONS: Stopping scan\n");
//         GINT_EnableCallback(GINT0);
//         for (i = 0; i < sDeviceConfig.u8EndpointButtonsCount; i++) {
//             ResetButtonsState(&sButtonsTrackers[i]);
//         }
//         ZTIMER_eStop(u8TimerButtonScan);
//     } else {
//         ZTIMER_eStop(u8TimerButtonScan);
//         ZTIMER_eStart(u8TimerButtonScan, BUTTON_SCAN_TIME_MSEC);
//     }
// }

// static void APP_GINT_cb(void) {
// DBG_vPrintf(TRACE_BUTTONS, "GINT_CALLBACK: GINT callback triggered!\n");
// GINT_DisableCallback(GINT0);
// ZTIMER_eStart(u8TimerButtonScan, BUTTON_SCAN_TIME_MSEC);
// }

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
