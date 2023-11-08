/*
SYNTIANT CONFIDENTIAL
_____________________

Copyright (c) 2017-2020 Syntiant Corporation
All Rights Reserved.

NOTICE:  All information contained herein is, and remains the property of
Syntiant Corporation and its suppliers, if any.  The intellectual and
technical concepts contained herein are proprietary to Syntiant Corporation
and its suppliers and may be covered by U.S. and Foreign Patents, patents in
process, and are protected by trade secret or copyright law.  Dissemination of
this information or reproduction of this material is strictly forbidden unless
prior written permission is obtained from Syntiant Corporation.
*/
#ifndef SYNTIANT_PACKAGE_H
#define SYNTIANT_PACKAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant_ilib/syntiant_portability.h>
#include <syntiant_packager/syntiant_package_consts.h>

/**
 * @brief General TLV structure
 */
typedef struct tlv_ {
    uint32_t tag;
    uint32_t length;
    int8_t val[1];    /* infinitely long array */
} tlv_t;

/**
 * @brief calculate CRC32 on bytes without using any library
 *
 * @param[in] bytes array of bytes
 * @param len len of array
 * @return uint32_t CRC32 value
 */
uint32_t crc32_no_lib(uint8_t *bytes, size_t len);

/**
 * @brief get Intial CRC32 value
 *
 * @return uint32_t intermediate CRC32 value
 */
uint32_t crc32_no_lib_init(void);

/**
 * @brief update the CRC32 using given bytes
 *
 * @param crc initial or current CRC32 value
 * @param bytes array of bytes
 * @param len length of array
 * @return uint32_t intermediate CRC32 value
 */
uint32_t crc32_no_lib_update(uint32_t crc, uint8_t *bytes, size_t len);

/**
 * @brief finalize the CRC32 value
 *
 * @param crc intermediate CRC32 value
 * @return uint32_t final CRC32 value
 */
uint32_t crc32_no_lib_finalize(uint32_t crc);

#if defined(WINDOWS_KERNEL) || defined(_MSC_VER) || defined(__MINGW32__)
#pragma pack(push)
#pragma pack(1)

#if defined(SYNTIANT_PACK)
#undef SYNTIANT_PACK
#define SYNTIANT_PACK
#endif

#endif
/**
 * @brief fc parameters v2
 *
 * @param num_features totall number of input features
 * @param num_static_features total number of static features
 * @param dnn_frame_size number of features to feed NN before it stars
 * @param num_nodes num of num nodes per layer
 * @param scale_factor scale factor for each layer
 * @param num_biases num of biases for each layer 
 * @param input_clipping_threshold  input features <= this value would be made zero.
 * @param dnn_run_threshold  if any input feature >= this threshold, NN would run.
 * @param quantization_scheme linear or squared quantization for each layer.
 * @param out_shift applies 2^-x on DNN summation, multiply scale_factor 2^x
*/
typedef struct syntiant_fc_params_v2_t_{
    uint16_t num_features;
    uint16_t num_static_features;
    uint16_t dnn_frame_size; 
    uint8_t num_nodes[4]; 
    uint8_t scale_factor[4]; 
    uint8_t num_biases[4]; 
    uint8_t input_clipping_threshold; 
    uint8_t dnn_run_threshold; 
    uint8_t quantization_scheme[4]; 
    uint8_t out_shift[4]; 
} syntiant_fc_params_v2_t;

SYNTIANT_CASSERT(sizeof(syntiant_fc_params_v2_t) == FC_CONFIG_PARAMS_V2_SIZE,
                 "Unexpected size for syntiant_fc_params_v2_t")
