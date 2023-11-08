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

#include <syntiant_ilib/ndp10x_regs.h>
#include <syntiant_ilib/ndp10x_spi_regs.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp10x.h>
#include <syntiant_ilib/syntiant_ndp10x_driver.h>
#include <syntiant-firmware/ndp10x_firmware.h>
#include <syntiant_ilib/syntiant_ndp10x_mailbox.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>
#include <syntiant_ilib/syntiant_ndp_error.h>
#include <syntiant_packager/syntiant_package_consts.h>
#include <syntiant_packager/syntiant_package_tags.h>

extern int
syntiant_ndp10x_run_smap_op(struct syntiant_ndp_device_s *ndp,
                            unsigned int op, uint32_t address,
                            unsigned int count);

extern int
syntiant_ndp10x_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, int count);

extern int
syntiant_ndp10x_pkg_parse_hw_config_v2_tlv(
    struct syntiant_ndp_device_s *ndp);

extern int
syntiant_ndp10x_read(
    struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address, void *value);

extern int
syntiant_ndp10x_write(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, uint32_t value);

extern int 
syntiant_ndp10x_posterior_init(
    struct syntiant_ndp_device_s *ndp, uint32_t states, uint32_t classes, uint32_t ph_type);

extern int 
syntiant_ndp10x_posterior_config_no_sync(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config);

extern int
syntiant_ndp10x_load_fw_pointers(struct syntiant_ndp_device_s *ndp);

extern void
syntiant_ndp10x_reset_firmware_state(struct syntiant_ndp_device_s *ndp);

extern int
syntiant_ndp10x_do_mailbox_req_no_sync(struct syntiant_ndp_device_s *ndp,
                                       uint8_t req);

extern int
syntiant_ndp10x_config_no_sync(struct syntiant_ndp_device_s *ndp,
                       struct syntiant_ndp10x_config_s *config);

int syntiant_ndp10x_pkg_parse_store_version(struct syntiant_ndp_device_s *ndp,
                                            unsigned int tag, uint8_t *ver,
                                            unsigned int len);

