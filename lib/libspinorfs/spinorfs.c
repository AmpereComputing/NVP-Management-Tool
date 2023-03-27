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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <mtd/mtd-user.h>

#include "lfs.h"
#include "spinorfs.h"
#include "utils.h"

#define DEFAULT_SPI_PAGE_SIZE       4096
#define DEFAULT_READ_PRO_SIZE       512
#define DEFAULT_LFS_BLOCK_CYCLE     (-1)
/* Purpose: block-level wear-leveling */
#define DEFAULT_LFS_LOOKAHEAD_SIZE  16

struct mtd_info_user mtd;
struct erase_info_user erase;

static int dev_fd = -1;

/* lfs definition and control buffer for flash SPI-NOR */
lfs_t lfs_flash = {0};
lfs_file_t file_flash = {0};

/* statically allocated read buffer */
uint8_t lfs_read_buf[DEFAULT_READ_PRO_SIZE];
/* statically allocated program buffer */
uint8_t lfs_prog_buf[DEFAULT_READ_PRO_SIZE];
/* statically allocated lookahead buffer (track 128*8=1024 blocks) */
uint8_t lfs_lookahead_buf[DEFAULT_LFS_LOOKAHEAD_SIZE];

struct lfs_config cfg_flash = {0};

/* Flash offset to mount the partition */
lfs_off_t lfs_offset = 0;
/* Partition size */
lfs_size_t lfs_part_size = 0;

/**
 * @fn flash_erase
 *
 * @brief Erase the content of given SPI NOR device, at given offset and length.
 * @param  fd [IN] - The file descriptor of SPI NOR device to be erased
 * @param  offset [IN] - The offset in SPI NOR device to begin erasing
 * @param  length [IN] - Number of bytes will be erased
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_erase(int fd, unsigned long offset, unsigned long length)
{
    int i, blocks;

    erase.start = offset;
    erase.length = (length + mtd.erasesize - 1) / mtd.erasesize;
    erase.length *= mtd.erasesize;

    blocks = erase.length / mtd.erasesize;
    erase.length = mtd.erasesize;

    /* Erasing required flash sector based on input file size */
    for (i = 1; i <= blocks; i++) {
        log_printf(LOG_DEBUG, "\rErasing blocks: %d/%d (%d%%)",
            i, blocks, PERCENTAGE(i, blocks));
        if (ioctl(fd, MEMERASE, &erase) < 0) {
            log_printf(LOG_ERROR,
                "Error While erasing blocks 0x%.8x-0x%.8x: %m\n",
                (unsigned int) erase.start,
                (unsigned int) (erase.start + erase.length));
            return -1;
        }
        erase.start += mtd.erasesize;
    }

    log_printf(LOG_DEBUG, "\rErasing blocks: %d/%d (100%%)\n",
        blocks, blocks);

    return 0;
}

