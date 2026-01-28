#ifndef DEVICE_DEFINITIONS_H
#define DEVICE_DEFINITIONS_H

// TODO: update pins
/* Button defintions */
#define BTN_CENTER_DIO (14)  // center button DIO
#define BTN_CENTER_MASK (1 << BTN_CENTER_DIO)
#define BTN_INTERRUPT_MASK BTN_CENTER_MASK
#define BTN_RESET_MASK BTN_INTERRUPT_MASK
/* Button defintions - end */

/* Led definitions */
#define LED_CENTER_DIO (18)
#define LED_CENTER_MASK (1UL << LED_CENTER_DIO)
#define RESET_LED_MASK LED_CENTER_MASK
/* Led definitions - end */

#define LEDS_AMOUNT 1
#define BUTTONS_AMOUNT 1

#endif /* DEVICE_DEFINITIONS_H */
