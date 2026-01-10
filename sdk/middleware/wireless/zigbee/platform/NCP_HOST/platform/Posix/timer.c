/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <time.h>
#include <assert.h>

uint32_t zbPlatGetTime(void)
{
    static struct timespec ts_start = {0};
    struct timespec ts = {0};

    if ((ts_start.tv_sec == 0) && (ts_start.tv_nsec == 0))
    {
        assert(clock_gettime(CLOCK_MONOTONIC, &ts_start) == 0);
    }
    assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);

    /* convert time in msec */
    return (uint32_t)((ts.tv_nsec - ts_start.tv_nsec) / 1e6 + (ts.tv_sec - ts_start.tv_sec) * 1000);
}
