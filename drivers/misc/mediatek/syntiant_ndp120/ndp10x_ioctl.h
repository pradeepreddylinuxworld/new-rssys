/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2019-2020 Syntiant Corporation
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
#ifndef NDP10X_IOCTL_H
#define NDP10X_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * INIT mode:
 * _OVERRIDE -> reinitialize ignoring any previous initialization
 * _NO_OVERRIDE -> return EINVAL if already initialized
 * _UNINIT -> uninitialize, return EINVAL if not already initialized
 */
#define NDP10X_IOCTL_INIT_OVERRIDE 0
#define NDP10X_IOCTL_INIT_NO_OVERRIDE 1
#define NDP10X_IOCTL_INIT_UNINIT 2

/* parallel to struct syntiant_ndp_config_s */
/* lengths are in/out parameters */
struct ndp_config_s {
    __u64 device_type;            /**< device type identifier string */
    __u64 firmware_version;       /**< firmware version string */
    __u64 parameters_version;     /**< parameters version string */
    __u64 labels;                 /**< class labels strings array.  Each
                                       label is a C (NUL-terminated) string
                                       followed immediately by the next label.
                                       The label strings will be 0 length if
                                       label strings are not available */
    __u64 pkg_version;            /**< package version string */
    __u64 dsp_firmware_version;   /**< dsp FW version string */
    unsigned int device_type_len;
    unsigned int classes;         /**< number of active classes */
    unsigned int firmware_version_len;
    /**< length of supplied firmware version string */
    unsigned int parameters_version_len;
    /**< length of supplied parameters version string */
    unsigned int labels_len;      /**< length of supplied labels string array */
    unsigned int pkg_version_len; /**< length of supplied package version str */
    unsigned int dsp_firmware_version_len; /**< length of supplied DSP FW version str */
};

struct ndp_load_s {
    __u64 package;
    unsigned int length;
    int error_code;
};


struct ndp_timespec_s {
    uint64_t tv_sec;        /* seconds */
    uint64_t tv_nsec;       /* nanoseconds */
};

struct ndp_watch_s {
    uint64_t classes;    /**< bit map of classes on which to report a match */
    int timeout;         /**< seconds -- < 0 -> wait indefinitely */
    int flush;           /**< discard outstanding watch events
                              if extract_match_mode != 0, also reenable
                              background audio collection */
    int match;           /**< set if a match is detected */
    int class_index;     /**< match index if a match is detected */
    unsigned int info;   /**< additional information if a match is detected */
    int extract_match_mode;    /**< when watching in this mode, driver stops
                                    collecting audio in the background.
                                    if return status is EINTR, then
                                    the driver will not resume collecting
                                    audio until flush && extract_match_mode */
    int extract_before_match;  /**< ms of audio to extract before match.
                                    Will return amount of audio available for
                                    extraction */
    struct ndp_timespec_s ts;
    int32_t noise_data;  /**< noise level at the time of reporting */
    uint8_t noise_level; /**< noise level reported from device */
    uint8_t sensor;  /**< 0 for ww, 1 for sensors */
};

struct ndp_transfer_s {
    __u64 out;
    __u64 in;
    int mcu;
    uint32_t addr;
    unsigned int count;
};

struct ndp_driver_config_s {
    unsigned int spi_speed;
    unsigned int spi_read_pad_bytes;
    int spi_split_flag;
    uint32_t spi_send_speed;
    unsigned int extract_ring_size;   /* MS, default 10000 */
    unsigned int send_ring_size;      /* MS, default 100 */
    unsigned int result_per_frame;
};

struct ndp_pcm_extract_s {
    __u64 buffer;
    unsigned int buffer_length;
    int nonblock;       /**< don't block for the expected data length but
                             return immediately with whatever portion is
                             available.  nonblock must be set, when unbuffered
                             is set */
    int flush;
    int unbuffered;     /**< extract directly from NDP (vs driver buffer) */
    unsigned int type;  /**< SYNTIANT_NDP_EXTRACT_TYPE_* type.  buffered mode
                           only supports SYNTIANT_NDP_EXTRACT_TYPE_INPUT */
    unsigned int from;  /**< SYNTIANT_NDP_EXTRACT_FROM_* data extraction range
                             ignored for buffered mode (which is governed by
                             armed vs disarmed mode, etc...) */
    unsigned int extracted_length;
    unsigned int remaining_length;
};

struct ndp_pcm_send_s {
    __u64 buffer;
    unsigned int buffer_length;
    int nonblock;
    unsigned int sent_length;
};

struct syn_ndp_statistics_s {
    uint64_t isrs;
    uint64_t polls;
    uint64_t frames;
    uint64_t results;
    uint64_t results_dropped;
    uint64_t extracts;
    uint64_t extract_bytes;
    uint64_t extract_bytes_dropped;
    uint64_t sends;
    uint64_t send_bytes;
    unsigned int send_ring_used;
    unsigned int extract_ring_used;
    unsigned int result_ring_used;
    int clear;
};

struct ndp10x_serial_s {
    unsigned int ifc_type;
    unsigned int ifc_addr;
    unsigned int outlen;
    unsigned int inlen;
    __u64 out;
    __u64 in;
    int continue_;
    int muted_;
};

#define RESULT_MAX_CLASSES 64
#define RESULT_MAX_FEATURES 40

#define INIT _IOR('a', 'a', int)
#define NDP10X_CONFIG _IOR('a', 'b', __u64)
#define NDP_CONFIG _IOR('a', 'c', __u64)
#define LOAD _IOR('a', 'd', __u64)
#define TRANSFER _IOR('a', 'e', __u64)
#define WATCH _IOR('a', 'f', __u64)
#define DRIVER_CONFIG _IOR('a', 'g', __u64)
#define PCM_EXTRACT _IOR('a', 'h', __u64)
#define PCM_SEND _IOR('a', 'i', __u64)
#define STATS _IOR('a', 'j', __u64)
#define SERIAL _IOR('a', 'k', __u64)
#define GPIO _IOR('a', 'l', __u64)

#ifdef __cplusplus
}
#endif

#endif