int
syntiant_ndp10x_pkg_parse_version_string_v1_tlv
(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_board_calibration_params_v1_v2_tlv(
    struct syntiant_ndp_device_s *ndp, int padded);

int syntiant_ndp10x_pkg_parse_nn_params_v3_tlv_secured
(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_board_calibration_params_v3_tlv(
                        struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_board_calibration_params_v4_tlv(
                        struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_nn_labels_v1_tlv(struct syntiant_ndp_device_s *ndp);


int
syntiant_ndp10x_pkg_parse_nn_labels_v1_tlv(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_nn_labels_v1_tlv(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_firmware_tlv(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_nn_params_encrypted_v1_tlv
(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_nn_params_v3_tlv(
    struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_ph_params_v4_tlv(
    struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_ph_params_v5_tlv(
    struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_ph_params_v6_tlv(
    struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_parse_tag_values(struct syntiant_ndp_device_s *ndp);

int
syntiant_ndp10x_pkg_parse_b0_nn_config_v2_v3_tlv(
    struct syntiant_ndp_device_s *ndp, int aligned_value);


int
syntiant_ndp10x_pkg_parse_store_version(struct syntiant_ndp_device_s *ndp,
                                        unsigned int tag, uint8_t *ver,
                                        unsigned int len)
{
    int s;
    uint32_t config_address, valid;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;

    config_address = NDP10X_ILIB_SCRATCH_ORIGIN +
        offsetof(struct syntiant_ndp10x_config_layout, valid);
    s = syntiant_ndp10x_read(ndp, 1, config_address, &valid);
    if (s) goto error;
    
    if (tag == TAG_FIRMWARE_VERSION_STRING_V1) {
        valid |= SYNTIANT_CONFIG_FW_VERSION_VALID;
    } else if (tag == TAG_NN_VERSION_STRING_V1) {
        valid |= SYNTIANT_CONFIG_NN_VERSION_VALID;
    } else if (tag == TAG_PACKAGE_VERSION_STRING) {
        valid |= SYNTIANT_CONFIG_PKG_VERSION_VALID;
    }
    
    s = syntiant_ndp10x_write(ndp, 1, config_address, valid);
    if (s) goto error;

    if (tag == TAG_FIRMWARE_VERSION_STRING_V1) {
        config_address =  NDP10X_ILIB_SCRATCH_ORIGIN +  
            offsetof(struct syntiant_ndp10x_config_layout, fw_version_size);
    } else if (tag == TAG_NN_VERSION_STRING_V1) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, params_version_size);
    } else if (tag == TAG_PACKAGE_VERSION_STRING) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, pkg_version_size);
    }
        
    s = syntiant_ndp10x_write(ndp, 1, config_address, len);
    if (s) goto error;

    if (tag == TAG_FIRMWARE_VERSION_STRING_V1) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, fw_version);
        ndp10x->fwver_len = len;
    } else if (tag == TAG_NN_VERSION_STRING_V1) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, params_version);
        ndp10x->paramver_len = len;
    } else if (tag == TAG_PACKAGE_VERSION_STRING) {
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, pkg_version);
        ndp10x->pkgver_len = len;
    }

    s = syntiant_ndp10x_write_block(ndp, 1, config_address, ver, (int) len);

 error:
    return s;
}

/**
 * @brief function to parse version strings
 * @param ndp device object
 */
int
syntiant_ndp10x_pkg_parse_version_string_v1_tlv
(struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t tag, len;
    uint8_t *ver;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    if (pstate->mode == PACKAGE_MODE_TAG_START && 
        pstate->partially_read_length == *(uint32_t*)pstate->length) { 

        tag = *(uint32_t*) pstate->tag;
        len = *(uint32_t*) pstate->length;
        if (tag == TAG_FIRMWARE_VERSION_STRING_V1) {
            ver = pstate->data.fw_version;
        } else if (tag == TAG_NN_VERSION_STRING_V1) {
            ver = pstate->data.params_version;
        } else if (tag == TAG_PACKAGE_VERSION_STRING) {
            ver = pstate->data.pkg_version;
        } else {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
        s = syntiant_ndp10x_pkg_parse_store_version(ndp, tag, ver, len);
        if (s) goto error;
    }
 error:
    return s;
}

static int
syntiant_ndp10x_pkg_core_parse_board_calibration_params_tlv(
    struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_config_s *ndp10x_config,
    int padded)
{
    int i = 0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint8_t *dnn_input = padded ?
                        &pstate->data.board_params.board_params_v2.dnn_input :
                        &pstate->data.board_params.board_params_v1.dnn_input;

    if (pstate->mode == PACKAGE_MODE_TAG_START &&
        pstate->partially_read_length == *(uint32_t*)pstate->length) {

        /* translate dnn inputs */
        switch (*dnn_input) {
        case SYNTIANT_PACKAGE_DNN_INPUT_NONE:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM0:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM1:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM_SUM:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_LEFT:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_RIGHT:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_SUM:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_MONO:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_DIRECT:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_SPI:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_SPI_DIRECT:
            *dnn_input = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT;
            break;

        default:
            s = SYNTIANT_NDP_ERROR_PACKAGE;
            goto error;
        }
        ndp10x_config->dnn_input = *dnn_input;

        ndp10x_config->input_clock_rate = padded ?
            pstate->data.board_params.board_params_v2.input_clock_rate:
            pstate->data.board_params.board_params_v1.input_clock_rate;

        ndp10x_config->pdm_clock_rate = padded ?
                pstate->data.board_params.board_params_v2.pdm_clock_rate:
                pstate->data.board_params.board_params_v1.pdm_clock_rate;

        for (i = 0 ; i < 2; i++){
            ndp10x_config->pdm_dc_offset[i] = (padded ?
                pstate->data.board_params.board_params_v2.pdm_dc_offset[i]:
                pstate->data.board_params.board_params_v1.pdm_dc_offset[i]);
        }

        ndp10x_config->pdm_clock_ndp = padded ?
            pstate->data.board_params.board_params_v2.pdm_clock_ndp:
            pstate->data.board_params.board_params_v1.pdm_clock_ndp;

        ndp10x_config->power_offset = padded ?
            pstate->data.board_params.board_params_v2.power_offset:
            pstate->data.board_params.board_params_v1.power_offset;

        ndp10x_config->preemphasis_exponent = padded ?
            pstate->data.board_params.board_params_v2.preemphasis_exponent:
            pstate->data.board_params.board_params_v1.preemphasis_exponent;

        for (i = 0 ; i < 2; i++){
            ndp10x_config->pdm_in_shift[i] = (padded ?
                pstate->data.board_params.board_params_v2.cal_pdm_in_shift[i]:
                pstate->data.board_params.board_params_v1.cal_pdm_in_shift[i]);
        }

        for (i = 0 ; i < 2; i++){
            ndp10x_config->pdm_out_shift[i] = (padded ?
                pstate->data.board_params.board_params_v2.cal_pdm_out_shift[i]:
                pstate->data.board_params.board_params_v1.cal_pdm_out_shift[i]);
        }

        ndp10x_config->power_scale_exponent = padded ?
            pstate->data.board_params.board_params_v2.power_scale_exponent:
            pstate->data.board_params.board_params_v1.power_scale_exponent;

        ndp10x_config->agc_on = padded ?
            pstate->data.board_params.board_params_v2.agc:
            pstate->data.board_params.board_params_v1.agc;

        for (i = 0 ; i < SYNTIANT_NDP10X_MAX_FREQUENCY_BINS ; i++){
            ndp10x_config->filter_eq[i] = padded ?
                pstate->data.board_params.board_params_v2.equalizer[i]:
                pstate->data.board_params.board_params_v1.equalizer[i];
        }

        ndp10x_config->set = (SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT |
            SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE |
            SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE |
            SYNTIANT_NDP10X_CONFIG_SET_PDM_DC_OFFSET |
            SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP |
            SYNTIANT_NDP10X_CONFIG_SET_POWER_OFFSET |
            SYNTIANT_NDP10X_CONFIG_SET_PREEMPHASIS_EXPONENT |
            SYNTIANT_NDP10X_CONFIG_SET_PDM_IN_SHIFT |
            SYNTIANT_NDP10X_CONFIG_SET_PDM_OUT_SHIFT |
            SYNTIANT_NDP10X_CONFIG_SET_POWER_SCALE_EXPONENT);
        ndp10x_config->set1 = SYNTIANT_NDP10X_CONFIG_SET1_FILTER_EQ;
    }
error:
    return s;
}

static int
syntiant_ndp10x_core_pkg_parse_board_calibration_params_tlv(
                                struct syntiant_ndp_device_s *ndp,
                                struct syntiant_ndp10x_config_s *ndp10x_config)
{
    int i;
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    s = syntiant_ndp10x_pkg_core_parse_board_calibration_params_tlv(ndp,
                                                    ndp10x_config, 1);
    if (s) goto error;
    if (ndp10x_config->agc_on) {
        ndp10x_config->set |= SYNTIANT_NDP10X_CONFIG_SET_AGC_ON;
        ndp10x_config->agc_nom_speech_quiet =
            pstate->data.board_params.board_params_v3.agc_ref_lvl;
        for (i = 0; i < 2; i++) {
            ndp10x_config->agc_max_adj[i] =
                pstate->data.board_params.board_params_v3.agc_max_adj[i];
        }
        ndp10x_config->set1 |=
            (SYNTIANT_NDP10X_CONFIG_SET1_AGC_MAX_ADJ |
             SYNTIANT_NDP10X_CONFIG_SET1_AGC_NOM_SPEECH_TARGET);
    }
error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_board_calibration_params_v3_tlv(
                        struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_config_s ndp10x_config;

    memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));

    s = syntiant_ndp10x_core_pkg_parse_board_calibration_params_tlv(ndp,
            &ndp10x_config);
    if (s) goto error;
    s = syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_board_calibration_params_v4_tlv(
                        struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_config_s ndp10x_config;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));

    s = syntiant_ndp10x_core_pkg_parse_board_calibration_params_tlv(ndp,
            &ndp10x_config);
    if (s) goto error;
    ndp10x_config.set1 |= SYNTIANT_NDP10X_CONFIG_SET1_NOISE_THRESHOLD;
    ndp10x_config.noise_threshold =
                    pstate->data.board_params.board_params_v4.noise_threshold;
    ndp10x_config.set1 |= SYNTIANT_NDP10X_CONFIG_SET1_NOISE_THRESHOLD_WIN;
    ndp10x_config.noise_thresh_win =
            pstate->data.board_params.board_params_v4.noise_thresh_win;

    s = syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
error:
    return s;
}


int
syntiant_ndp10x_pkg_parse_board_calibration_params_v1_v2_tlv(
    struct syntiant_ndp_device_s *ndp, int padded){
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_config_s ndp10x_config;
    memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));

    s = syntiant_ndp10x_pkg_core_parse_board_calibration_params_tlv(ndp,
            &ndp10x_config,padded);
    if (s) goto error;
    s = syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
    if (s) goto error;
error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_hw_config_v2_tlv(
    struct syntiant_ndp_device_s *ndp){
    uint32_t freqctl, dnnctl0, dnnctl7;
    int i = 0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t address;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    if (pstate->mode == PACKAGE_MODE_TAG_START &&
        pstate->partially_read_length == *(uint32_t*)pstate->length){

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL, &freqctl);
        if (s)
            goto error;
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_RSTB_MASK_INSERT(freqctl, 1);

        freqctl = NDP10X_DSP_CONFIG_FREQCTL_FRAMESTEP_MASK_INSERT(
        freqctl, pstate->data.hw_params.frame_step);
        freqctl = NDP10X_DSP_CONFIG_FREQCTL_NUMFILT_MASK_INSERT(
        freqctl, pstate->data.hw_params.num_filter_banks);


        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, &dnnctl0);
        if (s)
            goto error;

        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_MEMACCESS_MASK_INSERT(dnnctl0, 0);
        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_NUMLAYERS_MASK_INSERT
            (dnnctl0, 4);
        /* must be the same as freqctl.numflit */
        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_NUMFILT_MASK_INSERT
            (dnnctl0, pstate->data.hw_params.num_filter_banks);

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, dnnctl0);
        if (s)
            goto error;

        address = NDP10X_DSP_CONFIG_MELBINS0;
        for (i = 0; i < (int) pstate->data.hw_params.mel_bins_length;
             i += 3) {
            uint32_t melbin = NDP10X_DSP_CONFIG_MELBINS0_FILTBIN00(
                                  pstate->data.hw_params.mel_bins[i])
                | NDP10X_DSP_CONFIG_MELBINS0_FILTBIN01
                (pstate->data.hw_params.mel_bins[i + 1])
                | NDP10X_DSP_CONFIG_MELBINS0_FILTBIN02
                (pstate->data.hw_params.mel_bins[i + 2]);
            s = syntiant_ndp10x_write(ndp, 1, address, melbin);
            if (s) goto error;
            address += 4;
        }

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL5, 0);
        if (s) goto error;
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL6, 0);
        if (s) goto error;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL7, &dnnctl7);
        if (s)
            goto error;

        ndp10x->classes = pstate->data.hw_params.num_activations_used;
        ndp10x->audio_frame_step = pstate->data.hw_params.frame_step;
        ndp10x->freq_frame_size = pstate->data.hw_params.num_filter_banks;

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, dnnctl0);
        if (s)
            goto error;
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL, freqctl);
        if (s) goto error;
    }
    
 error:
    return s;
}

