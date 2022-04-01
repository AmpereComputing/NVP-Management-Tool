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

#ifndef _SPINOR_FUNC_H_
#define _SPINOR_FUNC_H_

#include "utils.h"

#define PROC_MTD_INFO               "/proc/mtd"
#define HOST_SPI_FLASH_MTD_NAME     "hnor"
#define MTD_DEV_SIZE                20

#define DEFAULT_SPI_PAGE_SIZE       4096
#define DEFAULT_READ_PRO_SIZE       512
#define DEFAULT_LFS_BLOCK_CYCLE     (-1)
/* Purpose: block-level wear-leveling */
#define DEFAULT_LFS_LOOKAHEAD_SIZE  16

extern int find_host_mtd_partition (int *fd);
extern int spinor_lfs_mount (uint32_t size, uint32_t offset);
extern int spinor_lfs_unmount(void);
extern int spinor_lfs_dump_nvp(char *nvp_file, char *dump_file);
extern int spinor_lfs_upload_nvp(char *nvp_file, char *upload_file);
extern int spinor_lfs_operate_field(nvparm_ctrl_t *ctrl);

#endif  /* _SPINOR_FUNC_H_ */