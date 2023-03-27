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

#ifndef _HOSTFW_NVP_H_
#define _HOSTFW_NVP_H_

#include "utils.h"

#define PROC_MTD_INFO               "/proc/mtd"
#define HOST_SPI_FLASH_MTD_NAME     "hnor"
#define MTD_DEV_SIZE                20
#define DEFAULT_PAGE_SIZE           4096

extern int spinor_handler (nvparm_ctrl_t *ctrl);

#endif  /* _HOSTFW_NVP_H_ */