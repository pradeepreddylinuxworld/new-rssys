/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
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
 * ILib-internal NDP120 driver-specific definitions
 */
#ifndef SYNTIANT_NDP120_DRIVER_H
#define SYNTIANT_NDP120_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant-firmware/ndp120_firmware.h>
#include <syntiant-dsp-firmware/ndp120_dsp_mailbox.h>

enum syntiant_ndp120_device_mailbox_directions_e {
    SYNTIANT_NDP120_DEVICE_MAILBOX_HOST_TO_MCU = 0x0,
    SYNTIANT_NDP120_DEVICE_MAILBOX_MCU_TO_HOST = 0x1,
    SYNTIANT_NDP120_DEVICE_MAILBOX_DIRECTIONS = 0x2
};

typedef enum syntiant_ndp120_bootloader_mode_e {
    SYNTIANT_NDP120_BOOTLOADER_MODE_START = 0,
    SYNTIANT_NDP120_BOOTLOADER_MODE_IN_PROGRESS = 1,
    SYNTIANT_NDP120_BOOTLOADER_MODE_COMPLETE = 2
} syntiant_ndp120_bootloader_mode_t ;

typedef struct {
    uint32_t adx;
    uint32_t bytes_rem_segment;
    uint32_t hdr_idx;
    uint32_t segment_count;
    uint8_t  hdr[sizeof(uint32_t) * 2];
} syntiant_ndp120_multisegment_state_t;

typedef struct {
    uint32_t window_lower;
    uint32_t window_upper;
    uint32_t window_idx;
    /* chunk remainder since MCU transfers
       must be multiples of 4 bytes */
    uint32_t remainder_len;
    uint8_t remainder[4];

    syntiant_ndp120_bootloader_mode_t mode;
} syntiant_ndp120_bootloader_state_t;

typedef struct {
    uint8_t mbin_seq;
    uint8_t mbin_resp_seq;
    uint8_t mbout_seq;
    uint8_t mbin_resync;

    uint8_t watermarkint_resync;
    int watermarkint_state;
    ndp120_dsp_mailbox_msg_t watermarkint;

    int mbin_state;
    int mbin_sync_count;
    int mbin_data_count;

    uint8_t mbin_resp;
    uint8_t mbout;
    uint32_t mbin_data;
    uint32_t watermarkint_data;

} syntiant_ndp120_mb_state_s;

/**
 * @brief NDP120 device-specific interface library internal state object
 */
typedef struct syntiant_ndp120_device_s {
    uint32_t mcu_fw_pointers_addr; /**< 0 means MCU is not running */

    uint32_t mcu_fw_state_addr;
    uint32_t mcu_fw_posterior_state_addr;
    uint32_t mcu_fw_smax_smoother_addr;
    uint32_t mcu_fw_posterior_parameters_addr;
    uint32_t mcu_fw_orchestrator_graph_addr;
    uint32_t mcu_fw_dbg_state_addr;

    uint32_t dsp_fw_state_addr;
    uint32_t dsp_pcm_audio_sample_last_ptr;
    uint32_t dsp_pcm_audio_annotation_last_ptr;
    uint32_t dsp_function_sample_last_ptr;

    unsigned int dnn_input;
    unsigned int input_clock_rate;
    unsigned int core_clock_rate;
    unsigned int audio_frame_size;
    unsigned int audio_frame_step;
    unsigned int audio_sample_size_bytes;
    unsigned int dnn_frame_size;

    uint32_t classes[MAX_NNETWORKS];
    uint32_t fwver_len;
    uint32_t dspfwver_len;
    uint32_t paramver_len;
    uint32_t labels_len;
    uint32_t pkgver_len;
    uint32_t matches;
    uint32_t tankptr_last;
    uint32_t tankptr_match;
    uint32_t num_networks;
    uint32_t last_network_id;
    uint32_t max_feature_len;

    uint32_t match_producer[MAX_NNETWORKS];
    uint32_t match_consumer[MAX_NNETWORKS];
    syntiant_ndp120_mb_state_s mb_state;
    uint8_t mbin_resp;
    uint8_t mbin;
    unsigned int chip_is_locked;
    unsigned int secure_boot_required;
    unsigned int nn_params_loaded;
    unsigned int nn_metadata_loaded;
    syntiant_ndp120_bootloader_state_t bl_state;
    syntiant_ndp120_multisegment_state_t ms_state;
} syntiant_ndp120_device_t;

extern struct syntiant_ndp_driver_s syntiant_ndp120_driver;
extern struct syntiant_ndp_driver_s* syntiant_ndp120_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif
