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

#include <syntiant_ilib/syntiant_portability.h>
#include <syntiant-firmware/ndp120_mb.h>

#include <syntiant_ilib/syntiant_ndp120_mailbox.h>
#include <syntiant_ilib/syntiant_ndp_error.h>

static syntiant_ndp120_mb_names_t  syntiant_ndp120_mcu_mb_error_names[] =
{
    {NDP_MB_ERROR_NONE,                 "NONE" },
    {NDP_MB_ERROR_UNEXPECTED,           "UNEXPECTED" },
    {NDP_MB_ERROR_PACKAGE_MAGIC_TLV,    "MAGIC_TLV" },
    {NDP_MB_ERROR_PACKAGE_FW_SIZE,      "FW_SIZE" },
    {NDP_MB_ERROR_PACKAGE_INTEGRITY,    "INTEGRITY" },
    {NDP_MB_ERROR_PACKAGE_MISSING_FW,   "MISSING_FW" },
    {NDP_MB_ERROR_PACKAGE_FORMAT,       "PACKAGE_FORMAT" },
    {NDP_MB_ERROR_AUTH,                 "AUTH" },
    { 0xff, NULL },
}; 

static 
syntiant_ndp120_mb_names_t syntiant_ndp120_mcu_mb_op_names[] =
{
    { SYNTIANT_NDP120_MB_MCU_NOP,           "NOP" },
    { SYNTIANT_NDP120_MB_MCU_CONT,          "CONT" },
    { SYNTIANT_NDP120_MB_MCU_MATCH,         "MATCH" },
    { SYNTIANT_NDP120_MB_MCU_DATA,          "DATA" },
    { SYNTIANT_NDP120_MB_MCU_MIADDR,        "MIADDR" },
    { SYNTIANT_NDP120_MB_MCU_LOAD,          "LOAD" },
    { SYNTIANT_NDP120_MB_MCU_RUNNING,       "RUNNING" },
    { SYNTIANT_NDP120_MB_MCU_RUNNING_TEST,  "RUNNING_TEST" },
    { SYNTIANT_NDP120_MB_MCU_BOOTING,       "BOOTING" },
    { SYNTIANT_NDP120_MB_MCU_LOAD_DONE,     "LOAD_DONE" },

    /* these are duplicates of some below because
       they go out via mbin, and come back via
       watermarkint */
    { NDP120_DSP_MB_H2D_PING,       "PING"},
    { NDP120_DSP_MB_H2D_ADX_UPPER,  "ADX_UPPER"},
    { NDP120_DSP_MB_H2D_ADX_LOWER,  "ADX_LOWER"},
    { 0xff, NULL },
}; 


static 
syntiant_ndp120_mb_names_t syntiant_ndp120_dsp_mb_op_names[] =
{
    {NDP120_DSP_MB_D2H_WATERMARK,           "WATERMARK"},
    {NDP120_DSP_MB_D2H_EXTRACT_READY,       "EXTRACT_READY"},
    {NDP120_DSP_MB_D2H_RUNNING,             "RUNNING"},
    {NDP120_DSP_MB_D2H_DEBUG,               "DEBUG"},
    {NDP120_DSP_MB_H2D_PING,                "PING"},
    {NDP120_DSP_MB_H2D_ADX_UPPER,           "ADX_UPPER"},
    {NDP120_DSP_MB_H2D_ADX_LOWER,           "ADX_LOWER"},
    {0xFF, NULL}
};


void syntiant_ndp120_mcu_mb_error_decoder(uint8_t error, char* buf, uint32_t len) {
    syntiant_ndp120_mb_names_t  *p;
    error &= 0x1f;

    if (!buf || !len) return;
    buf[0] = 0;

    for (p = syntiant_ndp120_mcu_mb_error_names; p->name; ++p) {
        if (p->op == error) {
            snprintf(buf, len, "%s", p->name);
            return;
        }
    }
    snprintf(buf, len, "<UNKNOWN>");
}

void syntiant_ndp120_mcu_mb_op_decoder(uint8_t op, char * buf, uint32_t len) {
    char errorBuf[32];
    syntiant_ndp120_mb_names_t  *p;
    op &= 0x7f;

    if (!buf || !len) return;
    buf[0] = 0;

    /* error condition */
    if (op & SYNTIANT_NDP120_MB_MCU_DATA_MASK) {
        snprintf(buf, len, "DATA: 0x%02X", op & 0x3F);
    } else if (op & SYNTIANT_NDP120_MB_MCU_ERROR_MASK) {
        syntiant_ndp120_mcu_mb_error_decoder(op, errorBuf, sizeof(errorBuf));
        snprintf(buf, len, "ERROR: %s (0x%02X)", errorBuf, op & 0x1F);
    } else {
        for (p = syntiant_ndp120_mcu_mb_op_names; p->name; ++p) {
            if (p->op == op) {
                snprintf(buf, len, "%s", p->name);
                return;
            }
        }
        snprintf(buf, len, "<UNKNOWN>");
    }
}

void syntiant_ndp120_dsp_mb_op_decoder(ndp120_dsp_mailbox_msg_t msg, char * buf,
        uint32_t len) {
    syntiant_ndp120_mb_names_t  *p;
    for (p = syntiant_ndp120_dsp_mb_op_names; p->name; ++p) {
        if (p->op == NDP120_DSP_MB_GET_MESSAGE(msg)) {
            snprintf(buf, len, "%s", p->name);
            return;
        }
    }
}
