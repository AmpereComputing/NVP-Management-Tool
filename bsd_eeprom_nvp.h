/**
 *
 * Copyright (c) 2021, Ampere Computing LLC
 *
 *  This program and the accompanying materials
 *  are licensed and made available under the terms and conditions of the BSD License
 *  which accompanies this distribution.  The full text of the license may be found at
 *  http://opensource.org/licenses/bsd-license.php
 *
 *  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 **/

#ifndef _BSD_EEPROM_NVP_H_
#define _BSD_EEPROM_NVP_H_

#include "utils.h"

#define EEPROM_256B_PAGE_SIZE           0x100
#define EEPROM_128B_PAGE_SIZE           0x80
#define EEPROM_32B_PAGE_SIZE            0x20
#define EEPROM_8B_PAGE_SIZE             0x8
#define EEPROM_MAX_PAGE_SIZE_SUPPORT    EEPROM_256B_PAGE_SIZE

#define EEPROM_RD_FLG                   0
#define EEPROM_WR_FLG                   1

#define MAX_EEPROM_ADDR_LEN             2

#define BSD_PARTITION_NAME              "nvparamb"
#define BSD_NVP_FILE                    "NVPBERLY"
/* EEPROM starts with 32 bytes of BSV */
#define BSD_OFFSET                      32
#define BSD_CHECKSUM_OFFSET             44
#define BSD_VALID_BIT_ARR_SIZE          8
#define BSD_NVP_HEADER_ADJUST           4

enum eeprom_type {
    EEPROM_256B     = 0,
    EEPROM_128B     = 1,
    EEPROM_32B      = 2,
    EEPROM_8B       = 3
};

/* EEPROM flash is on the physical I2C2, address 0x50 */
#define DEFAULT_I2C_BUS                 1
#define DEFAULT_I2C_EEPROM_ADDR         0x50
#define DEFAULT_I2C_EEPROM_TYPE         EEPROM_256B

/**
 * On AC03, the UART settings were added to the EEPROM NVPARAMs after the ROM
 * was already taped out. This workaround forces the checksum calculation of
 * this NVPARAM blob to exclude the additional bytes so both the ROM and FW
 * are able to verify it.
 **/
#define BSD_WA_BYTES_TO_CHECKSUM           148

extern int bsd_eeprom_handler (nvparm_ctrl_t *ctrl);

#endif  /* _BSD_EEPROM_NVP_H_ */