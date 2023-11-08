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
#ifndef NDP120_DSP_SAMPLE_H_
#define NDP120_DSP_SAMPLE_H_

#ifndef __KERNEL__
#include <stdint.h>
#endif

enum {
    NDP120_DSP_SAMPLE_MODE_RUN,
    NDP120_DSP_SAMPLE_MODE_BLOCK,
    NDP120_DSP_SAMPLE_MODE_DROP
};

enum {
    NDP120_DSP_SAMPLE_THRESH_ANY,
    NDP120_DSP_SAMPLE_THRESH_HALF,
    NDP120_DSP_SAMPLE_THRESH_FULL
};

enum {
    NDP120_DSP_SAMPLE_TYPE_PCM_AUDIO,
    NDP120_DSP_SAMPLE_TYPE_FUNCTION
};

typedef struct {
    uint8_t type;
    uint8_t params;
    uint16_t frame_num;
} ndp120_dsp_sample_header_t;

typedef struct {
    uint8_t  mode; 
    uint8_t  thresh; 
    uint32_t buf_size;
    uint32_t head_idx;
    uint32_t tail_idx;
} ndp120_dsp_sample_buf_state_t;

typedef struct {
    uint8_t src_type;
    uint8_t src_param;
    uint8_t _dummy0; /* maintain 4-align */
    uint8_t _dummy1;
} ndp120_dsp_audio_sample_annotation_t;

#endif
