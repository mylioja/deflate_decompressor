//
//    Copyright (C) 2020 Martti Ylioja
//    SPDX-License-Identifier: GPL-3.0-or-later
//
#include "performance_timer.h"

#ifdef _MSC_VER
#define WINDOWS
#endif

#ifdef WINDOWS
#include <Windows.h>
#else
#include <sys/time.h>
#endif

namespace PerformanceTimer {

    int64_t get_timestamp()
    {
#ifdef WINDOWS
        LARGE_INTEGER start;
        QueryPerformanceCounter(&start);
        return start.QuadPart;
#else
        struct timeval time;
        gettimeofday(&time, 0);
        return uint64_t(1000000) * uint64_t(time.tv_sec) + time.tv_usec;
#endif
    }


    int64_t get_elapsed_time(int64_t start)
    {
#ifdef WINDOWS
        int64_t elapsed = get_timestamp() - start;

        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);

        //  convert to microseconds
        elapsed *= 1000000;
        elapsed /= frequency.QuadPart;

        return elapsed;
#else
        return get_timestamp() - start;
#endif
    }

}
