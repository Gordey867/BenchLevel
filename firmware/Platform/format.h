#ifndef BENCH_FORMAT_H
#define BENCH_FORMAT_H
#include <stddef.h>
int bench_format(char *, size_t, const char *, ...) __attribute__((format(printf,3,4)));
#endif