/**
 * @brief fc parameters v3
 *
 * @param num_features totall number of input features
 * @param num_static_features total number of static features
 * @param dnn_frame_size number of features to feed NN before it stars
 * @param num_nodes num of num nodes per layer
 * @param scale_factor scale factor for each layer
 * @param num_biases num of biases for each layer 
 * @param input_clipping_threshold  input features <= this value would be made zero
 * @param dnn_run_threshold  if any input feature >= this threshold, NN would run
 * @param quantization_scheme linear or squared quantization for each layer
 * @param out_shift applies 2^-x on DNN summation, multiply scale_factor 2^x
*/
typedef struct syntiant_fc_params_v3_t_{
    uint8_t dnn_run_threshold;
    uint8_t input_clipping_threshold;
    uint16_t num_features;
    uint16_t num_static_features;
    uint16_t dnn_frame_size;
    uint8_t num_nodes[4];
    uint8_t scale_factor[4];
    uint8_t num_biases[4];
    uint8_t quantization_scheme[4];
    uint8_t out_shift[4];
} syntiant_fc_params_v3_t;
SYNTIANT_CASSERT(sizeof(syntiant_fc_params_v3_t) == FC_CONFIG_PARAMS_V3_SIZE,
                 "Unexpected size for syntiant_fc_params_v3_t") 

/**
 * @brief union for storing fc parameters v2 and v3
 *
 * @param fc_params_v2 fc params v2 
 * @param fc_params_v3 fc params v3
 */
typedef union syntiant_fc_params_t_{
    syntiant_fc_params_v2_t fc_params_v2;
    syntiant_fc_params_v3_t fc_params_v3;
} syntiant_fc_params_t;


/**
 * @brief metadata for fc params
 *
 * @param params_to_read reserved
 * @param num_features number of features
 * @param num_static_features total number of static features
 * @param dnn_frame_size number of features to fee NN before it starts
 * @param num_nodes num of num nodes per layer
 * @param num_biases num of biases for each layer
*/
typedef struct syntiant_fc_params_metadata_t_{
    uint32_t params_to_read;
    uint16_t num_features;
    uint16_t num_static_features;
    uint16_t dnn_frame_size;
    uint16_t rsvd;
    uint8_t num_nodes[4];
    uint8_t num_biases[4];
} syntiant_fc_params_metadata_t;
SYNTIANT_CASSERT(sizeof(syntiant_fc_params_metadata_t)
                 == FC_PARAMS_METADATA_SIZE,
                 "Unexpected size for syntiant_fc_params_metadata_t") 

/**
 * @brief general information about number of states and classes and recently parsed timeout
 * @param num_states total number of states
 * @param num_classes total number of classes
 * @param timeout timeout for the current state
 * @param cur_state current state
 * @param cur_class current class
 */
typedef struct syntiant_ph_params_metadata_v1_t_{
    uint32_t num_states;
    uint32_t num_classes;
    uint32_t timeout;
    uint32_t cur_state;
    uint32_t cur_class;
} syntiant_ph_params_metadata_v1_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_params_metadata_v1_t)
                 == PH_PARAMS_METADATA_v1_SIZE,
                 "Unexpected size for syntiant_ph_params_metadata_v1_t")

/**
 * @brief general information about number of states and classes and recently parsed timeout
 * @param num_states total number of states
 * @param num_classes total number of classes
 * @param ph_type posterior handler frame processor function type
 * @param timeout timeout for the current state
 * @param cur_state current state
 * @param cur_class current class
 */
typedef struct syntiant_ph_params_metadata_v2_t_{
    uint32_t num_states;
    uint32_t num_classes;
    uint32_t ph_type;
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t timeout_action_arg;
    uint32_t cur_state;
    uint32_t cur_class;
} syntiant_ph_params_metadata_v2_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_params_metadata_v2_t)
                  == PH_PARAMS_METADATA_v2_SIZE,
                 "Unexpected size for syntiant_ph_params_metadata_v2_t")

/**
 * @brief general information about number of states and classes and recently parsed timeout
 * @param num_states total number of states
 * @param num_classes total number of classes
 * @param ph_type posterior handler frame processor function type
 * @param timeout timeout for the current state
 * @param cur_state current state
 * @param cur_class current class
 * @param adaptive_frames
 * @param adaptive_denominator
 * @param adaptive_max_updates
 */
