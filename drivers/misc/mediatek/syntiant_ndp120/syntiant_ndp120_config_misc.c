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
#include <syntiant_ilib/syntiant_ndp120.h>
#include <syntiant_ilib/syntiant_ndp_error.h>
#include <syntiant_ilib/ndp120_regs.h>
#include <syntiant_ilib/ndp120_spi_regs.h>
#include <syntiant-dsp-firmware/ndp120_dsp_mailbox.h>
#include <syntiant-dsp-firmware/ndp120_dsp_fw_state.h>
 
/* ndp120 reg state for config funcs */
struct syntiant_ndp120_config_state_s {
    unsigned int dnn_input;
    unsigned int input_clock_rate;
    unsigned int core_clock_rate;
    uint32_t fw_state_addr;
    uint32_t fw_enable;
    uint32_t fw_max_adjustment_gain;
    uint32_t fw_nom_speech_quiet;
    uint32_t fw_match_per_frame;
    uint32_t smplctl;
    uint32_t smplmark;
    uint32_t smplsts;
    uint32_t tank;
    uint32_t i2sctl;
    uint32_t pdmctl;
    uint32_t clkctl0;
    uint32_t clkctl1;
    uint32_t clkctl2;
    uint32_t freqctl;
    uint32_t fifo_thresh[5];
    uint8_t ctl;
};

/* UNDER REVIEW
 * 
 * The below was added for completeness 
 * in parity with 10x code, and may or may 
 * not be appropriate here.  It's here 
 * as a reminder of what _might_ need 
 * to be implemented.
 */
static int
config_read_state(struct syntiant_ndp_device_s *ndp,
                                  struct syntiant_ndp120_config_state_s *st)
{
    int s;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    uint32_t addr;

    if (ndp120->mcu_fw_state_addr) {
        addr = ndp120->mcu_fw_state_addr
            + (uint32_t) offsetof(struct ndp120_fw_state_s, enable);
        s = syntiant_ndp120_read(ndp, 1, addr, &st->fw_enable);
        if (s)
            goto out;

        addr = ndp120->mcu_fw_state_addr
            + (uint32_t) offsetof(struct ndp120_fw_state_s,
                                  mb_state.enable_match_for_every_frame);
        s = syntiant_ndp120_read(ndp, 1, addr, &st->fw_match_per_frame);
        if (s)
            goto out;
    } else {
        st->fw_enable = 0;
        st->fw_match_per_frame = 0;
    }

    s = syntiant_ndp120_read(ndp, 0, NDP120_SPI_CTL, &st->ctl);
    if (s)
        goto out;

 out:
    return s;
}

/* UNDER REVIEW
 * 
 * The below was added for completeness 
 * in parity with 10x code, and may or may 
 * not be appropriate here.  It's here 
 * as a reminder of what _might_ need 
 * to be implemented.
 */
static int
config_set_input(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_misc_t *config,
                                 struct syntiant_ndp120_config_state_s *st)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    unsigned int i;
    uint32_t dsp_fw_state_adx;
    ndp120_dsp_data_flow_setup_t flow_setup;

    (void)st;

    if (config->set & NDP120_CONFIG_SET_MISC_INPUT) {

        dsp_fw_state_adx = syntiant_ndp120_get_dsp_fw_pointer(ndp);
        if (!dsp_fw_state_adx) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto error;
        }

        s = syntiant_ndp120_dsp_flow_setup_get_no_sync(ndp, &flow_setup);
        if(s) goto error;

        /* fixit */

        for(i = 0; i < ARRAY_LEN(flow_setup.src_pcm_audio); i++) {
            ndp120_dsp_data_flow_rule_t *rule = &flow_setup.src_pcm_audio[i];
            if (NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) {
                rule->src_param = (uint8_t)config->input;
            }

        }

        /* apply flow */
	SYNTIANT_PRINTF("call flow_setup_apply_no_sync\n");
        s = syntiant_ndp120_dsp_flow_setup_apply_no_sync(ndp, &flow_setup);
        if (s) goto error;

        /* ping DSP to reset buffers */
        syntiant_ndp120_do_mailbox_req_no_sync(ndp, NDP120_DSP_MB_H2D_PING, NULL);
        if (s) goto error;
    }

error:
    return s;
}

/* UNDER REVIEW
 * 
 * The below was added for completeness 
 * in parity with 10x code, and may or may 
 * not be appropriate here.  It's here 
 * as a reminder of what _might_ need 
 * to be implemented.
 */
static int
config_update_state(struct syntiant_ndp_device_s *ndp,
                                    struct syntiant_ndp120_config_state_s *st0,
                                    struct syntiant_ndp120_config_state_s *st)
{
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    uint32_t addr;
    int s = SYNTIANT_NDP_ERROR_NONE;
    int do_reset;

    do_reset = st0->smplctl != st->smplctl || st0->smplmark != st->smplmark;

    do_reset = 1;

