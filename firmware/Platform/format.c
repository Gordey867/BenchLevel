#include "format.h"
#include <stdarg.h>
#include <stdbool.h>
/* The telemetry contract only needs strings and 32-bit decimal/hex integers.
   Count the full result even when the destination is too short, like snprintf. */
int bench_format(char *out, size_t capacity, const char *fmt, ...)
{
    va_list ap;va_start(ap,fmt);size_t used=0;
#define EMIT(c) do { char byte=(c);if (used+1<capacity) out[used]=byte;++used; } while(0)
    while (*fmt) {
        if (*fmt!='%') { EMIT(*fmt++);continue; }
        ++fmt;unsigned width=0;char pad=' ';
        if (*fmt=='0') { pad='0';++fmt; }
        while (*fmt>='0' && *fmt<='9') width=width*10U+(unsigned)(*fmt++-'0');
        bool wide=*fmt=='l';if (wide) ++fmt;
        char spec=*fmt++;
        if (spec=='s') { const char *s=va_arg(ap,const char *);while (*s) EMIT(*s++);continue; }
        if (spec=='%') { EMIT('%');continue; }
        unsigned long value;bool negative=false;
        if (spec=='d') {
            long v=wide?va_arg(ap,long):(long)va_arg(ap,int);
            negative=v<0;value=negative?0UL-(unsigned long)v:(unsigned long)v;
        } else value=wide?va_arg(ap,unsigned long):(unsigned long)va_arg(ap,unsigned int);
        unsigned radix=spec=='X'?16U:10U;char digits[24];unsigned n=0;
        do { unsigned d=(unsigned)(value%radix);digits[n++]=(char)(d<10?'0'+d:'A'+d-10);value/=radix; } while(value);
        if (negative) EMIT('-');
        while (width>n+(negative?1U:0U)) { EMIT(pad);--width; }
        while (n) EMIT(digits[--n]);
    }
    va_end(ap);
    if (capacity) out[used<capacity?used:capacity-1]=0;
    return (int)used;
}
