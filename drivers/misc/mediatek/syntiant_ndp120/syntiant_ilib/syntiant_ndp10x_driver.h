/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 *  process, and are protected by trade secret or copyright law.  Dissemination
 *  of this information or reproduction of this material is strictly forbidden
 *  unless prior written permission is obtained from Syntiant Corporation.
 */
/*
 * ILib-internal NDP10x driver-specific definitions
 */
#ifndef SYNTIANT_NDP10X_DRIVER_H
#define SYNTIANT_NDP10X_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_packager/syntiant_package_consts.h>

enum syntiant_ndp10x_device_mailbox_directions_e {
    SYNTIANT_NDP10X_DEVICE_MAILBOX_HOST_TO_MCU = 0x0,
    SYNTIANT_NDP10X_DEVICE_MAILBOX_MCU_TO_HOST = 0x1,
    SYNTIANT_NDP10X_DEVICE_MAILBOX_DIRECTIONS = 0x2
};

enum { SYNTIANT_NDP10X_DEVICE_MAX_DATA = 32 };

enum syntiant_ndp10x_device_mb_op_e {
    SYNTIANT_NDP10X_MB_OP_NONE = 0,
    SYNTIANT_NDP10X_MB_OP_NOP = 1,
    SYNTIANT_NDP10X_MB_OP_MIADDR = 2,
    SYNTIANT_NDP10X_MB_OP_SER = 3
};

enum syntiant_ndp10x_device_mb_phase_e {
    SYNTIANT_NDP10X_MB_PHASE_IDLE = 0,
    SYNTIANT_NDP10X_MB_PHASE_OP = 1,
    SYNTIANT_NDP10X_MB_PHASE_EXTOP = 2,
    SYNTIANT_NDP10X_MB_PHASE_ERROR = 3,
    SYNTIANT_NDP10X_MB_PHASE_FLUSH = 4,
    SYNTIANT_NDP10X_MB_PHASE_ADDR = 5
};

/**
 * @brief NDP10x mailbox data transfer subprotocol state object
 */
struct syntiant_ndp10x_device_mb_state_s {
    uint8_t data[SYNTIANT_NDP10X_DEVICE_MAX_DATA / 8];
    uint8_t error;
    uint8_t op;
    uint8_t phase;
    uint8_t index;
    uint8_t bits;
    uint8_t out;
    uint8_t unexpected;
};

/**
 * @brief NDP10x device-specific interface library internal state object
 */
#define SYNTIANT_NDP10X_DEVICE_FW_VERSION_MAX 32
struct syntiant_ndp10x_device_s {
    unsigned int dnn_input;
    unsigned int tank_bits;
    unsigned int tank_input;
    unsigned int tank_size;
    unsigned int audio_frame_size;
    unsigned int audio_frame_step;
    unsigned int freq_frame_size;
    unsigned int dnn_frame_size;
    unsigned int input_clock_rate;
    uint32_t fw_pointers_addr; /**< 0 means MCU is not running */
    uint32_t fw_state_addr;
    uint32_t fw_agc_addr;
    uint32_t fw_posterior_state_addr;
    uint32_t fw_posterior_parameters_addr;
    uint32_t tank_address;
    uint32_t tank_max_size;
    uint32_t tank;
    unsigned int classes;
    unsigned int fwver_len;
    unsigned int paramver_len;
    unsigned int labels_len;
    unsigned int pkgver_len;
    unsigned int secured;
    unsigned int tlv_tag;
    unsigned int tlv_len;
    unsigned int tlv_parsed;
    unsigned int load_offset;
    unsigned int intctl;
    unsigned int fw_loaded;
    uint32_t addrprotlo[2];
    uint32_t addrprothi[2];
    uint8_t secured_fw_ver[SYNTIANT_NDP10X_DEVICE_FW_VERSION_MAX];
    unsigned int secured_fw_ver_len;
    uint32_t matches;
    uint32_t prev_matches;
    uint32_t tankptr_last;
    uint32_t tankptr_match;
    uint32_t match_ring_size;
    uint32_t match_producer;
    uint32_t match_consumer;
    uint32_t accelptr_last;
    struct syntiant_ndp10x_device_mb_state_s
        mb[SYNTIANT_NDP10X_DEVICE_MAILBOX_DIRECTIONS];
    uint8_t mbin_resp;
    uint8_t mbin;
};

extern struct syntiant_ndp_driver_s syntiant_ndp10x_driver;
    
/**
 * @brief config layout as it is stored in scratch
 *
 * @param input_clk input clock rate
 * @param valid indictae which of label or versions are valid
 * @param label_size size of label
 * @param labels labels
 * @param fw_version_size size of fw version
 * @param fw_version fw version
 * @param params_version_size size of parameters version
 * @param params_version parameters version
 * @param pkg_version_size size of package version
 * @param pkg_version package version
 */

struct syntiant_ndp10x_config_layout{
    uint32_t input_clk;
    uint32_t valid;
    uint32_t label_size;
    uint8_t labels[LABELS_MAX_SIZE];
    uint32_t fw_version_size;
    uint8_t fw_version[VERSION_MAX_SIZE];
    uint32_t params_version_size;
    uint8_t params_version[VERSION_MAX_SIZE];
    uint32_t pkg_version_size;
    uint8_t pkg_version[VERSION_MAX_SIZE];
};


#ifdef __cplusplus
}
#endif

#endif
