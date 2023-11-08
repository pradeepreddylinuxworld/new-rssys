/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2021 Syntiant Corporation
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

#ifndef NDP10X_SMAP_H
#define NDP10X_SMAP_H

#ifdef __cplusplus
extern "C" {
#endif
    
/**
 * @brief defines the host <-> MCU communcation area for secured
 *  MCU access protocol (SMAP).
 *
 * These definitions are used by multiple different firmware images
 * and should (practically) never be changed.
 *
 */
enum ndp10x_fw_smap_region_constants_e {
    NDP10X_FW_SMAP_BASE = 0x20017c00, /* host <-> fw communication */
    NDP10X_FW_SMAP_SIZE = 0x400,
    NDP10X_FW_SMAP_CONTROL_SIZE = 8,
    NDP10X_FW_SMAP_DATA_SIZE = NDP10X_FW_SMAP_SIZE - NDP10X_FW_SMAP_CONTROL_SIZE
};

/* host <-> fw communication structures */
enum ndp10x_fw_smap_e {
    NDP10X_FW_SMAP_CONTROL_RUN_M = 0x1,
    NDP10X_FW_SMAP_CONTROL_OP_S = 1,
    NDP10X_FW_SMAP_CONTROL_OP_M = 0x3 << NDP10X_FW_SMAP_CONTROL_OP_S,
    NDP10X_FW_SMAP_CONTROL_OP_READ = 0,
    NDP10X_FW_SMAP_CONTROL_OP_WRITE = 1,
    NDP10X_FW_SMAP_CONTROL_OP_LOAD = 2,
    NDP10X_FW_SMAP_CONTROL_STATUS_S = 3,
    NDP10X_FW_SMAP_CONTROL_STATUS_M = 0x7 << NDP10X_FW_SMAP_CONTROL_STATUS_S,
    NDP10X_FW_SMAP_CONTROL_STATUS_SUCCESS = 0,
    NDP10X_FW_SMAP_CONTROL_STATUS_UNSUP = 1,
    NDP10X_FW_SMAP_CONTROL_STATUS_ACCESS = 2,
    NDP10X_FW_SMAP_CONTROL_STATUS_SIZE = 3,
    NDP10X_FW_SMAP_CONTROL_STATUS_AES = 4,
    NDP10X_FW_SMAP_CONTROL_STATUS_MORE = 5,
    NDP10X_FW_SMAP_CONTROL_STATUS_FAIL = 6,
    NDP10X_FW_SMAP_CONTROL_LEN_S = 16,
    NDP10X_FW_SMAP_CONTROL_LEN_M = 0x3ff << NDP10X_FW_SMAP_CONTROL_LEN_S
};
    
struct ndp10x_fw_smap_s {
    uint32_t control;
    uint32_t address;
    uint32_t data[1];
};

#ifdef __cplusplus
}
#endif
#endif