typedef struct syntiant_ph_params_metadata_v3_t_{
    uint32_t adaptive_frames;
    uint32_t adaptive_denominator;
    uint32_t adaptive_max_updates;
    uint32_t num_states;
    uint32_t num_classes;
    uint32_t ph_type;
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t timeout_action_arg;
    uint32_t cur_state;
    uint32_t cur_class;
} syntiant_ph_params_metadata_v3_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_params_metadata_v3_t)
                  == PH_PARAMS_METADATA_v3_SIZE,
                 "Unexpected size for syntiant_ph_params_metadata_v3_t")

/**
 * @brief union for ph params metadata
 *
 * @param v1 ph params metadata v1
 * @param v2 ph params metadata v2
 * @param v3 ph params metadata v3
 */

typedef union syntiant_ph_params_metadata_t_{
    syntiant_ph_params_metadata_v1_t v1;
    syntiant_ph_params_metadata_v2_t v2;
    syntiant_ph_params_metadata_v3_t v3;
} syntiant_ph_params_metadata_t;


/**
 * @brief posterior params for class
 *
 * @param phwin window
 * @param phth threshold
 * @param phbackoff backoff
 * @param phaction action
 * @param pharg argument
 * @param phqueuesize smoothing queue size
 */
typedef struct syntiant_ph_params_t_{
    uint32_t phwin;
    uint32_t phth;
    uint32_t phbackoff;
    uint32_t phaction;
    uint32_t pharg;
    uint32_t phqueuesize;
} syntiant_ph_params_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_params_t)
                 == PH_PARAMS_SIZE,
                 "Unexpected size for syntiant_ph_params_t") 
/**
 * @brief posterior params for class
 *
 * @param phwin window
 * @param phth threshold
 * @param phbackoff backoff
 * @param phaction action
 * @param pharg argument
 * @param phqueuesize smoothing queue size
 * @param adaptive_threshold_on
 */
typedef struct syntiant_ph_param_t_{
    uint32_t phwin;
    uint32_t phth;
    uint32_t phbackoff;
    uint32_t phaction;
    uint32_t pharg;
    uint32_t phqueuesize;
    uint32_t adaptive_threshold_on;
} syntiant_ph_params_v2_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_params_v2_t)
                 == PH_PARAMS_SIZE_V2,
                 "Unexpected size for syntiant_ph_params_v2_t")

/**
 * @brief Posterior handler collection parameters used for parsing.
 */
typedef struct syntiant_ph_collection_params_t_{
    struct {
        uint32_t num_ph; /* add gree space to the struct for the future */
    } collection;
    struct {
        uint32_t num_classes;
        uint32_t num_states;
        uint32_t ph_type;
    } ph;
    struct {
        uint32_t timeout;
        uint32_t timeout_action;
        uint32_t timeout_action_arg0;
        uint32_t timeout_action_arg1;
    } state;
    struct {
        uint32_t window;
        uint32_t threshold;
        uint32_t backoff;
        uint32_t queuesize;
        uint32_t action;
        uint32_t action_arg0;
        uint32_t action_arg1;
    } class_;
    struct {
        uint32_t cur_ph;
        uint32_t cur_state;
        uint32_t cur_class;
        uint32_t parsing;
        uint32_t parsed;
    } parser;
} syntiant_ph_collection_params_t;
SYNTIANT_CASSERT(sizeof(syntiant_ph_collection_params_t)
                  == PH_COLLECTION_PARAMS_SIZE,
                 "Unexpected size for syntiant_ph_collection_params_t")

/**
 * @brief Neural network metadata parameters used for parsing.
 */
