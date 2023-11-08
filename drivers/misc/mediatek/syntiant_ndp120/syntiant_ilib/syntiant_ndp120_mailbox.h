/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
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
 * Syntiant NDP120 SPI mailbox protocol definitions
 */
#ifndef SYNTIANT_NDP120_MAILBOX_H
#define SYNTIANT_NDP120_MAILBOX_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant-dsp-firmware/ndp120_dsp_mailbox.h>
#include <syntiant-firmware/ndp120_mb.h>

void syntiant_ndp120_mcu_mb_op_decoder(uint8_t op, char * buf, uint32_t len);
void syntiant_ndp120_mcu_mb_error_decoder(uint8_t error, char* buf, uint32_t len);
void syntiant_ndp120_dsp_mb_op_decoder(ndp120_dsp_mailbox_msg_t msg, char * buf, uint32_t len);


enum syntiant_ndp120_mcu_mb_op_e {
    SYNTIANT_NDP120_MB_MCU_NOP              = 0x0,
    SYNTIANT_NDP120_MB_MCU_ACK              = SYNTIANT_NDP120_MB_MCU_NOP,
    SYNTIANT_NDP120_MB_MCU_CONT             = 0x1,
    SYNTIANT_NDP120_MB_MCU_MATCH            = 0x2,
    SYNTIANT_NDP120_MB_MCU_PREPARE          = NDP_MBIN_REQUEST_PREPARE,
    SYNTIANT_NDP120_MB_MCU_DATA             = 0x4,
    SYNTIANT_NDP120_MB_MCU_MIADDR           = 0x8,
    SYNTIANT_NDP120_MB_MCU_LOAD             = 0x9,
    SYNTIANT_NDP120_MB_MCU_RUNNING          = 0x10,
    SYNTIANT_NDP120_MB_MCU_BOOTING          = 0x11,
    SYNTIANT_NDP120_MB_MCU_LOAD_DONE        = 0x12,
    SYNTIANT_NDP120_MB_MCU_RUNNING_TEST     = 0x13,
     
    /* bitmasks */
    SYNTIANT_NDP120_MB_MCU_ERROR_MASK       = 0x20,
    SYNTIANT_NDP120_MB_MCU_NETWORK_MASK     = 0x40
};

enum syntiant_ndp120_mcu_mbin_op_e {
    SYNTIANT_NDP120_MB_MCU_DATA_MASK        = 0x40
};

#if 0
/* error codes (5-bit) */
enum syntiant_ndp120_mb_error_e {
    NDP_MB_ERROR_FAIL               = 0x0,
    NDP_MB_ERROR_UNEXPECTED         = 0x1,
    NDP_MB_ERROR_PACKAGE_MAGIC_TLV  = 0x2,
    NDP_MB_ERROR_PACKAGE_FW_SIZE    = 0x3,
    NDP_MB_ERROR_PACKAGE_INTEGRITY  = 0x4,
    NDP_MB_ERROR_PACKAGE_MISSING_FW = 0x5,
    NDP_MB_ERROR_PACKAGE_FORMAT     = 0x6,
    NDP_MB_ERROR_AUTH               = 0x7,
    SYNTIANT_NDP10x_MB_ERROR_AES    = 0x8
};
#endif

typedef struct {
    uint8_t op;
    const char * name;
} syntiant_ndp120_mb_names_t;

#ifdef __cplusplus
}
#endif

#endif
