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
 * nvparm is an engineering tool running on the BMC Linux console.
 * nvparm enables users to edit NVPARAM fields of Validation NVPARAM and
 * Dynamic NVPARAM partitions of Host SPI NOR and the Boot Strap Data EEPROM.
 **/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <mtd/mtd-user.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#include "version.h"
#include "utils.h"
#include "bsd_eeprom_nvp.h"
#include "hostfw_nvp.h"

/* Option string of this application */
#define OPTION_STRING   "t:u:f:i:rew:v:d:b:s:o:D:phV"

static nvparm_ctrl_t nvparm_ctrl = { 0 };

/**
 * @fn help()
 *
 * @brief Print help message.
 * @param none
 * @return void
 **/
static void help (void)
{
    log_printf(LOG_NORMAL, "nvparm version: %d.%d.%d\n\n",
                           NVPARM_VERSION_MAJOR,
                           NVPARM_VERSION_MINOR,
                           NVPARM_VERSION_PATCH);
    log_printf (LOG_NORMAL, "Usage: nvparm <args>\n\n"
        "Arguments:\n"
        "  -t <nvp_part>    : Partition name of Dynamic NVPARAM or Validation NVPARAM or Static NVPARAM.\n"
        "  -u <nvp_guid>    : Partition’s GUID from the GPT header.\n"
        "                     Specially, 0 is fixed for Boot Strap Data partition.\n"
        "  -f <nvp_file>    : Name of NVP file (Without file extension).\n"
        "                     Specially, NVPBERLY is the fixed nvp file for Boot Strap Data partition.\n"
        "  -i <field_index> : Index of the target field in nvp file, start from 0.\n"
        "  -r               : Read a field and its associated valid bit.\n"
        "  -v <valid_bit>   : Enable or disable valid bit.\n"
        "  -w <nvp_data>    : Write data to a field and its associated valid bit.\n"
        "  -e               : Erase field at field_index.\n"
        "  -d <raw_file>    : Dump specific NVP file into raw file.\n"
        "  -o <new_nvp_file>: New NVP file.\n"
        "  -b <i2c_bus>     : The I2C bus number. Default is 10 (I2C11).\n"
        "  -s <target_addr>  : The target address of the EEPROM. Default is 0x50.\n"
        "  -p               : Print GPT header. NVP partition names and GUIDs will be displayed.\n"
        "  -V               : Show version information.\n"
        "  -D <device>      : The MTD partition path\n"
        "  -h               : Print this help.\n"
    );
}

/**
 * @fn parse_opt()
 *
 * @brief Parsing input options.
 * @param argc - argument count
 * @param argv - argument values
 * @return - Success: 0; Failure: 1
 **/
