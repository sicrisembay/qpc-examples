#include "DSP2833x_Device.h"
#include "led.h"

void LED_init(void)
{
    EALLOW;
    GpioCtrlRegs.GPAPUD.bit.GPIO31 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO31 = 1;
    GpioCtrlRegs.GPAMUX2.bit.GPIO31 = 0;

    GpioCtrlRegs.GPBPUD.bit.GPIO34 = 1;
    GpioCtrlRegs.GPBDIR.bit.GPIO34 = 1;
    GpioCtrlRegs.GPBMUX1.bit.GPIO34 = 0;
    EDIS;

    LED_off(LED_1);
    LED_off(LED_2);
}


void LED_on(LED_ID_T id)
{
    if(id < N_LED) {
        switch(id) {
            case LED_1: {
                GpioDataRegs.GPACLEAR.bit.GPIO31 = 1;
                break;
            }
            case LED_2: {
                GpioDataRegs.GPBCLEAR.bit.GPIO34 = 1;
                break;
            }
            default: {
                break;
            }
        }
    }
}


void LED_off(LED_ID_T id)
{
    if(id < N_LED) {
        switch(id) {
            case LED_1: {
                GpioDataRegs.GPASET.bit.GPIO31 = 1;
                break;
            }
            case LED_2: {
                GpioDataRegs.GPBSET.bit.GPIO34 = 1;
                break;
            }
            default: {
                break;
            }
        }
    }
}