typedef struct syntiant_nn_metadata_v1_t_{
    uint32_t nn_num;
    struct {
        uint32_t num_layers;
        uint32_t is_nn_cached;
        uint32_t nn_input_isa_idx;
        uint32_t nn_output_isa_idx;
        uint32_t nn_input_layer_type;
    } base_meta;
    struct {
        uint32_t x;
        uint32_t y;
        uint32_t z;
    } inp_size;
    struct {
        uint32_t input_coord;
        uint32_t output_coord;
    } coords;
    struct {
        uint32_t input_base_coord_max;
        uint32_t output_base_coord_max;
        uint32_t input_base_coord_add;
        uint16_t input_offset_add;
        uint16_t input_offset_max;
        uint16_t output_base_coord_add;
        uint16_t output_base_coord_stride;
    } cache_params;
    struct {
        uint32_t cur_nn;
        uint32_t cur_layer;
        uint32_t parsing;
        uint32_t parsed;
    } parser;
} syntiant_nn_metadata_v1_t;
SYNTIANT_CASSERT(sizeof(syntiant_nn_metadata_v1_t)
                  == NN_METADATA_V1_SIZE,
                 "Unexpected size for syntiant_nn_metadata_v1_t")

typedef union syntiant_nn_metadata_t_{
    syntiant_nn_metadata_v1_t v1;
} syntiant_nn_metadata_t;

/**
 * @brief A series of DSP flow rules with their set id.
 */
typedef struct syntiant_dsp_flow_collection_v1_t_{
    uint8_t set_id;
    uint8_t src_type;
    uint8_t src_param;
    uint8_t dst_type;
    uint8_t dst_param;
    int8_t  algo_config_index;
    uint8_t pad[2];
} syntiant_dsp_flow_collection_v1_t;
SYNTIANT_CASSERT(sizeof(syntiant_dsp_flow_collection_v1_t)
                  == DSP_FLOW_COLLECTION_V1_SIZE,
                 "Unexpected size for syntiant_dsp_flow_collection_v1_t")

typedef union syntiant_dsp_flow_collection_t_{
    syntiant_dsp_flow_collection_v1_t v1;
} syntiant_dsp_flow_collection_t;

/* This define should match with the NDP120_DSP_ALGO_CONFIG_LEN_BYTES
 * in syntiant-dsp-firmware/ndp120_dsp_algo_interface.h
 * We can not include that interface here as it breaks the linux driver
 * compilation
 */
#define NDP120_ALGO_CONFIG_AREA_LEN (64)

typedef struct {
    uint32_t index;
    uint8_t config[NDP120_ALGO_CONFIG_AREA_LEN];
} syntiant_dsp_fe_config_data_t ;

/**
 * @brief A series of MCU orchestrator nodes.
 */
typedef struct syntiant_mcu_orch_v1_t_{
    uint8_t id;
    uint8_t flow_id;
    uint8_t status;
    uint8_t type;
    uint8_t action;
    uint8_t num_inputs;
    uint8_t num_outputs;
    uint8_t input_edges[4];
    uint8_t next_ids[4];
    uint8_t pad;
} syntiant_mcu_orch_v1_t;
SYNTIANT_CASSERT(sizeof(syntiant_mcu_orch_v1_t)
                  == MCU_ORCHESTRATOR_V1_SIZE,
                 "Unexpected size for syntiant_mcu_orch_v1_t")

typedef union syntiant_mcu_orch_t_{
    syntiant_mcu_orch_v1_t v1;
} syntiant_mcu_orch_t;

/**
 * @brief hw parameters
 *
 * @param sample_frequency_hz sample frequency in hz
 * @param frame_size frame size in num of samples
 * @param frame_step frame step in num of samples
 * @param num_filter_banks num of filter banks
 * @param num_activations_usednum of activations used for final layer
 * @param mel_bins_length num of melbins
 * @param mel_bin FFT bins
 */
typedef struct syntiant_hw_params_v2_t_{
   uint32_t sample_frequency_hz;
   uint32_t frame_size;
   uint32_t frame_step;
   uint32_t num_filter_banks;
   uint32_t num_activations_used;
   uint32_t mel_bins_length;
   uint32_t mel_bins[40+2]; /* this 42 will be replaced with mel_bins_length */
} syntiant_hw_params_v2_t;
SYNTIANT_CASSERT(sizeof(syntiant_hw_params_v2_t)
                 == HW_PARAMS_V2_SIZE,
                 "Unexpected size for syntiant_hw_params_v2_t") 

