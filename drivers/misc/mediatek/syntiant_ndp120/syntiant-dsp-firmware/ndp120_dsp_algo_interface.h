/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2021 Syntiant Corporation
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

#ifndef NDP120_DSP_ALGO_INTERFACE_H_
#define NDP120_DSP_ALGO_INTERFACE_H_

#ifndef __KERNEL__
#include <stdlib.h>
#include <stdint.h>
#endif

enum {
    NDP120_DSP_ALGO_MAX_COUNT = 4,
    NDP120_DSP_ALGO_CONFIG_MAX_COUNT = 4,
    NDP120_DSP_ALGO_CONFIG_LEN_BYTES = 64,
    NDP120_DSP_ALGO_API_VERSION = 1,
    NDP120_DSP_ALGO_API_MISMATCH_ERROR = 0xF0
};

typedef struct {
    uint32_t        data_cnt;          /* Number of pointers in data & len */
    const uint8_t **data;              /* array of _pointers_              */
    uint32_t       *data_len;          /* ptr to array of uint32_t         */
    void           *algo_config;       /* ptr to opaque config area        */
    uint32_t        algo_config_len;   /* length in bytes                  */
} ndp120_dsp_algo_process_params_t;

enum {
    NDP120_DSP_PCM0_DATA = 0,
    NDP120_DSP_PCM1_DATA,
    NDP120_DSP_PCM2_DATA,
    NDP120_DSP_PCM3_DATA,
    NDP120_DSP_SPI_DATA,
    NDP120_DSP_NN_DATA,
    NDP120_DSP_I2S_DATA,
    NDP120_DSP_DATA_LEN
};

#define DSP_ALGO_PARAMS_DATA_CNT(p) ((p).data_cnt)
#define DSP_ALGO_PARAMS_DATA_PTR(p, idx) ((p).data[idx])
#define DSP_ALGO_PARAMS_DATA_LEN(p, idx) ((p).data_len[idx])
#define DSP_ALGO_PARAMS_DEBUG_PTR(p) ((p).debug_data)
#define DSP_ALGO_PARAMS_DEBUG_LEN(p) (((p).debug_data_len))

typedef int   (*dsp_algo_init_ptr_t)(void *algo_config, uint32_t algo_config_len);
typedef int   (*dsp_algo_process_ptr_t)(ndp120_dsp_algo_process_params_t *params);
typedef void  (*dsp_algo_deinit_ptr_t)(void);
typedef const uint32_t dsp_algo_api_ver_t;

typedef struct {
    const int32_t algo_id;
    const dsp_algo_init_ptr_t init_func;
    const dsp_algo_deinit_ptr_t deinit_func;
    const dsp_algo_process_ptr_t process_func;
    dsp_algo_api_ver_t *api_version;
} ndp120_dsp_algo_entry_t;

#endif
