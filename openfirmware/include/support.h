#ifndef SUPPORT_H
#define SUPPORT_H

/* freestanding string/memory routines (src/support.c) */
void *memset(void *dst, int c, unsigned long n);
void *memcpy(void *dst, const void *src, unsigned long n);
unsigned long strlen(const char *s);
char *strncpy(char *dst, const char *src, unsigned long n);

#endif /* SUPPORT_H */
