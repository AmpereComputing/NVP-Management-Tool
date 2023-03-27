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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "gpt.h"
#include "spinorfs.h"

#include "utils.h"

struct gpt_partition partitions[GPT_ENTRIES];
static int part_used_num = 0;

/* Use default the LBA size unless Makefile define */
#ifdef GPT_LBA_SIZE
int lba_size = GPT_LBA_SIZE;
#else
int lba_size = DEFAULT_GPT_LBA_SIZE;
#endif

/**
 * @fn is_used_partition
 *
 * @brief Verify an GPT partition entry in-used
 * @param  entry [IN] - The GPT partition entry need to be verified
 * @return  0 - Unused Entry
 *          1 - Used Entry
 **/
static inline int is_used_partition (const struct gpt_partition *entry)
{
    int is_used = 0;
    for (int i = 0; i < GPT_GUID_SIZE; i++) {
        if (entry->partition_type_guid[i] != 0) {
            is_used = 1;
            break;
        }
    }
    return is_used;
}

/**
 * @fn print_partition_name
 *
 * @brief Print the GPT partition name
 * @param  name [IN] - The partition name as array bytes
 * @param  length [IN] - Length of the above array
 **/
static void print_partition_name(char *name, int length)
{
    int j = 0;
    for (int i = 0; i < length; i++) {
        if (name[i] == 0)
            j++;
        else
            j = 0;

        if (j > 2)
            return;
        else if (j == 0)
            log_printf(LOG_NORMAL, "%c", name[i]);
    }
}


/**
 * @fn trim_partition_name
 *
 * @brief Trim NULL characters in middle of the GPT partition name
 * @param  name [IN] - The partition name as array bytes
 * @param  length [IN] - Length of the above array
 **/
static void trim_partition_name(char *name, int length)
{
    int j = 0;
    char * tmp = (char *)malloc(length * sizeof(char));
    if (tmp == NULL) {
        log_printf(LOG_ERROR, "Cannot allocate memory\n");
        return;
    }
    memset(tmp, 0, length);
    for (int i = 0, k = 0; i < length; i++) {
        if (name[i] == 0)
            j++;
        else
            j = 0;

        if (j > 2)
            break;
        else if (j == 0) {
            tmp[k] = name[i];
            k++;
        }
    }
    memcpy(name, tmp, length);
    free(tmp);
}

