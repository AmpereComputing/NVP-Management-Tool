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

#ifndef _UTILS_H_
#define _UTILS_H_

#include <stdint.h>
#include <limits.h>

#define GUID_STR_LEN                        36
#define GUID_BYTE_SIZE                      16

/* size of read/write buffer */
#define BUFSIZE                             (10 * 1024)
#define FLAG_NON                            0x00

/* error levels */
#define LOG_NORMAL                          1
#define LOG_ERROR                           2
#define LOG_DEBUG                           3

#define EXIT_FAILURE                        1
#define EXIT_SUCCESS                        0

#define MAX_NAME_LENGTH                     128
#define MAX_PART_NAME_LEN                   72
#define MAX_CMD_LEN                         100

#define PERCENTAGE(x, total)                (((x) * 100) / (total))
#define KB(x)                               ((x) / 1024)

#define UINT8_GET_BIT(arr,idx)    (((arr)[(idx)/8] & (1 << ((idx)%8))) != 0)
#define UINT8_SET_BIT(arr,idx)    ((arr)[(idx)/8] |= (1 << ((idx)%8)))
#define UINT8_CLEAR_BIT(arr,idx)  ((arr)[(idx)/8] &= ~(1 << ((idx)%8)))

#define NVP_REVISION                    0x0100

#define NVP_FIELD_SIZE_1                    1
#define NVP_FIELD_SIZE_4                    4
#define NVP_FIELD_SIZE_8                    8
#define MAX_NVP_FIELD_SIZE                  8

#define NVP_FIELD_IGNORE                    0
#define NVP_FIELD_SET                       1
#define NVP_VAL_BIT_PER_ELE                 8
#define NVP_SIGNATURE_SIZE                  8

#define UINT64_VALIDATE_NVP(field_size, data) \
        (((((field_size) == NVP_FIELD_SIZE_1) && \
         ((uint64_t)(data) <= (uint64_t)(UCHAR_MAX))) || \
         (((field_size) == NVP_FIELD_SIZE_4) && \
         ((uint64_t)(data) <= (uint64_t)(ULONG_MAX))) || \
         (((field_size) == NVP_FIELD_SIZE_8) && \
         ((uint64_t)(data) <= (uint64_t)(ULLONG_MAX)))) ? 0 : 1)

enum {
    OPTION_T = 0,
    OPTION_U,
    OPTION_F,
    OPTION_I,
    OPTION_R,
    OPTION_E,
    OPTION_W,
    OPTION_V,
    OPTION_D,
    OPTION_B,
    OPTION_S,
    OPTION_P,
    OPTION_H,
    OPTION_VER,
    OPTION_O,
    MAX_OPTIONS
};

enum {
    SPINOR = 0,
    EEPROM,
    MAX_DEVICE
};

enum nvparam_header_flags {
    NVPARAM_HEADER_FLAGS_WRITEABLE = 0x0001,       // Read-Only if 0
    NVPARAM_HEADER_FLAGS_VALIDATION_ONLY = 0x0002, // Fields may not be used in secure systems if 1
    NVPARAM_HEADER_FLAGS_CHECKSUM_VALID = 0x0004,  // Must treat checksum as don't-care if 0
    // All other bits reserved and must be 0
};

/*
 * In order to keep things human readable the signatures are
 * defined as strings. However, we convert the strings into
 * 64 bit unsigned ints for comparison. When we do that we
 * get backwards endianess and so the signature below is
 * defined as a set of characters rather than an 64 bit type
 * to compensate for the comparison.
 */
#define SIG_TO_UINT64(sig_string_ptr) *((uint64_t *)(sig_string_ptr))
#define SIGS_ARE_EQUAL(sig1, sig2)    \
                            (SIG_TO_UINT64((sig1)) == SIG_TO_UINT64((sig2)))
struct nvp_header {
    uint8_t signature[NVP_SIGNATURE_SIZE];
    uint16_t length;
    uint16_t revision;
    uint8_t checksum;
    uint8_t field_size;
    uint16_t flags;
    uint16_t count;
    uint16_t data_offset;
    uint32_t reserved;    // Padding required to place "valid" bit-array on 8-byte boundary
    /* The valid bit array is moved to outside of struct */
} __attribute__((packed));

typedef struct nvparm_ctrl {
    uint8_t device;
    uint8_t options[MAX_OPTIONS];
    char nvp_part[MAX_PART_NAME_LEN];
    uint8_t nvp_guid[GUID_BYTE_SIZE];
    char nvp_file[MAX_NAME_LENGTH];
    uint16_t field_index;
    uint64_t nvp_data;
    uint8_t valid_bit;
    char dump_file[MAX_NAME_LENGTH];
    char upload_file[MAX_NAME_LENGTH];
    uint8_t i2c_bus;
    uint8_t slave_addr;
} nvparm_ctrl_t;

extern void log_printf (int level, const char *fmt, ...);
extern void print_guid(uint8_t guid[16]);
extern int guid_str2int (char *guid_str, uint8_t *guid_int);
extern uint8_t calculate_sum8(const uint8_t *data, uint8_t length);

#endif /* _UTILS_H_ */