/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2020 Syntiant Corporation
 *   All Rights Reserved.
 *
 *  NOTICE:  All information contained herein is, and remains the property of
 *  Syntiant Corporation and its suppliers, if any.  The intellectual and
 *  technical concepts contained herein are proprietary to Syntiant Corporation
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents
 *  in process, and are protected by trade secret or copyright law.
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from
 *  Syntiant Corporation.
 */
#ifndef NDP120_IOCTL_H
#define NDP120_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/types.h>

struct ndp120_init_s {
    unsigned int input_freq;
    unsigned int mode;
};

#define NDP120_INIT _IOR('a', 'm', __u64)
#define NDP120_CONFIG _IOR('a', 'n', __u64)
#define NDP120_CONFIG_OTHER _IOR('a', 'o', __u64)
#define NDP120_CONFIG_S _IOR('a', 'p', __u64)
#define NDP120_FLOW_SETUP _IOR('a', 'q', __u64)

#ifdef __cplusplus
}
#endif

#endif
