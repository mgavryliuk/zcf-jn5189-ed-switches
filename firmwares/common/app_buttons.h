#ifndef APP_BUTTONS_H
#define APP_BUTTONS_H

#include <jendefs.h>

#include "dbg.h"

#ifdef DEBUG_BUTTONS
#define TRACE_BUTTONS TRUE
#else
#define TRACE_BUTTONS FALSE
#endif

#define BUTTON_DBG(...) DBG_vPrintf(TRACE_BUTTONS, "[Button] " __VA_ARGS__)

#define BUTTONS_RESET_DEVICE_CYCLES (500)  // 500 * BUTTON_SCAN_TIME_MSEC = 5 sec
#define BUTTONS_DEBOUNCE_MASK (0b0111)
#define BUTTONS_SCAN_TIME_MSEC ZTIMER_TIME_MSEC(10)
// 40 * BUTTON_SCAN_TIME_MSEC = 400ms
#define BUTTONS_REGISTER_WINDOW_CYCLES (40)
// 70 * BUTTON_SCAN_TIME_MSEC = 700ms
#define BUTTONS_LONG_PRESS_REGISTER_CYCLES (70)
// 100 * BUTTON_SCAN_TIME_MSEC = 1sec
#define BUTTONS_IDLE_CYCLES_MAX (100)

typedef enum {
    BUTTON_MODE_TOGGLE = 0,
    BUTTON_MODE_MOMENTARY_ON_OFF = 1,
    BUTTON_MODE_MULTISTATE_INPUT = 2,
} ButtonMode_t;

typedef enum {
    BTN_CLICK_IDLE = 0,
    BTN_CLICK_SINGLE = 1,
    BTN_CLICK_DOUBLE = 2,
    BTN_CLICK_TRIPLE = 3,
    BTN_CLICK_LONG = 4,
} ButtonClickState_t;

typedef enum {
    BTN_TOGGLE_EVENT,
    BTN_MOMENTARY_PRESSED_EVENT,
    BTN_MOMENTARY_RELEASED_EVENT,
    BTN_SINGLE_CLICK_EVENT,
    BTN_DOUBLE_CLICK_EVENT,
    BTN_TRIPLE_CLICK_EVENT,
    BTN_LONG_PRESSED_EVENT,
    BTN_LONG_RELEASED_EVENT,
    BTN_LONG_EMPTY_EVENT,
} ButtonEvent_t;

typedef void (*ButtonHandler_cb_t)(void* ctx);
typedef void (*ButtonResetHandler_cb_t)(void);
typedef void (*ButtonPressEvent_cb_t)(ButtonEvent_t eButtonEvent, uint8_t u8Endpoint);

typedef struct {
    ButtonHandler_cb_t pfOnPressCallback;
    ButtonResetHandler_cb_t pfOnResetCallback;
    ButtonPressEvent_cb_t pfOnPressEventCallback;
} ButtonCallbacks_t;

typedef struct {
    ButtonClickState_t eClickState;
    bool_t bPressed;
    uint16_t u16StateCycles;
    uint8_t u8Debounce;
} ButtonState_t;

typedef struct {
    const uint32_t u32DioMask;
    void* const pvLedConfig;
    const uint8_t u8Endpoint;
} Button_t;

typedef struct {
    const Button_t* pButton;
    ButtonState_t sButtonState;
} ButtonWithState_t;

typedef struct {
    const uint32_t u32DioMask;
    void* const pvLedConfig;
} ResetButton_t;

typedef struct {
    const ResetButton_t* pButton;
    ButtonState_t sButtonState;
} ResetButtonWithState_t;

extern uint8_t g_u8ButtonScanTimerID;
extern const size_t g_numButtonsPins;
extern const uint32_t g_asButtonsPins[];

extern const size_t g_numButtons;
extern const Button_t g_asButtons[];
extern const ResetButton_t g_sResetButton;
extern const uint32_t g_u32ButtonsInterruptMask;

extern const ButtonCallbacks_t g_sButtonsCallbacks;
extern ButtonWithState_t g_asButtonsStates[];

void BUTTONS_HW_Init(void);
void BUTTONS_SW_Init(void);
void BUTTONS_SetMode(ButtonMode_t eMode);

#endif /* APP_BUTTONS_H */
