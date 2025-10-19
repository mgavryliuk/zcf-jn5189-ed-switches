#include <jendefs.h>

#include "JN5189.h"
#include "PWR_Interface.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_wwdt.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_iocon.h"
#include "fsl_power.h"
#include "fsl_reset.h"

#ifndef DEBUG_ENABLE
#define DEBUG_ENABLE FALSE
#endif

#define UART_PIO_TX (8U)
#define UART_PIO_RX (9U)
#define UART_BAUD_RATE (115200U)

static void Clocks_Init(void);
static void DBGConsole_Init(void);

static bool_t bWarmStart = FALSE;

void hardware_init(void) {
    POWER_Init();
    Clocks_Init();
    DBGConsole_Init();
    if (!bWarmStart) {
        bWarmStart = TRUE;
        PWR_vColdStart();
    }
    APP_Buttons_Init();
    LEDS_Hardware_Init();
    APP_WWDT_Init();
}

static void Clocks_Init(void) {
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
    CLOCK_AttachClk(kXTAL32M_to_ADC_CLK);

    CLOCK_EnableClock(kCLOCK_Iocon);
    CLOCK_EnableClock(kCLOCK_InputMux);
    CLOCK_EnableClock(kCLOCK_Gint);
    CLOCK_EnableClock(kCLOCK_Rtc);

    SYSCON->MAINCLKSEL = SYSCON_MAINCLKSEL_SEL(2);
    SystemCoreClockUpdate();
}

static void DBGConsole_Init(void) {
#ifdef DBG_ENABLE
    IOCON_PinMuxSet(IOCON, 0, UART_PIO_TX, IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    IOCON_PinMuxSet(IOCON, 0, UART_PIO_RX, IOCON_FUNC2 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    DbgConsole_Init(0, UART_BAUD_RATE, kSerialPort_Uart, CLOCK_GetFreq(kCLOCK_Xtal32M));
#endif
}
