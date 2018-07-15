/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include "srvport/srvport.h"
#include "util.h"
#include "sys.h"

#define REENT_PRINTF_BUF_SIZE 512

static void _reverse(char* s)
{
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i<j; i++, j--)
    {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}

char* itoa(int n, char* pbuf)
{
    int sign, i = 0;
    if ((sign = n) < 0)
        n = -n;
    do
        pbuf[i++] = n % 10 + '0';
    while ((n /= 10) > 0);
    if (sign < 0)
        pbuf[i++] = '-';
    pbuf[i] = '\0';
    _reverse(pbuf);
    return pbuf;
}

void reentPrintf(const char* fmt, ...)
{
    static char buf[REENT_PRINTF_BUF_SIZE];
    static xSemaphoreHandle mutex = NULL;

    va_list vl;
    va_start(vl, fmt);

    if (mutex == NULL)
    {
        mutex = xSemaphoreCreateMutex();
        ASSERT_ALWAYS(mutex != NULL);
    }

    if (xSemaphoreTake(mutex, portMAX_DELAY))
    {
        vsnprintf(buf, REENT_PRINTF_BUF_SIZE, fmt, vl);
        srvportPrint(buf);
        xSemaphoreGive(mutex);
    }
    va_end(vl);
}