int 
syntiant_ndp10x_pkg_parse_nn_labels_v1_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    uint32_t valid;
    uint32_t offset;
    int size;
    uint32_t config_address;

    if(pstate->mode == PACKAGE_MODE_VALUE_START){
        /*set valid bit and write length into config region*/
        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, valid);
        s = syntiant_ndp10x_read(ndp, 1, config_address, &valid);
        if (s) goto error;
        valid = valid | SYNTIANT_CONFIG_LABELS_VALID;
        s = syntiant_ndp10x_write(ndp, 1, config_address, valid);
        if (s) goto error;

        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, label_size);
        s = syntiant_ndp10x_write(ndp, 1,
        config_address, *(uint32_t*)pstate->length);
        if (s) goto error;
        ndp10x->labels_len = *(uint32_t*)pstate->length;
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE){
        offset = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
        pstate->partially_read_length -
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            pstate->partially_read_length - PARSER_SCRATCH_SIZE;

        size = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            PARSER_SCRATCH_SIZE;

        config_address = NDP10X_ILIB_SCRATCH_ORIGIN + 
            offsetof(struct syntiant_ndp10x_config_layout, labels);
        s = syntiant_ndp10x_write_block(ndp, 1, 
            config_address + offset, 
            pstate->data.labels, size);
        goto error;
    }

 error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_firmware_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    const int mbout_timeout = 8000;
    uint32_t offset;
    int size, i;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint32_t tank, tank_enable_field;
    uint8_t mbout;
    uint8_t ctl = 0;
    
    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        uint32_t clkctl0 = 0;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_TANK, &tank);
        if (s) goto error;

        ndp10x->tank = tank;
        tank_enable_field = NDP10X_DSP_CONFIG_TANK_ENABLE_EXTRACT(tank);
        if (tank_enable_field) {
            tank = NDP10X_DSP_CONFIG_TANK_ENABLE_MASK_INSERT(tank, 0);
            s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_TANK, tank);
            if (s) goto error;
        }

        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBIN, 0x0);
        if (s) goto error;
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_MBOUT_RESP, 0x0);
        if (s) goto error;
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, 0x7f);
        if (s) goto error;
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_INTSTS, 0x0);
        if (s) goto error;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0, &clkctl0);
        if (s) goto error;

        clkctl0 = NDP10X_CHIP_CONFIG_CLKCTL0_MCURSTB_MASK_INSERT(clkctl0, 0x0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0, clkctl0);
        if (s) goto error;

        s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_CTL, &ctl);
        if (s) goto error;
        ctl = (uint8_t) NDP10X_SPI_CTL_BOOTDISABLE_MASK_INSERT(ctl, 0);
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, ctl);
        if (s) goto error;

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_SYSCTL_MEMCTRL, 0);
        if (s) goto error;

        /* set mbout != test for mbout == 0 below indicates firmware is ready */
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_MBOXOUT, 0x91);
        if (s) goto error;
        
        syntiant_ndp10x_reset_firmware_state(ndp);
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        offset = (pstate->partially_read_length % PARSER_SCRATCH_SIZE) ?
        (pstate->partially_read_length -
        (pstate->partially_read_length % PARSER_SCRATCH_SIZE)):
        (pstate->partially_read_length - PARSER_SCRATCH_SIZE);

        size = (pstate->partially_read_length % PARSER_SCRATCH_SIZE) ?
        (pstate->partially_read_length % PARSER_SCRATCH_SIZE):
        PARSER_SCRATCH_SIZE;

        s = syntiant_ndp10x_write_block(
            ndp, 1, NDP10X_BOOTRAM_REMAP + offset, pstate->data.fw, size);
    }
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        uint32_t clkctl0 = 0;
        /*TODO: we may add a call to syntiant_ndp10x_verify_tags_in_config*/

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0, &clkctl0);
        if (s) goto error;

        clkctl0 = NDP10X_CHIP_CONFIG_CLKCTL0_MCURSTB_MASK_INSERT(clkctl0, 0x1);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_CHIP_CONFIG_CLKCTL0, clkctl0);
        if (s) goto error;


        s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_CTL, &ctl);
        if (s) goto error;

        ctl = (uint8_t) NDP10X_SPI_CTL_BOOTDISABLE_MASK_INSERT(ctl, 0);
        s = syntiant_ndp10x_write(ndp, 0, NDP10X_SPI_CTL, ctl);
        if (s) goto error;

        /* wait for firmware to clear mbout */
        for (i = 0; i < mbout_timeout; i++) {
            s = syntiant_ndp10x_read(ndp, 0, NDP10X_SPI_MBOUT, &mbout);
            if (s) {
                goto error;
            }
            if (!mbout) {
                break;
            }
        }
        if (i == mbout_timeout) {
            s = SYNTIANT_NDP_ERROR_TIMEOUT;
            goto error;
        }

        s = syntiant_ndp10x_do_mailbox_req_no_sync
            (ndp, SYNTIANT_NDP10X_MB_REQUEST_MIADDR);
        if (s != SYNTIANT_NDP_ERROR_NONE
            && s != SYNTIANT_NDP_ERROR_FAIL) {
            goto error;
        }
        ndp10x->fw_loaded = 1;
        
        s = syntiant_ndp10x_load_fw_pointers(ndp);
        if (s) {
            goto error;
        }

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_TANKADDR,
                                  ndp10x->tank_address);
        if (s) goto error;

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_TANK, ndp10x->tank);
        if (s) goto error;
    }

 error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_b0_nn_config_v2_v3_tlv(
    struct syntiant_ndp_device_s *ndp, int aligned_value){
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    uint32_t num_nodes, freqctl, freqctl0, dnnctl00, dnnctl0, dnnctl1, dnnctl2;
    uint32_t num_biases;
    uint32_t dnnctl3, dnnctl80, dnnctl8, i;
    struct syntiant_ndp10x_config_s ndp10x_config;
    uint32_t tmp_32;
    uint8_t tmp_8_p[4];

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL, &freqctl);
        if (s)
            goto error;

        /* leave the frequency block in reset until DNN parameters are loaded */
        freqctl0 = NDP10X_DSP_CONFIG_FREQCTL_RSTB_MASK_INSERT(freqctl, 0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DSP_CONFIG_FREQCTL, freqctl0);
        if (s)
            goto error;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, &dnnctl0);
        if (s)
            goto error;

        dnnctl00 = NDP10X_DNN_CONFIG_DNNCTL0_RSTB_MASK_INSERT(dnnctl0, 0);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, dnnctl00);
        if (s)
            goto error;
        dnnctl00 = NDP10X_DNN_CONFIG_DNNCTL0_RSTB_MASK_INSERT(dnnctl00, 1);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, dnnctl00);
        if (s)
            goto error;

        dnnctl00 = NDP10X_DNN_CONFIG_DNNCTL0_MEMACCESS_MASK_INSERT(dnnctl00, 1);
        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL0, dnnctl00);
        if (s)
            goto error;
        /* It is assumed that number of layers is 4 in NDP10X-b0 devices*/
        dnnctl0 = NDP10X_DNN_CONFIG_DNNCTL0_NUMLAYERS_MASK_INSERT
            (dnnctl0, 4);
    }
    /* fc params */
    else if(pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE &&
        (aligned_value ?
            pstate->partially_read_length == sizeof(syntiant_fc_params_v3_t) :
            pstate->partially_read_length == sizeof(syntiant_fc_params_v2_t))){

        /* sets dnn frame size,
        number of input features, and number of static features*/
        memset(&ndp10x_config, 0, sizeof(struct syntiant_ndp10x_config_s));
        ndp10x_config.set = ( SYNTIANT_NDP10X_CONFIG_SET_DNN_FRAME_SIZE |
            SYNTIANT_NDP10X_CONFIG_SET_DNN_MINIMUM_THRESHOLD |
            SYNTIANT_NDP10X_CONFIG_SET_DNN_RUN_THRESHOLD |
            SYNTIANT_NDP10X_CONFIG_SET_DNN_STATIC_INPUTS |
            SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUTS);

        ndp10x_config.dnn_frame_size = aligned_value ?
                pstate->data.fc_params.fc_params_v3.dnn_frame_size:
                pstate->data.fc_params.fc_params_v2.dnn_frame_size;

        ndp10x_config.dnn_minimum_threshold = aligned_value ?
                pstate->data.fc_params.fc_params_v3.input_clipping_threshold:
                pstate->data.fc_params.fc_params_v2.input_clipping_threshold;
        ndp10x_config.dnn_run_threshold = aligned_value ?
                pstate->data.fc_params.fc_params_v3.dnn_run_threshold:
                pstate->data.fc_params.fc_params_v2.dnn_run_threshold;
        ndp10x_config.dnn_static_inputs = aligned_value ?
            pstate->data.fc_params.fc_params_v3.num_static_features:
            pstate->data.fc_params.fc_params_v2.num_static_features;
        ndp10x_config.dnn_inputs = aligned_value ?
            pstate->data.fc_params.fc_params_v3.num_features:
            pstate->data.fc_params.fc_params_v2.num_features;

        s = syntiant_ndp10x_config_no_sync(ndp, &ndp10x_config);
        if (s) goto error;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL1, &dnnctl1);
        if (s)
            goto error;

        /* layer 0: number of nodes */ 
        /* increased by 1 to map 0:255 to 1:256*/
        num_nodes = aligned_value ?
                        (pstate->data.fc_params.fc_params_v3.num_nodes[0]+1U):
                        (pstate->data.fc_params.fc_params_v2.num_nodes[0]+1U);
        dnnctl1 = NDP10X_DNN_CONFIG_DNNCTL1_NUMLAYER0_MASK_INSERT(
                                                          dnnctl1, num_nodes);

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL2, &dnnctl2);
        if (s) goto error;

        num_biases = aligned_value ?
                            pstate->data.fc_params.fc_params_v3.num_biases[0]:
                            pstate->data.fc_params.fc_params_v2.num_biases[0];
        dnnctl2 = NDP10X_DNN_CONFIG_DNNCTL2_NUMBIAS0_MASK_INSERT(
                            dnnctl2, num_biases);

        num_nodes = aligned_value ?
                    (pstate->data.fc_params.fc_params_v3.num_nodes[1]+1U):
                    (pstate->data.fc_params.fc_params_v2.num_nodes[1]+1U);
        dnnctl2 = NDP10X_DNN_CONFIG_DNNCTL2_NUMLAYER1_MASK_INSERT(
            dnnctl2, num_nodes);

        num_biases = aligned_value ?
                        pstate->data.fc_params.fc_params_v3.num_biases[1]:
                        pstate->data.fc_params.fc_params_v2.num_biases[1];
        dnnctl2 = NDP10X_DNN_CONFIG_DNNCTL2_NUMBIAS1_MASK_INSERT(
            dnnctl2, num_biases);

        num_nodes = aligned_value ?
                    (pstate->data.fc_params.fc_params_v3.num_nodes[2]+1U):
                    (pstate->data.fc_params.fc_params_v2.num_nodes[2]+1U);
        dnnctl2 = NDP10X_DNN_CONFIG_DNNCTL2_NUMLAYER2_MASK_INSERT(
            dnnctl2, num_nodes);

        num_biases = aligned_value ?
                    pstate->data.fc_params.fc_params_v3.num_biases[2]:
                    pstate->data.fc_params.fc_params_v2.num_biases[2];
        dnnctl2 = NDP10X_DNN_CONFIG_DNNCTL2_NUMBIAS2_MASK_INSERT(
            dnnctl2, num_biases);

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL2, dnnctl2);
        if (s) goto error;

        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL3, &dnnctl3);
        if (s) goto error;

        num_nodes = aligned_value ?
                    pstate->data.fc_params.fc_params_v3.num_nodes[3]+1U:
                    pstate->data.fc_params.fc_params_v2.num_nodes[3]+1U;
        dnnctl3 = NDP10X_DNN_CONFIG_DNNCTL3_NUMLAYER3_MASK_INSERT(
            dnnctl3, num_nodes);

        num_biases = aligned_value ?
                    pstate->data.fc_params.fc_params_v3.num_biases[3]:
                    pstate->data.fc_params.fc_params_v2.num_biases[3];
        dnnctl3 = NDP10X_DNN_CONFIG_DNNCTL3_NUMBIAS3_MASK_INSERT(
            dnnctl3, num_biases);

        s = syntiant_ndp10x_write(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL3, dnnctl3);
        if (s) goto error;

        tmp_32 = aligned_value ?
                *(uint32_t* )pstate->data.fc_params.fc_params_v3.scale_factor:
                *(uint32_t* )pstate->data.fc_params.fc_params_v2.scale_factor; 
        s = syntiant_ndp10x_write(
           ndp, 1, NDP10X_DNN_CONFIG_DNNCTL4, tmp_32);
        if (s) goto error;
        s = syntiant_ndp10x_read(ndp, 1, NDP10X_DNN_CONFIG_DNNCTL8, &dnnctl80);
        if (s) goto error;

        dnnctl8 = dnnctl80;
        for (i = 0 ; i < 4; i++) {
            tmp_8_p[i] = aligned_value ?
                    pstate->data.fc_params.fc_params_v3.quantization_scheme[i]:
                    pstate->data.fc_params.fc_params_v2.quantization_scheme[i];
        }
        for (i = 0; i < 4; i++) {
            dnnctl8 = NDP10X_DNN_CONFIG_DNNCTL8_EXPANDWEIGHTS_MASK_INSERT
                (dnnctl8, i, (unsigned int) tmp_8_p[i]);
        }

        for (i = 0 ; i < 4; i++) {
            tmp_8_p[i] = aligned_value ?
                            pstate->data.fc_params.fc_params_v3.out_shift[i]:
                            pstate->data.fc_params.fc_params_v2.out_shift[i];
        }
        for (i = 0; i < 4; i++) {
            dnnctl8 = NDP10X_DNN_CONFIG_DNNCTL8_BIASSHIFT_MASK_INSERT
                (dnnctl8, i, (unsigned int) tmp_8_p[i]);
        }
        if (dnnctl8 != dnnctl80) {
            s = syntiant_ndp10x_write(
                ndp, 1, NDP10X_DNN_CONFIG_DNNCTL8, dnnctl8);
            if (s) goto error;
        }
    }
    
 error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_nn_params_encrypted_v1_tlv
