/*
 * Pavel Kirienko, 2013 (pavel.kirienko@gmail.com)
 */

#ifndef UTIL_H_
#define UTIL_H_

char* itoa(int n, char* pbuf);

/**
 * We can't use usual printf() because it is not reentrant
 */
void reentPrintf(const char* fmt, ...);

#endif
