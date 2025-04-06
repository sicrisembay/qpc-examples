/*
 * qpc_hooks.c
 *
 *  Created on: 5 Apr 2025
 *      Author: Sicris Rey Embay
 */

#include "qpc.h"
#include "ti/sysbios/knl/Clock.h"
#include "DSP2833x_Device.h"
#include "bsp.h"
#include "uart.h"

Q_DEFINE_THIS_MODULE("hooks")

static QSTimeCtr tick_count = 0U;

Q_NORETURN Q_onError(char const * const module, int_t const id) {
    Q_UNUSED_PAR(module);
    Q_UNUSED_PAR(id);

    while(1) {

    }
}


static Clock_Struct qpc_tick_struct;

void QPC_tick(UArg a0)
{
    tick_count++;
    QTIMEEVT_TICK((void *)&QPC_tick);
}


void QF_onStartup(void)
{
    Error_Block eb;
    Clock_Params clkParams;
    Clock_Handle clkHdl_AO;

    /* Create 1ms periodic clock for QPC framework */
    Error_init(&eb);
    Clock_Params_init(&clkParams);
    clkParams.instance->name = "QPC_tick";
    clkParams.period = 1;
    clkParams.startFlag = true;
    Clock_construct(&qpc_tick_struct, QPC_tick, 1, &clkParams);
    clkHdl_AO = Clock_handle(&qpc_tick_struct);

    QF_CRIT_STAT
    QF_CRIT_ENTRY();
    Q_ASSERT_INCRIT(200, (clkHdl_AO != NULL) && (Error_check(&eb) == FALSE));
    QF_CRIT_EXIT();
}


void QF_onCleanup(void)
{
}


static uint8_t qsTxBuf[512];
static uint8_t qsRxBuf[256];
static Semaphore_Struct sem_rx;
static Semaphore_Handle sem_rx_handle;
static Task_Struct task_qspy_worker;
static uint16_t stkSto[256];

void tx_done_callback(void)
{

}

void rx_callback(void)
{
    Semaphore_post(sem_rx_handle);
}


static void qspy_worker(UArg a0, UArg a1)
{
    uint8_t * pBlock;
    uint16_t txLen;

    while(1) {
        if(Semaphore_pend(sem_rx_handle, 2)) {
            uint8_t rxData = 0;
            size_t rxLen = 0;
            while(1) {
                rxLen = UART_receive(&rxData, 1);
                if(rxLen == 0) {
                    break;
                } else {
                    QS_RX_PUT(rxData & 0x00FF);
                }
            }

            QS_rxParse();
        }

        txLen = 16;  // peripheral FIFO size
        QF_CRIT_STAT
        QF_CRIT_ENTRY();
        pBlock = (uint8_t *)QS_getBlock(&txLen);
        QF_CRIT_EXIT();
        if(txLen > 0) {
            UART_send(pBlock, txLen);
        }
    }
}


uint8_t QS_onStartup(void const *arg)
{
    Error_Block eb;
    Task_Params taskParams;
    Semaphore_Params semParams;

    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&sem_rx, 0, &semParams);
    sem_rx_handle = Semaphore_handle(&sem_rx);

    QS_initBuf(qsTxBuf, sizeof(qsTxBuf));
    QS_rxInitBuf(qsRxBuf, sizeof(qsRxBuf));
    UART_init(&tx_done_callback, &rx_callback);

    Error_init(&eb);
    Task_Params_init(&taskParams);
    taskParams.priority = 1U;
    taskParams.stackSize = 256;
    taskParams.stack = stkSto;
    taskParams.instance->name = "qspy worker";
    Task_construct(&task_qspy_worker, &qspy_worker, &taskParams, &eb);

    return (uint8_t)1;
}


void QS_onCleanup(void)
{
    /// TODO
}


QSTimeCtr QS_onGetTime(void)
{
    return tick_count;
}


void QS_onFlush(void)
{
}


void QS_onReset(void)
{
    /* Tickle dog */
    EALLOW;
    SysCtrlRegs.WDKEY = 0x0055;
    SysCtrlRegs.WDKEY = 0x00AA;
    EDIS;

    /* Enable watchdog */
    EALLOW;
    SysCtrlRegs.WDCR = 0x0000;  /* writing value other than b101 to WDCHK will immediately resets the device */
    EDIS;

    /* Should not reach here */
    while(1);
}


void QS_onCommand(uint8_t cmdId,
                  uint32_t param1, uint32_t param2, uint32_t param3)
{
    (void)cmdId;
    (void)param1;
    (void)param2;
    (void)param3;
}


