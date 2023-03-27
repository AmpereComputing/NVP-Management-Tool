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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "utils.h"

/**
 * @fn log_printf
 *
 * @brief Print logs
 * @param  level [IN] - Console output selection
 *          fmt [IN] - Text to print
 **/
void log_printf (int level, const char *fmt, ...)
{
    FILE *fp = NULL;
    va_list ap;

    if (level == LOG_NORMAL) {
        fp = stdout;
    } else if (level == LOG_ERROR) {
        fp = stderr;
    } else if (level == LOG_DEBUG) {
#ifdef DEBUG
        fp = stdout;
#endif
    }
    if (fp) {
        va_start(ap, fmt);
        vfprintf(fp, fmt, ap);
        va_end(ap);
        fflush(fp);
    }

}

/**
 * @fn print_guid
 *
 * @brief Print the GUID
 * @param  guid [IN] - GUID number as array bytes
 **/
void print_guid(uint8_t guid[16])
{
    log_printf(LOG_NORMAL, "%.2X%.2X%.2X%.2X"
               "-%.2X%.2X"
               "-%.2X%.2X"
               "-%.2X%.2X-%.2X%.2X%.2X%.2X%.2X%.2X",
               guid[3], guid[2], guid[1], guid[0],
               guid[5], guid[4],
               guid[7], guid[6],
               guid[8], guid[9],
               guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
}
