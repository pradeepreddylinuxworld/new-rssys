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
#ifndef SYNTIANT_PACKAGE_CONSTS_H
#define SYNTIANT_PACKAGE_CONSTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "syntiant_package_tags.h"

/**
 * @brief sizes defined for pkg parser's parsing granularity
 *
 * TAG_SIZE size of tag which is 4 bytes in A0 and B0
 * LENGTH_SIZE size of length which is 4 bytes in A0 and B0
 * VALID_SIZE size of validity of each section in config region
 * CRC_SIZE size of crc which is 4 bytes in A0 and B0
 * PARSER_SCRATCH_SIZE maximum size of scratch for parser to store data before 
 * processing
 * VERSION_MAX_SIZE maximum possible size of version
 * LABELS_MAX_SIZE maximum possible size of labels
 * FW_MAX_SIZE maximum possible size of firmware
 */
enum syntiant_package_constants_e{
    TAG_SIZE = 4,                 /*size of tag in TLV*/
    LENGTH_SIZE = 4,              /*size of length in TLV*/
    CRC_SIZE = 4,                 /*size of CRC*/
    INPUT_CLK_RATE_SIZE = 4,      /*size of input clk rate*/   
    PARSER_SCRATCH_SIZE = 2048,    /*maximum size of scratch to store parser's data*/ 
    VERSION_MAX_SIZE = 128,       /*maximum possible size of version*/
    LABELS_MAX_SIZE = 2048,       /*maximum possible size of labels*/
    FW_MAX_SIZE = 256*1024,       /*maximum possible size of firmware*/
    NN_NDP10X_MAX_SIZE = 0x48000,
    NN_MAX_SIZE = 768*1024,
    BOARD_PARAMS_V1_SIZE = 72,
    BOARD_PARAMS_V2_SIZE = 72,
    BOARD_PARAMS_V3_SIZE = 84,
    BOARD_PARAMS_V4_SIZE = 92,
    FC_CONFIG_PARAMS_V2_SIZE = 28,
    FC_CONFIG_PARAMS_V3_SIZE = 28,
    FC_PARAMS_METADATA_SIZE = 20,
    PH_PARAMS_SIZE = 24,
    PH_PARAMS_SIZE_V2 = 28,
    PH_PARAMS_METADATA_v1_SIZE = 20,
    PH_PARAMS_METADATA_v2_SIZE = 32,
    PH_PARAMS_METADATA_v3_SIZE = 44,
    HW_PARAMS_V2_SIZE = 192,
    PH_COLLECTION_PARAMS_SIZE = 80,
    NN_METADATA_V1_SIZE = 80,
    DSP_FLOW_COLLECTION_V1_SIZE = 8,
    MCU_ORCHESTRATOR_V1_SIZE = 16,
    NN_PARAMS_ENCRYPTED_METADATA_V1_SIZE = 32
};

/**
 * @brief valid bits for config sections
 *
 * SYNTIANT_CONFIG_LABELS_VALID if labels in config area valid
 * SYNTIANT_CONFIG_FW_VERSION_VALID if fw version in config area is valid  
 * SYNTIANT_CONFIG_NN_VERSION_VALID if params version in config area is valid  
 * SYNTIANT_CONFIG_PKG_VERSION_VALID if pkg version in config area is valid 
 */
enum syntiant_config_validity_constants_e {

    /* from package loading */
    SYNTIANT_CONFIG_LABELS_VALID                = 0x00000001U,
    SYNTIANT_CONFIG_FW_VERSION_VALID            = 0x00000002U,
    SYNTIANT_CONFIG_NN_VERSION_VALID            = 0x00000004U,
    SYNTIANT_CONFIG_PKG_VERSION_VALID           = 0x00000008U,
    SYNTIANT_CONFIG_DSP_FW_VERSION_VALID        = 0x00000010U,

    /* from config (?), maybe packages*/

    SYNTIANT_CONFIG_INPUT_CLK_VALID             = 0x00000020U,

    /* pdm modes have two channels, thus the bits
       are spread out */
    SYNTIANT_CONFIG_PDM_SAMPLE_RATE_VALID       = 0x00000001U,
    SYNTIANT_CONFIG_PDM_RATE_VALID              = 0x00000004U,
    SYNTIANT_CONFIG_PDM_CLK_MODE_VALID          = 0x00000010U,
    SYNTIANT_CONFIG_PDM_MODE_VALID              = 0x00000040U,
    SYNTIANT_CONFIG_PDM_MAX_SPL_VALID           = 0x00000100U,
    /* end spread */