/**
 * @fn flash_read
 *
 * @brief Read content from file
 * @param  fd [IN] - File descriptor of file to read from
 * @param  buf [OUT] - Buffer contains read data
 * @param  count [IN] - Size to read in bytes
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_read(int fd, void *buf, size_t count)
{
    size_t result;
    int ret = 0;

    result = read(fd, buf, count);

    if (count != result) {
        if ((signed)result < 0) {
            log_printf(LOG_ERROR, "Error while reading data: %m\n");
            ret = -1;
        }
        log_printf(LOG_ERROR, "Short read count returned while reading\n");
        ret = -1;
    }
    return ret;
}

/**
 * @fn flash_rewind
 *
 * @brief Rewind pointer to specified offset
 * @param  fd [IN] - File descriptor of file to seek
 * @param  offset [IN] - Desired location in file
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_rewind(int fd, unsigned long offset)
{
    if (lseek(fd, offset, SEEK_SET) < 0) {
        log_printf(LOG_ERROR, "Error while seeking to %ld: %m\n", offset);
        return -1;
    }
    return 0;
}

/**
 * @fn flash_write
 *
 * @brief Write content of buffer to flash at desired offset
 * @param  fd [IN] - File descriptor of flash
 * @param  buffer [IN] - Data to be writen to flash
 * @param  buf_size [IN] - Data size
 * @param  offset [IN] - Location in flash to write
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_write(int fd, const void *buffer, size_t buf_size,
                       unsigned long offset)
{
    ssize_t result, size, written;
    unsigned char src[BUFSIZE];
    int i, tmp = 0;
    int ret = 0;
    unsigned char *buff = (unsigned char *)buffer;

    size = buf_size;
    i = BUFSIZE;
    written = 0;

    log_printf(LOG_DEBUG, "Writing data: 0k/%luk (0%%)",
        KB(size));

    tmp = flash_rewind(fd, offset);
    if (tmp < 0) {
        ret = -1;
        goto out;
    }

    /* Writing file content into flash */
    while (size) {
        if (size < BUFSIZE)
            i = size;

        log_printf(LOG_DEBUG, "\rWriting data: %dk/%luk (%lu%%)",
            KB(written + i), KB(buf_size),
            PERCENTAGE(written + i, buf_size));

        /* read from filename */
        memcpy((void*) src, buffer, i);

        /* write to device */
        result = write(fd, src, i);
        if (i != result) {
            printf("\n");
            if (result < 0) {
                log_printf(LOG_ERROR, "Error while writing data to"
                    "0x%.8x-0x%.8x\n",
                    written, written + i);
                ret = -1;
                goto out;
            }

            log_printf(LOG_ERROR, "Short write count returned while"
                "writing to x%.8x-0x%.8x: %d/%lu bytes"
                " written to flash\n", written, written + i,
                written + result, buf_size);
            ret = -1;
            goto out;
        }
        written += i;
        size -= i;
        buff += i;
    }

    log_printf(LOG_DEBUG,
        "\rWriting data: %luk/%luk (100%%)\n",
        KB(buf_size), KB(buf_size));

out:
    return ret;
}


/**
 * @fn flash_read_lfs
 *
 * @brief A block device operations to read a region in a block
 * @param  c [IN] - lfs configuration structure
 * @param  block [IN] - The target block to be read from
 * @param  off [IN] - The offset of the target block
 * @param  buffer [OUT] - Output buffer to store the read data
 * @param  size [IN] - Size of data that need to read
 * @return  0 - Success
 *          Others - Failure
 **/
static int flash_read_lfs(const struct lfs_config *c, lfs_block_t block,
                   lfs_off_t off, void *buffer, lfs_size_t size)
{
    int ret = LFS_ERR_OK;
    lfs_size_t block_count = (lfs_size_t) mtd.size / mtd.erasesize;
    lfs_size_t block_size = (lfs_size_t) mtd.erasesize;
    unsigned long offset = lfs_offset;

    UN_USED(c);

    log_printf(LOG_DEBUG, "[flash_read_lfs] block:%d, size:%d, off:%d.\n",
               block, size, off);

    if (block > block_count) {
        ret = LFS_ERR_INVAL;
        goto exit;
    }
    /* Calculate offset */
    offset = (unsigned long) block * block_size + off + lfs_offset;
    if (flash_rewind(dev_fd, offset) < 0) {
        log_printf(LOG_ERROR, "While seeking to offset: 0X%08x\n", offset);
        ret = LFS_ERR_IO;
        goto exit;
    }

    if (flash_read(dev_fd, buffer, (size_t) size) < 0) {
        ret = LFS_ERR_IO;
        goto exit;
    }

exit:
    return ret;
}

/**
 * @fn flash_write_lfs
 *
 * @brief A block device operations to write a region in a block
 * @param  c [IN] - lfs configuration structure
 * @param  block [IN] - The target block to be read from
 * @param  off [IN] - The offset of the target block
 * @param  buffer [IN] - Input buffer to write to the flash
 * @param  size [IN] - Size of data that need to write
 * @return  0 - Success
 *          Others - Failure
 **/