(struct syntiant_ndp_device_s *ndp)
{
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    const uint32_t smap_data_address =
        NDP10X_FW_SMAP_BASE + NDP10X_FW_SMAP_CONTROL_SIZE;
    int s = SYNTIANT_NDP_ERROR_NONE;
    int first;
    unsigned int size, offset, burst, max, adj;
    uint32_t tl[2];
    
    if (pstate->value_mode != PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        goto out;
    }
    
    adj = 0;
    first = !ndp10x->load_offset;
    if (first) {
        s = syntiant_ndp10x_run_smap_op
            (ndp, (unsigned int) NDP10X_FW_SMAP_CONTROL_OP_LOAD, 0, 0);
        if (s != SYNTIANT_NDP_ERROR_MORE) {
            if (s == SYNTIANT_NDP_ERROR_NONE) {
                s = SYNTIANT_NDP_ERROR_FAIL;
            }
            goto out;
        }
        tl[0] = TAG_NDP10X_B0_NN_PARAMETERS_ENCRYPTED_V1;
        tl[1] = *(uint32_t *) pstate->length;
        adj = sizeof(tl);
        s = syntiant_ndp10x_write_block
            (ndp, 1, smap_data_address, (uint8_t *) tl, (int) adj);
    }
    
    size = pstate->partially_read_length % PARSER_SCRATCH_SIZE;
    size = size ? size : PARSER_SCRATCH_SIZE;

    offset = 0;
    while (offset < size) {
        max = NDP10X_FW_SMAP_DATA_SIZE - adj;
        burst = size - offset;
        burst = burst <= max ? burst : max;
        s = syntiant_ndp10x_write_block
            (ndp, 1, smap_data_address + adj,
             &pstate->data.packed_params[offset], (int) burst);
        if (s) {
            goto out;
        }
        s = syntiant_ndp10x_run_smap_op
            (ndp, (unsigned int) NDP10X_FW_SMAP_CONTROL_OP_LOAD, 0,
             burst + adj);
        if (s == SYNTIANT_NDP_ERROR_MORE) {
            s = SYNTIANT_NDP_ERROR_NONE;
        }
        if (s) {
            goto out;
        }
        offset += burst;
        adj = 0;
    }
    ndp10x->load_offset += size;
    
 out:
    return s;
}

