/**
 *
 * Copyright (c) 2023, Ampere Computing LLC
 *
 * This program and the accompanying materials are licensed and made available under the terms
 * and conditions of the BSD-3-Clause License which accompanies this distribution. The full text of the
 * license may be found within the LICENSE file at the root of this distribution or online at
 * https://opensource.org/license/bsd-3-clause/
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 **/

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>

/* size of read/write buffer */
#define BUFSIZE                             (10 * 1024)

/* error levels */
#define LOG_NORMAL                          1
#define LOG_ERROR                           2
#define LOG_DEBUG                           3

#define EXIT_FAILURE                        1
#define EXIT_SUCCESS                        0

#define GUID_BYTE_SIZE                      16

#define PERCENTAGE(x, total)                (((x) * 100) / (total))
#define KB(x)                               ((x) / 1024)

/**
 * @fn log_printf
 *
 * @brief Print logs
 * @param  level [IN] - Console output selection
 *          fmt [IN] - Text to print
 **/
extern void log_printf (int level, const char *fmt, ...);

/**
 * @fn print_guid
 *
 * @brief Print the GUID
 * @param  guid [IN] - GUID number as array bytes
 **/
extern void print_guid(uint8_t guid[16]);

#endif /* _UTILS_H_ */