static int flash_write_lfs(const struct lfs_config *c, lfs_block_t block,
                    lfs_off_t off, const void *buffer, lfs_size_t size)
{
    int ret = LFS_ERR_OK;
    lfs_size_t block_count = (lfs_size_t) mtd.size / mtd.erasesize;
    lfs_size_t block_size = (lfs_size_t) mtd.erasesize;
    unsigned long offset = lfs_offset;

    UN_USED(c);

    log_printf(LOG_DEBUG, "[flash_write_lfs] block:%d, size:%d, off:%d.\n",
               block, size, off);

    if (block > block_count) {
        ret = LFS_ERR_INVAL;
        goto exit;
    }

    /* Calculate offset */
    offset = (unsigned long) block * block_size + off + lfs_offset;
    if (flash_write(dev_fd, buffer, (size_t) size,
        (unsigned long) offset) < 0) {
        ret = LFS_ERR_IO;
    }

exit:
    return ret;
}

/**
 * @fn flash_erase_lfs
 *
 * @brief A block device operations to erase a region in a block
 * @param  c [IN] - lfs configuration structure
 * @param  block [IN] - The target block to be read from
 * @return  0 - Success
 *          Others - Failure
 **/
static int flash_erase_lfs(const struct lfs_config *c, lfs_block_t block)
{
    int ret = LFS_ERR_OK;
    lfs_size_t block_count = (lfs_size_t) mtd.size / mtd.erasesize;
    lfs_size_t block_size = (lfs_size_t) mtd.erasesize;
    unsigned long offset = lfs_offset;

    UN_USED(c);

    log_printf(LOG_DEBUG, "[flash_erase_lfs] block:%d.\n", block);

    if (block > block_count) {
        ret = LFS_ERR_INVAL;
        goto exit;
    }
    /* Calculate offset */
    offset = (unsigned long) block * block_size + lfs_offset;
    if (flash_erase(dev_fd, offset, block_size) < 0) {
        ret = LFS_ERR_IO;
    }
exit:
    return  ret;
}

/**
 * @fn flash_sync_lfs
 *
 * @brief Sync the state of the underlying block device.
 * @param  c [IN] - lfs configuration structure
 * @return  0 - Success
 *          Others - Failure
 **/
static int flash_sync_lfs(const struct lfs_config *c )
{
    UN_USED(c);

    log_printf(LOG_DEBUG, "ENTER flash_sync_lfs.\n");
    /**
     * LittleFS sync APPI is used to flush any unwritten data (cache/buffer)
     * to the medium (block device). But our write function does not use
     * cache/buffer, we directly write data to device,
     * so do nothing with flash_sync_lfs
     **/
    return LFS_ERR_OK;
}


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
int spinorfs_mount(int mtd_fd, uint32_t size, uint32_t offset)
{
    int ret = EXIT_SUCCESS;
    int err = -1;

    if (mtd_fd == -1) {
        log_printf(LOG_ERROR,"Invalid MTD description file info.\n");
        return EXIT_FAILURE;
    }

    /* Get the MTD device info */
    ret = ioctl(mtd_fd, MEMGETINFO, &mtd);
    if (ret < 0) {
        log_printf(LOG_ERROR,"Can't read MTD device info.\n");
        return EXIT_FAILURE;
    }
    dev_fd = mtd_fd;

    lfs_part_size = (lfs_size_t) size;
    lfs_offset = (lfs_size_t) offset;

    // configuration of the filesystem is provided by this struct
    // block device operations
    cfg_flash.read = flash_read_lfs;
    cfg_flash.prog  = flash_write_lfs;
    cfg_flash.erase = flash_erase_lfs;
    cfg_flash.sync  = flash_sync_lfs;
    // block device configuration
    cfg_flash.read_size = DEFAULT_READ_PRO_SIZE; // SPI-NOR Page size
    cfg_flash.prog_size = DEFAULT_READ_PRO_SIZE; // SPI-NOR Page size
    cfg_flash.block_size = (lfs_size_t) mtd.erasesize;
    cfg_flash.block_count = (lfs_size_t) (lfs_part_size / mtd.erasesize);
    cfg_flash.cache_size = DEFAULT_READ_PRO_SIZE; // Equal to read/pro size
    cfg_flash.lookahead_size = DEFAULT_LFS_LOOKAHEAD_SIZE;
    cfg_flash.block_cycles = DEFAULT_LFS_BLOCK_CYCLE;
    cfg_flash.read_buffer = lfs_read_buf;
    cfg_flash.prog_buffer = lfs_prog_buf;
    cfg_flash.lookahead_buffer = lfs_lookahead_buf;

    err = lfs_mount(&lfs_flash, &cfg_flash);
    if (err) {
        log_printf(LOG_NORMAL,"Mount failed. Format then retry mount..\n");
        lfs_format(&lfs_flash, &cfg_flash);
        if (lfs_mount(&lfs_flash, &cfg_flash)) {
            log_printf(LOG_ERROR,"Cannot mount device!!! Going to exit...\n");
            ret = EXIT_FAILURE;
            dev_fd = -1;
        }
    }

    return ret;
}

