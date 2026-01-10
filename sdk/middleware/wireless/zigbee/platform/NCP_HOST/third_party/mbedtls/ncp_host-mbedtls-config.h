/*
 * Copyright 2020 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef NCP_HOST_MBEDTLS_CONFIG_H_
#define NCP_HOST_MBEDTLS_CONFIG_H_

#define MCUX_ENABLE_TRNG_AS_ENTROPY_SEED

/* Undef this flag to make sure to use hardware entropy */
#undef MBEDTLS_NO_DEFAULT_ENTROPY_SOURCES

#endif // NCP_HOST_MBEDTLS_CONFIG_H_
