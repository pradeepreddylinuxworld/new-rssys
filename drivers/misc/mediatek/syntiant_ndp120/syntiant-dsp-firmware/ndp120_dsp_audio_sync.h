/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018-2021 Syntiant Corporation
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

#ifndef NDP120_DSP_AUDIO_SYNC_H_
#define NDP120_DSP_AUDIO_SYNC_H_

#ifndef __KERNEL__
#include <stdint.h>
#endif

typedef enum {
 NDP120_DSP_AUDIO_SYNC_AUD0 = 0,
 NDP120_DSP_AUDIO_SYNC_AUD1 = 1,
 NDP120_DSP_AUDIO_SYNC_AUD2 = 2
} ndp120_dsp_audio_sync_channel_t;

typedef struct {
    int enable;
    int started;
    ndp120_dsp_audio_sync_channel_t ref_chan;
    ndp120_dsp_audio_sync_channel_t adj_chan;
    int sample_count_offset;
} ndp120_dsp_audio_sync_config_t;
#endif