    /* TODO update pdm ctl, dnn ctl etc. */
    if (do_reset) {

        if (st0->fw_enable != st->fw_enable) {
            addr = ndp120->mcu_fw_state_addr
                + (uint32_t) offsetof(struct ndp120_fw_state_s, enable);
            s = syntiant_ndp120_write(ndp, 1, addr, st->fw_enable);
            if (s)
                goto out;

        }

        if (st0->fw_match_per_frame != st->fw_match_per_frame) {
            SYNTIANT_PRINTF("setting match-per-frame: %s\n", st->fw_match_per_frame ? "on" : "off");
            addr = ndp120->mcu_fw_state_addr + (uint32_t) offsetof(struct ndp120_fw_state_s, mb_state.enable_match_for_every_frame);
            s = syntiant_ndp120_write(ndp, 1, addr, st->fw_match_per_frame);
            if (s)
                goto out;
        }
    }

    if (do_reset) {
        s = syntiant_ndp120_write(ndp, 0, NDP120_SPI_INTSTS,
                                  NDP120_SPI_INTSTS_MATCH_INT(1)
                                  | NDP120_SPI_INTSTS_DNN_INT(1)
                                  | NDP120_SPI_INTSTS_AE_INT(1)
                                  | NDP120_SPI_INTSTS_WM_INT(1));
        if (s) goto out;
    }

 out:
    return s;
}

static int
syntiant_ndp120_config_misc_no_sync(struct syntiant_ndp_device_s *ndp,
                               syntiant_ndp120_config_misc_t *config)
{
    struct syntiant_ndp120_config_state_s st, st0;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    ndp120_dsp_config_t  fw_config;
    ndp120_dsp_fifo_info_t fifo_info;

    uint32_t adx;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (!ndp120->mcu_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    if (config->set & (uint32_t) ~NDP120_CONFIG_SET_MISC_ALL_M) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    adx = ndp120->dsp_fw_state_addr + (uint32_t) offsetof(ndp120_dsp_fw_base_t, config);
    s = syntiant_ndp120_read_block(ndp, 1, adx, &fw_config, sizeof(fw_config));
    if (s) {
	    SYNTIANT_PRINTF("error in reading DSP FW config block\n");
	    goto error;
    }
    ndp120->audio_frame_step = fw_config.aud_samp_size_bytes/PCM_AUDIO_SAMPLE_WIDTH_BYTES;
    ndp120->audio_sample_size_bytes = fw_config.aud_samp_size_bytes;

    adx = ndp120->dsp_fw_state_addr + (uint32_t) offsetof(ndp120_dsp_fw_base_t, fifo_info);
    s = syntiant_ndp120_read_block(ndp, 1, adx, &fifo_info, sizeof(fifo_info));
    if (s) {
	    SYNTIANT_PRINTF("error reading fifo info\n");
 	    goto error;
    }
    ndp120->audio_frame_step = fw_config.fifo_threshold_bytes[4]/PCM_AUDIO_SAMPLE_WIDTH_BYTES; /* FIXME, this is only SPI */
    /*ndp120->audio_sample_size_bytes = dsp_fw_state.fifo_threshold_bytes[0]; */
    ndp120->audio_sample_size_bytes = 2;

    config->audio_buffer_used = (uint32_t)(fifo_info.fifo_end_adx[4] -
            fifo_info.fifo_start_adx[4]);

    if (!config->set && !config->get) {
        config->input = ndp120->dnn_input;
        config->audio_frame_size = ndp120->audio_frame_size;
        config->audio_sample_size_bytes = ndp120->audio_sample_size_bytes;
        config->audio_frame_step = ndp120->audio_frame_step;
        config->dnn_frame_size = ndp120->dnn_frame_size;
        goto error;
    }

    s = config_set_input(ndp, config, &st);
    if (s) {
	    SYNTIANT_PRINTF("error in setting input\n");
	    goto error;
    }

    s = config_read_state(ndp, &st);
    if (s) {
	    SYNTIANT_PRINTF("error in reading state\n");
	    goto error;
    }

    st.fw_state_addr = ndp120->mcu_fw_state_addr;
    st.input_clock_rate = ndp120->input_clock_rate;

    st0 = st;

    if (config->set & NDP120_CONFIG_SET_MISC_MATCH_PER_FRAME_ON) {
        st.fw_match_per_frame = config->match_per_frame_on;
    }

    config->match_per_frame_on = st0.fw_match_per_frame;

    s = config_update_state(ndp, &st0, &st);
    if (s) {
	SYNTIANT_PRINTF("error in updating state\n");
	goto error;
    }

    ndp120->input_clock_rate = st.input_clock_rate;

 error:
     return s;
}

int
syntiant_ndp120_config_misc(struct syntiant_ndp_device_s *ndp,
    syntiant_ndp120_config_misc_t *config)
{
    int s, s0;

    if (!ndp || !ndp->init) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) {
	SYNTIANT_PRINTF("error in config_misc:%d\n", s);
	return s;   
    }

    s = syntiant_ndp120_config_misc_no_sync(ndp, config);
    if(s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}
