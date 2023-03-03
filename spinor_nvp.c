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

#include "spinor_nvp.h"
#include "spinor_func.h"
#include "gpt.h"

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
        ret = get_gpt_disk_info(dev_fd, SHOW_GPT_ENABLE);
        goto out_dev;
    } else {
        ret = get_gpt_disk_info(dev_fd, SHOW_GPT_DISABLE);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    }

    /* Verify input partition name/GUID to get offset + size before mount */
    if (ctrl->options[OPTION_T]) {
        ret = get_gpt_part_name_info(ctrl->nvp_part, &offset, &size);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    } else if (ctrl->options[OPTION_U]) {
        ret = get_gpt_part_guid_info(ctrl->nvp_guid, &offset, &size);
        if (ret != EXIT_SUCCESS) {
            ret = EXIT_FAILURE;
            goto out_dev;
        }
    } else {
        ret = EXIT_FAILURE;
        goto out_dev;
    }

    /* Mount partition */
    ret = spinor_lfs_mount(size, offset);
    if (ret != EXIT_SUCCESS) {
        goto out_dev;
    }
    /* Dump nvp file */
    if (ctrl->options[OPTION_D]) {
        /* Find the file in mounted partition */
        ret = spinor_lfs_dump_nvp(ctrl->nvp_file, ctrl->dump_file);
        goto out_unmount;
    }
    /* Upload/overwrite nvp file */
    if (ctrl->options[OPTION_O]) {
        ret = spinor_lfs_upload_nvp(ctrl->nvp_file, ctrl->upload_file);
        goto out_unmount;
    }
    /* Operate on nvp field */
    if (ctrl->options[OPTION_R] || ctrl->options[OPTION_W] ||
        ctrl->options[OPTION_V] || ctrl->options[OPTION_E]) {
        ret = spinor_lfs_operate_field(ctrl);
    }

out_unmount:
    spinor_lfs_unmount();
out_dev:
    if (dev_fd != -1)
        close(dev_fd);
    return ret;
}