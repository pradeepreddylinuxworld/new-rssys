/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2021 Syntiant Corporation
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
#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/types.h>
#endif
#include <syntiant_ilib/syntiant_ndp.h>

syntiant_ndp_device_type_t syntiant_ndp_get_device_family(unsigned int device_id)
{
    switch (device_id) {
    case 0x2:
    case 0x13:
    case 0x14:
    case 0x18:
    case 0x20:
    case 0x24:
    case 0x28:
        return SYNTIANT_NDP_CORE_1;
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x35:
    case 0x36:
    case 0x37:
    case 0x38:
    case 0x39:
    case 0x3A:
    case 0x3B:
        return SYNTIANT_NDP_CORE_2;
    }
    return SYNTIANT_NDP_INV_CORE;
}

