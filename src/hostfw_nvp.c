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

#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>

#include "hostfw_nvp.h"
#include "spinorfs.h"

/**
 * @fn find_host_mtd_partition
 *
 * @brief Detect the Host MTD partition
 * @param  ctrl [IN] - Input control structure
 * @param  fd [OUT] - Device file descriptor output
 * @return  0 - Success
 *          1 - Failure
 **/
int find_host_mtd_partition (nvparm_ctrl_t *ctrl, int *fd)
{
    int ret = EXIT_SUCCESS;
    int nMTDDeviceNumber= -1;
    char *argv_dev_ptr;
    char temp_mtd[4] = {0}, *temp;
    char proc_buf[80];
    int dev_fd = -1;
    struct mtd_info_user mtd;
    char mtd_dev[MTD_DEV_SIZE] = {0};

    if (ctrl->options[OPTION_DEV]) {
        strncpy(&mtd_dev[0], ctrl->device_name, sizeof(mtd_dev));
        goto open_dev;
    }
    /* Finding the MTD partition for host SPI chip */
    FILE *proc_fp;
    if ((proc_fp = fopen(PROC_MTD_INFO, "r")) == NULL) {
        log_printf(LOG_ERROR, "Unable to open %s to get MTD info...\n",
            PROC_MTD_INFO);
        ret = EXIT_FAILURE;
        goto out;
    }

    while (fgets(proc_buf, sizeof(proc_buf), proc_fp) != NULL) {
        if (strstr(proc_buf, HOST_SPI_FLASH_MTD_NAME )) {
            temp  = strtok(proc_buf, ":");
            if(temp == NULL) {
                log_printf(LOG_ERROR,"Error in finding the BIOS Partition \n");
                fclose(proc_fp);
                ret = EXIT_FAILURE;
                goto out;
            }

            memcpy((char *)&temp_mtd, (char *)&temp[3], (strlen(temp) - 3));
            nMTDDeviceNumber = atoi(temp_mtd);
        }
    }
    fclose(proc_fp);
    if (nMTDDeviceNumber == -1) {
        log_printf(LOG_ERROR,"Unable to find HOST SPI MTD partition...\n");
        ret = EXIT_FAILURE;
        goto out;
    }

    ret = snprintf(&mtd_dev[0], MTD_DEV_SIZE, "/dev/mtd%d", nMTDDeviceNumber);
    if(ret  >= MTD_DEV_SIZE || ret < 0) {
        log_printf(LOG_ERROR,"Buffer Overflow.\n");
        ret = EXIT_FAILURE;
        goto out;
    }

open_dev:
    argv_dev_ptr = &mtd_dev[0];

    dev_fd  = open(argv_dev_ptr, O_SYNC | O_RDWR);
    if (dev_fd < 0) {
        log_printf(LOG_ERROR, "Failed to open: %s\n", argv_dev_ptr);
        ret = EXIT_FAILURE;
        goto out;
    }

    /* Get the MTD device info */
    ret = ioctl(dev_fd, MEMGETINFO, &mtd);
    if (ret < 0) {
        close(dev_fd);
        ret = EXIT_FAILURE;
        goto out;
    }
    *fd = dev_fd;

out:
    return ret;
}

/**
 * @fn dump_nvp_hdlr
 *
 * @brief Dump NVPARAM file into specific target file
 * @param  nvp_file [IN] - NVPARAM file to be dumped from
 * @param  dump_file [IN] - Target file stores the dumped NVPARAM data
 * @return  0 - Success
 *          1 - Failure
 **/
int dump_nvp_hdlr(char *nvp_file, char *dump_file)
{
    int ret = EXIT_SUCCESS;
    uint8_t buff[DEFAULT_PAGE_SIZE] = {0};
    FILE *fp = NULL;
    int byte_cnt = 0;
    uint32_t offset= 0;

    if (nvp_file == NULL || dump_file == NULL) {
        return EXIT_FAILURE;
    }
    /* Open nvp_file as READ ONLY */
    ret = spinorfs_open(nvp_file, SPINORFS_O_RDONLY);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n", ret, nvp_file);
        return EXIT_FAILURE;
    }

    fp = fopen(dump_file, "w");
    if (fp == NULL) {
        log_printf(LOG_ERROR, "Cannot open file %s\n", nvp_file);
        spinorfs_close();
        return EXIT_FAILURE;
    }
    while ((byte_cnt = spinorfs_read((char *)buff, offset, sizeof(buff))) > 0) {
        fwrite(buff, 1, byte_cnt, fp);
        if (ferror(fp)) {
            log_printf(LOG_ERROR, "ERROR in write to file %s\n", dump_file);
            ret = EXIT_FAILURE;
            break;
        }
        offset += byte_cnt;
    }

    spinorfs_close();
    if (fp)
        fclose(fp);

    return ret;
}

