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

#include "spinor_func.h"
#include "lfs.h"

struct mtd_info_user mtd;
struct erase_info_user erase;

static int dev_fd = -1;
char mtd_dev[MTD_DEV_SIZE];

struct stat filestat;
/* lfs definition and control buffer for flash SPI-NOR */
lfs_t lfs_flash;
lfs_file_t file_flash;

/* statically allocated read buffer */
uint8_t lfs_read_buf[DEFAULT_READ_PRO_SIZE];
/* statically allocated program buffer */
uint8_t lfs_prog_buf[DEFAULT_READ_PRO_SIZE];
/* statically allocated lookahead buffer (track 128*8=1024 blocks) */
uint8_t lfs_lookahead_buf[DEFAULT_LFS_LOOKAHEAD_SIZE];

struct lfs_config cfg_flash;

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
 * @fn flash_open
 *
 * @brief Open a file with specified flags
 * @param  filename [IN] - Name of file to be opened
 * @param  flags [IN] - Flags when opening file
 * @return  File descriptor
 *          0 - Success
 *         -1 - Failure
 **/
static int flash_open(char *filename, int flags)
{
    int fd = -1;

    fd = open(filename, flags);
    if (fd < 0) {
        log_printf(LOG_ERROR, "Failed to open the file: %s\n",
            filename);
    }

    return fd;
}

/**
 * @fn flash_read
 *
 * @brief Read content from file
 * @param  fd [IN] - File descriptor of file to read from
 * @param  filename [IN] - Name of file to be read
 * @param  buf [OUT] - Buffer contains read data
 * @param  count [IN] - Size to read in bytes
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_read(int fd, const char *filename, void *buf, size_t count)
{
    size_t result;
    int ret = 0;

    result = read(fd, buf, count);

    if (count != result) {
        printf("\n");
        if ((signed)result < 0) {
            log_printf(LOG_ERROR, "While reading data from"
                "%s: %m\n", filename);
            ret = -1;
        }
        log_printf(LOG_ERROR, "Short read count returned while reading"
            "from %s\n", filename);
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
 * @param  filename [IN] - Name of file
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_rewind(int fd, unsigned long offset, const char *filename)
{
    if (lseek(fd, offset, SEEK_SET) < 0) {
        log_printf(LOG_ERROR, "While seeking to start of %s: %m\n",
            filename);
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
 * @param  argv_ptr [IN] - String of mtd device
 * @return  0 - Success
 *         -1 - Failure
 **/
