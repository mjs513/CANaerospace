/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <errno.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <stm32f10x.h>
#include <core_cm3.h>

#include <stm32f10x_usart.h>
#include "srvport/srvport.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"

extern unsigned int _ebss;

#undef errno
extern int errno;

char *__env[1] = { 0 };
char **environ = __env;

int _write(int file, char *ptr, int len);

void _exit(int status)
{
    const char* pstr[] = { "EXIT", NULL };
    srvportDieWithHonour(pstr);
}

int _close(int file)
{
    return -1;
}

int _execve(char *name, char **argv, char **env)
{
    errno = ENOMEM;
    return -1;
}

int _fork()
{
    errno = EAGAIN;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

int _getpid()
{
    return 1;
}

int _isatty(int file)
{
    switch (file)
    {
    case STDOUT_FILENO:
    case STDERR_FILENO:
    case STDIN_FILENO:
        return 1;
    default:
        //errno = ENOTTY;
        errno = EBADF;
        return 0;
    }
}

int _kill(int pid, int sig)
{
    errno = EINVAL;
    return (-1);
}

int _link(char *old, char *new)
{
    errno = EMLINK;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    return 0;
}

caddr_t _sbrk(int incr)
{
    static unsigned char *heap_end = NULL;
    unsigned char *prev_heap_end;

    __disable_irq();
    {
        if (heap_end == NULL)
            heap_end = (unsigned char *) &_ebss;

#ifndef FREERTOS
        // This check is useless under FreeRTOS because task's stacks will be allocated inside the heap.
        const uint32_t msp = __get_MSP();
        if (heap_end + incr >= (unsigned char*)msp)
        {
            __enable_irq();
            srvportPrint("HEAP-STACK COLLISION\n");
            srvportFlush();
            errno = ENOMEM;
            return (caddr_t) -1;
        }
#endif
        prev_heap_end = heap_end;
        heap_end += incr;
    }
    __enable_irq();
    return (caddr_t) prev_heap_end;
}

int _read(int file, char *ptr, int len)
{
    int c, num = 0;
    if (file != STDIN_FILENO)
    {
        errno = EBADF;
        return -1;
    }
    while (num < len)
    {
        c = srvportGetChar();
        if (c < 0)
            continue;
        *ptr++ = (char)c;
        num++;
    }
    return num;
}

int _stat(const char *filepath, struct stat *st)
{
    st->st_mode = S_IFCHR;
    return 0;
}

clock_t _times(struct tms *buf)
{
    return -1;
}

int _unlink(char *name)
{
    errno = ENOENT;
    return -1;
}

int _wait(int *status)
{
    errno = ECHILD;
    return -1;
}

int _write(int file, char *ptr, int len)
{
    if (file != STDERR_FILENO && file != STDOUT_FILENO)
    {
        errno = EBADF;
        return -1;
    }
    return srvportWrite((const uint8_t*)ptr, len);
}
