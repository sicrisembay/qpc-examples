#ifndef BSP_LED_H_
#define BSP_LED_H_

typedef enum {
    LED_1,
    LED_2,

    N_LED
} LED_ID_T;

void LED_init(void);
void LED_on(LED_ID_T id);
void LED_off(LED_ID_T id);

#endif /* BSP_LED_H_ */

