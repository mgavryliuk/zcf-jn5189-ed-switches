#include "app_leds.h"

#include "ZTimer.h"
#include "device_config.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void LEDS_BlinkTurnOffCallback(void* pvParam);
static LedWithState_t* LEDS_GetStateFromLed(const Led_t* led);

LedWithState_t sNetworkSetupLedState;

void LEDS_Hardware_Init(void) {
    gpio_pin_config_t led_pin_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    for (uint8_t i = 0; i < g_numLedsPins; i++) {
        LEDS_DBG("LED(%u). Configuring Hardware...\n", g_asLedsPins[i]);
        IOCON_PinMuxSet(IOCON, 0, g_asLedsPins[i], IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
        GPIO_PinInit(GPIO, 0, g_asLedsPins[i], &led_pin_config);
    }
}

void LEDS_Timers_Init(void) {
    LEDS_DBG("Configuring Timers...\n");
    for (uint8_t i = 0; i < g_numLeds; i++) {
        g_asLedsStates[i].psLed = g_asLeds[i];
        g_asLedsStates[i].u8IsOn = FALSE;
        ZTIMER_eOpen(&g_asLedsStates[i].u8TimerID, LEDS_BlinkTurnOffCallback, &g_asLedsStates[i], ZTIMER_FLAG_PREVENT_SLEEP);
        LEDS_DBG("LED(%u). Blink turn off Timer ID - %d\n", g_asLeds[i]->u32DioMask, g_asLedsStates[i].u8TimerID);
    }

    sNetworkSetupLedState.psLed = &g_sNetworkSetupLed;
    sNetworkSetupLedState.u8IsOn = FALSE;
    ZTIMER_eOpen(&sNetworkSetupLedState.u8TimerID, LEDS_BlinkDuringNetworkSetup_Start, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    LEDS_DBG("LEDS_Init finished. Blink during setup Timer ID - %d\n", sNetworkSetupLedState.u8TimerID);
}

void LEDS_Blink(LedWithState_t* psLedWithState) {
    ZTIMER_teState state = ZTIMER_eGetState(sNetworkSetupLedState.u8TimerID);
    if (state == E_ZTIMER_STATE_RUNNING) {
        LEDS_DBG("Blink during setup in progress. Do nothing...");
        return;
    }

    LEDS_DBG("LED(%u). Blink called\n", psLedWithState->psLed->u32DioMask);
    if (psLedWithState->u8IsOn == FALSE) {
        GPIO_PortClear(GPIO, 0, psLedWithState->psLed->u32DioMask);
        psLedWithState->u8IsOn = TRUE;
        LEDS_DBG("LED(%u). Starting turn off timer %d\n", psLedWithState->psLed->u32DioMask, psLedWithState->u8TimerID);
        ZTIMER_teStatus status = ZTIMER_eStart(psLedWithState->u8TimerID, LEDS_BLINK_INTERVAL);
        LEDS_DBG("LED(%u). Start turn off timer %d status: %d\n", psLedWithState->psLed->u32DioMask, psLedWithState->u8TimerID, status);
    }
}

void LEDS_TurnOff(LedWithState_t* psLedWithState) {
    LEDS_DBG("LED(%u). Turn Off called\n", psLedWithState->psLed->u32DioMask);
    GPIO_PortSet(GPIO, 0, psLedWithState->psLed->u32DioMask);
    psLedWithState->u8IsOn = FALSE;
}

void LEDS_BlinkDuringNetworkSetup_Start(void* pvParam) {
    if (sNetworkSetupLedState.u8IsOn) {
        LEDS_DBG("Blink during setup. Turning OFF\n");
        GPIO_PortSet(GPIO, 0, sNetworkSetupLedState.psLed->u32DioMask);
        sNetworkSetupLedState.u8IsOn = FALSE;
        ZTIMER_eStart(sNetworkSetupLedState.u8TimerID, LEDS_CONTINOUS_BLINK_INTERVAL);
    } else {
        LEDS_DBG("Blink during setup. Turning ON\n");
        GPIO_PortClear(GPIO, 0, sNetworkSetupLedState.psLed->u32DioMask);
        sNetworkSetupLedState.u8IsOn = TRUE;
        ZTIMER_eStart(sNetworkSetupLedState.u8TimerID, LEDS_CONTINOUS_BLINK_INTERVAL);
    }
}

void LEDS_BlinkDuringNetworkSetup_Stop(void) {
    LEDS_DBG("Blink during setup. Stopping...\n");
    GPIO_PortSet(GPIO, 0, sNetworkSetupLedState.psLed->u32DioMask);
    sNetworkSetupLedState.u8IsOn = FALSE;
    ZTIMER_eStop(sNetworkSetupLedState.u8TimerID);
}

void LEDS_ButtonBlinkCallback(void* ctx) {
    Led_t* led = (Led_t*)ctx;
    LedWithState_t* state = LEDS_GetStateFromLed(led);
    if (!state)
        return;

    LEDS_DBG("LED(%u). Button blink callback called...\n", led->u32DioMask);
    LEDS_Blink(state);
}

static void LEDS_BlinkTurnOffCallback(void* pvParam) {
    LedWithState_t* ledWithState = (LedWithState_t*)pvParam;
    LEDS_DBG("LED(%u). Blink turn off callback called\n", ledWithState->psLed->u32DioMask);
    LEDS_TurnOff(ledWithState);
}

static LedWithState_t* LEDS_GetStateFromLed(const Led_t* led) {
    for (size_t i = 0; i < g_numLeds; i++) {
        if (g_asLedsStates[i].psLed == led)
            return &g_asLedsStates[i];
    }
    return NULL;
}