static int parse_opt (int argc, char** argv)
{
    int ret = EXIT_SUCCESS, argflag = -1;
    char *nvp_part = NULL;
    char *nvp_guid = NULL;
    char *nvp_file = NULL;

    char *input_index = NULL;
    char *input_write = NULL;
    char *input_valid_bit = NULL;
    char *input_dump = NULL;
    char *input_upload = NULL;
    char *input_i2c_bus = NULL;
    char *input_target = NULL;
    char *endptr = NULL; // Store the location where conversion stopped
    char *device_name= NULL;

    unsigned long input = ULONG_MAX;
    unsigned long long input_ll = ULLONG_MAX;

    if (argc < 2) {
        log_printf(LOG_ERROR, "At least 2 arguments are required\n");
        help();
        return EXIT_FAILURE;
    }

    while ((argflag = getopt(argc, (char **)argv, OPTION_STRING)) != -1) {
        switch (argflag) {
        case 't':
            nvparm_ctrl.options[OPTION_T] = 1;
            if (nvp_part != NULL) {
                free(nvp_part);
                nvp_part = NULL;
            }
            nvp_part = strdup(optarg);
            if (nvp_part == NULL) {
                log_printf(LOG_ERROR, "Option -t: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(nvp_part) >= MAX_PART_NAME_LEN) {
                log_printf(LOG_ERROR, "partition name is too long."
                                      " Allow less than %d characters\n",
                                      MAX_PART_NAME_LEN);
                ret = EXIT_FAILURE;
            } else {
                strncpy((char *)nvparm_ctrl.nvp_part, nvp_part,
                        sizeof(nvparm_ctrl.nvp_part));
                if (strcmp(BSD_PARTITION_NAME, nvp_part) == 0) {
                    nvparm_ctrl.device = EEPROM;
                }
            }
            break;
        case 'u':
            nvparm_ctrl.options[OPTION_U] = 1;
            if (nvp_guid != NULL) {
                free(nvp_guid);
                nvp_guid = NULL;
            }
            nvp_guid = strdup(optarg);
            if (nvp_guid == NULL) {
                log_printf(LOG_ERROR, "Option -u: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(nvp_guid) > GUID_STR_LEN) {
                log_printf(LOG_ERROR, "GUID is too long."
                                      " Maximum %d characters is allowed\n",
                                      GUID_STR_LEN);
                ret = EXIT_FAILURE;
            } else {
                uint8_t guid_num[GUID_BYTE_SIZE] = {0};
                if (guid_str2int(nvp_guid, guid_num) == EXIT_SUCCESS) {
                    memcpy(nvparm_ctrl.nvp_guid, guid_num, GUID_BYTE_SIZE);
                } else {
                    // Only accept special guid_str is '0' for BSD EEPROM
                    if (strlen(nvp_guid) == 1 && nvp_guid[0] == '0') {
                        nvparm_ctrl.device = EEPROM;
                    } else {
                        log_printf(LOG_ERROR, "Invalid GUID\n");
                        ret = EXIT_FAILURE;
                    }
                }
            }
            break;
        case 'f':
            nvparm_ctrl.options[OPTION_F] = 1;
            if (nvp_file != NULL) {
                free(nvp_file);
                nvp_file = NULL;
            }
            nvp_file = strdup(optarg);
            if (nvp_file == NULL) {
                log_printf(LOG_ERROR, "Option -u: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(nvp_file) >= MAX_NAME_LENGTH) {
                log_printf(LOG_ERROR, "nvp file name is too long."
                                      " Allow less than %d characters\n",
                                      MAX_NAME_LENGTH);
                ret = EXIT_FAILURE;
            } else {
                strncpy((char *)nvparm_ctrl.nvp_file, nvp_file,
                        sizeof(nvparm_ctrl.nvp_file));
            }
            break;
        case 'i':
            nvparm_ctrl.options[OPTION_I] = 1;
            if (input_index != NULL) {
                free(input_index);
                input_index = NULL;
            }
            input_index = strdup(optarg);
            if (input_index == NULL) {
                log_printf(LOG_ERROR, "Option -i: malloc failure\n");
                ret = EXIT_FAILURE;
            } else {
                input = strtoul(input_index, &endptr, 10);
                if (input_index == endptr) { // No conversion: "qabc"
                    log_printf(LOG_ERROR, "No conversion for wrong Input %s\n",
                               input_index);
                    ret = EXIT_FAILURE;
                } else if ((input == ULONG_MAX) && (errno == ERANGE)) {
                    log_printf(LOG_ERROR, "Input %s is %s\n",
                               input_index,
                               strerror(errno));
                    ret = EXIT_FAILURE;
                } else if (*endptr) { // Extra text after number: "0x7Fz"
                    log_printf(LOG_ERROR, "Extra text after number %s\n",
                               input_index);
                    ret = EXIT_FAILURE;
                } else {
                    nvparm_ctrl.field_index = (uint64_t)input;
                }
            }
            break;
        case 'r':
            nvparm_ctrl.options[OPTION_R] = 1;
            break;
        case 'e':
            nvparm_ctrl.options[OPTION_E] = 1;
            break;
        case 'w':
            nvparm_ctrl.options[OPTION_W] = 1;
            if (input_write != NULL) {
                free(input_write);
                input_write = NULL;
            }
            input_write = strdup(optarg);
            if (input_write == NULL) {
                log_printf(LOG_ERROR, "Option -w: malloc failure\n");
                ret = EXIT_FAILURE;
            } else {
                input_ll = strtoull(input_write, &endptr, 16);
                if (input_write == endptr) { // No conversion: "qabc"
                    log_printf(LOG_ERROR, "No conversion for wrong Input %s\n",
                            input_write);
                    ret = EXIT_FAILURE;
                } else if (errno == ERANGE) {
                    log_printf(LOG_ERROR, "Input %s is %s\n",
                            input_write,
                            strerror(errno));
                    ret = EXIT_FAILURE;
                } else if (*endptr) { // Extra text after number: "0x7Fz"
                    log_printf(LOG_ERROR, "Extra text after number %s\n",
                               input_write);
                    ret = EXIT_FAILURE;
                } else {
                    nvparm_ctrl.nvp_data = (uint64_t)input_ll;
                }
            }
            break;
        case 'v':
            nvparm_ctrl.options[OPTION_V] = 1;;
            if (input_valid_bit != NULL) {
                free(input_valid_bit);
                input_valid_bit = NULL;
            }
            input_valid_bit = strdup(optarg);
            if (input_valid_bit == NULL) {
                log_printf(LOG_ERROR, "Option -v: malloc failure\n");
                ret = EXIT_FAILURE;
            } else {
                input = strtoul(input_valid_bit, &endptr, 16);
                if (input_valid_bit == endptr) { // No conversion: "qabc"
                    log_printf(LOG_ERROR, "No conversion for wrong input %s\n",
                               input_valid_bit);
                    ret = EXIT_FAILURE;
                } else if ((input == ULONG_MAX) && (errno == ERANGE)) {
                    log_printf(LOG_ERROR, "Input %s is %s\n",
                            input_valid_bit,
                            strerror(errno));
                    ret = EXIT_FAILURE;
                } else if (*endptr) { // Extra text after number: "0x7Fz"
                    log_printf(LOG_ERROR, "Extra text after number %s\n",
                               input_valid_bit);
                    ret = EXIT_FAILURE;
                } else {
                    nvparm_ctrl.valid_bit = (uint8_t)input;
                }
            }
            break;
        case 'd':
            nvparm_ctrl.options[OPTION_D] = 1;
            if (input_dump != NULL) {
                free(input_dump);
                input_dump = NULL;
            }
            input_dump = strdup(optarg);
            if (input_dump == NULL) {
                log_printf(LOG_ERROR, "Option -d: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(input_dump) >= MAX_NAME_LENGTH) {
                log_printf(LOG_ERROR, "dump file name is too long."
                                      " Allow less than %d characters\n",
                                      MAX_NAME_LENGTH);
                ret = EXIT_FAILURE;
            } else {
                strncpy((char *)nvparm_ctrl.dump_file, input_dump,
                        sizeof(nvparm_ctrl.dump_file));
            }
            break;
        case 'b':
            nvparm_ctrl.options[OPTION_B] = 1;
            if (input_i2c_bus != NULL) {
                free(input_i2c_bus);
                input_i2c_bus = NULL;
            }
            input_i2c_bus = strdup(optarg);
            if (input_i2c_bus == NULL) {
                log_printf(LOG_ERROR, "Option -b: malloc failure\n");
                ret = EXIT_FAILURE;
            } else {
                input = strtoul(input_i2c_bus, &endptr, 10);
                if (input_i2c_bus == endptr) { // e.g. "qabc"
                    log_printf(LOG_ERROR, "No conversion for wrong input %s\n",
                               input_i2c_bus);
                    ret = EXIT_FAILURE;
                } else if ((input == ULONG_MAX) && (errno == ERANGE)) {
                    log_printf(LOG_ERROR, "Input %s is %s\n",
                               input_i2c_bus,
                            strerror(errno));
                    ret = EXIT_FAILURE;
                } else if (*endptr) { // e.g. "0x7Fz"
                    log_printf(LOG_ERROR, "Extra text after number %s\n",
                               input_i2c_bus);
                    ret = EXIT_FAILURE;
                } else {
                    nvparm_ctrl.i2c_bus = (uint8_t)input;
                }
            }
            break;
        case 's':
            nvparm_ctrl.options[OPTION_S] = 1;
            if (input_target) {
                free(input_target);
                input_target = NULL;
            }
            input_target = strdup(optarg);
            if (!input_target) {
                log_printf(LOG_ERROR, "Option -s: malloc failure\n");
                ret = EXIT_FAILURE;
            } else {
                input = strtoul(input_target, &endptr, 16);
                if (input_target == endptr) { // e.g. "qabc"
                    log_printf(LOG_ERROR, "No conversion for wrong input %s\n",
                               input_target);
                    ret = EXIT_FAILURE;
                } else if ((input == ULONG_MAX) && (errno == ERANGE)) {
                    log_printf(LOG_ERROR, "Input %s is %s\n",
                               input_target,
                            strerror(errno));
                    ret = EXIT_FAILURE;
                } else if (*endptr) { // e.g. "0x7Fz"
                    log_printf(LOG_ERROR, "Extra text after number %s\n",
                               input_target);
                    ret = EXIT_FAILURE;
                } else {
                    nvparm_ctrl.target_addr = (uint8_t)input;
                }
            }
            break;
        case 'p':
            nvparm_ctrl.options[OPTION_P] = 1;
            break;
        case 'h':
            nvparm_ctrl.options[OPTION_H] = 1;
            break;
        case 'V':
            nvparm_ctrl.options[OPTION_VER] = 1;
            break;
        case 'o':
            nvparm_ctrl.options[OPTION_O] = 1;
            if (input_upload != NULL) {
                free(input_upload);
                input_upload = NULL;
            }
            input_upload = strdup(optarg);
            if (input_upload == NULL) {
                log_printf(LOG_ERROR, "Option -o: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(input_upload) >= MAX_NAME_LENGTH) {
                log_printf(LOG_ERROR, "upload nvp file name is too long."
                                      " Allow less than %d characters\n",
                                      MAX_NAME_LENGTH);
                ret = EXIT_FAILURE;
            } else {
                strncpy((char *)nvparm_ctrl.upload_file, input_upload,
                        sizeof(nvparm_ctrl.upload_file));
            }
            break;
        case 'D':
            nvparm_ctrl.options[OPTION_DEV] = 1;
            if (device_name != NULL) {
                free(device_name);
                device_name = NULL;
            }
            device_name = strdup(optarg);
            if (device_name == NULL) {
                log_printf(LOG_ERROR, "Option -D: malloc failure\n");
                ret = EXIT_FAILURE;
            } else if (strlen(device_name) >= MAX_NAME_LENGTH) {
                log_printf(LOG_ERROR, "Device name is too long."
                                      " Allow less than %d characters\n",
                                      MAX_NAME_LENGTH);
                ret = EXIT_FAILURE;
            } else {
                strncpy((char *)nvparm_ctrl.device_name, device_name,
                        sizeof(nvparm_ctrl.device_name));
            }
            break;
        default:
            help();
            break;
        }
    }

    if (nvp_part) {
        free(nvp_part);
        nvp_part = NULL;
    }
    if (nvp_guid) {
        free(nvp_guid);
        nvp_guid = NULL;
    }
    if (nvp_file) {
        free(nvp_file);
        nvp_file = NULL;
    }
    if (input_index) {
        free(input_index);
        input_index = NULL;
    }
    if (input_write) {
        free(input_write);
        input_write = NULL;
    }
    if (input_valid_bit) {
        free(input_valid_bit);
        input_valid_bit = NULL;
    }
    if (input_dump) {
        free(input_dump);
        input_dump = NULL;
    }
    if (input_upload) {
        free(input_upload);
        input_upload = NULL;
    }
    if (input_i2c_bus) {
        free(input_i2c_bus);
        input_i2c_bus = NULL;
    }
    if (input_target) {
        free(input_target);
        input_target = NULL;
    }
    if (device_name) {
        free(device_name);
        device_name= NULL;
    }
    return ret;
}

/**
 * @fn verify_opt()
 *
 * @brief Verify input options.
 * @param none
 * @return - Success: 0; Failure: 1
 **/
static int verify_opt (void)
{
    int ret = EXIT_SUCCESS;
    nvparm_ctrl_t *ctrl = &nvparm_ctrl;

    if (ctrl->options[OPTION_P] || ctrl->options[OPTION_H] ||
        ctrl->options[OPTION_VER]) {
        if (ctrl->options[OPTION_T] || ctrl->options[OPTION_U] ||
            ctrl->options[OPTION_F] || ctrl->options[OPTION_I] ||
            ctrl->options[OPTION_R] || ctrl->options[OPTION_E] ||
            ctrl->options[OPTION_W] || ctrl->options[OPTION_V] ||
            ctrl->options[OPTION_B] || ctrl->options[OPTION_S] ||
            ctrl->options[OPTION_D] || ctrl->options[OPTION_O]) {
            ret = EXIT_FAILURE;
            log_printf(LOG_ERROR,
                       "Option -p, -h or -V can't be mixed to others.\n");
        } else if ((ctrl->options[OPTION_P] && ctrl->options[OPTION_H]) ||
                   (ctrl->options[OPTION_P] && ctrl->options[OPTION_VER]) ||
                   (ctrl->options[OPTION_H] && ctrl->options[OPTION_VER])) {
            ret = EXIT_FAILURE;
            log_printf(LOG_ERROR,
                       "Option -p, -h and -V can't be mixed together.\n");
        } else if ((ctrl->options[OPTION_H] || ctrl->options[OPTION_VER]) &&
                  ctrl->options[OPTION_DEV]) {
            ret = EXIT_FAILURE;
            log_printf(LOG_ERROR,
                       "Option -h or -V can't mix with -D option.\n");
        }
        goto exit_verify;
    }

    if (ctrl->options[OPTION_T] == 0 && ctrl->options[OPTION_U] == 0) {
        ret = EXIT_FAILURE;
        log_printf(LOG_ERROR, "Option -t or -u must be specified.\n");
        goto exit_verify;
    }

    if (ctrl->options[OPTION_T] && ctrl->options[OPTION_U]) {
        ret = EXIT_FAILURE;
        log_printf(LOG_ERROR, "Option -t and -u can't be mixed together\n");
        goto exit_verify;
    }

    if (ctrl->device == SPINOR) {
        /* Verify action request */
        if ((ctrl->options[OPTION_R] || ctrl->options[OPTION_E] ||
             ctrl->options[OPTION_W] || ctrl->options[OPTION_V] ||
             ctrl->options[OPTION_D] || ctrl->options[OPTION_P] ||
             ctrl->options[OPTION_O]) == 0) {
            log_printf(LOG_ERROR, "Must select one of options:"
                                    " -r, -e, -w, -v, -d, -p, -o\n");
            ret = EXIT_FAILURE;
            goto exit_verify;
        } else if ((ctrl->options[OPTION_R] && ctrl->options[OPTION_E]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_W]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_V]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_P]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_W]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_V]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_P]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_W] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_W] && ctrl->options[OPTION_P]) ||
                   (ctrl->options[OPTION_W] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_V] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_V] && ctrl->options[OPTION_P]) ||
                   (ctrl->options[OPTION_V] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_D] && ctrl->options[OPTION_P]) ||
                   (ctrl->options[OPTION_D] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_P] && ctrl->options[OPTION_O])) {
            log_printf(LOG_ERROR,
                    "Options -r, -e, -w/-v, -d, -p, -o can't be mixed together.\n"
                    "Except: -w and -v option can be mixed together\n.");
            ret = EXIT_FAILURE;
            goto exit_verify;
        }
        if ((ctrl->options[OPTION_F] || ctrl->options[OPTION_I]) == 0) {
            ret = EXIT_FAILURE;
            log_printf(LOG_ERROR, "Option -f and -i must be specified.\n");
        }
    } else if (ctrl->device == EEPROM) {
        /* Verify action request */
        if ((ctrl->options[OPTION_R] || ctrl->options[OPTION_E] ||
             ctrl->options[OPTION_W] || ctrl->options[OPTION_V] ||
             ctrl->options[OPTION_D] || ctrl->options[OPTION_O]) == 0) {
            log_printf(LOG_ERROR, "Must select one of options:"
                                    " -r, -e, -w, -v, -d, -o\n");
            ret = EXIT_FAILURE;
            goto exit_verify;
        } else if ((ctrl->options[OPTION_R] && ctrl->options[OPTION_E]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_W]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_V]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_R] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_W]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_V]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_E] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_W] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_W] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_V] && ctrl->options[OPTION_D]) ||
                   (ctrl->options[OPTION_V] && ctrl->options[OPTION_O]) ||
                   (ctrl->options[OPTION_D] && ctrl->options[OPTION_O])) {
            log_printf(LOG_ERROR,
                    "Options -r, -e, -w/-v, -d, -o can't be mixed together.\n"
                    "Except: -w and -v option can be mixed together\n.");
            ret = EXIT_FAILURE;
            goto exit_verify;
        } else if (ctrl->options[OPTION_DEV]) {
            log_printf(LOG_ERROR, "Can't use -D option for this case\n");
            ret = EXIT_FAILURE;
            goto exit_verify;
        }
        if (!(ctrl->options[OPTION_D] || ctrl->options[OPTION_O]) &&
            ctrl->options[OPTION_I] == 0) {
            ret = EXIT_FAILURE;
            log_printf(LOG_ERROR, "Option -i must be specified.\n");
        }
        /* Options: -f, -b, -s can be skipped to use default value */
    } else {
        log_printf(LOG_ERROR, "Unsupported!!!");
        ret = EXIT_FAILURE;
    }

exit_verify:
    return ret;
}

/**
 * @fn main()
 *
 * @brief Tool main function.
 * @param argc - argument count
 * @param argv - argument values
 * @return     - Success: 0; Failure: 1
 **/
int main (int argc, char **argv)
{
    int ret = EXIT_SUCCESS;

    ret = parse_opt(argc, argv);
    if (ret == EXIT_SUCCESS) {
        ret = verify_opt();
        if (ret == EXIT_SUCCESS) {
            if (nvparm_ctrl.options[OPTION_VER]) {
                log_printf(LOG_NORMAL, "nvparm version: %d.%d.%d\n",
                           NVPARM_VERSION_MAJOR,
                           NVPARM_VERSION_MINOR,
                           NVPARM_VERSION_PATCH);
            } else if (nvparm_ctrl.options[OPTION_H]) {
                help();
            } else if (nvparm_ctrl.device == SPINOR) {
                ret = spinor_handler(&nvparm_ctrl);
            } else {
                ret = bsd_eeprom_handler(&nvparm_ctrl);
            }
        }
    }

    return ret;
}