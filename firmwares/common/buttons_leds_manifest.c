#include "app_buttons.h"
#include "app_leds.h"
#include "app_zb_node.h"
#include "device_definitions.h"

const ButtonCallbacks_t g_sButtonsCallbacks = {
    .pfOnPressCallback = LEDS_ButtonBlinkCallback,
    .pfOnResetCallback = ZB_NODE_OnResetCallback,
};

LedWithState_t g_asLedsStates[LEDS_AMOUNT];
ButtonWithState_t g_asButtonsStates[BUTTONS_AMOUNT];