/**
 * @fn spinorfs_gpt_disk_info
 *
 * @brief Parse GPT info
 * @param  dev_fd [IN] - File descriptor of the flash
 * @param  show_gpt [IN] - Show the GPT info into console
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_gpt_disk_info (int dev_fd, int show_gpt)
{
    int ret = EXIT_SUCCESS;
    uint8_t *lba_buff = NULL;
    uint8_t *entry_buff = NULL;
    uint32_t entry_size = GPT_ENTRY_SIZE;
    uint32_t entry_num = 0;
    uint64_t start_entry_lba = 0;
    int tmp = 0;
    int part = -1;
    struct gpt_protective_mbr *pmbr = NULL;
    struct gpt_header *ph = NULL;
    struct gpt_partition *pentry = NULL;

    part_used_num = 0;
    lba_buff = (uint8_t *) malloc(lba_size);
    if (!lba_buff) {
        log_printf(LOG_ERROR, "Failed to allocate LBA memory\n");
        ret = EXIT_FAILURE;
        return ret;
    }
    memset(lba_buff, 0x00, lba_size);

    /* read LBA0 - Protective MBR */
    tmp = read(dev_fd, lba_buff, lba_size);
    if (tmp != lba_size) {
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    pmbr = (struct gpt_protective_mbr *) lba_buff;
    /* Verify MBR signature */
    if (MBR_SIGNATURE !=
        (uint16_t)(pmbr->signature[0] | (pmbr->signature[1]) << 8)) {
        log_printf(LOG_ERROR, "Invalid Protective MBR signature: 0x%x-0x%x\n",
                   pmbr->signature[0], pmbr->signature[1]);
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    /* Verify OSType */
    for (int i = 0 ; i < GPT_PARITION_RECORD_NUM; i++) {
        if (pmbr->partition_record[i].os_type == PMBR_OSTYPE) {
            part = i;
            break;
        }
    }
    if (part == -1) {
        log_printf(LOG_ERROR, "Invalid MBR partition record\n");
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    /* LBA of the GPT partition header */
    if (pmbr->partition_record[part].starting_lba !=
        GPT_PRIMARY_PARTITION_TABLE_LBA) {
        log_printf(LOG_ERROR,
                   "Invalid starting LBA of GPT Header: %d - 0x%.16llx\n",
                   part, pmbr->partition_record[part].starting_lba);
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }

    memset(lba_buff, 0x00, lba_size);
    /* seek to LBA1 position for GPT Header */
    if (lseek(dev_fd, lba_size, SEEK_SET) < 0) {
        log_printf(LOG_ERROR, "Problem in seek to LBA1\n");
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }

    tmp = read(dev_fd, lba_buff, lba_size);
    if (tmp != lba_size) {
        log_printf(LOG_ERROR, "Read LBA1 failed\n");
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    ph = (struct gpt_header *) lba_buff;
    /* Verify GPT Header signature */
    if (ph->signature != GPT_HEADER_SIGNATURE) {
        log_printf(LOG_ERROR,
                   "Incorrect GPT Header signature: 0x%.16llx\n",
                   ph->signature);
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    /* Verify GPT Header size */
    if (ph->header_size < GPT_HEADER_MIN_SIZE ||
        ph->header_size > (uint32_t)lba_size) {
        log_printf(LOG_ERROR, "GPT Header size incorrect: %d\n",
                   ph->header_size);
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    /* FIXME Don't need to verify GPT Header CRC */

    /* Get size of partition entry */
    entry_size = ph->partition_entry_size;
    if (entry_size < GPT_ENTRY_SIZE) {
        log_printf(LOG_ERROR, "Invalid partition entry size: 0x%.8x\n",
                   entry_size);
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }

    /* Get number of partition entry */
    entry_num = ph->num_partition_entries;

    /* Get starting LBA of partition entry */
    start_entry_lba = ph->partition_entry_lba;

    /* FIXME Don't need to verify Partition Entry Array CRC */

    /* Display all existing partition info */
    entry_buff = (uint8_t *) malloc (entry_size * sizeof(uint8_t));
    if (!entry_buff) {
        log_printf(LOG_ERROR, "Cannot allocate memory\n");
        ret = EXIT_FAILURE;
        goto out_free_lba;
    }
    /* seek to starting partition entry LBA */
    if (lseek(dev_fd, lba_size * start_entry_lba, SEEK_SET) < 0) {
        log_printf(LOG_ERROR, "Problem in seek to LBA: 0x%.16llx\n",
                   start_entry_lba);
        ret = EXIT_FAILURE;
        goto out_free_entry;
    }

    for (int i = 0; i < (int)entry_num; i ++) {
        int used_partition = 0;
        memset(entry_buff, 0x00, entry_size);
        tmp = read(dev_fd, entry_buff, entry_size);
        if (tmp != (int)entry_size) {
            log_printf(LOG_ERROR, "Read failed\n");
            ret = EXIT_FAILURE;
            goto out_free_entry;
        }
        pentry = (struct gpt_partition *) entry_buff;

        /* Verify if the partition is used */
        used_partition = is_used_partition(pentry);
        if (used_partition) {
            if (show_gpt) {
                log_printf(LOG_NORMAL, "[GPT Partition #%d]\n", i);
                log_printf(LOG_NORMAL, "  Name: ");
                print_partition_name((char *)pentry->partition_name,
                                     GPT_NAME_LEN);
                log_printf(LOG_NORMAL, "\n");
                log_printf(LOG_NORMAL, "  GUID: ");
                print_guid(pentry->unique_partition_guid);
                log_printf(LOG_NORMAL, "\n");
                log_printf(LOG_NORMAL,
                           "--------------------------------------------\n");
            } else {
                /* Remove NULL character from partition name */
                trim_partition_name((char *)pentry->partition_name,
                                     GPT_NAME_LEN);
            }
            memcpy(&partitions[part_used_num], (void *)entry_buff,
                   entry_size * sizeof(uint8_t));
            part_used_num++;
        }
    }

out_free_entry:
    if (entry_buff)
        free(entry_buff);
out_free_lba:
    if (lba_buff)
        free(lba_buff);

    return ret;
}

/**
 * @fn spinorfs_gpt_part_guid_info
 *
 * @brief Get offset and size of the partition via GUID
 * @param  guid [IN] - Partition GUID number
 * @param  offset [OUT] - The partition offset at the flash
 * @param  size [OUT] - The partition size in byte
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_gpt_part_guid_info(uint8_t *guid, uint32_t *offset, uint32_t *size)
{
    int i = 0, ret = EXIT_SUCCESS;
    for (i = 0; i < part_used_num; i++) {
        if (memcmp(guid, partitions[i].unique_partition_guid,
            GUID_BYTE_SIZE * sizeof(uint8_t)) == 0) {
            *offset = partitions[i].starting_lba * lba_size;
            *size = (partitions[i].ending_lba -
                     partitions[i].starting_lba + 1) * lba_size;
            break;
        }
    }
    if (i == part_used_num) {
        log_printf(LOG_ERROR, "not found GUID, part num:%d\n", part_used_num);
        ret = EXIT_FAILURE;
    }
    return ret;
}

/**
 * @fn spinorfs_gpt_part_name_info
 *
 * @brief Get offset and size of the partition via partition name
 * @param  part [IN] - Partition name
 * @param  offset [OUT] - The partition offset at the flash
 * @param  size [OUT] - The partition size in byte
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_gpt_part_name_info(char *part, uint32_t *offset, uint32_t *size)
{
    int i = 0, ret = EXIT_SUCCESS;

    for (i = 0; i < part_used_num; i++) {
        if (strcmp(part, (char *)partitions[i].partition_name) == 0) {
            *offset = partitions[i].starting_lba * lba_size;
            *size = (partitions[i].ending_lba -
                     partitions[i].starting_lba + 1) * lba_size;
            break;
        }
    }
    if (i == part_used_num) {
        log_printf(LOG_ERROR, "not found name:%s, part num:%d\n",
                   part, part_used_num);
        ret = EXIT_FAILURE;
    }
    return ret;
}