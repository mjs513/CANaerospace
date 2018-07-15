/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include "sys.h"
#include <stm32f10x.h>
#include <stdio.h>

#if !CH_DBG_ENABLED
const char *dbg_panic_msg;
#endif

static uint64_t _sys_time_usec = 0;

void systemTickHook(void)
{
    _sys_time_usec += 1000000ull / CH_FREQUENCY;
}

uint64_t sysTimestampMicros(void)
{
    register uint64_t ret = 0;
    chSysLock();
    ret = _sys_time_usec;
    chSysUnlock();
    return ret;
}

static void writepoll(const char* str)
{
    for (const char *p = str; *p; p++)
    {
        while (!(USART2->SR & USART_SR_TXE)) { }
        USART2->DR = *p;
    }
}

void systemHaltHook(void)
{
    port_disable();
    writepoll("\nPANIC [");
    const Thread *pthread = chThdSelf();
    if (pthread && pthread->p_name)
        writepoll(pthread->p_name);
    writepoll("] ");
    if (dbg_panic_msg != NULL)
        writepoll(dbg_panic_msg);
    writepoll("\n");
}

void _exit(int status){
    (void)status;
    chSysHalt();
    while(TRUE) { }
}

pid_t _getpid(void) { return 1; }

void _kill(pid_t id) { (void)id; }
