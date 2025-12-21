#include "ConfigurationCluster.h"
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

void CLDConfigurationClusterOnModeChangeCallback(teCLD_ButtonMode eMode) {
    switch (eMode) {
        case E_CLD_BUTTON_MODE_TOGGLE:
            BUTTONS_SetMode(BUTTON_MODE_TOGGLE);
            break;

        case E_CLD_BUTTON_MODE_MOMENTARY_ON_OFF:
            BUTTONS_SetMode(BUTTON_MODE_MOMENTARY_ON_OFF);
            break;

        case E_CLD_BUTTON_MODE_MULTISTATE_INPUT:
            BUTTONS_SetMode(BUTTON_MODE_MULTISTATE_INPUT);
            break;

        default:
            BUTTONS_SetMode(BUTTON_MODE_TOGGLE);
            break;
    };
}

const ConfigClusterModeChangedCallback g_cbZCLModeChanged = &CLDConfigurationClusterOnModeChangeCallback;
