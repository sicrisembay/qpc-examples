#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>
#include <ti/sysbios/BIOS.h>
#include "qpc.h"
#include "bsp.h"
#include "blinky.h"
#include "uart.h"

typedef struct {
    QEvt super;
    uint8_t data[16];
} medium_pool;


typedef struct {
    QEvt super;
    uint8_t data[32];
} large_pool;


static QF_MPOOL_EL(QEvt) small_pool_sto[32];
static QF_MPOOL_EL(medium_pool) medium_pool_sto[32];
static QF_MPOOL_EL(large_pool) large_pool_sto[32];
static QSubscrList subscribe_sto[MAX_PUB_SIG];


Int main()
{ 
    /* Initialize QF framework */
    QF_init();

    /* Initialize Event Pool */
    QF_poolInit(small_pool_sto, sizeof(small_pool_sto), sizeof(small_pool_sto[0]));
    QF_poolInit(medium_pool_sto, sizeof(medium_pool_sto), sizeof(medium_pool_sto[0]));
    QF_poolInit(large_pool_sto, sizeof(large_pool_sto), sizeof(large_pool_sto[0]));
    QF_psInit(subscribe_sto, Q_DIM(subscribe_sto));

    QS_INIT((void *)0);

    Blinky_ctor();

    QF_run();

    /* does not return */
    return(0);
}
