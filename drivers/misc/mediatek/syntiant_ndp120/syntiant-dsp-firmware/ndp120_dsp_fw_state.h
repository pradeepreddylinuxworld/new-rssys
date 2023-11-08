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

#ifndef NDP120_DSP_FW_STATE_H_
#define NDP120_DSP_FW_STATE_H_
#include <syntiant-dsp-firmware/ndp120_dsp_sample.h>

#ifndef __KERNEL__
#include <stdint.h>
#endif

#include <syntiant-dsp-firmware/ndp120_dsp_flow.h>
#include <syntiant-dsp-firmware/ndp120_dsp_dnn.h>
#include <syntiant-dsp-firmware/ndp120_dsp_fifo.h>
#include <syntiant-dsp-firmware/ndp120_dsp_algo_interface.h>
#include <syntiant-dsp-firmware/ndp120_dsp_audio_sync.h>

/* debug enums */
enum {
    NDP120_DEBUG_MESSAGES = 0x1,
    NDP120_DEBUG_DNN_CYCLE_COUNT = 0x2
};

/*
    Field `preset_id` is for use solely by the host to leave
    a breadcrumb WRT what "preset" config is in place.
    It has no operational use in the DSP code.
 */

typedef struct {
    ndp120_dsp_data_flow_rule_t src_pcm_audio[NDP120_PCM_DATA_FLOW_RULE_MAX];
    ndp120_dsp_data_flow_rule_t src_function[NDP120_FUNC_DATA_FLOW_RULE_MAX];
    ndp120_dsp_data_flow_rule_t src_nn[NDP120_NN_DATA_FLOW_RULE_MAX];
} ndp120_dsp_data_flow_setup_t;

typedef struct {
    uint32_t frame_cnt;         /* processed frames             */
    uint32_t dnn_int_cnt;       /* dnn interrupts               */
    uint32_t h2d_mb_cnt;        /* host   -> DSP mb             */
    uint32_t d2m_mb_cnt;        /* DSP    -> MCU mb             */
    uint32_t m2d_mb_cnt;        /* MCU    -> DSP mb             */
    uint32_t watermark_cnt;     /* DSP WM -> host               */
    uint32_t fifo_overflow_cnt; /* FW detected fifo overflow    */
    uint32_t nn_cycle_cnt[MAX_NETWORKS];  /* cycle count per NN */
    uint32_t nn_run_cnt[MAX_NETWORKS];  /* DNN run count per NN */
} ndp120_dsp_counters_t;

typedef struct {
    int32_t algo_id;            /* -1 means "none" */
    int32_t algo_config_index;  /* -1 means "none" */
    int32_t algo_init_status;
} ndp120_dsp_algo_t;

typedef struct {
        /* size of each sample, in bytes */
        uint32_t aud_samp_size_bytes;
        uint32_t func_samp_size_bytes;

        /* # of samples that can be stored */
        uint32_t aud_samp_cap;
        uint32_t func_samp_cap;

        uint32_t fifo_threshold_bytes[FIFO_CNT];
        uint32_t notify_on_sample_ready;

} ndp120_dsp_config_t;

typedef struct {
    uint32_t fifo_start_adx[FIFO_CNT];
    uint32_t fifo_end_adx[FIFO_CNT];
    uint32_t fifo_latest_frame_ptr[FIFO_CNT];
} ndp120_dsp_fifo_info_t ;

typedef struct {

        /* samples in buf */
        uint32_t aud_samp_cnt;
        /* aud_annotation_cnt is same as aud_samp_cnt */
        uint32_t func_samp_cnt;

        /* ptr to buffers */
        uint32_t aud_samp_buf_ptr;
        uint32_t aud_annotation_buf_ptr;
        uint32_t func_samp_buf_ptr;

        /* prods/cons for each type */
        uint32_t aud_samp_buf_prod;
        uint32_t aud_samp_buf_cons;

        uint32_t aud_annotation_buf_prod;
        uint32_t aud_annotation_buf_cons;

        uint32_t func_samp_buf_prod;
        uint32_t func_samp_buf_cons;

} ndp120_dsp_buffers_t;

/* Host facing data struct */
typedef struct ndp120_dsp_fw_base_s {
    uint32_t magic;

    /* Data flow */
    uint32_t data_flow_current_set_id;
    ndp120_dsp_data_flow_setup_t data_flow;

    /* Memory config */
    ndp120_dsp_config_t config;
    ndp120_dsp_buffers_t buffers;
    ndp120_dsp_fifo_info_t fifo_info;

    /* DNN */
    ndp120_metadata_t metadata;
    uint8_t dnn_last_network_completed;
    uint8_t _rsvd[3];

    /* uint32_t to assure alignment */
    uint32_t algo_config[NDP120_DSP_ALGO_CONFIG_MAX_COUNT][NDP120_DSP_ALGO_CONFIG_LEN_BYTES /
        sizeof(uint32_t)];

    /* Debug */
    ndp120_dsp_counters_t counters;
    volatile uint8_t debug_buf[128];
    uint32_t debug_flag;

} ndp120_dsp_fw_base_t;

typedef struct {
    uint8_t src_type;
    uint8_t src_param;
    uint8_t _dummy0;
    uint8_t _dummy1;
} ndp120_dsp_fw_aud2_config_t;

typedef struct {
    ndp120_dsp_fw_base_t base;
    /* AUD2 */
    ndp120_dsp_fw_aud2_config_t aud2_config;

    /* SYNC config */
    ndp120_dsp_audio_sync_config_t audio_sync_config;

    /* Algorithms and their config */
    ndp120_dsp_algo_t algos[NDP120_DSP_ALGO_MAX_COUNT];

    /* Any struct that needs to be shared with host must appear before
     * DNN state, which contains a pointer and the size of a pointer
     * causes havoc to host side if the host is running in 64bit mode
     */
    /* DNN */
    ndp120_dnn_network_state_t dnn_state[MAX_NETWORKS];

    void * mem_heap_base;
    void * mem_dnn_data_base;
} ndp120_dsp_fw_state_t;

/* this is an enum so it will be picked up by clang2py */
/* also, it has to be less than 2**31 -1 for ISO C     */

enum { NDP120_DSP_FW_STATE_MAGIC = 0xFACADE };

#endif