/**
 * @brief board parameters version 1
 *
 * @param dnn_input DNN input source
 * @param input_clock_rate input clock rate
 * @param pdm_clock_rate PDM clock rate
 * @param pdm_dc_offset DC offset for DN inputs
 * @param pdm_clock_ndp PDM clock source
 * @param power_offset
 * @param preemphasis_exponent
 * @param cal_pdm_in_shift
 * @param cal_pdm_out_shift
 * @param power_scale_exponent
 * @param agc
 * @param equalizer
 */
typedef struct syntiant_board_params_v1_t_{
    uint8_t dnn_input;
    uint32_t input_clock_rate;
    uint32_t pdm_clock_rate;
    int32_t pdm_dc_offset[2];
    uint8_t pdm_clock_ndp;
    uint8_t power_offset;
    uint32_t preemphasis_exponent;
    uint8_t cal_pdm_in_shift[2];
    uint8_t cal_pdm_out_shift[2];
    uint32_t power_scale_exponent;
    uint8_t agc;
    int8_t equalizer[40];
} SYNTIANT_PACK syntiant_board_params_v1_t;
SYNTIANT_CASSERT(sizeof(syntiant_board_params_v1_t)
                 == BOARD_PARAMS_V1_SIZE,
                 "Unexpected size for syntiant_board_params_v1_t") 

/**
 * @brief board parameters version 2
 *
 * @param input_clock_rate input clock rate
 * @param pdm_clock_rate PDM clock rate
 * @param preemphasis_exponent
 * @param power_scale_exponent
 * @param pdm_dc_offset DC offset for DN inputs
 * @param cal_pdm_in_shift
 * @param cal_pdm_out_shift 
 * @param dnn_input DNN input source
 * @param pdm_clock_ndp PDM clock source
 * @param power_offset
 * @ param agc
 * @param equalizer
 */
typedef struct syntiant_board_params_v2_t_{
    uint8_t agc;
    uint8_t dnn_input;
    uint8_t pdm_clock_ndp;
    uint8_t power_offset;
    uint8_t cal_pdm_in_shift[2];
    uint8_t cal_pdm_out_shift[2];
    uint32_t input_clock_rate;
    uint32_t pdm_clock_rate;
    int32_t pdm_dc_offset[2];
    uint32_t preemphasis_exponent;
    uint32_t power_scale_exponent;
    int8_t equalizer[40];
} syntiant_board_params_v2_t;
SYNTIANT_CASSERT(sizeof(syntiant_board_params_v2_t)
                 == BOARD_PARAMS_V2_SIZE,
                 "Unexpected size for syntiant_board_params_v2_t") 
/**
 * @brief board parameters version 3
 *
 * @param input_clock_rate input clock rate
 * @param pdm_clock_rate PDM clock rate
 * @param preemphasis_exponent
 * @param power_scale_exponent
 * @param pdm_dc_offset DC offset for DN inputs
 * @param cal_pdm_in_shift
 * @param cal_pdm_out_shift
 * @param dnn_input DNN input source
 * @param pdm_clock_ndp PDM clock source
 * @param power_offset
 * @ param agc
 * @param equalizer
 * @param agc max adjustment
 * @param agc reference quiet level
 */
typedef struct syntiant_board_params_v3_t_{
    uint8_t agc;
    uint8_t dnn_input;
    uint8_t pdm_clock_ndp;
    uint8_t power_offset;
    uint8_t cal_pdm_in_shift[2];
    uint8_t cal_pdm_out_shift[2];
    uint32_t input_clock_rate;
    uint32_t pdm_clock_rate;
    int32_t pdm_dc_offset[2];
    uint32_t preemphasis_exponent;
    uint32_t power_scale_exponent;
    int8_t equalizer[40];
    uint32_t agc_ref_lvl;
    uint32_t agc_max_adj[2];
} syntiant_board_params_v3_t;
SYNTIANT_CASSERT(sizeof(syntiant_board_params_v3_t)
                 == BOARD_PARAMS_V3_SIZE,
                 "Unexpected size for syntiant_board_params_v3_t")
