#include "app_common.h"

#include "ConfigurationCluster.h"
#include "MultistateInputBasic.h"
#include "OnOff.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_zb_node.h"
#include "device_definitions.h"
#include "zcl_customcommand.h"

static void BUTTONS_OnPressEventCallback(ButtonEvent_t eButtonEvent, uint8_t u8Endpoint);
static teZCL_Status eSendCommand(uint8 u8Endpoint, teCLD_OnOff_Command eOnOffCommand);
static teZCL_Status eReportAction(uint8 u8Endpoint, ButtonEvent_t eButtonEvent);

const ButtonCallbacks_t g_sButtonsCallbacks = {
    .pfOnPressCallback = LEDS_ButtonBlinkCallback,
    .pfOnResetCallback = ZB_NODE_OnResetCallback,
    .pfOnPressEventCallback = BUTTONS_OnPressEventCallback,
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

static void BUTTONS_OnPressEventCallback(ButtonEvent_t eButtonEvent, uint8_t u8Endpoint) {
    COMMON_DBG("Processing button event: %d\n", eButtonEvent);
    switch (eButtonEvent) {
        case BTN_TOGGLE_EVENT:
            eSendCommand(u8Endpoint, E_CLD_ONOFF_CMD_TOGGLE);
            break;

        case BTN_MOMENTARY_PRESSED_EVENT:
            eSendCommand(u8Endpoint, E_CLD_ONOFF_CMD_ON);
            break;

        case BTN_MOMENTARY_RELEASED_EVENT:
            eSendCommand(u8Endpoint, E_CLD_ONOFF_CMD_OFF);
            break;

        default:
            break;
    }
    eReportAction(u8Endpoint, eButtonEvent);
};

static teZCL_Status eSendCommand(uint8 u8Endpoint, teCLD_OnOff_Command eOnOffCommand) {
    tsZCL_Address addr;
    addr.uAddress.u16DestinationAddress = 0x0000;
    addr.eAddressMode = E_ZCL_AM_BOUND_NON_BLOCKING_NO_ACK;
    COMMON_DBG("Sending On/Off command status...\n");
    uint8 sequenceNo;
    teZCL_Status eStatus = eCLD_OnOffCommandSend(u8Endpoint, 1, &addr, &sequenceNo, eOnOffCommand);
    COMMON_DBG("Sending On/Off command status: %02x\n", eStatus);
    return eStatus;
}

static teZCL_Status eReportAction(uint8 u8Endpoint, ButtonEvent_t eButtonEvent) {
    teZCL_Status eStatus;
    tsZCL_ClusterInstance* psZCL_ClusterInstance;
    eStatus = eZCL_SearchForClusterEntry(u8Endpoint, GENERAL_CLUSTER_ID_MULTISTATE_INPUT_BASIC, TRUE, &psZCL_ClusterInstance);
    if (eStatus != E_ZCL_SUCCESS) {
        COMMON_DBG("Search for cluster entry %d in endpoint %d failed with status: %d\n", GENERAL_CLUSTER_ID_MULTISTATE_INPUT_BASIC,
                   u8Endpoint, eStatus);
        return eStatus;
    }

    ((tsCLD_MultistateInputBasic*)psZCL_ClusterInstance->pvEndPointSharedStructPtr)->u16PresentValue = (zuint16)eButtonEvent;
    COMMON_DBG("Reporting multistate action EP=%d value=%d...\n", u8Endpoint,
               ((tsCLD_MultistateInputBasic*)psZCL_ClusterInstance->pvEndPointSharedStructPtr)->u16PresentValue);

    tsZCL_Address addr;
    addr.uAddress.u16DestinationAddress = 0x0000;
    addr.eAddressMode = E_ZCL_AM_SHORT_NO_ACK;
    PDUM_thAPduInstance myPDUM_thAPduInstance = hZCL_AllocateAPduInstance();
    eStatus = eZCL_ReportAttribute(&addr, GENERAL_CLUSTER_ID_MULTISTATE_INPUT_BASIC, E_CLD_MULTISTATE_INPUT_BASIC_ATTR_ID_PRESENT_VALUE,
                                   u8Endpoint, 1, myPDUM_thAPduInstance);
    PDUM_eAPduFreeAPduInstance(myPDUM_thAPduInstance);
    COMMON_DBG("Sending report status: %02x\n", eStatus);
    return eStatus;
}
