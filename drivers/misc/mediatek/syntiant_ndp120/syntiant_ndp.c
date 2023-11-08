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
#include <syntiant_ilib/syntiant_portability.h>

#include <syntiant_ilib/syntiant_ndp_ilib_version.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>
#include <syntiant_ilib/syntiant_ndp_error.h>

char *
syntiant_ndp_ilib_version(void) {
    static char *version = SYNTIANT_NDP_ILIB_VERSION;

    return version;
}

int
syntiant_ndp_init(struct syntiant_ndp_device_s **ndpp,
    struct syntiant_ndp_integration_interfaces_s *iif,
    enum syntiant_ndp_init_mode_e init_mode)
{
    int s, s0;
    struct syntiant_ndp_device_s *ndp = *ndpp;
    int alloced = !ndp;

    if (!ndp) {
        ndp = (iif->malloc)(sizeof(struct syntiant_ndp_device_s));
        if (!ndp) {
            s = SYNTIANT_NDP_ERROR_NOMEM;
            goto error;
        }
        /* ndpp may be used by the client during a restart init */
        *ndpp = ndp;
    }

    ndp->init = 0;

    if (iif) {
        ndp->iif.d = iif->d;
        ndp->iif.malloc = iif->malloc;
        ndp->iif.free = iif->free;
        ndp->iif.mbwait = iif->mbwait;
        ndp->iif.get_type = iif->get_type;
        ndp->iif.sync = iif->sync;
        ndp->iif.unsync = iif->unsync;
        ndp->iif.transfer = iif->transfer;
    }

    s = (ndp->iif.get_type)(ndp->iif.d, &ndp->device_type);
    if (s) goto error;

    ndp->driver = syntiant_ndp_get_driver(ndp->device_type);
    if (!ndp->driver) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->init)(ndp, init_mode);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

error:
    if (!s) {
        ndp->init = 1;
    } else if (alloced && ndp) {
        (ndp->iif.free)(ndp);
        *ndpp = NULL;
    }

    return s;
}

int
syntiant_ndp_uninit(struct syntiant_ndp_device_s *ndp, int free,
    enum syntiant_ndp_init_mode_e init_mode)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        return SYNTIANT_NDP_ERROR_UNINIT;
    }

    ndp->init = 0;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->uninit)(ndp, init_mode);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

error:
    if (free) {
        (ndp->iif.free)(ndp);
    }
    return s;
}

int
syntiant_ndp_op_size(struct syntiant_ndp_device_s *ndp, int mcu,
                     unsigned int *size)
{
    int s;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->driver->op_size)(ndp->iif.d, mcu, size);

error:
    return s;
}

int
syntiant_ndp_read_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->read_block)(ndp, mcu, address, value, count);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
                         uint32_t address, void *value, unsigned int count)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->write_block)(ndp, mcu, address, value, count);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_write(struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address,
                   uint32_t value)
{
    int s;
    unsigned int size;

    s = syntiant_ndp_op_size(ndp, mcu, &size);
    if (!s) {
        s = syntiant_ndp_write_block(ndp, mcu, address, &value, size);
    }
    return s;
}

int
syntiant_ndp_read(struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address,
                  void *value)
{
    int s;
    unsigned int size;

    s = syntiant_ndp_op_size(ndp, mcu, &size);
    if (!s) {
        s = syntiant_ndp_read_block(ndp, mcu, address, value, size);
    }
    return s;
}

int
syntiant_ndp_interrupts(struct syntiant_ndp_device_s *ndp, int *on)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->interrupts)(ndp, on);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_poll(
    struct syntiant_ndp_device_s *ndp, uint32_t *notifications, int clear)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->poll)(ndp, notifications, clear);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_load(struct syntiant_ndp_device_s *ndp, void *package, int len)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->load)(ndp, package, len);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_get_config(
    struct syntiant_ndp_device_s *ndp, struct syntiant_ndp_config_s *config)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    config->device_id = ndp->device_type;
    s = (ndp->driver->get_config)(ndp, config);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_send_data(
    struct syntiant_ndp_device_s *ndp, uint8_t *data, unsigned int len,
    int type, uint32_t offset)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    if (len < 1) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->send_data)(ndp, data, len, type, offset);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_extract_data(struct syntiant_ndp_device_s *ndp, int type,
                          int from, uint8_t *data, unsigned int *len)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->extract_data)(ndp, type, from, data, len);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_get_match_summary(
    struct syntiant_ndp_device_s *ndp, uint32_t *summary)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->get_match_summary)(ndp, summary);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_get_match_binary(
    struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->get_match_binary)(ndp, matches, len);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}

int
syntiant_ndp_get_match_strength(
    struct syntiant_ndp_device_s *ndp, uint8_t *strengths, unsigned int len,
    int type)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    s = (ndp->driver->get_match_strength)(ndp, strengths, len, type);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}
