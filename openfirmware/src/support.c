/*
 * support.c - freestanding string/memory routines (no libc on the
 * target; gcc may also emit calls to memcpy/memset for struct copies).
 */

#include <stdint.h>
#include <stddef.h>

void *memset(void *dst, int c, size_t n)
{
    volatile uint8_t *d = dst;
    while (n--)
        *d++ = (uint8_t)c;
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    volatile uint8_t *d = dst;
    const uint8_t *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}
