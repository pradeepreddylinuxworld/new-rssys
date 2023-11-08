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
#ifndef __KERNEL__
#include <stdint.h>
#else
#include <linux/types.h>
#endif

#include <syntiant_ilib/syntiant_ndp_error.h>
#include <syntiant_ilib/syntiant_ndp120.h>
#include <syntiant-firmware/ndp120_firmware.h>
#include <syntiant-firmware/ndp120_ph.h>
#include <syntiant_ilib/syntiant_ndp120_mailbox.h>

#include "syntiant_ndp120_ph.h"

enum syntiant_ndp120_posterior_constants_e {
    POSTERIOR_ENABLE_BITS=(NDP120_FW_STATE_ENABLE_SMAX_SMOOTHER
                           | NDP120_FW_STATE_ENABLE_POSTERIOR)
};

#define PH_ACTION_MASK 0x000000ff
#define PH_ACTION_ARG0_MASK 0x0000ff00
#define PH_ACTION_ARG0_SHIFT 8
#define PH_ACTION_ARG1_MASK 0x00ff0000
#define PH_ACTION_ARG1_SHIFT 16

static int
syntiant_ndp120_posterior_config_encode_action(
    unsigned int ph_type, unsigned int atype,
    unsigned int arg0, unsigned int arg1, uint32_t *action)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    
    if (ph_type == 0){
        switch (atype) {
        case NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_MATCH:
            if (63 < arg0) {
                s = SYNTIANT_NDP_ERROR_ARG;
                goto error;
            }
            *action = NDP120_PH_ACTION_MATCH_M | arg0;
            break;
        case NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_STATE:
            *action = arg0;
            break;
        case NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_STAY:
            *action = NDP120_PH_ACTION_STAY;
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
    } else {
        *action = atype & PH_ACTION_ARG0_MASK;
        *action |= 
            (*action << PH_ACTION_ARG0_SHIFT) & (arg0 & PH_ACTION_ARG0_MASK);
        *action |=
            (*action << PH_ACTION_ARG1_SHIFT) & (arg1 & PH_ACTION_ARG1_MASK);
    }

 error:
    return s;
}

static void
syntiant_ndp120_posterior_config_decode_action(
    uint32_t action, unsigned int ph_type, unsigned int *atype,
    unsigned int *arg0, unsigned int *arg1)
{
    if (ph_type == 0) {
        if (action & NDP120_PH_ACTION_MATCH_M) {
            *atype = NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_MATCH;
            *arg0 = action & ~((unsigned int) NDP120_PH_ACTION_MATCH_M);
        } else if (action == NDP120_PH_ACTION_STAY) {
            *atype = NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_STAY;
        } else {
            *atype = NDP120_CONFIG_SET_POSTERIOR_CONFIG_ACTION_TYPE_STATE;
            *arg0 = action;
        }
    } else {
        *atype = action & PH_ACTION_MASK;
        *arg0 = (action & PH_ACTION_ARG0_MASK) >> PH_ACTION_ARG0_SHIFT;
        *arg1 = (action & PH_ACTION_ARG1_MASK) >> PH_ACTION_ARG1_SHIFT;
    }
}

