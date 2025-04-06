/*
 * uart.c
 *
 *  Created on: 6 Apr 2025
 *      Author: Sicris
 */

#include "qpc.h"
#include "DSP2833x_Device.h"
#include "xdc/std.h"
#include "xdc/runtime/Error.h"
#include "xdc/runtime/Assert.h"
#include "xdc/runtime/Types.h"
#include "ti/sysbios/family/c28/Hwi.h"
#include "ti/sysbios/BIOS.h"
#include "string.h"  // use of memset
#include "bsp.h"

Q_DEFINE_THIS_MODULE("uart")

#define BAUD_RATE   (115200U)

typedef struct {
    uint8_t prio;
} sender_t;

const sender_t uart_sender = {
   .prio = 0U
};

typedef struct {
    Hwi_Struct hwi_tx;
    uint8_t tx_buffer[256];
    uint16_t tx_tail_index;
    uint16_t tx_head_index;

    Hwi_Struct hwi_rx;
    uint8_t rx_buffer[256];
    uint16_t rx_tail_index;
    uint16_t rx_head_index;

    void (*tx_done_cb)(void);
    void (*rx_cb)(void);

} bsp_uart_t;

static bsp_uart_t bsp_uart;
static bool bInit = false;

static bool tx_buffer_empty(void)
{
    return (bsp_uart.tx_head_index == bsp_uart.tx_tail_index);
}

static bool tx_buffer_full(void)
{
    return ((bsp_uart.tx_head_index + 1) % sizeof(bsp_uart.tx_buffer)) == bsp_uart.tx_tail_index;
}

static bool rx_buffer_empty(void)
{
    return (bsp_uart.rx_head_index == bsp_uart.rx_tail_index);
}

static bool rx_buffer_full(void)
{
    return ((bsp_uart.rx_head_index + 1) % sizeof(bsp_uart.rx_buffer)) == bsp_uart.rx_tail_index;
}


static size_t tx_get_one(uint8_t * data)
{
    if(tx_buffer_empty()) {
        /* No items */
        return 0;
    }

    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    *data = bsp_uart.tx_buffer[bsp_uart.tx_tail_index];
    bsp_uart.tx_tail_index = (bsp_uart.tx_tail_index + 1) % sizeof(bsp_uart.tx_buffer);
    QF_CRIT_EXIT();
    return 1;
}


static void tx_write_one(const uint8_t data)
{
    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    if(tx_buffer_full()) {
        /* overwrite the oldest data */
        bsp_uart.tx_tail_index = (bsp_uart.tx_tail_index + 1) % sizeof(bsp_uart.tx_buffer);
    }
    bsp_uart.tx_buffer[bsp_uart.tx_head_index] = data;
    bsp_uart.tx_head_index = (bsp_uart.tx_head_index + 1) % sizeof(bsp_uart.tx_buffer);
    QF_CRIT_EXIT();
}


static size_t rx_get_one(uint8_t * data)
{
    if(rx_buffer_empty()) {
        /* No items */
        return 0;
    }

    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    *data = bsp_uart.rx_buffer[bsp_uart.rx_tail_index];
    bsp_uart.rx_tail_index = (bsp_uart.rx_tail_index + 1) % sizeof(bsp_uart.rx_buffer);
    QF_CRIT_EXIT();
    return 1;
}


static void rx_write_one(const uint8_t data)
{
    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    if(rx_buffer_full()) {
        /* overwrite the oldest data */
        bsp_uart.rx_tail_index = (bsp_uart.rx_tail_index + 1) % sizeof(bsp_uart.rx_buffer);
    }
    bsp_uart.rx_buffer[bsp_uart.rx_head_index] = data;
    bsp_uart.rx_head_index = (bsp_uart.rx_head_index + 1) % sizeof(bsp_uart.rx_buffer);
    QF_CRIT_EXIT();
}


static void tx_hwi_handler(UArg arg)
{
    uint16_t fifo_tx_cnt = SciaRegs.SCIFFTX.bit.TXFFST;
    uint8_t data = 0;

    if(tx_buffer_empty() && (fifo_tx_cnt == 0)) {
        /* Disable TX FIFO interrupt */
        SciaRegs.SCIFFTX.bit.TXFFIENA = 0;
        if(bsp_uart.tx_done_cb) {
            bsp_uart.tx_done_cb();
        }
    } else {
        while((fifo_tx_cnt < 16) && !tx_buffer_empty()) {
            tx_get_one(&data);
            SciaRegs.SCITXBUF = data;
            fifo_tx_cnt = SciaRegs.SCIFFTX.bit.TXFFST;
        }
    }
    /* Clear Interrupt Flag */
    SciaRegs.SCIFFTX.bit.TXFFINTCLR = 1;
}


