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

#ifndef NDP120_DSP_MAILBOX_H_
#define NDP120_DSP_MAILBOX_H_

typedef uint32_t ndp120_dsp_mailbox_msg_t;

#define NDP120_DSP_MB_PAYLOAD_SHIFT 0
#define NDP120_DSP_MB_PAYLOAD_MASK  0xFFFF
#define NDP120_DSP_MB_MESSAGE_SHIFT 0x10
#define NDP120_DSP_MB_MESSAGE_MASK  0xFF
#define NDP120_DSP_MB_SEQ_SHIFT     0x18
#define NDP120_DSP_MB_SEQ_MASK      0x3F
#define NDP120_DSP_MB_TYPE_SHIFT    0x1E
#define NDP120_DSP_MB_TYPE_MASK     0x1
#define NDP120_DSP_MB_TOGGLE_SHIFT  0x1F
#define NDP120_DSP_MB_TOGGLE_MASK   0x1

#define NDP120_DSP_MB_GET_PAYLOAD(x)        ((x >> NDP120_DSP_MB_PAYLOAD_SHIFT) & NDP120_DSP_MB_PAYLOAD_MASK) 
#define NDP120_DSP_MB_SET_PAYLOAD(x, y)     ((x & ~(NDP120_DSP_MB_PAYLOAD_MASK << NDP120_DSP_MB_PAYLOAD_SHIFT)) | (((y) & NDP120_DSP_MB_PAYLOAD_MASK) << NDP120_DSP_MB_PAYLOAD_SHIFT))

#define NDP120_DSP_MB_GET_MESSAGE(x)        ((x >> NDP120_DSP_MB_MESSAGE_SHIFT) & NDP120_DSP_MB_MESSAGE_MASK) 
#define NDP120_DSP_MB_SET_MESSAGE(x, y)     ((x & ~(NDP120_DSP_MB_MESSAGE_MASK << NDP120_DSP_MB_MESSAGE_SHIFT)) | (((y) & NDP120_DSP_MB_MESSAGE_MASK) << NDP120_DSP_MB_MESSAGE_SHIFT))

#define NDP120_DSP_MB_GET_SEQ(x)            ((x >> NDP120_DSP_MB_SEQ_SHIFT) & NDP120_DSP_MB_SEQ_MASK) 
#define NDP120_DSP_MB_SET_SEQ(x, y)         ((x & ~(NDP120_DSP_MB_SEQ_MASK << NDP120_DSP_MB_SEQ_SHIFT)) | (((y) & NDP120_DSP_MB_SEQ_MASK) << NDP120_DSP_MB_SEQ_SHIFT))

#define NDP120_DSP_MB_GET_TYPE(x)           ((x >> NDP120_DSP_MB_TYPE_SHIFT) & NDP120_DSP_MB_TYPE_MASK) 
#define NDP120_DSP_MB_SET_TYPE(x, y)        ((x & ~(NDP120_DSP_MB_TYPE_MASK << NDP120_DSP_MB_TYPE_SHIFT)) | (((y) & NDP120_DSP_MB_TYPE_MASK) << NDP120_DSP_MB_TYPE_SHIFT))

#define NDP120_DSP_MB_GET_TOGGLE(x)         ((x >> NDP120_DSP_MB_TOGGLE_SHIFT) & NDP120_DSP_MB_TOGGLE_MASK) 
#define NDP120_DSP_MB_SET_TOGGLE(x, y)      ((x & ~(NDP120_DSP_MB_TOGGLE_MASK << NDP120_DSP_MB_TOGGLE_SHIFT)) | (((y) & NDP120_DSP_MB_TOGGLE_MASK) << NDP120_DSP_MB_TOGGLE_SHIFT))

/* MCU -> DSP REQ */
enum {
    /* req */
    NDP120_DSP_MB_M2D_PING                  = 0x00,
    NDP120_DSP_MB_M2D_ADX_UPPER             = 0x04,
    NDP120_DSP_MB_M2D_ADX_LOWER             = 0x05,

    /* resp */
    NDP120_DSP_MB_D2M_PONG                  = NDP120_DSP_MB_M2D_PING,
    NDP120_DSP_MB_D2M_ADX_UPPER             = NDP120_DSP_MB_M2D_ADX_UPPER,
    NDP120_DSP_MB_D2M_ADX_LOWER             = NDP120_DSP_MB_M2D_ADX_LOWER
};

/* DSP -> MCU REQ */
enum {
    /* req */
    NDP120_DSP_MB_D2M_PING                  = 0x00,
    NDP120_DSP_MB_D2M_NN_DONE               = 0x01,

    /* resp */
    NDP120_DSP_MB_M2D_PONG                  = 0x00,
    NDP120_DSP_MB_M2D_NN_DONE               = NDP120_DSP_MB_D2M_NN_DONE 
};

/* HOST -> DSP REQ */
enum {
    /* req */
    NDP120_DSP_MB_H2D_PING                  = 0x0A, 
    NDP120_DSP_MB_H2D_ADX_UPPER             = 0x0B,
    NDP120_DSP_MB_H2D_ADX_LOWER             = 0x0C,
    NDP120_DSP_MB_H2D_TEST                  = 0x0D,

    /* resp */
    NDP120_DSP_MB_D2H_PONG                  = NDP120_DSP_MB_H2D_PING, 
    NDP120_DSP_MB_D2H_ADX_UPPER             = NDP120_DSP_MB_H2D_ADX_UPPER,
    NDP120_DSP_MB_D2H_ADX_LOWER             = NDP120_DSP_MB_H2D_ADX_LOWER,
    NDP120_DSP_MB_D2H_TEST                  = NDP120_DSP_MB_H2D_TEST 
};

/* DSP -> HOST REQ */
enum {
    /* req */
    NDP120_DSP_MB_D2H_PING                  = 0x0A, 
    NDP120_DSP_MB_D2H_EXTRACT_READY         = 0x0E,
    NDP120_DSP_MB_D2H_WATERMARK             = 0x0F,
    NDP120_DSP_MB_D2H_RUNNING               = 0x12,
    NDP120_DSP_MB_D2H_DEBUG                 = 0x13,

    /* resp */
    NDP120_DSP_MB_H2D_PONG                  = NDP120_DSP_MB_D2H_PING, 
    NDP120_DSP_MB_H2D_WATERMARK             = NDP120_DSP_MB_D2H_WATERMARK,
    NDP120_DSP_MB_H2D_EXTRACT_READY         = NDP120_DSP_MB_D2H_EXTRACT_READY,
    NDP120_DSP_MB_H2D_RUNNING               = NDP120_DSP_MB_D2H_RUNNING,
    NDP120_DSP_MB_H2D_DEBUG                 = NDP120_DSP_MB_D2H_DEBUG,                 
    NDP120_DSP_MB_H2D_ALGO_ERROR_INIT       = 0x80,
    NDP120_DSP_MB_H2D_ALGO_ERROR_PROCESS    = 0x81
};


enum {
    MB_REQ  = 0,
    MB_RESP = 1
};

#endif