static int
syntiant_ndp120_posterior_init(
struct syntiant_ndp_device_s *ndp, uint32_t ph_idx, uint32_t states,
uint32_t classes, uint32_t ph_type)
{
    uint32_t ph_params_addr = (uint32_t)
        (ndp->d.ndp120.mcu_fw_posterior_parameters_addr +
        ph_idx * sizeof(struct ndp120_ph_params_s));

    struct ndp120_ph_state_params_s sparams;
    struct ndp120_ph_class_params_s cparams;
    uint32_t class_base_for_state_offset;
    uint32_t params_base_addr, saddr, caddr;

    unsigned int i, j;
    int s;
    uint32_t offset = 0x0;

    if (!ph_params_addr) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

    sparams.timeout = 0;
    sparams.timeout_action = 0;

    cparams.window = 0;
    cparams.threshold = 65535;
    cparams.backoff = 0;
    cparams.smoothing_queue_size = 1;

    offset = offsetof(struct ndp120_ph_params_s, params_memory);

    params_base_addr = ph_params_addr + offset;

    class_base_for_state_offset
        = states
        * ((unsigned int) sizeof(struct ndp120_ph_state_params_s))
        / ((unsigned int) sizeof(uint32_t));

    for (i = 0; i < states; i++) {

        class_base_for_state_offset
            += i * (classes * ((unsigned int) sizeof(cparams))
                    / ((unsigned int) sizeof(uint32_t)));
        sparams.class_params_offset = class_base_for_state_offset;
        saddr = params_base_addr + i * ((unsigned int) sizeof(sparams));

        s = syntiant_ndp120_write_block(
            ndp, 1, saddr, &sparams, sizeof(sparams));
        if (s) goto error;

        for (j = 0; j < classes; j++) {
            cparams.action = NDP120_PH_ACTION_MATCH_M | j;
            caddr = params_base_addr + class_base_for_state_offset
                * ((unsigned int) sizeof(uint32_t))
                + j * ((unsigned int) sizeof(cparams));
            s = syntiant_ndp120_write_block(
                ndp, 1, caddr, &cparams, sizeof(cparams));
            if (s) goto error;
        }
    }

    s = syntiant_ndp120_write(ndp, 1,
        ph_params_addr + offsetof(struct ndp120_ph_params_s, num_classes),
        classes);
    if (s) goto error;

    s = syntiant_ndp120_write
        (ndp, 1, ph_params_addr
         + ((unsigned int) offsetof(struct ndp120_ph_params_s, num_states)),
        states);
    if (s) goto error;

    s = syntiant_ndp120_write(
        ndp, 1, ph_params_addr 
         + ((unsigned int) offsetof(struct ndp120_ph_params_s, ph_type)),
        ph_type);
    if (s) goto error;

error:
    return s;
}