static void rx_hwi_handler(UArg arg)
{
    uint8_t rx_data;
    uint16_t rx_count;

    rx_count = SciaRegs.SCIFFRX.bit.RXFFST;
    if(rx_count > 0) {
        for(uint16_t i = 0; i < rx_count; i++) {
            rx_data = SciaRegs.SCIRXBUF.bit.RXDT;
            rx_write_one(rx_data);
        }
    }
    /* Clear Interrupt Flag */
    SciaRegs.SCIFFRX.bit.RXFFOVRCLR = 1;
    SciaRegs.SCIFFRX.bit.RXFFINTCLR = 1;

    if(bsp_uart.rx_cb) {
        bsp_uart.rx_cb();
    }
}


void UART_init(void (*tx_done_cb)(void), void (*rx_cb)(void))
{
    Error_Block eb;
    Hwi_Params hwiParams;
    Types_FreqHz bios_cpu_freq;
    BIOS_getCpuFreq(&bios_cpu_freq);
    const uint32_t sci_clk_freq = bios_cpu_freq.lo / 4U;
    const uint16_t brr = (uint16_t)((sci_clk_freq / (BAUD_RATE * 8U)) - 1U);

    if(bInit != true) {
        bsp_uart_t * const me = &bsp_uart;
        memset(&bsp_uart, 0, sizeof(bsp_uart));

        me->tx_done_cb = tx_done_cb;
        me->rx_cb = rx_cb;

        EALLOW;
        SysCtrlRegs.PCLKCR0.bit.SCIAENCLK = 1;
        GpioCtrlRegs.GPAPUD.bit.GPIO29 = 0;
        GpioCtrlRegs.GPAMUX2.bit.GPIO29 = 1;

        GpioCtrlRegs.GPAPUD.bit.GPIO28 = 0;
        GpioCtrlRegs.GPAQSEL2.bit.GPIO28 = 3;
        GpioCtrlRegs.GPAMUX2.bit.GPIO28 = 1;
        EDIS;

        Error_init(&eb);
        Hwi_Params_init(&hwiParams);
        hwiParams.enableAck = true;
        hwiParams.instance->name = Q_this_module_;
        Hwi_construct(&me->hwi_tx, 97, tx_hwi_handler, &hwiParams, &eb);
        Q_ASSERT(Error_check(&eb) == FALSE);

        Hwi_Params_init(&hwiParams);
        hwiParams.enableAck = true;
        hwiParams.instance->name = Q_this_module_;
        Hwi_construct(&me->hwi_rx, 96, rx_hwi_handler, &hwiParams, &eb);
        Q_ASSERT(Error_check(&eb) == FALSE);

        /*
         * SCI and FIFO Initalization
         */
        SciaRegs.SCICCR.all = 0x0007;
        SciaRegs.SCICTL1.all = 0x0003;
        SciaRegs.SCICTL2.bit.TXINTENA = 1;
        SciaRegs.SCICTL2.bit.RXBKINTENA = 1;
        SciaRegs.SCIHBAUD = (brr >> 8) & 0x00FF;
        SciaRegs.SCILBAUD = brr & 0x00FF;
        SciaRegs.SCIFFTX.all = 0xC000;
        SciaRegs.SCIFFRX.all = 0x0021;
        SciaRegs.SCIFFCT.all = 0x0000;
        /* Release from reset and Enable FIFO */
        SciaRegs.SCICTL1.bit.SWRESET = 1;
        SciaRegs.SCIFFTX.bit.TXFIFOXRESET = 1;
        SciaRegs.SCIFFRX.bit.RXFIFORESET = 1;
        /*
         * Enable SCI interrupt
         */
        PieCtrlRegs.PIEIER9.bit.INTx1 = 1;
        PieCtrlRegs.PIEIER9.bit.INTx2 = 1;
        IER |= M_INT9;

        bInit = true;
    }
}


size_t UART_send(uint8_t * const pBuf, const size_t buflen)
{
    Q_REQUIRE(pBuf != NULL);

    size_t i = 0;
    uint16_t j = 0;
    bool fifo_first = false;
    const uint16_t tx_fifo_count = SciaRegs.SCIFFTX.bit.TXFFST;

    if((tx_fifo_count == 0) && (tx_buffer_empty())) {
        fifo_first = true;
        j = 0;
    }

    for(i = 0; i < buflen; i++) {
        if(fifo_first && (j < 16)) {
            SciaRegs.SCITXBUF = pBuf[i] & 0x00FF;
            j++;
        } else {
            if(tx_buffer_full()) {
                break;
            }
            tx_write_one(pBuf[i]);
        }
    }

    /* Reenable Tx interrupt */
    SciaRegs.SCIFFTX.bit.TXFFIENA = 1;

    return(i);
}


size_t UART_receive(uint8_t * const pBuf, const size_t bufLen)
{
    Q_REQUIRE(pBuf != NULL);
    size_t i = 0;

    if((bufLen == 0) || (rx_buffer_empty())) {
        return 0;
    }

    for(i = 0; (i < bufLen) && (!rx_buffer_empty()); i++) {
        rx_get_one(&(pBuf[i]));
    }

    return i;
}