/**
 * @fn upload_nvp_hdlr
 *
 * @brief Upload NVPARAM file into specific target NVPARAM file
 * @param  nvp_file [IN] - NVPARAM file to be overwritten
 * @param  upload_file [IN] - Upload NVPARAM file
 * @return  0 - Success
 *          1 - Failure
 **/
int upload_nvp_hdlr(char *nvp_file, char *upload_file)
{
    int ret = EXIT_SUCCESS;
    FILE *fp = NULL;
    ssize_t sz, bytes;
    uint8_t *buf = NULL, *write_buf = NULL;
    uint32_t offset= 0;

    if (nvp_file == NULL || upload_file == NULL) {
        return EXIT_FAILURE;
    }

    /* Open nvp_file as WRITE ONLY */
    ret = spinorfs_open(nvp_file, SPINORFS_O_WRONLY | SPINORFS_O_TRUNC);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n", ret, nvp_file);
        return EXIT_FAILURE;
    }

    fp = fopen(upload_file, "rb");
    if (fp == NULL) {
        log_printf(LOG_ERROR, "Cannot open file %s\n", upload_file);
        spinorfs_close();
        return EXIT_FAILURE;
    }

    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    log_printf(LOG_DEBUG, "[upload] new file %s size: %d\n", upload_file, sz);

    buf = malloc(sz);
    if (!buf) {
        log_printf(LOG_ERROR, "Not enough memory\n");
        ret = EXIT_FAILURE;
        goto out_upload;
    }
    fseek(fp, 0, SEEK_SET);
    fread(buf, sz, 1, fp);
    write_buf = buf;

loop:
    if (sz >= DEFAULT_PAGE_SIZE)
        bytes = DEFAULT_PAGE_SIZE;
    else
        bytes = sz;

    ret = spinorfs_write((char *)write_buf, offset, bytes);

    if (ret != bytes) {
        log_printf(LOG_ERROR, "ERROR write to NVP file\n");
        ret = EXIT_FAILURE;
    } else {
        sz -= bytes;
        write_buf += bytes;
        offset += bytes;
        if (sz > 0)
            goto loop;
        log_printf(LOG_DEBUG, "DONE write NVP file: %d\n", offset);
    }

    if (buf)
        free(buf);

out_upload:
    spinorfs_close();
    if (fp)
        fclose(fp);

    return ret;
}

/**
 * @fn operate_field_hdlr
 *
 * @brief Operate on specific NVP field and its associated valid bit
 * @param  ctrl [IN] - Input control structure
 * @return  0 - Success
 *          1 - Failure
 **/
