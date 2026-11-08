#ifndef BENCH_TIMER_INJECT_H
#define BENCH_TIMER_INJECT_H

#include <cplat/clock/timespec.h>
#ifdef __cplusplus
extern "C"
{
#endif
    int test_bench_elapsed_ns(const cplat_timespec *start, const cplat_timespec *end, uint64_t *output);
#ifdef __cplusplus
}
#endif

#endif /* BENCH_TIMER_INJECT_H */