/**
 * @brief board parameters version 4
 *
 * @param input_clock_rate input clock rate
 * @param pdm_clock_rate PDM clock rate
 * @param preemphasis_exponent
 * @param power_scale_exponent
 * @param pdm_dc_offset DC offset for DN inputs
 * @param cal_pdm_in_shift
 * @param cal_pdm_out_shift
 * @param dnn_input DNN input source
 * @param pdm_clock_ndp PDM clock source
 * @param power_offset
 * @ param agc
 * @param equalizer
 * @param agc max adjustment
 * @param agc reference quiet level
 * #noise_threshold noise threshold level
 * @noise_threshold_win noise threshold windows
 */
typedef struct syntiant_board_params_v4_t_{
    uint8_t agc;
    uint8_t dnn_input;
    uint8_t pdm_clock_ndp;
    uint8_t power_offset;
    uint8_t cal_pdm_in_shift[2];
    uint8_t cal_pdm_out_shift[2];
    uint32_t input_clock_rate;
    uint32_t pdm_clock_rate;
    int32_t pdm_dc_offset[2];
    uint32_t preemphasis_exponent;
    uint32_t power_scale_exponent;
    int8_t equalizer[40];
    uint32_t agc_ref_lvl;
    uint32_t agc_max_adj[2];
    int32_t noise_threshold;
    uint32_t noise_thresh_win;
} syntiant_board_params_v4_t;
SYNTIANT_CASSERT(sizeof(syntiant_board_params_v4_t)
                 == BOARD_PARAMS_V4_SIZE,
                 "Unexpected size for syntiant_board_params_v4_t")

/**
 * @brief union for storing board params
 *
 * @param board_params_v1 board params v1
 * @param board_params_v2 board params v2
 */

typedef union syntiant_board_params_t_{
    syntiant_board_params_v1_t board_params_v1;
    syntiant_board_params_v2_t board_params_v2;
    syntiant_board_params_v3_t board_params_v3;
    syntiant_board_params_v4_t board_params_v4;
} syntiant_board_params_t;

/**
 * @brief union for storing ph params
 *
 * @param ph_params posterior params v1
 * @param ph_params_v2 posterior params v2
 */
typedef union syntiant_ph_param_sp_t_ {
    syntiant_ph_params_t v1;
    syntiant_ph_params_v2_t v2;
} syntiant_ph_param_sp_t;
/**
 * @brief union for storing C data structures read from chunks
 *
 * @param ph_params posterior params
 * @param syntiant_fc_params_t fc params 
 * @param hw_params hardware config
 * @param board_params board parameters
 * @param fw_version version of fw
 * @param params_version version of DNN params
 * @param labels class labels
 * @param fw firmware 
 * @param packed_params packed params
 */
typedef union syntiant_pkg_parser_data_t_{
    syntiant_ph_param_sp_t ph_params;
    syntiant_ph_collection_params_t phc_params;
    syntiant_fc_params_t fc_params;
    syntiant_hw_params_v2_t hw_params;
    syntiant_board_params_t board_params;
    syntiant_nn_metadata_t nn_metadata;
    syntiant_dsp_flow_collection_t dsp_flow_collection;
    syntiant_dsp_fe_config_data_t dsp_fe_config;
    syntiant_mcu_orch_t mcu_orchestrator;
    uint8_t fw_version[VERSION_MAX_SIZE];
    uint8_t dsp_fw_version[VERSION_MAX_SIZE];
    uint8_t params_version[VERSION_MAX_SIZE];
    uint8_t pkg_version[VERSION_MAX_SIZE];
    uint8_t pkger_version[VERSION_MAX_SIZE];
    uint8_t labels[PARSER_SCRATCH_SIZE];
    uint8_t fw[PARSER_SCRATCH_SIZE];
    uint8_t packed_params[PARSER_SCRATCH_SIZE];
} syntiant_pkg_parser_data_t;

