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

#ifndef NDP120_DSP_DNN_H_
#define NDP120_DSP_DNN_H_

#ifndef __KERNEL__
#include <stdint.h>
#endif

enum {
    LAYERS_PER_NETWORK = 32,
    MAX_NETWORKS = 4
};

typedef struct {
    uint32_t cur_input_coords[LAYERS_PER_NETWORK];
    uint32_t cur_output_coords[LAYERS_PER_NETWORK];
    uint32_t cur_input_offsets[LAYERS_PER_NETWORK];
} ndp120_cache_metadata_t;

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} ndp120_3d_tensor_t;

typedef struct {
    uint32_t input_base_coord_max;
    uint32_t output_base_coord_max;
    uint32_t input_base_coord_add;
    uint16_t input_offset_add;
    uint16_t input_offset_max;
    uint16_t output_base_coord_add;
    uint16_t output_base_coord_stride;
} nn_layer_cache_desc_t;

typedef struct {
    uint32_t layers_per_nn;
    uint32_t is_nn_cached;
    uint32_t input_layer_isa_idx;
    uint32_t output_layer_isa_idx;
    uint32_t input_layer_type;
    ndp120_3d_tensor_t input_layer_size;
    uint32_t input_coords[LAYERS_PER_NETWORK];
    uint32_t output_coords[LAYERS_PER_NETWORK];
    nn_layer_cache_desc_t cache_instructions[LAYERS_PER_NETWORK];
} nn_metadata_t;

typedef struct {
    /* NN info */
    uint32_t input_size;
    nn_metadata_t * nn_meta;

    /* cached conv working data */
    ndp120_cache_metadata_t cache_metadata;

    /* working data */
    uint32_t input_offset;
    uint32_t write_offset;
} ndp120_dnn_network_state_t;

typedef struct ndp120_metadata_s {
    uint32_t nn_cnt;
    nn_metadata_t nn_metadata[MAX_NETWORKS];
} ndp120_metadata_t;


#endif
