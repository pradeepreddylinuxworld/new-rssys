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

/*
 * NOTE:
 * 1. this header file is used in the syntiant-ilib repo as an interface file
 * 2. Some compilers do not pack the structs which means they are always 32 bit
 * aligned so they are explicitly made 32 bit aligned so please follow that.
 */

#ifndef NDP120_FIRMWARE_H
#define NDP120_FIRMWARE_H

#include "syntiant-firmware/ndp120_result.h"
#include "syntiant-firmware/ndp120_mb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FW_VER "##FIRMWARE_VERSION##"
#define FW_VER_SIZE 24

#define NDP120_ILIB_SCRATCH_ORIGIN 0x20002000
#define NDP120_ILIB_SCRATCH_LENGTH 0x00c00


enum {
    NDP120_FW_STATE_ADDRESS_INDEX_FW_STATE              = 0x0,
    NDP120_FW_STATE_ADDRESS_INDEX_POSTERIOR_STATE       = 0x1,
    NDP120_FW_STATE_ADDRESS_INDEX_SMAX_SMOOTHER         = 0x2,
    NDP120_FW_STATE_ADDRESS_INDEX_POSTERIOR_PARAMS      = 0x3,
    NDP120_FW_STATE_ADDRESS_INDEX_ORCHESTRATOR_PARAMS   = 0x4,
    NDP120_FW_STATE_ADDRESS_INDEX_DBG_STATE             = 0x5,
    NDP120_FW_STATE_ADDRESS_INDEX_MAX_CNT               = 0x8
};

struct ndp120_fw_state_pointers_s {
    uint32_t addresses[NDP120_FW_STATE_ADDRESS_INDEX_MAX_CNT];
};

enum ndp120_firmware_constants_e {
    NDP120_MATCH_RING_SIZE = 2, /* Match ring size */
    MAX_NNETWORKS = 6           /* Maximum number of networks */
};


struct ndp120_fw_match_s {
    uint32_t summary;
    uint32_t tankptr;
};

/**
 * @brief Data Structure for storing firmware state.
 * Note: Please do not change this data struture because this struture is used
 * by ilib running on host for operational or debugging purposes.
 * Please add your data after this data structure and set the address for your
 * data in fw_state_pointers.
 *
 */
struct ndp120_fw_state_s {
    /*
     * These members must be in fixed locations and never change.
     * The uILib relies on these members and does not include
     * the firmware header files.
     */
    uint32_t tankptr;     /* tank pointer with all 17 bits */
    uint32_t match_producer[MAX_NNETWORKS];
    struct ndp120_fw_match_s match_ring[MAX_NNETWORKS][NDP120_MATCH_RING_SIZE];

    /*
     * the remaining members should be kept in a stable location but
     * will be accessed through these header files by the full ILib
     * so there is less pain of death to moving them
     */
    uint32_t debug;                   /* debug scratch area */
    uint32_t enable;
    uint32_t prev_enable;
    uint32_t reset;

    uint32_t tank_size;
    uint32_t tank_full;
    uint32_t result_fifo_full;

    /* interrupt counters*/
    uint32_t mb_int_count;
    uint32_t freq_int_count;
    uint32_t dnn_int_count;
    uint32_t unknown_int_count;

    /* firmware version */
    char version[FW_VER_SIZE];

    /* mailbox state */
    struct ndp120_mb_state_s mb_state;

    /* params needed for reading results */
    struct ndp120_nn_output_cache_s nn_output_cache[MAX_NNETWORKS];
    /* frame results after posterior computations */
    struct ndp120_result_s result[MAX_NNETWORKS];
};