int
syntiant_ndp10x_pkg_parse_nn_params_v3_tlv(
    struct syntiant_ndp_device_s *ndp){
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t dnn_address = NDP10X_DNNRAM;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint32_t offset = 0;
    int size = 0;
    
    if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        offset = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
            pstate->partially_read_length -
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            pstate->partially_read_length - PARSER_SCRATCH_SIZE;

        size = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            PARSER_SCRATCH_SIZE;

        s = syntiant_ndp10x_write_block(ndp, 1, dnn_address+offset,
                                        pstate->data.packed_params, size);
    }
    
    return s;
}

int
syntiant_ndp10x_pkg_parse_ph_params_v4_tlv(struct syntiant_ndp_device_s *ndp){
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_posterior_config_s ph_config;
    unsigned int cur_state, cur_class;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    memset(&ph_config, 0, sizeof(struct syntiant_ndp10x_posterior_config_s));
    /*set num of states and classes*/
    if (pstate->partially_read_length ==
        (sizeof(pstate->metadata.ph_metadata.v1) -
        sizeof(pstate->metadata.ph_metadata.v1.cur_state) -
        sizeof(pstate->metadata.ph_metadata.v1.cur_class))){
        
        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE;
        ph_config.states = pstate->metadata.ph_metadata.v1.num_states;
        ph_config.classes = pstate->metadata.ph_metadata.v1.num_classes;
        /* The ph_type for this version of ph_params defaults to 0, legacy ph */
        ph_config.ph_type = 0;
        
        /* for packages w/o hardware params TLV (e.g. mnist) */
        ndp10x->classes = pstate->metadata.ph_metadata.v1.num_classes;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE){
        cur_state = pstate->metadata.ph_metadata.v1.cur_state;
        cur_class = pstate->metadata.ph_metadata.v1.cur_class;
        if (cur_class > 0 &&
            cur_class <= pstate->metadata.ph_metadata.v1.num_classes){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE;
            ph_config.state = pstate->metadata.ph_metadata.v1.cur_state -1;
            ph_config.class_index = pstate->metadata.ph_metadata.v1.cur_class -1;

            ph_config.threshold = pstate->data.ph_params.v1.phth;
            ph_config.window = pstate->data.ph_params.v1.phwin;
            ph_config.backoff = pstate->data.ph_params.v1.phbackoff;
            ph_config.action_type = pstate->data.ph_params.v1.phaction;
            ph_config.smoothing_queue_size =
                                pstate->data.ph_params.v1.phqueuesize;
            switch (pstate->data.ph_params.v1.phaction) {
            case SYNTIANT_PH_ACTION_MATCH:
                ph_config.action_match = pstate->data.ph_params.v1.pharg;
                break;

            case SYNTIANT_PH_ACTION_STATE:
                ph_config.action_state = pstate->data.ph_params.v1.pharg;
                break;
            default:
                break;
            }
            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) {
                goto error;
            }
            
        }
        /*set state params*/
        else if(cur_state > 0 &&
            cur_state <= pstate->metadata.ph_metadata.v1.num_states){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT;
            ph_config.state = pstate->metadata.ph_metadata.v1.cur_state - 1;
            ph_config.timeout = pstate->metadata.ph_metadata.v1.timeout;
            ph_config.timeout_action_type
                = SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STATE;
            ph_config.timeout_action_match = 0;
            ph_config.timeout_action_state = 0;
            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
        }
    }
    
 error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_ph_params_v5_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_posterior_config_s ph_config;
    unsigned int cur_state, cur_class;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

