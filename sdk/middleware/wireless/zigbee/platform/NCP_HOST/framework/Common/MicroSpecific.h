/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __MICROSPECIFIC_H__
#define __MICROSPECIFIC_H__

#include <signal.h>

#define MICRO_RESTORE_INTERRUPTS(x) \
{                                                   \
    sigset_t x;                                     \
    sigemptyset (&x);                               \
    sigaddset(&x, SIGALRM);                         \
    sigprocmask(SIG_UNBLOCK, &x, NULL);             \
}

#define MICRO_DISABLE_AND_SAVE_INTERRUPTS(x)        \
{                                                   \
    sigset_t x;                                     \
    sigemptyset (&x);                               \
    sigaddset(&x, SIGALRM);                         \
    sigprocmask(SIG_BLOCK, &x, NULL);               \
}

#define MICRO_DISABLE_INTERRUPTS()

#endif /* __MICROSPECIFIC_H__ */