/**
 * @brief union for storing C data structures to help keeping track of velues 
 * partially read
 * @param ph_metadata metadata for posterior params
 * @param fc_metadata metadata for fc params
 * @param fw_metadata metadata for firmware that tracks num of chunks on which 
 * fw coming
 * @param labels_metadata metadata for firmware that tracks num of chunks on which 
 * fw coming
 */
typedef union syntiant_pkg_parser_metadata_t_{
    syntiant_ph_params_metadata_t ph_metadata;
    syntiant_fc_params_metadata_t fc_metadata;
    uint32_t fw_metadata;
    uint32_t labels_metadata;
    uint32_t scratch_metadata[NN_FLOW_METADATA_SIZE];
} syntiant_pkg_parser_metadata_t;


/*
 * @brief package parser state
 * @param data structs to store values
 * @param metadata struct to store metadata information for incrementally parsing
 * @param expected bytes to read for tag, length, (partial) values
 * @param header_val header magic value
 * @param calc_crc last calculated crc
 * @param partially_read_length amount partially read from the total value 
 * @param magic_header_found of header is found
 * @param mode parsing mode (starting/continuing to read tag, length, or value)
 * @param value_mode value parsing mode (partial value is ready or not)
 * @param ptr current pointer in a chunk
 * @param open_ram_begin pointers to begining of a chunk 
 * @param open_ram_end pointer to the end of a chunk 
 * @param tag last seen tag
 * @param length last seen length
 * @param exp_crc expected crc
 * @param is_current_fw_package if fw_package is observed
 * @param is_current_params_package if params package is observed
 * @param is_current_dsp_fw_package if dsp fw pacakge is observed
 */
typedef struct syntiant_pkg_parser_state_t_ {
    syntiant_pkg_parser_data_t data;
    syntiant_pkg_parser_metadata_t metadata;
    uint32_t expected_bytes;
    uint32_t header_val;
    uint32_t calc_crc;
    uint32_t partially_read_length;
    int magic_header_found;
    int mode;
    int value_mode;
    uint8_t* ptr;
    uint8_t* open_ram_begin;
    uint8_t* open_ram_end;
    uint8_t tag[TAG_SIZE];
    uint8_t length[LENGTH_SIZE];
    uint8_t exp_crc[CRC_SIZE];
    uint8_t is_multisegment;
    uint8_t is_current_fw_package;
    uint8_t is_current_params_package;
    uint8_t is_current_dsp_fw_package;
} syntiant_pkg_parser_state_t;

#if defined(WINDOWS_KERNEL) || defined(_MSC_VER) || defined(__MINGW32__)
#pragma pack(pop)
#endif

/**
 * @brief intialize and prepare syntiant_ndp_pkg_parser_state_t to start parsing 
 * a raw package
 * param[in] pstate parser state
 * param[in] config_address address of config
 * param[in] open_ram_begin first address of open ram
 * param[in] open_ram_end last address of open ram
 */
extern void syntiant_pkg_parser_init(syntiant_pkg_parser_state_t *pstate);

/**
 * @brief reset ptr in syntiant_ndp_pkg_parser_state_t to start processing a 
 * new chunk
 * param[in] pstate parser state 
 */
extern void syntiant_pkg_parser_reset(syntiant_pkg_parser_state_t *pstate);

/**
 * @brief pkg header information
 *
 * @param[in] tlv tag, length, and value as an array of bytes
 * @return true or false if tlv is Magic Number TLV or not
 */
extern int syntiant_pkg_check_header_tlv(uint8_t *tlv);


/**
 * @read specific amount of data from chunk, and update CRC
 *
 * @param[in] pstate package parser status
 * @param[in] dest destination to store data or point to data
 * @param[in] max_length maximum length of data to read
 * @param[in] fake if skip the data or parse it
 * @param[in] calc_crc if crc should be updated or not
 * @param[in] copy copy data from open ram to dest or set dest to point to a place in openram
 * @return amount of data that is read
 */
