#ifndef OVTIME_H
#define OVTIME_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define SEC_TO_MS(sec) ((sec)*1000)
#define SEC_TO_US(sec) ((sec)*1000000)
#define SEC_TO_NS(sec) ((sec)*1000000000)

#define NS_TO_SEC(ns)   ((ns)/1000000000)
#define NS_TO_MS(ns)    ((ns)/1000000)
#define NS_TO_US(ns)    ((ns)/1000)

static inline uint64_t ovtime_ms()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ms = SEC_TO_MS((uint64_t)ts.tv_sec) + NS_TO_MS((uint64_t)ts.tv_nsec);
    return ms;
}

static inline uint64_t ovtime_us()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t us = SEC_TO_US((uint64_t)ts.tv_sec) + NS_TO_US((uint64_t)ts.tv_nsec);
    return us;
}

static inline uint64_t ovtime_ns()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    uint64_t ns = SEC_TO_NS((uint64_t)ts.tv_sec) + (uint64_t)ts.tv_nsec;
    return ns;
}

#endif
