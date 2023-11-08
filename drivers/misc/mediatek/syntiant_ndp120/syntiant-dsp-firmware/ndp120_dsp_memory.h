
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
#ifndef NDP120_DSP_MEMORY_H_
#define NDP120_DSP_MEMORY_H_

#include <stdint.h>
#include <stddef.h>

typedef enum {
    NDP120_DSP_MEM_TYPE_HEAP = 0,
    NDP120_DSP_MEM_TYPE_DNN_DATA = 1
} ndp120_dsp_mem_type_t;

typedef struct {
    size_t size;
    uint8_t pattern[12];
} ndp120_dsp_memory_block_header_t;
#endif