#if 0
        printf("\nParser content:\n");
        printf("\tMETA_DATA\n");
        printf("\t\tnum_states: %d\n", pstate->metadata.ph_metadata.v2.num_states);
        printf("\t\tnum_classes: %d\n", pstate->metadata.ph_metadata.v2.num_classes);
        printf("\t\tposterior_type: %d\n", pstate->metadata.ph_metadata.v2.ph_type);
        printf("\t\ttimeout: %d\n", pstate->metadata.ph_metadata.v2.timeout);
        printf("\t\ttimeout_actions: %d\n", pstate->metadata.ph_metadata.v2.timeout_action);
        printf("\t\ttimeout_action_arg: %d\n", pstate->metadata.ph_metadata.v2.timeout_action_arg);
        printf("\t\tcur_state: %d\n", pstate->metadata.ph_metadata.v2.cur_state);
        printf("\t\tcur_class: %d\n", pstate->metadata.ph_metadata.v2.cur_class);
        printf("\tDATA\n");
        printf("\t\twin: %d\n", pstate->data.ph_params.v1.phwin);
        printf("\t\tthr: %d\n", pstate->data.ph_params.v1.phth);
        printf("\t\tboff: %d\n", pstate->data.ph_params.v1.phbackoff);
        printf("\t\taction: %d\n", pstate->data.ph_params.v1.phaction);
        printf("\t\targ: %d\n", pstate->data.ph_params.v1.pharg);
        printf("\t\tsmooth: %d\n", pstate->data.ph_params.v1.phqueuesize);
        printf("\tPARSER_STATS:\n");
        printf("\t\tparitally_read_len: %d\n", pstate->partially_read_length);
        printf("\t\tvalue_mode: %d\n", pstate->value_mode);
        printf("\nEND\n");
