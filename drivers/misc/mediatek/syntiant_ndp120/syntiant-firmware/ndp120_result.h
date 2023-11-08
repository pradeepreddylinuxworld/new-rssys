/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2017-2020 Syntiant Corporation
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains the property of
 * Syntiant Corporation and its suppliers, if any.  The intellectual and
 * technical concepts contained herein are proprietary to Syntiant Corporation
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination
 * of this information or reproduction of this material is strictly forbidden
 * unless prior written permission is obtained from Syntiant Corporation.
 *
 */

#ifndef NDP120_RESULTS_H
#define NDP120_RESULTS_H

#ifdef __cplusplus
extern "C" {
#endif

struct ndp120_main_state_s;

enum ndp_results_constants_e {
    NDP120_RESULT_NUM_CLASSES = 32,
    NDP120_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE = 12
};

/**
 * @brief Stores all the results collected from hardware and firmware
 *
 */
struct ndp120_result_s {
    uint32_t num_classes;
    uint32_t max_index;
    uint32_t softmax_strengths[NDP120_RESULT_NUM_CLASSES];
    uint32_t winner_one_hot;
    uint32_t summary;
    uint32_t tankptr;
    union {
        uint8_t relu [NDP120_RESULT_NUM_CLASSES];
        int8_t linear [NDP120_RESULT_NUM_CLASSES];
        int16_t linear16[NDP120_RESULT_NUM_CLASSES];
    } raw_strengths;
};

/**
 * @brief Cache ISA based params needed to read results of NN
 *
 */
struct ndp120_nn_output_cache_s {
    /* output activation type */
    uint16_t activation_type;
    /* Number of values present in the output layer */
    uint16_t num_entries;
    /* data bank coordinate of the activations */
    uint32_t output_coord;
};

/**
 * @brief intializes the result object
 *
 * @param result Result Object
 */
void ndp120_result_init(struct ndp120_result_s *result);

/**
 * @brief Computes the softmax on activations read from hardware.
 *
 * @param result Result Object
 */
void ndp120_result_compute_softmax(struct ndp120_result_s *result);

/**
 * @brief reads the activations from hardware registers
 *
 * @param result Result object
 */
void ndp120_result_read_activations(struct ndp120_result_s *result);

/**
 * @brief Represents the Softmax Smoother
 *
 */
struct ndp120_result_softmax_smoother_s {
    uint32_t num_classes;
    uint32_t queue_sizes[NDP120_RESULT_NUM_CLASSES];
    uint32_t queue_curr_ptr[NDP120_RESULT_NUM_CLASSES];
    uint32_t queues[NDP120_RESULT_NUM_CLASSES]
                   [NDP120_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE];
};

/**
 * @brief intializes the softmax smoother
 *
 * @param smoother Smoother object
 */
void ndp120_result_softmax_smoother_init(
    struct ndp120_result_softmax_smoother_s *smoother);

/**
 * @brief Set the params for softmax smoother
 *
 * @param smoother Smoother object
 * @param num_classes Num of classes for which smoothing needs to be done
 * @param queue_sizes Queue sizes for each class.
 */
void ndp120_result_softmax_smoother_set(
    struct ndp120_result_softmax_smoother_s *smoother, uint32_t num_classes,
    uint32_t *queue_sizes);

/**
 * @brief smoothes the softmax probabilties.
 *
 * @param smoother Smoother object
 * @param result result object
 */
void ndp_result_softmax_smoother_do_smoothing(
    struct ndp120_result_softmax_smoother_s *smoother,
    struct ndp120_result_s *result);

/**
 * @brief  Maintain a cache of NN ISA params that are needed to extract
 * and process activation of a NN properly.
 *
 * @param fw_main_state FW main state object
 */
void
nn_output_cache_init(struct ndp120_main_state_s *fw_main_state);

#ifdef __cplusplus
}
#endif
#endif