int operate_field_hdlr(nvparm_ctrl_t *ctrl)
{
    int ret = EXIT_SUCCESS;
    struct nvp_header header = {0};
    uint32_t offset = 0;
    uint64_t nvp_value = 0;
    uint8_t nvp_valid = 0;
    uint8_t *val_bit_arr = NULL;
    uint8_t val_bit_arr_sz = 0;
    uint8_t need_update_cs = 0;
    uint8_t *data_cs = NULL;
    struct nvp_header *p_nvp_header = NULL;

    if (ctrl->nvp_file == NULL) {
        return EXIT_FAILURE;
    }

    /* Open nvp_file */
    ret = spinorfs_open(ctrl->nvp_file, SPINORFS_O_RDWR);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n",
                   ret, ctrl->nvp_file);
        return EXIT_FAILURE;
    }
    /* Read NVP header at start of the file */
    offset = 0;
    ret = spinorfs_read((char *)&header, offset, sizeof(header));
    if (ret != (int)sizeof(header)) {
        log_printf(LOG_ERROR, "ERROR in read NVP header\n");
        spinorfs_close();
        return EXIT_FAILURE;
    }
    if (ctrl->field_index >= header.count) {
        log_printf(LOG_ERROR, "Invalid NVP field index\n");
        spinorfs_close();
        return EXIT_FAILURE;
    }

    /* Calculate size of nvp valid bit array */
    val_bit_arr_sz = header.count / NVP_VAL_BIT_PER_ELE +
                     ((header.count % NVP_VAL_BIT_PER_ELE) ? 1 : 0);
    val_bit_arr = (uint8_t *)malloc(val_bit_arr_sz);
    if (val_bit_arr == NULL) {
        log_printf(LOG_ERROR, "Can't allocate memory\n");
        spinorfs_close();
        return EXIT_FAILURE;
    }
    memset(val_bit_arr, 0, val_bit_arr_sz);

    /* Calculate offset of nvp valid bit array */
    offset = sizeof(header);
    ret = spinorfs_read((char *)val_bit_arr, offset, val_bit_arr_sz);
    if (ret != (int)val_bit_arr_sz) {
        log_printf(LOG_ERROR, "ERROR in read NVP valid bit array\n");
        free(val_bit_arr);
        spinorfs_close();
        return EXIT_FAILURE;
    }

    #ifdef DEBUG
    log_printf(LOG_DEBUG, "Valid bit array value:");
    for (int i = 0; i < val_bit_arr_sz; i++) {
        log_printf(LOG_DEBUG, " 0x%.2x", val_bit_arr[i]);
    }
    log_printf(LOG_DEBUG, "\n");
    log_printf(LOG_DEBUG, "NVP HEADER:\n");
    log_printf(LOG_DEBUG, "field_size: %d, flags:%d, count:%d, data_offset:%d\n",
    header.field_size, header.flags, header.count, header.data_offset);
    #endif

    if (ctrl->options[OPTION_R]) {
        /* Calculate offset of nvp field */
        offset = header.data_offset +
                 ctrl->field_index * header.field_size;
        ret = spinorfs_read((char *)&nvp_value, offset, header.field_size);
        if (ret != (int)header.field_size) {
            log_printf(LOG_ERROR, "ERROR in read NVP field\n");
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }

        /* Get the bit of nvp field index */
        nvp_valid = UINT8_GET_BIT(val_bit_arr, ctrl->field_index);
        if (header.field_size == NVP_FIELD_SIZE_1) {
            log_printf(LOG_NORMAL, "0x%.2x 0x%.2x\n",
                       nvp_valid, (uint8_t)nvp_value);
        } else if (header.field_size == NVP_FIELD_SIZE_4) {
            log_printf(LOG_NORMAL, "0x%.2x 0x%.8x\n",
                       nvp_valid, (uint32_t)nvp_value);
        } else if (header.field_size == NVP_FIELD_SIZE_8) {
            log_printf(LOG_NORMAL, "0x%.2x 0x%.16llx\n",
                       nvp_valid, nvp_value);
        } else {
            log_printf(LOG_ERROR, "Unsupported field size: %d\n",
                       header.field_size);
        }
    } else if (ctrl->options[OPTION_W]) {
        ret = UINT64_VALIDATE_NVP(header.field_size, ctrl->nvp_data);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR,
                       "NVP data exceeds MAX value of field size %d bytes\n",
                       header.field_size);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }

        offset = header.data_offset + ctrl->field_index * header.field_size;
        ret = spinorfs_write((char *)&(ctrl->nvp_data), offset, header.field_size);
        if (ret != (int)header.field_size) {
            log_printf(LOG_ERROR, "ERROR in write NVP field: %d\n",
                       ctrl->field_index);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }

        /* Update valid bit */
        if (ctrl->options[OPTION_V]) {
            if (ctrl->valid_bit == NVP_FIELD_IGNORE) {
                UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
            } else if (ctrl->valid_bit == NVP_FIELD_SET) {
                UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
            } else {
                log_printf(LOG_ERROR, "Unsupported valid bit value: 0x%.2x\n",
                           ctrl->valid_bit);
                free(val_bit_arr);
                spinorfs_close();
                return EXIT_FAILURE;
            }
        } else {
            /* Set the nvp field by default */
            UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
        }
        offset = sizeof(header);
        ret = spinorfs_write((char *)val_bit_arr, offset, val_bit_arr_sz);
        if (ret != (int)val_bit_arr_sz) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        if (header.flags & NVPARAM_HEADER_FLAGS_CHECKSUM_VALID) {
            need_update_cs = 1;
        }
    } else if (ctrl->options[OPTION_V]) {
        if (ctrl->valid_bit == NVP_FIELD_IGNORE) {
            UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
        } else if (ctrl->valid_bit == NVP_FIELD_SET) {
            UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
        } else {
            log_printf(LOG_ERROR, "Unsupported valid bit value: 0x%.2x\n",
                       ctrl->valid_bit);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        offset = sizeof(header);
        ret = spinorfs_write((char *)val_bit_arr, offset, val_bit_arr_sz);
        if (ret != (int)val_bit_arr_sz) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        if (header.flags & NVPARAM_HEADER_FLAGS_CHECKSUM_VALID) {
            need_update_cs = 1;
        }
    } else if (ctrl->options[OPTION_E]) {
        /* Erase NVP field by set its all data to 1 */
        nvp_value = ULLONG_MAX;
        offset = header.data_offset + ctrl->field_index * header.field_size;
        ret = spinorfs_write((char *)&(nvp_value), offset, header.field_size);
        if (ret != (int)header.field_size) {
            log_printf(LOG_ERROR, "ERROR in write NVP field: %d\n",
                       ctrl->field_index);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }

        /* Set the associated valid bit of NVP field to 0 */
        UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
        offset = sizeof(header);
        ret = spinorfs_write((char *)val_bit_arr, offset, val_bit_arr_sz);
        if (ret != (int)val_bit_arr_sz) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        if (header.flags & NVPARAM_HEADER_FLAGS_CHECKSUM_VALID) {
            need_update_cs = 1;
        }
    }
    #ifdef DEBUG
    log_printf(LOG_DEBUG, "Valid bit array value after update:");
    for (int i = 0; i < val_bit_arr_sz; i++) {
        log_printf(LOG_DEBUG, " 0x%.2x", val_bit_arr[i]);
    }
    log_printf(LOG_DEBUG, "\n");
    #endif
    if (need_update_cs) {
        /* Read all NVP data */
        data_cs = (uint8_t *)malloc (header.length);
        if (data_cs == NULL) {
            log_printf(LOG_ERROR, "Can't allocate memory\n");
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        ret = spinorfs_read((char *)data_cs, 0, header.length);
        if (ret != (int)header.length) {
            log_printf(LOG_ERROR, "ERROR in read NVP blobs\n");
            free(val_bit_arr);
            free(data_cs);
            spinorfs_close();
            return EXIT_FAILURE;
        }
        /* Clear current checksum */
        p_nvp_header = (struct nvp_header *) data_cs;
        p_nvp_header->checksum = 0;

        header.checksum = calculate_sum8(data_cs, header.length);
        log_printf(LOG_DEBUG, "New checksum: 0x%x\n", header.checksum);
        free(data_cs);

        /* Write checksum to file */
        ret = spinorfs_write((char *)&header, 0, sizeof (header));
        if (ret != (int)sizeof (header)) {
            log_printf(LOG_ERROR, "ERROR in write NVP blobs\n");
            free(val_bit_arr);
            spinorfs_close();
            return EXIT_FAILURE;
        }
    }

    free(val_bit_arr);
    spinorfs_close();

    return EXIT_SUCCESS;
}

/**
 * @fn spinor_handler
 *
 * @brief The main handler for SPI NOR flash
 * @param  ctrl [IN] - The nvparam controller structure
 * @return  0 - Success
 *         -1 - Failure
 **/
int spinor_handler (nvparm_ctrl_t *ctrl)
{
    int ret = EXIT_SUCCESS;
    uint32_t size = 0, offset = 0;
    int dev_fd = -1;

    /* Finding the MTD partition for host SPI chip */
    ret = find_host_mtd_partition(ctrl, &dev_fd);
    if (ret != EXIT_SUCCESS) {
        return ret;
    }

    /* Print GPT header */
    if (ctrl->options[OPTION_P]) {
        ret = spinorfs_gpt_disk_info(dev_fd, SHOW_GPT_ENABLE);
        goto out_dev;
    } else {
        ret = spinorfs_gpt_disk_info(dev_fd, SHOW_GPT_DISABLE);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    }

    /* Verify input partition name/GUID to get offset + size before mount */
    if (ctrl->options[OPTION_T]) {
        ret = spinorfs_gpt_part_name_info(ctrl->nvp_part, &offset, &size);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    } else if (ctrl->options[OPTION_U]) {
        ret = spinorfs_gpt_part_guid_info(ctrl->nvp_guid, &offset, &size);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    } else {
        ret = EXIT_FAILURE;
        goto out_dev;
    }

    /* Mount partition */
    ret = spinorfs_mount(dev_fd, size, offset);
    if (ret != EXIT_SUCCESS) {
        goto out_dev;
    }
    /* Dump nvp file */
    if (ctrl->options[OPTION_D]) {
        /* Find the file in mounted partition */
        ret = dump_nvp_hdlr(ctrl->nvp_file, ctrl->dump_file);
        goto out_unmount;
    }
    /* Upload/overwrite nvp file */
    if (ctrl->options[OPTION_O]) {
        ret = upload_nvp_hdlr(ctrl->nvp_file, ctrl->upload_file);
        goto out_unmount;
    }
    /* Operate on nvp field */
    if (ctrl->options[OPTION_R] || ctrl->options[OPTION_W] ||
        ctrl->options[OPTION_V] || ctrl->options[OPTION_E]) {
        ret = operate_field_hdlr(ctrl);
    }

out_unmount:
    spinorfs_unmount();
out_dev:
    if (dev_fd != -1) {
        close(dev_fd);
    }
    return ret;
}