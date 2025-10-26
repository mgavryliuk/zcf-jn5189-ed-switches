#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include <jendefs.h>

#ifdef DEBUG_BUTTONS
#define TRACE_BUTTONS TRUE
#else
#define TRACE_BUTTONS FALSE
#endif

#ifndef RESET_DEVICE_CYCLES
#define RESET_DEVICE_CYCLES 500  // 500 * BUTTON_SCAN_TIME_MSEC = 5 sec
#endif

#define BTN_DEBOUNCE_MASK 0b0111

#define BUTTON_SCAN_TIME_MSEC ZTIMER_TIME_MSEC(10)
// 40 * BUTTON_SCAN_TIME_MSEC = 400ms
#define BTN_REGISTER_WINDOW_CYCLES 40
// 70 * BUTTON_SCAN_TIME_MSEC = 700ms
#define BTN_LONG_PRESS_REGISTER_CYCLES 70
// 100 * BUTTON_SCAN_TIME_MSEC = 1sec
#define BTN_IDLE_CYCLES_MAX 100

typedef void (*ButtonPressHandler)(void* ctx);

typedef enum {
    IDLE = 0,
    SINGLE_CLICK,
    DOUBLE_CLICK,
    TRIPLE_CLICK,
    LONG_CLICK,
} ButtonState;

typedef struct {
    // Zigbee endpoint
    const uint16_t u16Endpoint;

    // Button config
    const uint32_t u32DioPin;
    const uint32_t u32DioMask;

    // Button State
    bool_t bPressed;
    uint16_t u16PressedCycles;
    uint8_t u8Debounce;
    ButtonState eState;

    // Press callback
    ButtonPressHandler pfOnPressCallback;
    void* pvOnPressContext;
} Button;

typedef struct {
    const uint32_t u32DioMask;

    // State
    bool_t bPressed;
    uint16_t u16PressedCycles;
    uint8_t u8Debounce;
} ResetMaskConfig;

void APP_Buttons_Init(void);
void APP_Buttons_cbTimerScan(void* pvParam);

#endif /* APP_BUTTONS_H */