#endif

    memset(&ph_config, 0, sizeof(struct syntiant_ndp10x_posterior_config_s));
    /*set num of states and classes*/
    if ((pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) &&
        (pstate->partially_read_length ==
        (sizeof(pstate->metadata.ph_metadata.v2) -
         sizeof(pstate->metadata.ph_metadata.v2.cur_state) -
         sizeof(pstate->metadata.ph_metadata.v2.cur_class)))){

        /* setting the state and classes */
        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE;
        ph_config.states = pstate->metadata.ph_metadata.v2.num_states;
        ph_config.classes = pstate->metadata.ph_metadata.v2.num_classes;
        ph_config.ph_type = pstate->metadata.ph_metadata.v2.ph_type;

        /* for packages w/o hardware params TLV (e.g. mnist) */
        ndp10x->classes = pstate->metadata.ph_metadata.v2.num_classes;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;

        /* Set the first state params */
        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                     | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2;
        ph_config.timeout = pstate->metadata.ph_metadata.v2.timeout;
        ph_config.timeout_action_type =
            pstate->metadata.ph_metadata.v2.timeout_action;
        ph_config.timeout_action_arg =
            pstate->metadata.ph_metadata.v2.timeout_action_arg;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;

    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE){
        cur_state = pstate->metadata.ph_metadata.v2.cur_state;
        cur_class = pstate->metadata.ph_metadata.v2.cur_class;
        if (cur_class > 0 &&
            cur_class <= pstate->metadata.ph_metadata.v2.num_classes){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION_V2
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE;
            ph_config.state = pstate->metadata.ph_metadata.v2.cur_state -1;
            ph_config.class_index = pstate->metadata.ph_metadata.v2.cur_class -1;

            ph_config.threshold = pstate->data.ph_params.v1.phth;
            ph_config.window = pstate->data.ph_params.v1.phwin;
            ph_config.backoff = pstate->data.ph_params.v1.phbackoff;
            ph_config.action_type = pstate->data.ph_params.v1.phaction;
            ph_config.action_arg = pstate->data.ph_params.v1.pharg;
            ph_config.smoothing_queue_size = pstate->data.ph_params.v1.phqueuesize;

            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
        }
        /*set state params*/
        else if(cur_state > 0 &&
            cur_state <= pstate->metadata.ph_metadata.v2.num_states){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2;
            ph_config.state = pstate->metadata.ph_metadata.v2.cur_state - 1;
            ph_config.timeout = pstate->metadata.ph_metadata.v2.timeout;
            ph_config.timeout_action_type =
                pstate->metadata.ph_metadata.v2.timeout_action;
            ph_config.timeout_action_arg =
                pstate->metadata.ph_metadata.v2.timeout_action_arg;
            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
        }
    }

 error:
    return s;
}

