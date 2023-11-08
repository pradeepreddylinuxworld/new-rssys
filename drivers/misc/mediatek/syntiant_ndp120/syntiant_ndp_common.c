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

#include <syntiant_ilib/syntiant_portability.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>
#ifndef CHIP_TYPE_120
#include <syntiant_ilib/syntiant_ndp10x.h>
#endif
#ifndef CHIP_TYPE_10X
#include <syntiant_ilib/syntiant_ndp120_driver.h>
#endif
#include <syntiant_ilib/syntiant_ndp.h>

struct syntiant_ndp_driver_s* syntiant_ndp_get_driver(unsigned int device_id)
{
    struct syntiant_ndp_driver_s* driver = NULL;

    switch (syntiant_ndp_get_device_family(device_id)) {
#ifndef CHIP_TYPE_10X
    case SYNTIANT_NDP_CORE_2:
        driver = syntiant_ndp120_get_driver();
        break;
#endif
#ifndef CHIP_TYPE_120
    case SYNTIANT_NDP_CORE_1:
        driver = syntiant_ndp10x_get_driver();
        break;
#endif
    default:
        break;
    }

    return driver;
}