int
syntiant_ndp120_posterior_config_no_sync(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp120_posterior_config_s *config)
{
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    uint32_t fw_state_addr = ndp120->mcu_fw_state_addr;
    uint32_t ph_params_addr = ndp->d.ndp120.mcu_fw_posterior_parameters_addr;
    uint32_t states, classes, ph_type, ph_num;
    uint32_t state_addr, params_addr;
    struct ndp120_ph_state_params_s sparams;
    struct ndp120_ph_class_params_s cparams;
    int s;
    uint32_t fw_state_enable;
    uint32_t offset=0x0;

    if (config->set
        & ((unsigned int) ~NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ALL_M)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if ((config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ENABLE)
        && (config->set
            & ((unsigned int) ~NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ENABLE))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if ((config->set & (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_STATES
                        | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_CLASSES
                        | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_PH_TYPE))
        && (config->set
            & ~((unsigned int)
                (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_STATES
                 | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_CLASSES
                 | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_PH_TYPE)))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if ((config->set & (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT
                        | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION))
        && (config->set
            & ~((unsigned int)
                (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT
                 | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION)))) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    
    if (!fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }


    offset = offsetof(struct ndp120_fw_state_s, enable);
    s = syntiant_ndp120_read(ndp, 1, fw_state_addr + offset, &fw_state_enable);
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ENABLE) {
        /* set */
        if (config->enable) {
            fw_state_enable |= POSTERIOR_ENABLE_BITS;
        } else {
            fw_state_enable &= ~((unsigned int) POSTERIOR_ENABLE_BITS);
        }

        s = syntiant_ndp120_write(ndp, 1, fw_state_addr + offset,
                                  fw_state_enable);
        if (s) goto error;
        DEBUG_PRINTF("0x%X --> 0x%X (fw_state_enable)", fw_state_enable, fw_state_addr + offset);

        s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_NOP, NULL);
        if (s) goto error;
        goto got_enable;

    } else {
        /* get */
        config->enable = (fw_state_enable & POSTERIOR_ENABLE_BITS)
            == POSTERIOR_ENABLE_BITS;
    }

    if (!ph_params_addr) {
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

    if (config->enable && 
            (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_NUM_PH)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    offset = offsetof(struct ndp120_ph_params_collection_s, num_params);
    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_NUM_PH) {
        s = syntiant_ndp120_write
            (ndp, 1, ph_params_addr + offset, config->ph_num);
        goto error;
        
    } else {
        s = syntiant_ndp120_read(ndp, 1, ph_params_addr + offset, &ph_num);
        if (s) goto error;
        config->ph_num = ph_num;
    }

    if (config->ph_idx >= ph_num){
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    ph_params_addr += config->ph_idx * (uint32_t) sizeof(struct ndp120_ph_params_s);

    offset = offsetof(struct ndp120_ph_params_s, num_states);
    s = syntiant_ndp120_read(ndp, 1, ph_params_addr + offset, &states);
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_STATES) {
        states = config->states;
    }

    offset = offsetof(struct ndp120_ph_params_s, num_classes);
    s = syntiant_ndp120_read(ndp, 1, ph_params_addr + offset, &classes);
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_CLASSES) {
        classes = config->classes;
    }

    offset = offsetof(struct ndp120_ph_params_s, ph_type);
    s = syntiant_ndp120_read(ndp, 1, ph_params_addr + offset, &ph_type);
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_PH_TYPE) {
        ph_type = config->ph_type;
    }

    if (config->set
        & (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_STATES
              | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_CLASSES
              | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_PH_TYPE)) {
        s = syntiant_ndp120_posterior_init
            (ndp, config->ph_idx, states, classes, ph_type);
        if (s) goto error;
        goto got_structure;
    }

    if (!config->set && !states) {
        goto got_structure;
    }

    if (states <= config->state) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    offset = offsetof(struct ndp120_ph_params_s, params_memory);
    state_addr = ph_params_addr + offset;
    state_addr += config->state * ((unsigned int) sizeof(sparams));
    s = syntiant_ndp120_read_block(ndp, 1, state_addr, &sparams,
                                   sizeof(sparams));
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT) {
        sparams.timeout = config->timeout;
    }

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION) {
        s = syntiant_ndp120_posterior_config_encode_action
            (config->ph_type, config->timeout_action_type,
             config->timeout_action_arg0, config->timeout_action_arg1,
             &sparams.timeout_action);
        if (s)
            goto error;
    }

    if (!(config->set
          & ~(unsigned int)
          (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT
           | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION))
        && !classes) {
        goto got_state;
    }

    if (classes <= config->class_index) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    offset = offsetof(struct ndp120_ph_params_s, params_memory);

    params_addr = ph_params_addr + offset
        + sparams.class_params_offset * ((unsigned int) sizeof(uint32_t))
        + config->class_index * ((unsigned int) sizeof(cparams));
    s = syntiant_ndp120_read_block(
        ndp, 1, params_addr, &cparams, sizeof(cparams));
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_THRESHOLD) {
        cparams.threshold = config->threshold;
    }

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_WINDOW) {
        cparams.window = config->window;
    }

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_BACKOFF) {
        cparams.backoff = config->backoff;
    }

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE) {
        if (NDP120_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE
            < config->smoothing_queue_size) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        cparams.smoothing_queue_size = config->smoothing_queue_size;
    }

    if (config->set & NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ACTION) {
        s = syntiant_ndp120_posterior_config_encode_action
            (ph_type, config->action_type, config->action_arg0,
             config->action_arg1, &cparams.action);
        if (s)
            goto error;
    }

    if (config->set & (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_THRESHOLD
                          | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_WINDOW
                          | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_BACKOFF
                          | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ACTION
                          | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE))
    {
        s = syntiant_ndp120_write_block(
            ndp, 1, params_addr, &cparams, sizeof(cparams));
        if (s) goto error;
    }

    config->threshold = cparams.threshold;
    config->window = cparams.window;
    config->backoff = cparams.backoff;
    config->smoothing_queue_size = cparams.smoothing_queue_size;

    syntiant_ndp120_posterior_config_decode_action
        (cparams.action, ph_type, &config->action_type,
         &config->action_arg0, &config->action_arg1);
                          
got_state:
    if (config->set
        & (NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT
           | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION)) {
        s = syntiant_ndp120_write_block(
            ndp, 1, state_addr, &sparams, sizeof(sparams));
        if (s) goto error;
    }
    config->timeout = sparams.timeout;
    syntiant_ndp120_posterior_config_decode_action
        (sparams.timeout_action, ph_type, &config->timeout_action_type,
         &config->timeout_action_arg0, &config->timeout_action_arg1);

got_structure:
    config->states = states;
    config->classes = classes;
    config->ph_type = ph_type;

got_enable:
error:
    return s;
}

int
syntiant_ndp120_posterior_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp120_posterior_config_s *config)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) {
	DEBUG_PRINTF("Error in syntiant_ndp120_posterior_config\n");
	goto error;
    }

    s = syntiant_ndp120_posterior_config_no_sync(ndp, config);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
error:
    return s;
}
