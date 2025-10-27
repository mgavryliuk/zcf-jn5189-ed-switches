#include <jendefs.h>

#include "JN5189.h"
#include "MemManager.h"
#include "PWR_Interface.h"
#include "RNG_Interface.h"
#include "SecLib.h"
#include "TimersManager.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_wwdt.h"
#include "dbg.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_iocon.h"
#include "fsl_os_abstraction.h"
#include "fsl_power.h"
#include "fsl_reset.h"

#ifndef DEBUG_ENABLE
#define DEBUG_ENABLE FALSE
#endif

#define UART_PIO_TX (8U)
#define UART_PIO_RX (9U)
#define UART_BAUD_RATE (115200U)

#define HW_DBG(...) DBG_vPrintf(DEBUG_ENABLE, "[HW INIT] " __VA_ARGS__)

static void CLOCKS_Init(void);
static void DBGConsole_Init(void);
extern void OSA_TimeInit(void);

/**
 * @brief Initializes essential hardware components.
 *
 * This function is called both on cold start and every wake-up from sleep.
 * Therefore, any initialization that must persist or be refreshed after
 * waking up (e.g. timers, memory pools, RNG, peripherals) should be placed here.
 *
 * It prevents the need to duplicate initialization logic inside the wake-up handler.
 */
void hardware_init(void) {
    static bool_t bWarmStart = FALSE;

    POWER_Init();
    CLOCKS_Init();
    DBGConsole_Init();
    if (!bWarmStart) {
        HW_DBG("Cold start\n");
        bWarmStart = TRUE;
        PWR_vColdStart();
        HW_DBG("PWR_vColdStart done\n");
    }

    SecLib_Init();
    HW_DBG("SecLib_Init done\n");
    RNG_Init();
    HW_DBG("RNG_Init done\n");
    TMR_Init();
    HW_DBG("DBG_vPrintf done\n");
    MEM_Init();
    HW_DBG("MEM_Init done\n");

    APP_WWDT_Init();
    BUTTONS_Hardware_Init();
    LEDS_Hardware_Init();
}

static void CLOCKS_Init(void) {
    RESET_PeripheralReset(kUSART0_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kADC0_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kGPIO0_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kGINT_RST_SHIFT_RSTn);
    RESET_PeripheralReset(kRTC_RST_SHIFT_RSTn);

    CLOCK_EnableAPBBridge();
    CLOCK_EnableClock(kCLOCK_Xtal32M);
    CLOCK_EnableClock(kCLOCK_Fro32k);

    CLOCK_AttachClk(kXTAL32M_to_MAIN_CLK);
    CLOCK_AttachClk(kMAIN_CLK_to_ASYNC_APB);
    CLOCK_AttachClk(kXTAL32M_to_OSC32M_CLK);
    CLOCK_AttachClk(kFRO32K_to_OSC32K_CLK);
    CLOCK_AttachClk(kOSC32M_to_USART_CLK);
    CLOCK_AttachClk(kOSC32K_to_WDT_CLK);

    CLOCK_EnableClock(kCLOCK_Iocon);
    CLOCK_EnableClock(kCLOCK_InputMux);
    CLOCK_EnableClock(kCLOCK_Gint);

    SYSCON->MAINCLKSEL = SYSCON_MAINCLKSEL_SEL(2);
    SystemCoreClockUpdate();

    OSA_TimeInit();
}

static void DBGConsole_Init(void) {
#ifdef DBG_ENABLE
    IOCON_PinMuxSet(IOCON, 0, UART_PIO_TX, IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    IOCON_PinMuxSet(IOCON, 0, UART_PIO_RX, IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    DbgConsole_Init(0, UART_BAUD_RATE, kSerialPort_Uart, CLOCK_GetFreq(kCLOCK_Xtal32M));
#endif
}
