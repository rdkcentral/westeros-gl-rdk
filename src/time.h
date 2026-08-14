#pragma once

#include <time.h>

inline long long GetTimeUS()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000 + (long long)ts.tv_nsec / 1000;
}