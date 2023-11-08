/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2017-2018 Syntiant Corporation
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

#ifndef NDP120_PH_H
#define NDP120_PH_H

#include "syntiant-firmware/ndp120_result.h"
#include "syntiant-firmware/ndp120_firmware.h"

#ifdef CORTEX_M0
typedef uint32_t pointer_t;
#else
typedef void *pointer_t;
#endif

enum ndp120_ph_action_e {
    NDP120_PH_ACTION_STATE_M = 0x7f,
    NDP120_PH_ACTION_STAY = NDP120_PH_ACTION_STATE_M,
    NDP120_PH_ACTION_MATCH_M = 0x80
};

enum ndp120_ph_action_v2_e {
    NDP120_PH_ACTION_V2_MATCH = 0,
    NDP120_PH_ACTION_V2_STATE = 1,
    NDP120_PH_ACTION_V2_STAY = 2
};

/**
 * @brief parameters for each class
 *
 */
struct ndp120_ph_class_params_s {
    uint32_t window;
    uint32_t threshold;
    uint32_t backoff;
    uint32_t action;
    uint32_t smoothing_queue_size;
};
#define PH_PARAM_DWORD_SIZE     (sizeof(struct ndp120_ph_class_params_s)/sizeof(uint32_t))

/**
 * @brief parameters for each state
 *
 */
struct ndp120_ph_state_params_s {
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t class_params_offset;
};

/**
 * @brief All parameters used by posterior handler
 *
 */
struct ndp120_ph_params_s {
    uint32_t num_classes;
    uint32_t num_states;
    /* the frame processing function to be used */
    uint32_t ph_type;
    uint32_t params_memory[NDP120_RESULT_NUM_CLASSES * PH_PARAM_DWORD_SIZE];
};

struct ndp120_ph_params_collection_s {
    struct ndp120_ph_params_s sph_params[MAX_NNETWORKS];
    uint32_t num_params;
};

/**
 * @brief Posterior Handler state
 *
 */
struct ndp120_ph_state_s {
    uint32_t current_class;
    uint32_t class_counts[NDP120_RESULT_NUM_CLASSES];
    uint32_t current_state;
    uint32_t backoff_counter;
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t frame_counter;
    uint32_t winner;
    uint32_t winner_prob;
    uint32_t threshold;
    uint32_t match;
    uint32_t window;
    uint32_t tankptr;

    uint32_t class_tankptr;
    pointer_t params_addr;
};

/**
 * @brief intializes the posterior handler params. It sets the memory to zero.
 *
 * @param params
 */
void ndp_ph_params_init(struct ndp120_ph_params_s *params);

/**
 * @brief initializes the Softmax smoother from posterior handler params.
 *
 * @param ph_params Posterior Handler params
 * @param smoother Smoother object
 */
void ndp120_ph_init_result_smoother(struct ndp120_ph_params_s *ph_params,
    struct ndp120_result_softmax_smoother_s *smoother);

/**
 * @brief intializes the posterior handler. It sets the memory to zero.
 *
 * @param ph_state Posterior handler state
 */
void ndp_ph_init(struct ndp120_ph_state_s *ph_state);

/**
 * @brief process a new frame result and picks a winner
 *
 * @param ph_state Posterior handler state
 */
void ndp_ph_process_frame(struct ndp120_ph_state_s *ph_state, uint8_t,
                             struct ndp120_result_s *result, uint32_t tankptr);

/**
 * @brief uses a single counter to keep track of the winning classes. This is
 *  the default frame processor.
 *
 * @param ph_state Posterior handler state
 */
void ndp120_single_counter_frame_processor(struct ndp120_ph_state_s *ph_state,
                                           uint8_t nn,
                                           struct ndp120_result_s *result, uint32_t tankptr);

/**
 * @brief uses a per class counter to keep track of the winning classes. We do a state
 *  transition when a winning class has been detected (i.e., it has meet the per class
 *  threshold for the per class defined number of frames or window size). In this design,
 *  we stay in the current state for time_out (defined per state) number of frames and
 *  then trainsition to the state defined by the timeout_action. The current implementation
 *  does NOT use backoff and leverages states to avoid redundant predictions. Further, we
 *  assume the per class variables, threshold and window size, are the same across all the
 *  states. This is necessary to have a valid count when we move from one state to another 
 *  (note that the counters persist through state transitions). Lastly, window size can be
 *  used to enforce which class is currently active (i.e., pick a large window size for the
 *  classes that are not active in the current state).
 *
 * @param ph_state Posterior handler state
 * @param result The result structure
 * @param tankptr The audio tank pointer
 */
void ndp120_multi_counter_frame_processor(struct ndp120_ph_state_s *ph_state, uint8_t,
                                          struct ndp120_result_s *result, uint32_t tankptr);

void ndp_ph_init_result_smoother(void);


struct ndp120_ph_state_params_s *
ndp120_ph_state_params_get(
    struct ndp120_ph_params_s *ph_params, uint32_t state_num);

void
ndp_result_compute_softmax
(struct ndp120_nn_output_cache_s *nn_output_cache,
 struct ndp120_result_s *result);

typedef struct syntiant_ndp120_ph_params_t_{
    uint32_t phwin;
    uint32_t phth;
    uint32_t phbackoff;
    uint32_t phaction;
    uint32_t pharg;
    uint32_t phqueuesize;
} syntiant_ndp120_ph_params_t;


typedef struct syntiant_ph_params_state_t_{
    uint32_t num_classes;
    uint32_t timeout;
    syntiant_ndp120_ph_params_t ph_class[1];

}syntiant_ph_params_state_t;

typedef struct syntiant_ndp120_ph_params_metadeta_t_{
    uint32_t num_states;
    syntiant_ph_params_state_t state[1];
} syntiant_ndp120_ph_params_metadeta_t;

void ndp_ph_tag_ph_params_v4_tlv(void *config, int length);


struct ndp120_ph_class_params_s *
ndp_ph_class_params_get(struct ndp120_ph_params_s *ph_params,
    uint32_t state_num, uint32_t class_num);

int
ndp_valid_nn_done_message(void);

#endif
