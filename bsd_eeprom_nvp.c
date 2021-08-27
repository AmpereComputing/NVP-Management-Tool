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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "bsd_eeprom_nvp.h"

/**
 * @fn eeprom_get_page_size
 *
 * @brief Get the page size value from eeprom type.
 * @param  type [IN] - The type of eeprom
 * @return  pagesize of the eeprom. Default is EEPROM_256B_PAGE_SIZE
 **/
static int eeprom_get_page_size(uint8_t type)
{
    int pagesize = EEPROM_256B_PAGE_SIZE;

    switch (type) {
    case EEPROM_256B:
        pagesize = EEPROM_256B_PAGE_SIZE;
        break;
    case EEPROM_128B:
        pagesize = EEPROM_128B_PAGE_SIZE;
        break;
    case EEPROM_32B:
        pagesize = EEPROM_32B_PAGE_SIZE;
        break;
    case EEPROM_8B:
        pagesize = EEPROM_8B_PAGE_SIZE;
        break;
    default:
        pagesize = EEPROM_256B_PAGE_SIZE;
        break;
    }
    return pagesize;
}

/**
 * @fn open_i2c_dev
 *
 * @brief Open the i2c bus device.
 * @param  i2c_device [IN] - I2C Device path
 * @return  File descriptor of the device
 **/
static int open_i2c_dev(char *i2c_device)
{
    int fd = open(i2c_device, O_RDWR);

    if (fd < 0) {
        log_printf(LOG_ERROR, "Failed to open I2C device!\n");
        return -1;
    }

    return fd;
}

/**
 * @fn i2c_master_write
 *
 * @brief Write data to I2C device.
 * @param  i2c_device [IN] - I2C Device path
 * @param  slave [IN] - EEPROM slave address
 * @param  data [IN] - Data buffer to write to device
 * @param  count [IN] - Number of data in byte
 * @return  0 - Success
 *          1 - Failure
 **/
static int i2c_master_write(char *i2c_dev, uint8_t slave,
                            uint8_t *data, size_t count)
{
    int ret = EXIT_SUCCESS;
    int fd = -1;

    fd = open_i2c_dev(i2c_dev);
    if (fd < 0)
        return EXIT_FAILURE;

    if (ioctl(fd, I2C_SLAVE, slave) < 0) {
        close(fd);
        ret = EXIT_FAILURE;
        return ret;
    }

    /* Write the specified data onto the I2C bus */
    ret = write(fd, data, count);
    if (ret != (int)count) {
        log_printf(LOG_ERROR, "Failed to write data to I2C bus\n");
        ret = EXIT_FAILURE;
    }
    close(fd);
    return ret;
}

/**
 * @fn i2c_master_read
 *
 * @brief Read data from I2C device.
 * @param  i2c_device [IN] - I2C Device path
 * @param  slave [IN] - EEPROM slave address
 * @param  wr_data [IN] - Data to write to device
 * @param  data [OUT] - Data buffer to read from device
 * @param  data_len [IN] - Number of data in byte
 * @return  0 - Success
 *          1 - Failure
 **/
static int i2c_master_read(char *i2c_dev, uint8_t slave, uint8_t *wr_data,
                           uint8_t *data, size_t data_len)
{
    int ret = EXIT_SUCCESS;
    struct i2c_rdwr_ioctl_data ioctl_data;
    struct i2c_msg i2c_msgs[2];
    int fd = -1;

    fd = open_i2c_dev(i2c_dev);
    if (fd < 0)
        return EXIT_FAILURE;

    if (data_len > EEPROM_MAX_PAGE_SIZE_SUPPORT) {
        log_printf(LOG_NORMAL,
                   "[WARN] Sequential read should not exceed %d bytes, \
                    otherwise the read data will be rolled over!\n",
                    EEPROM_MAX_PAGE_SIZE_SUPPORT);
    }

    /* A dummy write operation should be done according to the I2C protocol */
    ioctl_data.nmsgs = 2;
    ioctl_data.msgs = i2c_msgs;
    ioctl_data.msgs[0].len = MAX_EEPROM_ADDR_LEN;
    ioctl_data.msgs[0].addr = slave;
    ioctl_data.msgs[0].flags = 0;
    ioctl_data.msgs[0].buf = wr_data;

    /* Read operation */
    ioctl_data.msgs[1].len = data_len;
    ioctl_data.msgs[1].addr = slave;
    ioctl_data.msgs[1].flags = I2C_M_RD | I2C_M_NOSTART;
    ioctl_data.msgs[1].buf = data;

    if (ioctl(fd, I2C_RDWR, &ioctl_data) < 0) {
        log_printf(LOG_ERROR,
                   "Failed to read data from EEPROM @0x%x via i2c!\n", slave);
        ret = EXIT_FAILURE;
    }
    close(fd);

    return ret;
}

