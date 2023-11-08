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
#ifndef NDP120_DSP_FLOW_H_
#define NDP120_DSP_FLOW_H_

#ifndef __KERNEL__
#include <stdint.h>
#endif
#include <syntiant-dsp-firmware/ndp120_dsp_sample.h>

/* pass in a src_type */
#define NDP120_DSP_DATA_FLOW_SRC_TYPE_STR(x) \
    (x) == NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO ? "PCM" : \
    (x) == NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION ? "FUNC" : \
    (x) == NDP120_DSP_DATA_FLOW_SRC_TYPE_NN ? "NN" : "UNKNOWN"

/* pass in a _rule_ */
#define NDP120_DSP_DATA_FLOW_RULE_DST_STR(x) \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_NONE ? "<INVALID>" : \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_FUNCTION ? "FUNC" : \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_NN ? "NN" : \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_HOST_EXTRACT ? \
    ((x).dst_param ==  NDP120_DSP_DATA_FLOW_DST_SUBTYPE_AUDIO ? "HOST_EXT_AUDIO" : "HOST_EXT_FEATURE") : \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_I2S ? "I2S" : \
    (x).dst_type == NDP120_DSP_DATA_FLOW_DST_TYPE_MCU ? "MCU" : "UNKNOWN"

enum {
    /* If you update this, update the _STR 
       macro above */
    NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO,
    NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION,
    NDP120_DSP_DATA_FLOW_SRC_TYPE_NN
};

#define NDP120_PCM_DATA_FLOW_RULE_MAX 16
#define NDP120_FUNC_DATA_FLOW_RULE_MAX 32
#define NDP120_NN_DATA_FLOW_RULE_MAX 32

enum {
    /* NONE mostly exists so that the SRC 
       and DST enums line up, numerically */
    NDP120_DSP_DATA_FLOW_DST_TYPE_NONE,         
    NDP120_DSP_DATA_FLOW_DST_TYPE_FUNCTION,
    NDP120_DSP_DATA_FLOW_DST_TYPE_NN,
    NDP120_DSP_DATA_FLOW_DST_TYPE_HOST_EXTRACT,
    NDP120_DSP_DATA_FLOW_DST_TYPE_I2S,
    NDP120_DSP_DATA_FLOW_DST_TYPE_MCU
    /* If you update this, update the _STR 
       macro above */
};

enum {
    NDP120_DSP_DATA_FLOW_DST_SUBTYPE_AUDIO = NDP120_DSP_SAMPLE_TYPE_PCM_AUDIO,
    NDP120_DSP_DATA_FLOW_DST_SUBTYPE_FEATURE = NDP120_DSP_SAMPLE_TYPE_FUNCTION
};

#define NDP120_DSP_FLOW_RULE_IS_VALID(x) ((x).dst_type != NDP120_DSP_DATA_FLOW_DST_TYPE_NONE)
#define NDP120_DSP_FLOW_RULE_INVALIDATE(x) ((x).dst_type = NDP120_DSP_DATA_FLOW_DST_TYPE_NONE)

typedef struct {
    uint8_t set_id;
    uint8_t src_param;
    uint8_t dst_param;
    uint8_t dst_type;
    int8_t  algo_config_index; /* -1 means "none" */
    uint8_t _dummy0;
    uint8_t _dummy1;
    uint8_t _dummy2;
} ndp120_dsp_data_flow_rule_t;

#endif