    SYNTIANT_CONFIG_PDM_MAIN_CLK_AT_LAST_CONFIG = 0x00000400U,
    SYNTIANT_CONFIG_EXT_CLK_FREQ_VALID          = 0x00000800U,
    SYNTIANT_CONFIG_MAIN_CLK_SRC_VALID          = 0x00010000U,
    SYNTIANT_CONFIG_PLL_CLK_FREQ_VALID          = 0x00020000U,
    SYNTIANT_CONFIG_FLL_CLK_FREQ_VALID          = 0x00040000U,
    SYNTIANT_CONFIG_PLL_PRESET_VALID            = 0x00080000U,
    SYNTIANT_CONFIG_FLL_PRESET_VALID            = 0x00100000U,

    /* don't forget to update if you change the above */
    SYNTIANT_CONFIG_VALID_MASK                  = 0xFFFFFU
};

/**
 * @brief Package Parsing Errors
 *
 */
enum syntiant_package_parsing_errors_e {
    SYNTIANT_PACKAGE_ERROR_NONE = 0,
    SYNTIANT_PACKAGE_ERROR_HEADER = -1,
    SYNTIANT_PACKAGE_ERROR_FIRMWARE_VERSION_STRING_V1 = -2,
    SYNTIANT_PACKAGE_ERROR_PAD = -3,
    SYNTIANT_PACKAGE_ERROR_CHECKSUM = -4,
    SYNTIANT_PACKAGE_ERROR_NN_VERSION_STRING = -5,
    SYNTIANT_PACKAGE_ERROR_NDP10X_NN_CONFIG_V1 = -6,
    SYNTIANT_PACKAGE_ERROR_NDP10X_NN_PARAMETERS_V1 = -7,
    SYNTIANT_PACKAGE_ERROR_FIRMWARE = -8,
    SYNTIANT_PACKAGE_ERROR_NDP10X_HW_CONFIG_V1 = -9,
    SYNTIANT_PACKAGE_ERROR_NN_LABELS_V1 = -10,
    SYNTIANT_PACKAGE_ERROR_NDP10X_HW_CONFIG_V2 = -11,
    SYNTIANT_PACKAGE_ERROR_NN_PARAMETERS_V2 = -12,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V1 = -13,
    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_ENCRYPTED_FIRMWARE = -14,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V2 = -15,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V3 = -16,
    SYNTIANT_PACKAGE_ERROR_FIRMWARE_VERSION_STRING_V2 = -17,
    SYNTIANT_PACKAGE_ERROR_NN_VERSION_STRING_V2 = -18,
    SYNTIANT_PACKAGE_ERROR_PACKAGE_VERSION_STRING = -19,
    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V1 = -20,
    SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V1 = -21,
    SYNTIANT_PACKAGE_ERROR_NDP10X_NN_PARAMETERS_V3 = -22,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V4 = -23,
    SYNTIANT_PACKAGE_ERROR_NN_LABELS_V2 = -24,
    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V2 = -25,
    SYNTIANT_PACKAGE_ERROR_NDP10X_B0_NN_CONFIG_V3 = -26,
    SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V2 = -27,
    SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V3 = -28,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V5 = -29,
    SYNTIANT_PACKAGE_ERROR_NN_PH_COLLECTION_V1 = -30,
    SYNTIANT_PACKAGE_ERROR_NN_METADATA_V1 = -31,
    SYNTIANT_PACKAGE_ERROR_BOARD_CALIBRATION_PARAMS_V4 = -32,
    SYNTIANT_PACKAGE_ERROR_DSP_FLOW_COLLECTION_V1 = -33,
    SYNTIANT_PACKAGE_ERROR_MCU_ORCHESTRATOR_V1 = -34,
    SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V6 = -35,
    SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR = -999,
    SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV = -1000
};

#define SYNTIANT_PACKAGE_ERROR_NAMES                                           \
    {   "none",                                                                \
        "header",                                                              \
        "firmware_version_string_v1",                                          \
        "pad",                                                                 \
        "checksum",                                                            \
        "nn_version_string",                                                   \
        "ndp10x_nn_config_v1",                                                 \
        "ndp10x_nn_parameters_v1",                                             \
        "firmware",                                                            \
        "ndp10x_hw_config_v1",                                                 \
        "nn_labels_v1",                                                        \
        "ndp10x_hw_config_v2",                                                 \
        "nn_parameters_v2",                                                    \
        "nn_ph_parameters_v1",                                                 \
        "ndp10x_b0_encrypted_firmware",                                        \
        "nn_ph_parameters_v2",                                                 \
        "nn_ph_parameters_v3",                                                 \
        "firmware_version_string_v2",                                          \
        "nn_version_string_v2",                                                \
        "package_version_string",                                              \
        "ndp10x_b0_nn_config_v1"                                               \
        "ndp10x_b0_nn_config_v1",                                              \
        "board_calibration_params_v1",                                         \
        "ndp10x_nn_parameters_v3",                                             \
        "nn_ph_parameters_v4",                                                 \
        "nn_labels_v2",                                                        \
        "ndp10x_b0_nn_config_v2",                                              \
        "ndp10x_b0_nn_config_v3",                                              \
        "board_calibration_params_v2",                                         \
        "board_calibration_params_v3",                                         \
        "nn_ph_parameters_v5",                                                 \
        "nn_ph_collection_v1",                                                 \
        "nn_metadata_v1",                                                      \
        "board_calibration_params_v4",                                         \
        "dsp_flow_collection_v1",                                              \
        "mcu_orchestrator_v1",                                                 \
        "nn_ph_parameters_v6",                                                 \
        "incrementally_parsing_error",                                         \
        "unknown_tlv"                                                          \
    }

