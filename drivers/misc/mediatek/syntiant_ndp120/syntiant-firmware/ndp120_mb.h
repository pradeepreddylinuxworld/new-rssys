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

#ifndef NDP120_MB_H
#define NDP120_MB_H

#include "syntiant-firmware/ndp120_result.h"

/**
 * @brief defines the states of MCU to HOST mailbox exchange
 *
 */
enum m2h_state_e { M2H_STATE_IDLE = 0, M2H_STATE_MATCH = 1 };

/**
 * @brief defines the states of HOST to MCU mailbox exchange
 *
 */
enum h2m_state_e {
    H2M_STATE_IDLE = 0,
    H2M_STATE_EXTOP = 1,
    H2M_STATE_DATA_OUT = 2,
    H2M_STATE_LOAD = 3
};

/**
 * @brief define constants to extract information from mailbox.
 *
 */
enum ndp_mb_protcol_e {
    NDP_MB_HOST_TO_MCU_OWNER = 0x80,
    NDP_MB_HOST_TO_MCU_M = 0x7F,
    NDP_MB_HOST_TO_MCU_S = 0,
    NDP_MB_MCU_TO_HOST_OWNER = 0x80,
    NDP_MB_MCU_TO_HOST_M = 0x7F,
    NDP_MB_MCU_TO_HOST_S = 0
};


#define NDP_MB_HOST_TO_MCU_INSERT(m, r) \
    ((((m) & ~NDP_MB_HOST_TO_MCU_M) \
      ^ NDP_MB_HOST_TO_MCU_OWNER) \
     | ((r) << NDP_MB_HOST_TO_MCU_S))

#define NDP_MB_MCU_TO_HOST_INSERT(m, r) \
    ((((m) & ~NDP_MB_MCU_TO_HOST_M) \
      ^ NDP_MB_MCU_TO_HOST_OWNER) \
     | ((r) << NDP_MB_MCU_TO_HOST_S))


/* Host -> MCU */
enum ndp_mbin_req_e {
    NDP_MBIN_REQUEST_NOP      = 0x0,
    NDP_MBIN_REQUEST_CONT     = 0x1,
    NDP_MBIN_REQUEST_PREPARE  = 0x2,
    NDP_MBIN_REQUEST_DATA     = 0x4,
    NDP_MBIN_REQUEST_MIADDR   = 0x8,
    NDP_MBIN_REQUEST_LOAD     = 0x9,

    NDP_MBIN_REQUEST_IGNORE_START = 0xa,
    NDP_MBIN_REQUEST_IGNORE_END   = 0xf,

    NDP_MBIN_H2DSP_REQUEST    = (1<<6)
};


/* mbin_respone */
/* response from MCU -> Host */
enum ndp_mb_response_e {
    NDP_MBIN_RESPONSE_SUCCESS   = 0x0,
    NDP_MBIN_RESPONSE_CONT      = 0x1,
    NDP_MBIN_RESPONSE_ERROR     = (1<<5),
    NDP_MBIN_RESPONSE_H2DSP_RESP = (1<<6)
};


/* mbout */
/* request from MCU -> Host */
enum ndp_mbout_req_e {
    NDP_MBOUT_REQ_SUCCESS   = 0x0,
    NDP_MBOUT_REQ_CONT      = 0x1,
    NDP_MBOUT_REQ_RUNNING   = 0x10,
    NDP_MBOUT_REQ_BOOTING   = 0x11,
    NDP_MBOUT_REQ_LOAD_DONE = 0x12,
    NDP_MBOUT_REQ_RUNNING_TEST = 0x13,
    NDP_MBOUT_REQ_ERROR     = (1<<5),
    NDP_MBOUT_REQ_MATCH_NETWORK_MASK = 0x40
};

#define NDP120_MB_REQUEST_DECODER                                              \
    {                                                                          \
        "nop", "cont", "match", "extop", "data(0)", "data(1)", "data(2)",      \
            "data(3)", "miaddr"                                                \
    }