extern unsigned int syntiant_pkg_read_chunk_data(syntiant_pkg_parser_state_t
                                                 *pstate, uint8_t* dest,
                                                 uint32_t max_length,
                                                 int fake, int calc_crc,
                                                 int copy);

/**
 * @read tag
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_read_tag(syntiant_pkg_parser_state_t *pstate);

/**
 * @read length
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_read_length(syntiant_pkg_parser_state_t *pstate);

/**
 * @read value
 *
 * @param[in] pstate parser status
 * @param[in] collect_log if print log or not
 * @return error code
 */
extern int syntiant_pkg_read_value(syntiant_pkg_parser_state_t *pstate,
                                   int collect_log);

/**
 * @parse part of chunk
 *
 * @param[in] pstate parser status
 * @param[in] collect_log 1 means to print log, and 0 means no need to print log
 * @return error code
 */
extern int syntiant_pkg_parse_chunk(syntiant_pkg_parser_state_t *pstate,
                                   int collect_log);

/**
 * @prepare chunk for processing
 *
 * @param[in] pstate parser status
 * @param[in] chunk pointer to the begining of a chunk
 * @param[in] length chunk length 
 * @param[in] copy copy the chunk to another memory before process or not
 * @return error code
 */
extern void syntiant_pkg_preprocess_chunk(syntiant_pkg_parser_state_t *pstate,
                                          uint8_t* chunk, int length,
                                          int copy);

/**
 * @parse sentinel value
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_parse_checksum_value(syntiant_pkg_parser_state_t
                                             *pstate);


/**
 * @parse posterior handling parameters value
 *
 * @param[in] pstate parser status
 * @param[in] collect_log if print log or not
 * @return error code
 */
extern int syntiant_pkg_parse_posterior_params(syntiant_pkg_parser_state_t
                                               *pstate, int collect_log);


/**
 * @parse neural network metadata value
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_parse_nn_metadata(syntiant_pkg_parser_state_t *pstate);


/**
 * @parse DSP FE config
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int
syntiant_pkg_parse_dsp_fe_config(syntiant_pkg_parser_state_t *pstate);

/**
 * @parse DSP flow rule collections
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int
syntiant_pkg_parse_dsp_flow_collection(syntiant_pkg_parser_state_t *pstate);


/**
 * @parse mcu orchestrator nodes params
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_parse_mcu_orchestrator(syntiant_pkg_parser_state_t
                                               *pstate);


/**
 * @parse fc parameters
 *
 * @param[in] pstate parser status
 * @param[in] collect_log if print log or not
 * @return error code
 */
extern int syntiant_pkg_parse_fc_config_params(syntiant_pkg_parser_state_t
                                               *pstate, int collect_log);


/**
 * @parse values which are stored in parser state such as version, etc.
 *
 * @param[in] pstate parser status
 * @param[in] collect_log if print log or not
 * @return error code
 */
extern int syntiant_pkg_parse_stored_params(syntiant_pkg_parser_state_t
                                            *pstate, int collect_log);

/**
 * @prints values which are stored in parser state such as version, etc.
 *
 * @param[in] pstate parser status
 * @return error code
 */
extern int syntiant_pkg_print_stored_params(syntiant_pkg_parser_state_t
                                            *pstate);

/**
 * @parse partially stored parameters such as fw and packed params
 *
 * @param[in] pstate parser status
 * @param[in] collect_log if print log or not
 * @return error code
 */
extern int syntiant_pkg_parse_partially_stored_params
(syntiant_pkg_parser_state_t *pstate, int collect_log);

/**
 * @parse header value
 *
 * @param[in] pstate parser status
 * @return error code
 */

extern int syntiant_pkg_parse_header_value(syntiant_pkg_parser_state_t
                                           *pstate);

#ifdef __cplusplus
}
#endif

#endif