#define SYNTIANT_PACKAGE_ERROR_NAME(e)                                         \
    ( ( (e) <= SYNTIANT_PACKAGE_ERROR_NONE &&                                  \
        (e) >= SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V6                      \
      ) ? syntiant_package_error_names[-(e)] :                                 \
      ( (e) <= SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR &&                 \
        (e) >= SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV                              \
      ) ? syntiant_package_error_names[                                        \
            -(e)                                                               \
            +SYNTIANT_PACKAGE_INCREMENTALLY_PARSING_ERROR                      \
            -SYNTIANT_PACKAGE_ERROR_NN_PH_PARAMETERS_V6                        \
            +1                                                                 \
          ] : "*unknown*"                                                      \
    )

extern char *syntiant_package_error_names[];

extern const char* syntiant_package_error_name(int e);


/**
 * @brief Package parsing mode when reading a chunk of package
 *
 */
enum syntiant_pkg_mode_e {
    PACKAGE_MODE_DONE = 0x0,
    PACKAGE_MODE_TAG_START = 0x1,
    PACKAGE_MODE_TAG_CONT = 0x2,
    PACKAGE_MODE_LENGTH_START = 0x3,
    PACKAGE_MODE_LENGTH_CONT = 0x4,
    PACKAGE_MODE_VALUE_START = 0x5,
    PACKAGE_MODE_VALUE_CONT = 0x6,
    PACKAGE_MODE_VALID_PARTIAL_VALUE = 0x7,
    PACKAGE_MODE_NO_PARTIAL_VALUE = 0x8
};

/**
 * @brief Posterior action enum
 *
 */
enum syntiant_packager_ph_action_e {
    SYNTIANT_PH_ACTION_MATCH = 0,
    SYNTIANT_PH_ACTION_STATE = 1,
    SYNTIANT_PH_ACTION_STAY = 2
};


/**
 * @brief DNN inputs
 *
 */
enum syntiant_package_dnn_input_e {
    SYNTIANT_PACKAGE_DNN_INPUT_NONE = 0, 
    SYNTIANT_PACKAGE_DNN_INPUT_PDM0 = 1, 
    SYNTIANT_PACKAGE_DNN_INPUT_PDM1 = 2, 
    SYNTIANT_PACKAGE_DNN_INPUT_PDM_SUM = 3, 
    SYNTIANT_PACKAGE_DNN_INPUT_I2S_LEFT = 4, 
    SYNTIANT_PACKAGE_DNN_INPUT_I2S_RIGHT = 5, 
    SYNTIANT_PACKAGE_DNN_INPUT_I2S_SUM = 6, 
    SYNTIANT_PACKAGE_DNN_INPUT_I2S_MONO = 7, 
    SYNTIANT_PACKAGE_DNN_INPUT_I2S_DIRECT = 8, 
    SYNTIANT_PACKAGE_DNN_INPUT_SPI = 9, 
    SYNTIANT_PACKAGE_DNN_INPUT_SPI_DIRECT = 10
};

/**
 * @brief Posterior handler collection parser states
 *
 */
enum syntiant_phc_parser_state_e {
    PHC_PARSE_COLLECTION_PARAMS = 1,
    PHC_PARSE_PH_PARAMS = 2,
    PHC_PARSE_STATE_PARAMS = 3,
    PHC_PARSE_CLASS_PARAMS = 4
};

/**
 * @brief Neural network metadata parser states
 *
 */
enum syntiant_neural_network_metadata_parser_state_e {
    NNM_PARSE_NN_NUM = 1,
    NNM_PARSE_BASE_META = 2,
    NNM_PARSE_INP_SIZE = 3,
    NNM_PARSE_COORD = 4,
    NNM_PARSE_CACHE_INST = 5
};

#define NN_FLOW_RULES_TOTAL (16 + 32 + 32)
#define NN_FLOW_METADATA_SIZE (DSP_FLOW_COLLECTION_V1_SIZE * NN_FLOW_RULES_TOTAL)

#ifdef __cplusplus
}
#endif

#endif