/**
 * @fn detect_eeprom
 *
 * @brief Detect the EEPROM device.
 * @param  i2c_device [IN] - I2C Device path
 * @param  slave [IN] - EEPROM slave address
 * @return  0 - Success
 *          1 - Failure
 **/
static int detect_eeprom(char *i2c_dev, uint8_t slave)
{
    uint8_t buff[1];
    int ret = EXIT_SUCCESS;

    ret = i2c_master_write(i2c_dev, slave, buff, 0);
    if (ret < 0)
        ret = EXIT_FAILURE;
    return ret;
}

/**
 * @fn eeprom_rd_wr
 *
 * @brief Read/write data from/to EEPROM device.
 * @param  i2c_device [IN] - I2C Device path
 * @param  slave [IN] - EEPROM slave address
 * @param  offset [IN] - The offset to read/write data
 * @param  buf [OUT/IN] - Data buffer for read/write data
 * @param  size [IN] - Size of data to read/write
 * @param  wr_flag [IN] - Option to select read or write operation
 * @return  Size of read/written data. Error return -1
 **/
static ssize_t eeprom_rd_wr(char *i2c_dev, uint8_t slave,
                            uint32_t offset, uint8_t *buf,
                            ssize_t size, uint8_t rw_flag)
{
    ssize_t ret, bytes, len;
    int pagesize;
    uint8_t wr_buf[EEPROM_MAX_PAGE_SIZE_SUPPORT + MAX_EEPROM_ADDR_LEN];
    uint8_t rd_buf[EEPROM_MAX_PAGE_SIZE_SUPPORT];
    uint8_t *p = buf;
    uint16_t buf_off, off_tmp;
    uint32_t off;

    len = size;
    pagesize = eeprom_get_page_size(EEPROM_256B);
    off = offset;
loop:
    if (rw_flag == EEPROM_WR_FLG)
        log_printf(LOG_DEBUG, "\rPrograming FW file: %d/%d (%d%%)",
                   (int)(size - len), (int)size,
                   (int)PERCENTAGE(size - len, size));
    else
        log_printf(LOG_DEBUG, "\rReading from EEPROM: %d/%d (%d%%)",
                   (int)(size - len), (int)size,
                   (int)PERCENTAGE(size - len, size));
    buf_off = 0;

    /**
     * The slave I2C EEPROM bus addresses start from 0x50 upto 0x53.
     * Each I2C slave can address a range of 64KB.
     * Readjust the offset to address a total of 256KB eeprom memory.
     **/
    if (off >= 0x30000) {
        off_tmp = (uint16_t)(off - 0x30000);
    } else if (off >= 0x20000) {
        off_tmp = (uint16_t)(off - 0x20000);
    } else if (off >= 0x10000) {
        off_tmp = (uint16_t)(off - 0x10000);
    } else {
        off_tmp = (uint16_t)off;
    }
    slave = slave + off / 0x10000;

    /* EEPROM offset address */
    if (pagesize == EEPROM_256B_PAGE_SIZE ||
        pagesize == EEPROM_128B_PAGE_SIZE ||
        pagesize == EEPROM_32B_PAGE_SIZE) {
        wr_buf[buf_off++] = (off_tmp & 0xFF00) >> 8;
        wr_buf[buf_off++] = (off_tmp & 0x00FF);
    } else {
        wr_buf[buf_off++] = off_tmp & 0x00FF;
    }
    if (len >= pagesize)
        bytes = pagesize;
    else
        bytes = len;
    if (rw_flag == EEPROM_WR_FLG) {
        memcpy(&wr_buf[buf_off], p, bytes);
        ret = i2c_master_write(i2c_dev, slave, wr_buf, bytes + buf_off);
        if (ret == EXIT_FAILURE) {
            log_printf(LOG_ERROR, "Fail to send wr data\n");
            return -1;
        }
        /* delay 10ms for the I2C write is done */
        usleep(10 * 1000);
    } else {
        ret = i2c_master_read(i2c_dev, slave, wr_buf,
                              rd_buf, bytes);
        if (ret == -1) {
            log_printf(LOG_ERROR, "Fail to read data\n");
            return -1;
        }
        memcpy(p, rd_buf, bytes);
    }
    off += bytes;
    p += bytes;
    len -= bytes;
    if (len > 0)
        goto loop;

    return (int)(size - len);
}