enum ndp_mbout_response_e {
    NDP_MBOUT_RESPONSE_DSP2H_RESP = (1<<6)
};

/* error codes (6-bit) */
enum ndp_mb_error_e {
    NDP_MB_ERROR_NONE = 0x0,
    NDP_MB_ERROR_UNEXPECTED = 0x1,
    NDP_MB_ERROR_PACKAGE_MAGIC_TLV = 0x2,
    NDP_MB_ERROR_PACKAGE_FW_SIZE = 0x3,
    NDP_MB_ERROR_PACKAGE_INTEGRITY = 0x4,
    NDP_MB_ERROR_PACKAGE_MISSING_FW = 0x5,
    NDP_MB_ERROR_PACKAGE_FORMAT = 0x6,
    NDP_MB_ERROR_AUTH = 0x7,
    NDP_MB_ERROR_AES = 0x8,
    NDP_MB_ERROR_I2C = 0xa,
    NDP_MB_ERROR_HW_SERIAL_FLASH = 0x10,
    NDP_MB_ERROR_HW_MCU_EXCEPTION = 0x11
};

#define NDP_MB_ERROR_INSERT(e) \
    ((NDP_MBIN_RESPONSE_ERROR) | e)



#define NDP120_SPI_MATCH_MATCH_MASK 0x40
enum ndp_mb_match_offsets_e {
    NDP120_MB_MATCH_SUMMARY_O = 0x00,
    NDP120_MB_MATCH_BINARY_O = 0x04,
    NDP120_MB_MATCH_STRENGTH_O = 0x0c
};

/**
 * @brief Mailbox state object
 *
 */

struct ndp120_mb_state_s {
    /* start ordering match with ndp10x*/
    uint32_t enable_match_for_every_frame;

    uint32_t m2h_state;
    uint32_t m2h_req;
    uint32_t m2h_rsp_success;
    uint32_t m2h_rsp_unknown;
    uint32_t m2h_match_skipped;

    uint32_t h2m_state;
    uint32_t h2m_req_nop;
    uint32_t h2m_req_extop;
    uint32_t h2m_req_data;
    uint32_t h2m_req_cont;

    uint32_t h2m_unexpected_nop;
    uint32_t h2m_unexpected_extop;
    uint32_t h2m_unexpected_cont;
    uint32_t h2m_unexpected_data;
    uint32_t h2m_req_unknown;
    uint32_t h2m_extop_unknown;

    uint32_t h2m_data;
    uint32_t h2m_data_count;
    uint32_t previous_mbox;
    /* end match with ndp10x*/

    uint32_t h2m_req_load;
    uint32_t h2m_unexpected_load;

    uint32_t h2m_req_prepare;
    uint32_t h2m_unexpected_prepare;

    uint32_t previous_mbox_out;
    uint32_t previous_mbox_in;
};

/**
 * @brief Send host to MCU request response. 
 * enum ndp_mb_response_e 
 *
 * @param bstate : firmware mailbox state object
 * @param response : response defined in ndp_mb_response_e
 */
void
ndp_send_h2m_mbin_response(
    struct ndp120_mb_state_s* bstate, uint8_t response);

/**
 * @brief Sends a M2H request to host
 *
 * @param bstate : firmware mailbox state object
 * @param request Request to be sent to Host
 */
void
ndp_send_m2h_mbout_request(
    struct ndp120_mb_state_s* bstate, uint8_t request);

/**
 * @brief Send a MATCH request to host based on result
 *
 * @param mb_state   firmware mailbox state object
 * @param result result calculated by posterior handler.
 * @param network_id neural network id
 * @param network_output output data from neural network graph
 */
void
ndp_mb_send_match(
        struct ndp120_mb_state_s *mb_state,
        struct ndp120_result_s *result,
        uint8_t network_id,
        uint8_t network_output);

#endif
