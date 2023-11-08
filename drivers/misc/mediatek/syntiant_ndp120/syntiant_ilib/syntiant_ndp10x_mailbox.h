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
/*
 * Syntiant NDP10x SPI mailbox protocol definitions
 */
#ifndef SYNTIANT_NDP10x_MAILBOX_H
#define SYNTIANT_NDP10x_MAILBOX_H

#ifdef __cplusplus
extern "C" {
#endif

enum syntiant_ndp10x_mb_protcol_e {
    SYNTIANT_NDP10X_MB_HOST_TO_MCU_OWNER = 0x08,
    SYNTIANT_NDP10X_MB_HOST_TO_MCU_M = 0x07,
    SYNTIANT_NDP10X_MB_HOST_TO_MCU_S = 0,
    SYNTIANT_NDP10X_MB_MCU_TO_HOST_OWNER = 0x80,
    SYNTIANT_NDP10X_MB_MCU_TO_HOST_M = 0x70,
    SYNTIANT_NDP10X_MB_MCU_TO_HOST_S = 4
};

#define SYNTIANT_NDP10X_MB_HOST_TO_MCU_INSERT(m, r)                            \
    ((((m) & ~SYNTIANT_NDP10X_MB_HOST_TO_MCU_M)                                \
         ^ SYNTIANT_NDP10X_MB_HOST_TO_MCU_OWNER)                               \
        | ((r) << SYNTIANT_NDP10X_MB_HOST_TO_MCU_S))

#define SYNTIANT_NDP10X_MB_MCU_TO_HOST_INSERT(m, r)                            \
    ((((m) & ~SYNTIANT_NDP10X_MB_MCU_TO_HOST_M)                                \
         ^ SYNTIANT_NDP10X_MB_MCU_TO_HOST_OWNER)                               \
        | ((r) << SYNTIANT_NDP10X_MB_MCU_TO_HOST_S))

/* short and long request codes are numbered contiguously (for now) */
enum syntiant_ndp10x_mb_request_e {
    SYNTIANT_NDP10X_MB_REQUEST_NOP = 0x0,
    SYNTIANT_NDP10X_MB_REQUEST_CONT = 0x1,
    SYNTIANT_NDP10X_MB_REQUEST_MATCH = 0x2,
    SYNTIANT_NDP10X_MB_REQUEST_SER = 0x2,
    SYNTIANT_NDP10X_MB_REQUEST_EXTOP = 0x3,
    SYNTIANT_NDP10X_MB_REQUEST_DATA = 0x4,
    SYNTIANT_NDP10X_MB_REQUEST_MIADDR = 0x8
};
#define SYNTIANT_NDP10X_MB_M2H_REQUEST_DECODER                                 \
    {                                                                          \
        "nop", "cont", "match", "extop", "data(0)", "data(1)", "data(2)",      \
            "data(3)"                                                          \
    }
#define SYNTIANT_NDP10X_MB_H2M_REQUEST_DECODER                                 \
    {                                                                          \
        "nop", "cont", "ser", "extop", "data(0)", "data(1)", "data(2)",        \
            "data(3)", "miaddr"                                                \
    }

/* response codes (3-bit) */
enum syntiant_ndp10x_mb_response_e {
    SYNTIANT_NDP10X_MB_RESPONSE_SUCCESS = 0x0,
    SYNTIANT_NDP10X_MB_RESPONSE_CONT = SYNTIANT_NDP10X_MB_REQUEST_CONT,
    SYNTIANT_NDP10X_MB_RESPONSE_ERROR = 0x2,
    SYNTIANT_NDP10X_MB_RESPONSE_DATA = SYNTIANT_NDP10X_MB_REQUEST_DATA
};
#define SYNTIANT_NDP10X_MB_RESPONSE_DECODER                                    \
    {                                                                          \
        "success", "cont", "error", "ILLEGAL(3)", "data(0)", "data(1)",        \
            "data(2)", "data(3)"                                               \
    }

/* error codes (6-bit) */
enum syntiant_ndp10x_mb_error_e {
    SYNTIANT_NDP10X_MB_ERROR_FAIL = 0x0,
    SYNTIANT_NDP10X_MB_ERROR_UNEXPECTED = 0x1
};
#define SYNTIANT_NDP10X_MB_ERROR_DECODER                                       \
    {                                                                          \
        "fail", "unexpected"                                                   \
    }

#ifdef __cplusplus
}
#endif

#endif
