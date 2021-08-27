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

#ifndef _GPT_H_
#define _GPT_H_

#include <stdint.h>
#include <stdio.h>

#define MBR_BOOTCODE_SIZE                   440
#define GPT_NAME_LEN                        72
#define GPT_ENTRIES                         128
#define GPT_ENTRY_SIZE                      128
#define GPT_PRIMARY_LBA                     1
#define GPT_PARITION_RECORD_NUM             4
#define GPT_HEADER_MIN_SIZE                 92
#define GPT_GUID_SIZE                       16

/* Signature - "EFI PART" */
#define GPT_HEADER_SIGNATURE                0x5452415020494645ULL

#define GPT_PRIMARY_PARTITION_TABLE_LBA     0x00000001ULL

#define PMBR_OSTYPE                         0xEE
#define MBR_SIGNATURE                       0xAA55
#define DEFAULT_GPT_LBA_SIZE                512

#define SHOW_GPT_DISABLE                    0
#define SHOW_GPT_ENABLE                     1


struct gpt_record {
    uint8_t boot_indicator;
    uint8_t starting_chs[3];
    uint8_t os_type;
    uint8_t ending_chs[3];
    uint32_t starting_lba;
    uint32_t size_in_lba;
} __attribute__((packed));

struct gpt_protective_mbr {
    uint8_t boot_code[MBR_BOOTCODE_SIZE];
    uint32_t unique_mbr_signature;
    uint16_t unknown;
    struct gpt_record partition_record[GPT_PARITION_RECORD_NUM];
    uint8_t signature[2];
} __attribute__((packed));

struct gpt_partition {
    uint8_t partition_type_guid[GPT_GUID_SIZE];
    uint8_t unique_partition_guid[GPT_GUID_SIZE];
    uint64_t starting_lba;
    uint64_t ending_lba;
    uint64_t attributes;
    uint8_t partition_name[GPT_NAME_LEN];
} __attribute__((packed));

struct gpt_header {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    uint8_t disk_guid[GPT_GUID_SIZE];
    uint64_t partition_entry_lba;
    uint32_t num_partition_entries;
    uint32_t partition_entry_size;
    uint32_t partition_array_crc32;
} __attribute__((packed));

extern int get_gpt_disk_info (int dev_fd, int show_gpt);
extern int get_gpt_part_guid_info(uint8_t *guid,
                                  uint32_t *offset, uint32_t *size);
extern int get_gpt_part_name_info(char *part,
                                  uint32_t *offset, uint32_t *size);
#endif  /* _GPT_H_ */