/**
 * @fn bsd_eeprom_handler
 *
 * @brief BSD EEPROM handler.
 * @param  ctrl [IN] - NVPARAM controller struct
 * @return  0 - Success
 *          1 - Failure
 **/
int bsd_eeprom_handler (nvparm_ctrl_t *ctrl)
{
    int ret = EXIT_SUCCESS;
    ssize_t sz = 0;
    char i2cdev[16] = {0};
    struct nvp_header header = {0};
    uint32_t offset = BSD_OFFSET;
    uint64_t nvp_value = 0;
    uint8_t nvp_valid = 0;
    uint8_t *val_bit_arr = NULL;
    uint8_t val_bit_arr_sz = 0;
    uint8_t *data_cs = NULL;
    uint8_t need_update_cs = 0;
    uint8_t checksum = 0;

    if (strlen((char *)ctrl->nvp_file) > 0 &&
        strcmp((char *)ctrl->nvp_file, BSD_NVP_FILE) != 0) {
        log_printf(LOG_ERROR, "Unsupport nvp file: %s\n",
                   (char *)ctrl->nvp_file);
        return EXIT_FAILURE;
    }
    if (ctrl->options[OPTION_B] == 0) {
        ctrl->i2c_bus = DEFAULT_I2C_BUS;
    }
    if (ctrl->options[OPTION_S] == 0) {
        ctrl->slave_addr = DEFAULT_I2C_EEPROM_ADDR;
    }
    /* Create the device file string */
    ret = snprintf(i2cdev, sizeof(i2cdev), "/dev/i2c-%d", ctrl->i2c_bus);
    if (ret >= (signed int)sizeof(i2cdev)) {
        log_printf(LOG_ERROR, "Overflow device length %s\n", i2cdev);
        return EXIT_FAILURE;
    }

    /* Try to probe the EEPROM */
    if (detect_eeprom(i2cdev, ctrl->slave_addr)) {
        log_printf(LOG_ERROR, "I2C device NOT FOUND!\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }

    /* Read NVP header */
    sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset, (uint8_t *)&header,
                      sizeof(struct nvp_header) - BSD_NVP_HEADER_ADJUST,
                      EEPROM_RD_FLG);
    if (sz == -1) {
        log_printf(LOG_ERROR, "ERROR in read NVP header\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }
    /* Verify Signature */
    if (memcmp(BSD_NVP_FILE, header.signature, sizeof(header.signature))) {
        log_printf(LOG_ERROR, "Failed to validate NVP\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }

    /* Validate current checksum */
    data_cs = (uint8_t *)malloc (header.length);
    if (data_cs == NULL) {
        log_printf(LOG_ERROR, "Can't allocate memory\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }
    sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, 0x00, data_cs,
                      header.length, EEPROM_RD_FLG);
    if (sz == -1) {
        log_printf(LOG_ERROR, "ERROR in read nvpberly file\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }
    checksum = calculate_sum8(data_cs, header.length);
    if (checksum != 0) {
        log_printf(LOG_NORMAL, "WARN current checksum invalid\n");
    }

    /* Dump the NVP blob */
    if (ctrl->options[OPTION_D]) {
        FILE *fp = NULL;
        uint8_t *buff = NULL;

        buff = (uint8_t *)malloc (header.length);
        if (buff == NULL) {
            log_printf(LOG_ERROR, "Can't allocate memory\n");
            ret = EXIT_FAILURE;
            goto out_hdl;
        }

        /*
         * NVPBERLY is special structure which includes BSV data also.
         * So dump data from offset 0x00
         */
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr,
                          0, (uint8_t *)&buff,
                          header.length, EEPROM_RD_FLG);
        if (sz == -1 || sz != header.length) {
            log_printf(LOG_ERROR, "ERROR in read NVP blob\n");
            ret = EXIT_FAILURE;
            free(buff);
            goto out_hdl;
        }

        fp = fopen(ctrl->dump_file, "w");
        if (fp == NULL) {
            log_printf(LOG_ERROR, "Cannot open file %s\n", ctrl->dump_file);
            ret = EXIT_FAILURE;
            free(buff);
            goto out_hdl;
        }
        fwrite(buff, 1, header.length, fp);
        if (ferror(fp)) {
            log_printf(LOG_ERROR, "ERROR in dump NVP blob\n");
            ret = EXIT_FAILURE;
        }
        free(buff);
        fclose(fp);
        goto out_hdl;
    }

    /* Upload/overwrite nvpberly file */
    if (ctrl->options[OPTION_O]) {
        FILE *fp = NULL;
        uint8_t *buff = NULL;
        ssize_t bytes = 0;

        fp = fopen(ctrl->upload_file, "rb");
        if (fp == NULL) {
            log_printf(LOG_ERROR, "Cannot open file %s\n", ctrl->upload_file);
            ret = EXIT_FAILURE;
            goto out_hdl;
        }
        fseek(fp, 0, SEEK_END);
        sz = ftell(fp);
        log_printf(LOG_DEBUG, "size of new NVP file: %d bytes\n", sz);

        buff = (uint8_t *)malloc(sz);
        if (!buff) {
            log_printf(LOG_ERROR, "Not enough memory\n");
            ret = EXIT_FAILURE;
            fclose(fp);
            goto out_hdl;
        }
        fseek(fp, 0, SEEK_SET);
        fread(buff, sz, 1, fp);

        /* The NVPBERLY file includes BSV data so offset is 0x00 */
        bytes = eeprom_rd_wr(i2cdev, ctrl->slave_addr,
                          0, (uint8_t *)&buff,
                          sz, EEPROM_WR_FLG);
        if (bytes == -1 || sz != bytes) {
            log_printf(LOG_ERROR, "ERROR in write new NVP blob\n");
            ret = EXIT_FAILURE;
        }

        free(buff);
        fclose(fp);
        goto out_hdl;
    }

    /* BSD is special case which fixes valid bit array size */
    val_bit_arr_sz = BSD_VALID_BIT_ARR_SIZE;
    val_bit_arr = (uint8_t *)malloc (val_bit_arr_sz);
    if (val_bit_arr == NULL) {
        log_printf(LOG_ERROR, "Can't allocate memory\n");
        ret = EXIT_FAILURE;
        goto out_hdl;
    }
    memset(val_bit_arr, 0, val_bit_arr_sz);
    /* Calculate offset of nvp valid bit array */
    offset = BSD_OFFSET + sizeof(header) - BSD_NVP_HEADER_ADJUST;
    sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr,
                      offset, val_bit_arr,
                      val_bit_arr_sz, EEPROM_RD_FLG);
    if (sz == -1) {
        log_printf(LOG_ERROR, "ERROR in read NVP valid bit array.\n");
        ret = EXIT_FAILURE;
        goto out_val_arr;
    }
    #ifdef DEBUG
    log_printf(LOG_DEBUG, "Valid bit array value:");
    for (int i = 0; i < val_bit_arr_sz; i++) {
        log_printf(LOG_DEBUG, " 0x%.2x", val_bit_arr[i]);
    }
    log_printf(LOG_DEBUG, "\n");
    #endif

    if (ctrl->options[OPTION_R]) {
        if (ctrl->field_index >= header.count) {
            log_printf(LOG_ERROR, "Failed to validate NVP\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        /* Calculate offset of nvp field */
        offset = header.data_offset +
                 ctrl->field_index * header.field_size;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr,
                          offset, (uint8_t *)&nvp_value,
                          header.field_size, EEPROM_RD_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in read NVP field: %d\n",
                       ctrl->field_index);
            ret = EXIT_FAILURE;
            goto out_val_arr;
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
        }
        goto out_val_arr;
    }

    if (ctrl->options[OPTION_W]) {
        ret = UINT64_VALIDATE_NVP(header.field_size, ctrl->nvp_data);
        if (ret != EXIT_SUCCESS) {
            log_printf(LOG_ERROR,
                       "NVP data exceeds MAX value of field size %d bytes\n",
                       header.field_size);
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        /* Write new data */
        offset = header.data_offset +
                 ctrl->field_index * header.field_size;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset,
                          (uint8_t *)&(ctrl->nvp_data),
                          header.field_size, EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in write NVP data.\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
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
                ret = EXIT_FAILURE;
                goto out_val_arr;
            }
        } else {
            /* Set the nvp field by default */
            UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
        }
        offset = BSD_OFFSET + sizeof(header) - BSD_NVP_HEADER_ADJUST;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset, val_bit_arr,
                          val_bit_arr_sz, EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid bit.\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        need_update_cs = 1;
    } else if (ctrl->options[OPTION_V]) {
        if (ctrl->valid_bit == NVP_FIELD_IGNORE) {
            UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
        } else if (ctrl->valid_bit == NVP_FIELD_SET) {
            UINT8_SET_BIT(val_bit_arr, ctrl->field_index);
        } else {
            log_printf(LOG_ERROR, "Unsupported valid bit value: 0x%.2x\n",
                       ctrl->valid_bit);
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        offset = BSD_OFFSET + sizeof(header) - BSD_NVP_HEADER_ADJUST;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset, val_bit_arr,
                          val_bit_arr_sz, EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid bit.\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        need_update_cs = 1;
    } else if (ctrl->options[OPTION_E]) {
        /* Erase NVP field by set its all data to 1 */
        nvp_value = ULLONG_MAX;
        offset = header.data_offset +
                 ctrl->field_index * header.field_size;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset,
                          (uint8_t *)&(nvp_value),
                          header.field_size, EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in write NVP data.\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        /* Set the associated valid bit of NVP field to 0 */
        UINT8_CLEAR_BIT(val_bit_arr, ctrl->field_index);
        offset = BSD_OFFSET + sizeof(header) - BSD_NVP_HEADER_ADJUST;
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, offset, val_bit_arr,
                          val_bit_arr_sz, EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in write NVP valid bit.\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        need_update_cs = 1;
    }
    #ifdef DEBUG
    log_printf(LOG_DEBUG, "Valid bit array value after update:");
    for (int i = 0; i < val_bit_arr_sz; i++) {
        log_printf(LOG_DEBUG, " 0x%.2x", val_bit_arr[i]);
    }
    log_printf(LOG_DEBUG, "\n");
    #endif
    if (need_update_cs) {
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, 0x00, data_cs,
                    header.length, EEPROM_RD_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in read nvpberly file\n");
            ret = EXIT_FAILURE;
            goto out_val_arr;
        }
        /* Reset checksum before calculate new value */
        data_cs[BSD_CHECKSUM_OFFSET] = 0;
        checksum = calculate_sum8(data_cs, header.length);
        /* Update new checksum */
        sz = eeprom_rd_wr(i2cdev, ctrl->slave_addr, BSD_CHECKSUM_OFFSET,
                          &checksum, sizeof(checksum), EEPROM_WR_FLG);
        if (sz == -1) {
            log_printf(LOG_ERROR, "ERROR in update new checksum.\n");
            ret = EXIT_FAILURE;
        }
        log_printf(LOG_DEBUG, "DONE Update new checksum\n");
    }

out_val_arr:
    if (val_bit_arr)
        free(val_bit_arr);

out_hdl:
    if (data_cs)
        free(data_cs);

    return ret;
}