#ifndef APP_GENERIC_EVENTS_H_
#define APP_GENERIC_EVENTS_H_

#include <jendefs.h>

#include "app_buttons_actions.h"

#ifdef DEBUG_EVENTS
#define TRACE_EVENTS TRUE
#else
#define TRACE_EVENTS FALSE
#endif

typedef enum {
    APP_NO_EVENT,
    APP_WAKE_UP_EVENT,
    APP_RESET_DEVICE_EVENT,
    APP_BUTTON_EVENT,
} APP_teEventType;

typedef struct {
    uint8_t u8Endpoint;
    teButtonAction eButtonAction;
} APP_tsEventButton;

typedef struct {
    APP_teEventType eType;
    union {
        APP_tsEventButton sButton;
    } uEvent;
} APP_tsEvent;

void APP_vSendWakeUpEvent(void);
void APP_vSendResetDeviceEvent(void);
void APP_vSendButtonEvent(uint8_t u8Endpoint, teButtonAction eButtonAction);
void APP_vProcessEvents(void);

#endif /*APP_GENERIC_EVENTS_H_*/
