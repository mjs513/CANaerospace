/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef PROJECT_CHCONF_H_
#define PROJECT_CHCONF_H_

#define CH_FREQUENCY                    1000

#define CH_USE_HEAP                     TRUE
#define CH_USE_DYNAMIC                  FALSE
#define CH_USE_MAILBOXES                FALSE
#define CH_USE_MESSAGES                 FALSE
#define CH_USE_CONDVARS                 FALSE

void systemHaltHook(void);
#define SYSTEM_HALT_HOOK                systemHaltHook

void systemTickHook(void);
#define SYSTEM_TICK_EVENT_HOOK          systemTickHook

#if defined(DEBUG) && DEBUG
#   define CH_DBG_SYSTEM_STATE_CHECK       TRUE
#   define CH_DBG_ENABLE_CHECKS            TRUE
#   define CH_DBG_ENABLE_ASSERTS           TRUE
#   define CH_DBG_ENABLE_STACK_CHECK       TRUE
#   define CH_DBG_FILL_THREADS             TRUE
#   define CH_DBG_THREADS_PROFILING        TRUE
#elif defined(RELEASE) && RELEASE
#   define CH_DBG_THREADS_PROFILING        FALSE
#else
#   error "Invalid configuration: Either DEBUG or RELEASE must be true"
#endif

#include "../../chibios/os/kernel/templates/chconf.h"

#endif