static int flash_write(int fd, const void *buffer, size_t buf_size,
                       unsigned long offset, char *argv_ptr)
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

    tmp = flash_rewind(fd, offset, argv_ptr);
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
                log_printf(LOG_ERROR, "While writing data to"
                    "0x%.8x-0x%.8x on %s\n",
                    written, written + i, argv_ptr);
                ret = -1;
                goto out;
            }

            log_printf(LOG_ERROR, "Short write count returned while"
                "writing to x%.8x-0x%.8x on %s: %d/%lu bytes"
                " written to flash\n", written, written + i,
                argv_ptr, written + result, buf_size);
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

    dev_fd  = flash_open(argv_dev_ptr, O_SYNC | O_RDWR);
    if (dev_fd < 0) {
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
int flash_read_lfs(const struct lfs_config *c, lfs_block_t block,
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
    if (flash_rewind(dev_fd, offset, (char*) mtd_dev) < 0) {
        log_printf(LOG_ERROR, "While seeking to offset: 0X%08x\n", offset);
        ret = LFS_ERR_IO;
        goto exit;
    }

    if (flash_read(dev_fd, (char*) mtd_dev, buffer, (size_t) size) < 0) {
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
int flash_write_lfs(const struct lfs_config *c, lfs_block_t block,
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
        (unsigned long) offset, (char*) mtd_dev) < 0) {
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
int flash_erase_lfs(const struct lfs_config *c, lfs_block_t block)
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
int flash_sync_lfs(const struct lfs_config *c )
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
 * @fn spinor_lfs_mount
 *
 * @brief Mount a partition as LittleFS filesystem
 * @param  size [IN]   - Size of the partition
 * @param  offset [IN] - The location of partition in the flash
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_mount(uint32_t size, uint32_t offset)
{
    int ret = EXIT_SUCCESS;
    int err = -1;

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
        }
    }

    return ret;
}

/**
 * @fn spinor_lfs_unmount
 *
 * @brief Release any resources we were using
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_unmount(void)
{
    int ret = EXIT_SUCCESS;

    ret = lfs_unmount(&lfs_flash);
    if (ret != 0) {
        log_printf(LOG_ERROR, "ERROR in unmount LFS\n");
        ret = EXIT_FAILURE;
    }
    return ret;
}

/**
 * @fn spinor_lfs_file_read
 *
 * @brief Read content from file at specific offset
 * @param  lfs [IN] - LFS config structure
 * @param  file [IN] - LFS file type
 * @param  offset [IN] - File offset
 * @param  buff [OUT] - Content is read from file
 * @param  size [IN] - Content size
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_file_read(lfs_t *lfs, lfs_file_t *file, lfs_off_t offset,
                         uint8_t *buff, lfs_size_t size)
{
    lfs_size_t byte_cnt = 0;
    /* Seek file to offset */
    if (lfs_file_seek(lfs, file, offset, LFS_SEEK_SET) != (lfs_soff_t)offset) {
        log_printf(LOG_ERROR, "ERROR in seek to offset: 0x%.8x\n", offset);
        return EXIT_FAILURE;
    }
    /* Read Valid bit array */
    byte_cnt = lfs_file_read(lfs, file, buff, size);
    if (byte_cnt != size) {
        log_printf(LOG_ERROR, "ERROR in read lfs file: %d\n", byte_cnt);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @fn spinor_lfs_file_write
 *
 * @brief Write content into file at specific offset
 * @param  lfs [IN] - LFS config structure
 * @param  file [IN] - LFS file type
 * @param  offset [IN] - File offset
 * @param  buff [IN] - Content to write to file
 * @param  size [IN] - Content size
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_file_write(lfs_t *lfs, lfs_file_t *file, lfs_off_t offset,
                         uint8_t *buff, lfs_size_t size)
{
    lfs_size_t byte_cnt = 0;
    /* Seek file to offset */
    if (lfs_file_seek(lfs, file, offset, LFS_SEEK_SET) != (lfs_soff_t)offset) {
        log_printf(LOG_ERROR, "ERROR in seek to offset: 0x%.8x\n", offset);
        return EXIT_FAILURE;
    }
    byte_cnt = lfs_file_write(lfs, file, buff, size);
    if (byte_cnt != size) {
        log_printf(LOG_ERROR, "ERROR in write lfs file: %d\n", byte_cnt);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @fn spinor_lfs_dump_nvp
 *
 * @brief Dump NVPARAM file into specific target file
 * @param  nvp_file [IN] - NVPARAM file to be dumped from
 * @param  dump_file [IN] - Target file stores the dumped NVPARAM data
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_dump_nvp(char *nvp_file, char *dump_file)
{
    int ret = EXIT_SUCCESS;
    uint8_t buff[DEFAULT_SPI_PAGE_SIZE] = {0};
    FILE *fp = NULL;
    int byte_cnt = 0;

    if (nvp_file == NULL || dump_file == NULL) {
        return EXIT_FAILURE;
    }
    /* Open nvp_file as READ ONLY */
    ret = lfs_file_open(&lfs_flash, &file_flash, nvp_file, LFS_O_RDONLY);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n", ret, nvp_file);
        ret = EXIT_FAILURE;
        goto out_dump;
    }
    fp = fopen(dump_file, "w");
    if (fp == NULL) {
        log_printf(LOG_ERROR, "Cannot open file %s\n", nvp_file);
        ret = EXIT_FAILURE;
        goto out_lfs_open;
    }
    while ((byte_cnt = lfs_file_read(&lfs_flash, &file_flash, buff,
                                DEFAULT_SPI_PAGE_SIZE * sizeof(uint8_t))) > 0){
        fwrite(buff, 1, byte_cnt, fp);
        if (ferror(fp)) {
            log_printf(LOG_ERROR, "ERROR in write to file %s\n", dump_file);
            ret = EXIT_FAILURE;
            break;
        }
    }

    if (fp)
        fclose(fp);
out_lfs_open:
    lfs_file_close(&lfs_flash, &file_flash);
out_dump:
    return ret;
}


/**
 * @fn spinor_lfs_upload_nvp
 *
 * @brief Upload NVPARAM file into specific target NVPARAM file
 * @param  nvp_file [IN] - NVPARAM file to be overwritten
 * @param  upload_file [IN] - Upload NVPARAM file
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_upload_nvp(char *nvp_file, char *upload_file)
{
    int ret = EXIT_SUCCESS;
    FILE *fp = NULL;
    int byte_cnt = 0;
    ssize_t sz, bytes, count = 0;
    uint8_t *buf = NULL;

    if (nvp_file == NULL || upload_file == NULL) {
        return EXIT_FAILURE;
    }
    /* Open nvp_file as WRITE ONLY */
    ret = lfs_file_open(&lfs_flash, &file_flash, nvp_file,
                        LFS_O_WRONLY | LFS_O_TRUNC);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n", ret, nvp_file);
        ret = EXIT_FAILURE;
        goto out_upload;
    }

    fp = fopen(upload_file, "rb");
    if (fp == NULL) {
        log_printf(LOG_ERROR, "Cannot open file %s\n", upload_file);
        ret = EXIT_FAILURE;
        goto out_lfs_open;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    log_printf(LOG_DEBUG, "[upload] new file %s size: %d\n", upload_file, sz);

    buf = malloc(sz);
    if (!buf) {
        log_printf(LOG_ERROR, "Not enough memory\n");
        ret = EXIT_FAILURE;
        fclose(fp);
        goto out_lfs_open;
    }
    fseek(fp, 0, SEEK_SET);
    fread(buf, sz, 1, fp);

loop:
    if (sz >= DEFAULT_SPI_PAGE_SIZE)
        bytes = DEFAULT_SPI_PAGE_SIZE;
    else
        bytes = sz;

    byte_cnt = lfs_file_write(&lfs_flash, &file_flash, buf, bytes);

    if ((byte_cnt < 0) || (byte_cnt != bytes)) {
        log_printf(LOG_ERROR, "ERROR write to NVP file: %d\n", byte_cnt);
        ret = EXIT_FAILURE;
    } else {
        sz -= bytes;
        buf += bytes;
        count +=bytes;
        if (sz > 0)
            goto loop;
        log_printf(LOG_DEBUG, "DONE write NVP file: %d\n", count);
    }

    if (fp)
        fclose(fp);
out_lfs_open:
    lfs_file_close(&lfs_flash, &file_flash);
    if (buf)
        free(buf);
out_upload:
    return ret;
}

/**
 * @fn spinor_lfs_operate_field
 *
 * @brief Operate on specific NVP field and its associated valid bit
 * @param  ctrl [IN] - Input control structure
 * @return  0 - Success
 *          1 - Failure
 **/
int spinor_lfs_operate_field(nvparm_ctrl_t *ctrl)
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
    ret = lfs_file_open(&lfs_flash, &file_flash,
                        ctrl->nvp_file, LFS_O_RDWR);
    if (ret < 0) {
        log_printf(LOG_ERROR, "ERROR %d in open file %s\n",
                   ret, ctrl->nvp_file);
        return EXIT_FAILURE;
    }
    /* Read NVP header at start of the file */
    offset = 0;
    ret = spinor_lfs_file_read(&lfs_flash, &file_flash,
                               offset, (uint8_t *)&header, sizeof(header));
    if (ret != EXIT_SUCCESS) {
        log_printf(LOG_ERROR, "ERROR in read NVP header\n");
        lfs_file_close(&lfs_flash, &file_flash);
        return EXIT_FAILURE;
    }
    if (ctrl->field_index >= header.count) {
        log_printf(LOG_ERROR, "Invalid NVP field index\n");
        lfs_file_close(&lfs_flash, &file_flash);
        return EXIT_FAILURE;
    }

    /* TODO Calculate size of nvp valid bit array */
    val_bit_arr_sz = header.count / NVP_VAL_BIT_PER_ELE +
                     ((header.count % NVP_VAL_BIT_PER_ELE) ? 1 : 0);
    val_bit_arr = (uint8_t *)malloc(val_bit_arr_sz);
    if (val_bit_arr == NULL) {
        log_printf(LOG_ERROR, "Can't allocate memory\n");
        lfs_file_close(&lfs_flash, &file_flash);
        return EXIT_FAILURE;
    }
    log_printf(LOG_DEBUG, "Valid bit array size: %d\n", val_bit_arr_sz);
    memset(val_bit_arr, 0, val_bit_arr_sz);

    /* Calculate offset of nvp valid bit array */
    offset = sizeof(header);
    ret = spinor_lfs_file_read(&lfs_flash, &file_flash,
                               offset, val_bit_arr, val_bit_arr_sz);
    if (ret != EXIT_SUCCESS) {
        log_printf(LOG_ERROR, "ERROR in read NVP valid bit array\n");
        lfs_file_close(&lfs_flash, &file_flash);
        free(val_bit_arr);
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
        ret = spinor_lfs_file_read(&lfs_flash, &file_flash, offset,
                                   (uint8_t *)&nvp_value, header.field_size);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in read NVP field\n");
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
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
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }

        offset = header.data_offset + ctrl->field_index * header.field_size;
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, offset,
                                    (uint8_t *)&(ctrl->nvp_data),
                                    header.field_size);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP field: %d\n",
                       ctrl->field_index);
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
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
                lfs_file_close(&lfs_flash, &file_flash);
                free(val_bit_arr);
                return EXIT_FAILURE;
            }
        } else {
            /* Set the nvp field by default */
            UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
        }
        offset = sizeof(header);
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, offset,
                                    val_bit_arr, val_bit_arr_sz);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
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
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }
        offset = sizeof(header);
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, offset,
                                    val_bit_arr, val_bit_arr_sz);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }
        if (header.flags & NVPARAM_HEADER_FLAGS_CHECKSUM_VALID) {
            need_update_cs = 1;
        }
    } else if (ctrl->options[OPTION_E]) {
        /* Erase NVP field by set its all data to 1 */
        nvp_value = ULLONG_MAX;
        offset = header.data_offset + ctrl->field_index * header.field_size;
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, offset,
                                    (uint8_t *)&(nvp_value),
                                    header.field_size);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP field: %d\n",
                       ctrl->field_index);
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }

        /* Set the associated valid bit of NVP field to 0 */
        UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
        offset = sizeof(header);
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, offset,
                                    val_bit_arr, val_bit_arr_sz);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid field: %d\n",
                       ctrl->field_index);
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
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
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }
        ret = spinor_lfs_file_read(&lfs_flash, &file_flash,
                                0, data_cs, header.length);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in read NVP blobs\n");
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            free(data_cs);
            return EXIT_FAILURE;
        }
        /* Clear current checksum */
        p_nvp_header = (struct nvp_header *) data_cs;
        p_nvp_header->checksum = 0;

        header.checksum = calculate_sum8(data_cs, header.length);
        log_printf(LOG_DEBUG, "New checksum: 0x%x\n", header.checksum);
        free(data_cs);

        /* Write checksum to file */
        ret = spinor_lfs_file_write(&lfs_flash, &file_flash, 0,
                                    (uint8_t *)&header, sizeof (header));
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR, "ERROR in write NVP blobs\n");
            lfs_file_close(&lfs_flash, &file_flash);
            free(val_bit_arr);
            return EXIT_FAILURE;
        }
    }

    lfs_file_close(&lfs_flash, &file_flash);
    free(val_bit_arr);

    return EXIT_SUCCESS;
}


