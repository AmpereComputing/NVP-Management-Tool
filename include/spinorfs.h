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

#ifndef _SPINORFS_H_
#define _SPINORFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// File open flags
enum spinofs_open_flags {
    // open flags
    SPINORFS_O_RDONLY = 1,         // Open a file as read only
    SPINORFS_O_WRONLY = 2,         // Open a file as write only
    SPINORFS_O_RDWR   = 3,         // Open a file as read and write
    SPINORFS_O_CREAT  = 0x0100,    // Create a file if it does not exist
    SPINORFS_O_EXCL   = 0x0200,    // Fail if a file already exists
    SPINORFS_O_TRUNC  = 0x0400,    // Truncate the existing file to zero size
    SPINORFS_O_APPEND = 0x0800,    // Move to end of file on every write
};

/**
 * @fn spinorfs_mount
 *
 * @brief Mount a partition as LittleFS filesystem
 * @param  mtd_fd [IN] - MTD device file descriptor info
 * @param  size [IN]   - Size of the partition
 * @param  offset [IN] - The location of partition in the flash
 * @return  0 - Success
 *          1 - Failure
 **/
extern int spinorfs_mount(int mtd_fd, uint32_t size, uint32_t offset);

/**
 * @fn spinorfs_unmount
 *
 * @brief Release any resources we were using
 * @return  0 - Success
 *          1 - Failure
 **/
extern int spinorfs_unmount(void);

/**
 * @fn spinorfs_open
 *
 * @brief Open file with specified mode.
 * @param  file [IN] - File to be opened
 * @param  flags [IN] - Access mode to file
 * @return  0 - Success
 *          1 - Failure
 **/
extern int spinorfs_open(char *file, int flags);

/**
 * @fn spinorfs_open
 *
 * @brief Close the opened file.
 * Call this when finish operating on the opened file by spinorfs_open.
 * @return  0 - Success
 *          1 - Failure
 **/
extern int spinorfs_close();

/**
 * @fn spinorfs_read
 *
 * @brief Read size bytes of file into buffer. File should be opened
 *        before reading.
 * @param  buff [IN] - Target buffer stores the read file data
 * @param  offset [IN] - File offset
 * @param  size [IN] - Content size
 * @return  The number of bytes read, or -1 on failure
 **/
extern int spinorfs_read(char *buff, uint32_t offset, uint32_t size);

/**
 * @fn spinorfs_write
 *
 * @brief Write data buffer into file.
 * @param  file [IN] - file to be written to
 * @param  buff [IN] - Target buffer stores the write file data
 * @param  offset [IN] - File offset
 * @param  size [IN] - Content size
 * @return  The number of bytes written, or -1 on failure
 **/
extern int spinorfs_write(char *buff, uint32_t offset, uint32_t size);

/**
 * @fn spinorfs_gpt_disk_info
 *
 * @brief Parse GPT info
 * @param  dev_fd [IN] - File descriptor of the flash
 * @param  show_gpt [IN] - Show the GPT info into console
 * @return  0 - Success
 *          1 - Failure
 **/
extern int spinorfs_gpt_disk_info (int dev_fd, int show_gpt);

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
extern int spinorfs_gpt_part_guid_info(uint8_t *guid,
                                  uint32_t *offset, uint32_t *size);

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
extern int spinorfs_gpt_part_name_info(char *part,
                                  uint32_t *offset, uint32_t *size);
#endif  /* _SPINORFS_H_ */