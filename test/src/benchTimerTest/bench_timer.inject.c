#ifndef _IN_TEST_SRC
    #include "bench_timer.c"
#endif
#include "bench_timer.inject.h"
int test_bench_elapsed_ns(const cplat_timespec *start, const cplat_timespec *end, uint64_t *output)
{
    return elapsed_ns(start, end, output);
}
