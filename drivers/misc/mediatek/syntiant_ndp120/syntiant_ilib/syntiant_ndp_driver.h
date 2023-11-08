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
 * ILib-internal driver interface definitions
 */
#ifndef SYNTIANT_NDP_DRIVER_H
#define SYNTIANT_NDP_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <syntiant_ilib/syntiant_ndp.h>

#include <syntiant_ilib/syntiant_ndp10x_driver.h>
#include <syntiant_ilib/syntiant_ndp120_driver.h>

#include <syntiant_packager/syntiant_package.h>

struct syntiant_ndp_driver_s {
    unsigned int *device_types;
    int (*init)(struct syntiant_ndp_device_s *ndp,
        enum syntiant_ndp_init_mode_e init_mode);
    int (*uninit)(struct syntiant_ndp_device_s *ndp,
        enum syntiant_ndp_init_mode_e init_mode);
    int (*op_size)(struct syntiant_ndp_device_s *ndp, int mcu,
                   unsigned int *size);
    int (*interrupts)(struct syntiant_ndp_device_s *ndp, int *on);
    int (*poll)(
        struct syntiant_ndp_device_s *ndp, uint32_t *notifications, int clear);
    int (*load)(struct syntiant_ndp_device_s *ndp, void *package, int len);
    int (*get_config)(struct syntiant_ndp_device_s *ndp,
        struct syntiant_ndp_config_s *config);
    int (*send_data)(struct syntiant_ndp_device_s *ndp, uint8_t *data,
                     unsigned int len, int type, uint32_t offset);
    int (*extract_data)(struct syntiant_ndp_device_s *ndp,
                        int type, int from, uint8_t *data, unsigned int *len);
    int (*get_match_summary)(
        struct syntiant_ndp_device_s *ndp, uint32_t *summary);
    int (*get_match_binary)(
        struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len);
    int (*get_match_strength)(struct syntiant_ndp_device_s *ndp,
        uint8_t *strengths, unsigned int len, int type);
    int (*read_block)(struct syntiant_ndp_device_s *ndp, int mcu,
                      uint32_t address, void *value, unsigned int count);
    int (*write_block)(struct syntiant_ndp_device_s *ndp, int mcu,
                       uint32_t address, void *value, unsigned int count);
};

/**
 * @brief NDP interface library internal state object
 */
struct syntiant_ndp_device_s {
    struct syntiant_ndp_integration_interfaces_s iif;
    struct syntiant_ndp_driver_s *driver;
    unsigned int device_type;
    uint8_t init;
    syntiant_pkg_parser_state_t pstate;
    union {
        struct syntiant_ndp10x_device_s ndp10x;
        syntiant_ndp120_device_t ndp120;
    } d;
};

struct syntiant_ndp_driver_s* syntiant_ndp_get_driver(unsigned int device_type);

#ifdef __cplusplus
}
#endif

#endif