/* max 256 bytes */
struct ndp120_debug_cnt_s {
   uint32_t signature;          /* identifier for this struct */
   uint32_t frame_cnt;          /* Frame count */
   uint32_t dsp2mcu_intr_cnt;   /* DSP 2 MCU int cnt */
   uint32_t dsp2mcu_nn_done_cnt;/* DSP 2 MCU done cnt */
   uint32_t mcu2host_match_cnt; /* MCU 2 Host match cnt */
   uint32_t mcu2host_mpf_cnt;   /* MCU 2 Host match per frame cnt */
   uint32_t mcu2dsp_done_cnt;   /* MCU 2 DSP done cnt */
   uint32_t matches;            /* Matches detected in posterior handler */
   uint32_t dsp2mcu_queue_cnt;  /* DSP 2 MCU queue cnt */
   uint32_t nn_orch_flwchg_cnt; /* Flow map change cnt in NN orchestrator */
   uint32_t unknown_activation_cnt;/* Unknown activation count */
   uint32_t unknown_int_count;  /* Unknown interrupt count */
   uint32_t inv_nn_orch_node_cnt;/* Invalid NN orchestrator node */
   uint32_t mbin_int_cnt;       /* MBIN int count */
   uint32_t mbout_int_cnt;      /* MBOUT int count */
   uint32_t buffer_bgn;         /* Debug buffer begin */
   uint32_t buffer_end;         /* Debug buffer end */
   uint32_t curr_ptr;           /* Debug buffer current pointer */
   uint32_t inv_num_class_cnt;  /* Invalid number of classes */
   uint32_t num_frames;         /* maximum frames for logging */
   uint32_t dbg1;
   uint32_t dbg2;
   uint32_t enable;             /* Debug logging enable flag */
};

typedef enum ndp120_debug_tag_t_ {
   NDP120_MCU_MATCH_TAG = 0xEA,
   NDP120_MCU_PH_TAG = 0xEB,
   NDP120_MCU_ACTIVATION_TAG = 0xEF,
   NDP120_MCU_ORCH_TAG = 0xEC,
   NDP120_MCU_SMOOTHER_TAG = 0xEE,
   NDP120_MCU_SOFTMAX_TAG = 0xE9,
   NDP120_MCU_FRAME_TAG = 0xE8
} ndp120_debug_tag_t;

enum {
    NDP120_DEBUG_STRUCT_SIGNATURE = 0x7EADDEB6U,
    NDP120_DEBUG_TLV_LENGTH = 4,
    NDP120_DEBUG_ENABLE_PH = (1 << 1),
    NDP120_DEBUG_ENABLE_ACTIVATION = (1 << 2),
    NDP120_DEBUG_ENABLE_ORCH = (1 << 3),
    NDP120_DEBUG_ENABLE_SOFTMAX = (1 << 4),
    NDP120_DEBUG_ENABLE_SMOOTHER = (1 << 5),
    NDP120_DEBUG_ENABLE_VERBOSE = (NDP120_DEBUG_ENABLE_PH |
                                NDP120_DEBUG_ENABLE_ACTIVATION |
                                NDP120_DEBUG_ENABLE_ORCH |
                                NDP120_DEBUG_ENABLE_SOFTMAX |
                                NDP120_DEBUG_ENABLE_SMOOTHER
                                )
};

/**
 * @brief enum values to enable/disable various components of firmware. This
 * constants will also be used by ilib code to enable/disable the components.
 *
 * Note: please release header files to ilib if you change this constants.
 */
enum ndp120_fw_state_address_enable_e {
    NDP120_FW_STATE_ENABLE_POSTERIOR = 0x1,
    NDP120_FW_STATE_ENABLE_SMAX_SMOOTHER = 0x2
};

/**
 * @brief enum values to reset various components of firmware. This
 * constants will also be used by ilib code to reset the components.
 *
 * Note: please release header files to ilib if you change this constants.
 */
enum ndp120_fw_state_address_reset_e {
    NDP120_FW_STATE_RESET_POSTERIOR_STATE = 0x1,
    NDP120_FW_STATE_RESET_SMAX_SMOOTHER = 0x2,
    NDP120_FW_STATE_RESET_POSTERIOR_PARAMS = 0x4
};

#ifdef __cplusplus
}
#endif
#endif /* NDP120_FIRMWARE_H */