/**
 * @fn spinorfs_unmount
 *
 * @brief Release any resources we were using
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_unmount(void)
{
    int ret = EXIT_SUCCESS;

    ret = lfs_unmount(&lfs_flash);
    if (ret != 0) {
        log_printf(LOG_ERROR, "ERROR in unmount LFS\n");
        ret = EXIT_FAILURE;
    } else {
        dev_fd = -1;
        memset(&cfg_flash, 0, sizeof(cfg_flash));
        memset(&lfs_flash, 0, sizeof(lfs_flash));
    }
    return ret;
}

/**
 * @fn spinorfs_open
 *
 * @brief Open file with specified mode.
 * @param  file [IN] - File to be opened
 * @param  flags [IN] - Access mode to file
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_open(char *file, int flags)
{
    int ret = EXIT_SUCCESS;
    if (file == NULL) {
        return EXIT_FAILURE;
    }
    /* Open file as READ ONLY */
    ret = lfs_file_open(&lfs_flash, &file_flash, file, flags);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n", ret, file);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @fn spinorfs_open
 *
 * @brief Close the opened file.
 * Call this when finish operating on the opened file by spinorfs_open.
 * @return  0 - Success
 *          1 - Failure
 **/
int spinorfs_close()
{
    int ret = EXIT_SUCCESS;
    if (file_flash.cfg == NULL) {
        log_printf(LOG_ERROR, "Tried to close file without open before\n");
        return EXIT_FAILURE;
    }

    /* Open file as READ ONLY */
    ret = lfs_file_close(&lfs_flash, &file_flash);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in close file\n", ret);
        ret = EXIT_FAILURE;
    }
    memset(&file_flash, 0, sizeof(file_flash));
    return ret;
}

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
int spinorfs_read(char *buff, uint32_t offset, uint32_t size)
{
    lfs_ssize_t byte_cnt = 0;

    if (buff == NULL) {
        return -1;
    }

    /* Seek file to offset */
    if (lfs_file_seek(&lfs_flash, &file_flash, offset, LFS_SEEK_SET) != (lfs_soff_t)offset) {
        log_printf(LOG_ERROR, "ERROR in seek to offset: 0x%.8x\n", offset);
        return -1;
    }

    byte_cnt = lfs_file_read(&lfs_flash, &file_flash, buff, size);

    if (byte_cnt < 0) {
        log_printf(LOG_ERROR, "ERROR in read lfs file: %d\n", byte_cnt);
        return -1;
    }
    return (int)byte_cnt;
}

/**
 * @fn spinorfs_write
 *
 * @brief Write data buffer into file.
 * @param  buff [IN] - Target buffer stores the write file data
 * @param  offset [IN] - File offset
 * @param  size [IN] - Content size
 * @return  The number of bytes written, or -1 on failure
 **/
int spinorfs_write(char *buff, uint32_t offset, uint32_t size)
{
    lfs_ssize_t byte_cnt = 0;

    if (buff == NULL) {
        return EXIT_FAILURE;
    }

    /* Seek file to offset */
    if (lfs_file_seek(&lfs_flash, &file_flash, offset, LFS_SEEK_SET) != (lfs_soff_t)offset) {
        log_printf(LOG_ERROR, "ERROR in seek to offset: 0x%.8x\n", offset);
        return EXIT_FAILURE;
    }

    byte_cnt = lfs_file_write(&lfs_flash, &file_flash, buff, size);

    if (byte_cnt < 0) {
        log_printf(LOG_ERROR, "ERROR in write lfs file: %d\n", byte_cnt);
        return -1;
    }

    return (int)byte_cnt;
}