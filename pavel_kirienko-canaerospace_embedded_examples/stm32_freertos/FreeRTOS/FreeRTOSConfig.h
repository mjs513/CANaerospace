/*
 * Example configuration
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#define configUSE_PREEMPTION		1
#define configUSE_IDLE_HOOK			1
#define configUSE_TICK_HOOK			1
#define configCPU_CLOCK_HZ			( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ			( ( portTickType ) 1000 )
#define configMAX_PRIORITIES		( ( unsigned portBASE_TYPE ) 3 )
#define configMINIMAL_STACK_SIZE	( ( unsigned short ) 512 )
#define configTOTAL_HEAP_SIZE		( ( size_t ) ( 40 * 1024 ) )
#define configMAX_TASK_NAME_LEN		( 9 )
#define configUSE_TRACE_FACILITY	0
#define configUSE_16_BIT_TICKS		0
#define configIDLE_SHOULD_YIELD		1

#define configUSE_CO_ROUTINES 		0
#define configMAX_CO_ROUTINE_PRIORITIES ( 2 )

#define configUSE_MUTEXES				1
#define configUSE_COUNTING_SEMAPHORES 	0
#define configUSE_ALTERNATIVE_API 		0
#define configUSE_RECURSIVE_MUTEXES		1
#define configQUEUE_REGISTRY_SIZE		0
#define configGENERATE_RUN_TIME_STATS	0

#if defined(RELEASE_BUILD)

#  define configCHECK_FOR_STACK_OVERFLOW	1
#  define configASSERT(x)

#elif defined(DEBUG_BUILD)

extern void assert_failed(const char*, int);

#  define configCHECK_FOR_STACK_OVERFLOW	2
#  define configASSERT(x) if ((x) == 0) assert_failed(__FILE__, __LINE__)

#else
#  error "Either DEBUG_BUILD or RELEASE_BUILD must be defined"
#endif

#define configUSE_MALLOC_FAILED_HOOK   1

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */

#define INCLUDE_vTaskPrioritySet		0
#define INCLUDE_uxTaskPriorityGet		0
#define INCLUDE_vTaskDelete				0
#define INCLUDE_vTaskCleanUpResources	0
#define INCLUDE_vTaskSuspend			1
#define INCLUDE_vTaskDelayUntil			1
#define INCLUDE_vTaskDelay				1

/* This is the raw value as per the Cortex-M3 NVIC.  Values can be 255
(lowest) to 0 (1?) (highest). */
#define configKERNEL_INTERRUPT_PRIORITY 		255
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 	191 /* equivalent to 0xb0, or priority 11. */


/* This is the value being used as per the ST library which permits 16
priority values, 0 to 15.  This must correspond to the
configKERNEL_INTERRUPT_PRIORITY setting.  Here 15 corresponds to the lowest
NVIC value of 255. */
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY	15

/*-----------------------------------------------------------
 * Macros required to setup the timer for the run time stats.
 *-----------------------------------------------------------*/
/* The run time stats time base just uses the existing high frequency timer
test clock.  All these macros do is clear and return the high frequency
interrupt count respectively. */
extern unsigned long ulRunTimeStatsClock;
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() ulRunTimeStatsClock = 0
#define portGET_RUN_TIME_COUNTER_VALUE() ulRunTimeStatsClock

/* FreeRTOS-CLI */
#define configCOMMAND_INT_MAX_OUTPUT_SIZE 256

#endif /* FREERTOS_CONFIG_H */

