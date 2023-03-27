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
 * @fn hex_to_bin()
 *
 * @brief Convert a hex digit to its real value.
 * @param ch - ASCII character represents hex digit
 * @return         - Error: -1; Other: Real value
 **/
static int hex_to_bin (char ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return ch - '0';
    ch = tolower(ch);
    if ((ch >= 'a') && (ch <= 'f'))
        return ch - 'a' + 10;
    return -1;
}

/**
 * GUID 16 bytes format:
 * 0        9    14   19   24
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 *    le     le   le   be       be
 **/
const uint8_t guid_index[GUID_BYTE_SIZE] =
                                        {3,2,1,0,5,4,7,6,8,9,10,11,12,13,14,15};

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

/**
 * @fn guid_str_valid()
 *
 * @brief Validate input string as GUID format.
 * @param guid_str - pointer to GUID string
 * @return         - Valid: 1; Invalid: 0
 **/
static int guid_str_valid (const char *guid_str)
{
    int i, valid;

    if ((guid_str == NULL) || (strlen(guid_str) != GUID_STR_LEN))
        return 0;

    for (i = 0, valid = 1; guid_str[i] && valid; i++) {
        switch (i) {
        case 8:
        case 13:
        case 18:
        case 23:
            valid = (guid_str[i] == '-');
            break;
        default:
            valid = isxdigit(guid_str[i]);
            break;
        }
    }

    if (i != GUID_STR_LEN || !valid)
        return 0;

    return 1;
}

/**
 * @fn guid_str2int()
 *
 * @brief Convert string GUID to integer array data.
 * @param guid_str - pointer to GUID string
 * @param guid_int - pointer to allocated integer array data
 * @return         - Success: 0; Error: 1
 **/
int guid_str2int (char *guid_str, uint8_t *guid_int)
{
    static const uint8_t si[GUID_BYTE_SIZE] =
                                {0,2,4,6,9,11,14,16,19,21,24,26,28,30,32,34};
    unsigned int i;

    if (!guid_str_valid(guid_str)) {
        return EXIT_FAILURE;
    }

    for (i = 0; i < GUID_BYTE_SIZE; i++) {
        int hi = hex_to_bin(guid_str[si[i] + 0]);
        int lo = hex_to_bin(guid_str[si[i] + 1]);

        guid_int[guid_index[i]] = (uint8_t) ((hi << 4) | lo);
    }

    return EXIT_SUCCESS;
}

/**
 * @fn calculate_sum8
 *
 * @brief Calculate checksum.
 * @param  data [IN] - Data need to be calculated checksum
 * @param  length [IN] - Data size
 * @return  checksum
 **/
uint8_t calculate_sum8(const uint8_t *data, uint8_t length)
{
    uint8_t ret = 0;

    for (uint8_t i = 0; i < length; i++) {
        ret = (uint8_t)(ret + data[i]);
    }
    ret = (uint8_t)(0x100 - ret);

    log_printf(LOG_DEBUG, "Checksum ret: 0x%x\n", ret);

    return (ret);
}