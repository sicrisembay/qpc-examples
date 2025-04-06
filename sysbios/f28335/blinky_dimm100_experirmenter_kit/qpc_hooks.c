/*
 * qpc_hooks.c
 *
 *  Created on: 5 Apr 2025
 *      Author: Sicris Rey Embay
 */

#include "qpc.h"
#include "ti/sysbios/knl/Clock.h"

Q_DEFINE_THIS_MODULE("hooks")

Q_NORETURN Q_onError(char const * const module, int_t const id) {
    Q_UNUSED_PAR(module);
    Q_UNUSED_PAR(id);

    while(1) {

    }
}


static Clock_Struct qpc_tick_struct;

void QPC_tick(UArg a0)
{
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