int
syntiant_ndp10x_pkg_parse_ph_params_v6_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp10x_posterior_config_s ph_config;
    unsigned int cur_state, cur_class;
    struct syntiant_ndp10x_device_s *ndp10x = &ndp->d.ndp10x;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    memset(&ph_config, 0, sizeof(struct syntiant_ndp10x_posterior_config_s));
    /*set num of states and classes*/
    if ((pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) &&
        (pstate->partially_read_length ==
        (sizeof(pstate->metadata.ph_metadata.v3) -
         sizeof(pstate->metadata.ph_metadata.v3.cur_state) -
         sizeof(pstate->metadata.ph_metadata.v3.cur_class)))){

        /* setting the state and classes */
        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES
             | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE;
        ph_config.states = pstate->metadata.ph_metadata.v3.num_states;
        ph_config.classes = pstate->metadata.ph_metadata.v3.num_classes;
        ph_config.ph_type = pstate->metadata.ph_metadata.v3.ph_type;

        /* for packages w/o hardware params TLV (e.g. mnist) */
        ndp10x->classes = pstate->metadata.ph_metadata.v3.num_classes;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) {
            goto error;
        }

        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ADAPTIVE;
        ph_config.adaptive_frames =
                pstate->metadata.ph_metadata.v3.adaptive_frames;
        ph_config.adaptive_denominator =
                pstate->metadata.ph_metadata.v3.adaptive_denominator;
        ph_config.adaptive_max_updates =
                pstate->metadata.ph_metadata.v3.adaptive_max_updates;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) {
            goto error;
        }
        /* Set the first state params */
        ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                     | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2;
        ph_config.timeout = pstate->metadata.ph_metadata.v3.timeout;
        ph_config.timeout_action_type =
            pstate->metadata.ph_metadata.v3.timeout_action;
        ph_config.timeout_action_arg =
            pstate->metadata.ph_metadata.v3.timeout_action_arg;
        s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
        if (s) {
            goto error;
        }

    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE){
        cur_state = pstate->metadata.ph_metadata.v3.cur_state;
        cur_class = pstate->metadata.ph_metadata.v3.cur_class;
        if (cur_class > 0 &&
            cur_class <= pstate->metadata.ph_metadata.v3.num_classes){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE;
            if (pstate->metadata.ph_metadata.v3.ph_type == 0) {
                ph_config.set |= SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION;
                switch (pstate->data.ph_params.v2.phaction) {
                case SYNTIANT_PH_ACTION_MATCH:
                    ph_config.action_match = pstate->data.ph_params.v2.pharg;
                    break;

                case SYNTIANT_PH_ACTION_STATE:
                    ph_config.action_state = pstate->data.ph_params.v2.pharg;
                    break;
                default:
                    break;
                }
            } else {
                ph_config.set |= SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION_V2;
            }
            ph_config.state = pstate->metadata.ph_metadata.v3.cur_state -1;
            ph_config.class_index = pstate->metadata.ph_metadata.v3.cur_class -1;

            ph_config.threshold = pstate->data.ph_params.v2.phth;
            ph_config.window = pstate->data.ph_params.v2.phwin;
            ph_config.backoff = pstate->data.ph_params.v2.phbackoff;
            ph_config.action_type = pstate->data.ph_params.v2.phaction;
            ph_config.action_arg = pstate->data.ph_params.v2.pharg;
            ph_config.smoothing_queue_size = pstate->data.ph_params.v2.phqueuesize;
            ph_config.adaptive_threshold_on = pstate->data.ph_params.v2.adaptive_threshold_on;

            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) {
                goto error;
            }
        }
        /*set state params*/
        else if(cur_state > 0 &&
            cur_state <= pstate->metadata.ph_metadata.v3.num_states){
            ph_config.set = SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT
                    | SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2;
            ph_config.state = pstate->metadata.ph_metadata.v3.cur_state - 1;
            ph_config.timeout = pstate->metadata.ph_metadata.v3.timeout;
            ph_config.timeout_action_type =
                pstate->metadata.ph_metadata.v3.timeout_action;
            ph_config.timeout_action_arg =
                pstate->metadata.ph_metadata.v3.timeout_action_arg;
            s = syntiant_ndp10x_posterior_config_no_sync(ndp, &ph_config);
            if (s) {
                goto error;
            }
        }
    }

 error:
    return s;
}

int
syntiant_ndp10x_parse_tag_values(struct syntiant_ndp_device_s *ndp){
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    int aligned_value = 0;

    switch(*(uint32_t*)pstate->tag) {

    case TAG_FIRMWARE_VERSION_STRING_V1:
    case TAG_NN_VERSION_STRING_V1:
    case TAG_PACKAGE_VERSION_STRING:
        s = syntiant_ndp10x_pkg_parse_version_string_v1_tlv(ndp);
        break;

    case TAG_NDP10X_HW_CONFIG_V2:
        s = syntiant_ndp10x_pkg_parse_hw_config_v2_tlv(ndp);
        break;

    case TAG_BOARD_CALIBRATION_PARAMS_V2:
        aligned_value = 1;
        /* FALLTHRU */
    case TAG_BOARD_CALIBRATION_PARAMS_V1:
        s = syntiant_ndp10x_pkg_parse_board_calibration_params_v1_v2_tlv(ndp,
                aligned_value);
        break;
    case TAG_BOARD_CALIBRATION_PARAMS_V3:
        s = syntiant_ndp10x_pkg_parse_board_calibration_params_v3_tlv(ndp);
        break;

    case TAG_BOARD_CALIBRATION_PARAMS_V4:
        s = syntiant_ndp10x_pkg_parse_board_calibration_params_v4_tlv(ndp);
        break;

    case TAG_NN_LABELS_V1:
        s = syntiant_ndp10x_pkg_parse_nn_labels_v1_tlv(ndp);
        break;

    /*bellows are all partially stored*/
    case TAG_FIRMWARE:
        s = syntiant_ndp10x_pkg_parse_firmware_tlv(ndp);
        break;

    case TAG_NDP10X_B0_NN_CONFIG_V3:
        aligned_value = 1;
        /* FALLTHRU */
    case TAG_NDP10X_B0_NN_CONFIG_V2:
        s = syntiant_ndp10x_pkg_parse_b0_nn_config_v2_v3_tlv(ndp,
            aligned_value);
        break;

    case TAG_NDP10X_NN_PARAMETERS_V3:
        s = syntiant_ndp10x_pkg_parse_nn_params_v3_tlv(ndp);
        break;

    case TAG_NN_PH_PARAMETERS_V4:
        s = syntiant_ndp10x_pkg_parse_ph_params_v4_tlv(ndp);
        break;

    case TAG_NN_PH_PARAMETERS_V5:
        s = syntiant_ndp10x_pkg_parse_ph_params_v5_tlv(ndp);
        break;

    case TAG_NN_PH_PARAMETERS_V6:
        s = syntiant_ndp10x_pkg_parse_ph_params_v6_tlv(ndp);
        break;

    case TAG_NDP10X_B0_NN_PARAMETERS_ENCRYPTED_V1:
        s = syntiant_ndp10x_pkg_parse_nn_params_encrypted_v1_tlv(ndp);
        break;

    default:
        break;
    }

    return s;
}
