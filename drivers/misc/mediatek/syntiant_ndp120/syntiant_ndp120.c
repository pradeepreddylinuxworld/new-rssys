/*
 * SYNTIANT CONFIDENTIAL
 * _____________________
 *
 *   Copyright (c) 2018 Syntiant Corporation
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
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>
#include <syntiant_ilib/syntiant_ndp_error.h>

#include <syntiant-dsp-firmware/ndp120_dsp_fw_state.h>
#include <syntiant-dsp-firmware/ndp120_dsp_sample.h>
#include <syntiant-dsp-firmware/ndp120_dsp_mailbox.h>
#include <syntiant-dsp-firmware/ndp120_dsp_mailbox.h>
#include <syntiant_ilib/syntiant_ndp120.h>
#include <syntiant_ilib/ndp120_regs.h>
#include <syntiant_ilib/ndp120_spi_regs.h>
#include <syntiant_ilib/syntiant_ndp120_driver.h>
#include <syntiant_ilib/syntiant_ndp120_mailbox.h>

#include <syntiant_packager/syntiant_package_consts.h>

#include <syntiant-firmware/ndp120_ph.h>
#include <syntiant-firmware/ndp120_firmware.h>
#include <syntiant-firmware/ndp120_result.h>
#include <syntiant-firmware/ndp120_dnn_regs.h>
#ifndef __KERNEL__
#include <unistd.h>
#include <sys/time.h>
#endif
#include "syntiant_ndp120_ph.h"

/* Convenience macros for operations on timevals.
NOTE: `timercmp' does not work for >= or <=.  */
#ifndef timerisset
# define timerisset(tvp)    ((tvp)->tv_sec || (tvp)->tv_usec)
#endif
#ifndef timerclear
# define timerclear(tvp)    ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif
#ifndef timercmp
# define timercmp(a, b, CMP)                                \
  (((a)->tv_sec == (b)->tv_sec) ?                           \
   ((a)->tv_usec CMP (b)->tv_usec) :                        \
   ((a)->tv_sec CMP (b)->tv_sec))
#endif
#ifndef timeradd
# define timeradd(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;        \
    while ((result)->tv_usec >= 1000000)                    \
      {                                                     \
        ++(result)->tv_sec;                                 \
        (result)->tv_usec -= 1000000;                       \
      }                                                     \
  } while (0)
#endif

#ifndef timersub
# define timersub(a, b, result)                             \
  do {                                                      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;           \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;        \
    if ((result)->tv_usec < 0) {                            \
      --(result)->tv_sec;                                   \
      (result)->tv_usec += 1000000;                         \
    }                                                       \
  } while (0)
#endif

#if NDP120_DEBUG
int DEBUG_PRINTF(const char * fmt, ...){
    int ret;
    va_list args;
    va_start(args, fmt);
    #ifdef __KERNEL__
    ret = vprintk(fmt, args);
    #else
    ret = vfprintf(stderr, fmt, args) + fprintf(stderr, "\n");
    #endif
    va_end(args);
    return ret;
} 
#else
int DEBUG_PRINTF(const char * fmt, ...){ (void)fmt; return 0; }
#endif

static ndp120_debug_callback_ptr_t ndp120_debug_callback = NULL;

void syntiant_ndp120_set_debug_callback(ndp120_debug_callback_ptr_t p) {
    ndp120_debug_callback = p;
}

int syntiant_ndp120_mailbox_trace = 0;
uint32_t syntiant_ndp120_mailbox_req_number[2] = { 0, 0 };
uint32_t syntiant_ndp120_mailbox_rsp_number[2] = { 0, 0 };
uint32_t syntiant_ndp120_mailbox_unexpected[2] = { 0, 0 };
uint32_t syntiant_ndp120_mailbox_error[2] = { 0, 0 };

unsigned int syntiant_ndp120_device_types[] = {
    0x30, 0x31, 0x32, 0x33, /* QFN48  NDP120A0 Samples */
    0x34, 0x35, 0x36, 0x37, /* QFN48  NDP120B0         */
    0x38, 0x39, 0x3A, 0x3B, /* WLBA42 NDP120B0         */
    0x0  /* len of list */
};

/* typedefs */

/* pre-declare */

#define OP_SIZE(mcu) (mcu ? 4 : 1)
#define roundUp4(x) (((x) + 3) / 4 * 4)

static uint8_t get_mbin_seq(struct syntiant_ndp_device_s *ndp);
static void mailbox_reset_state(struct syntiant_ndp_device_s *ndp);

/* helper functions */ 

int syntiant_ndp120_read(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value) {
    return syntiant_ndp120_read_block(ndp, mcu, address, value, OP_SIZE(mcu));
}

int
syntiant_ndp120_write(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, uint32_t value) {
    return syntiant_ndp120_write_block(ndp, mcu, address, &value, OP_SIZE(mcu));
}

static uint32_t umin(uint32_t a, uint32_t b) {
    return a < b ? a : b;
}


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define SPI 0
#define MCU 1

#define ndp_spi_read_block(reg, data, len)                                     \
    syntiant_ndp120_read_block(ndp, SPI, reg, data, len)
#define ndp_mcu_read_block(reg, data, len)                                     \
    syntiant_ndp120_read_block(ndp, MCU, reg, data, len)
#define ndp_spi_write_block(reg, data, len)                                    \
    syntiant_ndp120_write_block(ndp, SPI, reg, data, len)
#define ndp_mcu_write_block(reg, data, len)                                    \
    syntiant_ndp120_write_block(ndp, MCU, reg, data, len)

#define ndp_spi_read(reg, data) syntiant_ndp120_read(ndp, SPI, reg, data)
#define ndp_mcu_read(reg, data) syntiant_ndp120_read(ndp, MCU, reg, data)
#define ndp_spi_write(reg, data) syntiant_ndp120_write(ndp, SPI, reg, data)
#define ndp_mcu_write(reg, data) syntiant_ndp120_write(ndp, MCU, reg, data)

enum {

    MB_STATE_NONE = 0,
    MB_STATE_SYNC = 1,
    MB_STATE_DATA = 2
};

static uint8_t get_mbin_seq(struct syntiant_ndp_device_s *ndp) {
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;
    mb_state->mbin_seq ^= 0x80;
    return mb_state->mbin_seq;
}

static void mailbox_reset_state(struct syntiant_ndp_device_s *ndp) {
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;
    mb_state->mbin_resync = 1;
    mb_state->watermarkint_resync = 1;
}

static int
mcu_fw_pointers(struct syntiant_ndp_device_s *ndp, int clear)
{
    int s = 0;
    struct ndp120_fw_state_pointers_s fwps;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    if (clear) {
        ndp120->mcu_fw_pointers_addr = ndp120->mcu_fw_state_addr
            = ndp120->mcu_fw_posterior_state_addr = ndp120->mcu_fw_smax_smoother_addr
            = ndp120->mcu_fw_posterior_parameters_addr = 0;
        goto error;
    }

    s = ndp_mcu_read_block(ndp120->mcu_fw_pointers_addr, fwps.addresses, sizeof(fwps.addresses));
    if (s) goto error;

    ndp120->mcu_fw_state_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_FW_STATE];
    ndp120->mcu_fw_posterior_state_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_POSTERIOR_STATE];
    ndp120->mcu_fw_smax_smoother_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_SMAX_SMOOTHER];
    ndp120->mcu_fw_posterior_parameters_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_POSTERIOR_PARAMS];
    ndp120->mcu_fw_orchestrator_graph_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_ORCHESTRATOR_PARAMS];
    ndp120->mcu_fw_dbg_state_addr
        = fwps.addresses[NDP120_FW_STATE_ADDRESS_INDEX_DBG_STATE];

    DEBUG_PRINTF("    miaddr: 0x%08X", ndp120->mcu_fw_pointers_addr);
    DEBUG_PRINTF("  fw_state: 0x%08X", ndp120->mcu_fw_state_addr);
    DEBUG_PRINTF("  ph_state: 0x%08X", ndp120->mcu_fw_posterior_state_addr);
    DEBUG_PRINTF("  smoother: 0x%08X", ndp120->mcu_fw_smax_smoother_addr);
    DEBUG_PRINTF("  ph_param: 0x%08X", ndp120->mcu_fw_posterior_parameters_addr);
    DEBUG_PRINTF("orch_graph: 0x%08X", ndp120->mcu_fw_orchestrator_graph_addr);
    DEBUG_PRINTF(" dbg_state: 0x%08X", ndp120->mcu_fw_dbg_state_addr);

error:
    return s;
}

static int clear_mcu_fw_pointers(struct syntiant_ndp_device_s *ndp) {
    return mcu_fw_pointers(ndp, 1);
}

static int get_mcu_fw_pointers(struct syntiant_ndp_device_s *ndp) {
    return mcu_fw_pointers(ndp, 0);
}

static int clear_dsp_fw_pointers(struct syntiant_ndp_device_s *ndp) {
    ndp->d.ndp120.dsp_fw_state_addr = 0;
    return 0;
}

uint32_t syntiant_ndp120_get_dsp_fw_pointer(struct syntiant_ndp_device_s *ndp) {
    return ndp->d.ndp120.dsp_fw_state_addr;
}

uint32_t syntiant_ndp120_get_mcu_fw_pointer(struct syntiant_ndp_device_s *ndp) {
    return ndp->d.ndp120.mcu_fw_state_addr;
}

uint32_t syntiant_ndp120_get_mcu_dbg_state_addr(
        struct syntiant_ndp_device_s *ndp) {
    return ndp->d.ndp120.mcu_fw_dbg_state_addr;
}

static int
halt_mcu(struct syntiant_ndp_device_s *ndp)
{
    int s;
    uint8_t data;

    s = ndp_spi_read(NDP120_SPI_CTL, &data);
    if (s) goto error;
    data = NDP120_SPI_CTL_MCUHALT_INSERT(data, 1);
    s = ndp_spi_write(NDP120_SPI_CTL, data);
    if (s) goto error;

error:
    return s;
}

void syntiant_ndp120_mbin_send(struct syntiant_ndp_device_s *ndp, uint8_t data)
{
    data = (uint8_t)((data & 0x7F) | get_mbin_seq(ndp));
#if NDP120_DEBUG_MB
    {
        char buf[32] = "";
        syntiant_ndp120_mcu_mb_op_decoder(data & 0x7F, buf, sizeof(buf));
        DEBUG_PRINTF("0x%02X (%s) --> mbin", data, buf);
    }
#endif
    ndp_spi_write(NDP120_SPI_MBIN, data);
}

int syntiant_ndp120_mbin_resp_get(struct syntiant_ndp_device_s *ndp, uint8_t *data) {
    int s;
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;
    s = ndp_spi_read(NDP120_SPI_MBIN_RESP, data);
    if (s) goto error;

#if NDP120_DEBUG_MB
    {
        char buf[32] = "";
        syntiant_ndp120_mcu_mb_op_decoder(*data, buf, sizeof(buf));
        DEBUG_PRINTF("0x%02X (%s) <-- mbin_resp", *data, buf);
    }
#endif

    if ((*data & 0x80) != mb_state->mbin_resp_seq && !mb_state->mbin_resync) {
        DEBUG_PRINTF("mbin_resp sequence number is incorrect");
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    } 

    if (mb_state->mbin_resync) {
        mb_state->mbin_resync = 0;
        mb_state->mbin_resp_seq = *data & 0x80;
    }

    mb_state->mbin_resp_seq ^= 0x80;
    *data &= 0x7f;
error:
    return s;
}

#if NDP120_DEBUG_SIMULATOR
static int syntiant_ndp120_poll(struct syntiant_ndp_device_s *ndp,
        uint32_t *notifications, int clear);
static void MAYBE_POLL(struct syntiant_ndp_device_s *ndp)
{
    static int first = 1;
    uint32_t not;
    if (first) {
        printf("remove me, this is only for sim development %s:%d\n", __FILE__, __LINE__);
        first = 0;
    }
    syntiant_ndp120_poll(ndp, &not, 1);
}
#else
#define MAYBE_POLL(ndp) 
#endif


int
syntiant_ndp120_do_mailbox_req_no_sync(struct syntiant_ndp_device_s *ndp,
        uint8_t req, uint32_t *resp) {

    int s;
    int is_mcu = 1;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    switch (req) {
    case SYNTIANT_NDP120_MB_MCU_MIADDR:
        clear_mcu_fw_pointers(ndp);
        ndp120->mb_state.mbin_state = MB_STATE_DATA;
        ndp120->mb_state.mbin_data_count = 0;
        ndp120->mb_state.mbin_data = 0;
        break;
    case NDP120_DSP_MB_H2D_ADX_LOWER:
        clear_dsp_fw_pointers(ndp);
        ndp120->mb_state.watermarkint_state = MB_STATE_SYNC;
        ndp120->mb_state.watermarkint_data = 0;
        is_mcu = 0;
        break;
    }

    syntiant_ndp120_mbin_send(ndp, req);
    s = ndp->iif.mbwait(ndp->iif.d);
    if(s) {
	DEBUG_PRINTF("Error syntiant_ndp120_do_mailbox_req_no_sync, %d\n", s);
	goto error;
    }
    MAYBE_POLL(ndp);

    if(resp != NULL ) {
        *resp = is_mcu ? ndp120->mb_state.mbin_resp : ndp120->mb_state.watermarkint;
    }
    switch (req) {
    case SYNTIANT_NDP120_MB_MCU_NOP:
        break;

    case SYNTIANT_NDP120_MB_MCU_MIADDR:
        ndp120->mcu_fw_pointers_addr = ndp120->mb_state.mbin_data;
        get_mcu_fw_pointers(ndp);
        break;

    case NDP120_DSP_MB_H2D_ADX_LOWER:
        ndp120->dsp_fw_state_addr =
            (ndp120->dsp_fw_state_addr & 0xFFFF0000) | ndp120->mb_state.watermarkint_data;
        DEBUG_PRINTF("LOWER: DSP FW PTR: 0x%x", ndp120->dsp_fw_state_addr);
        break;
    }

    error:
    return s;
}

static int
syntiant_read_match_producer(struct syntiant_ndp_device_s *ndp)
{
    int s;
    uint32_t addr;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    uint32_t net_id = ndp120->last_network_id;

    addr = (uint32_t) (ndp120->mcu_fw_state_addr +
                (uint32_t) offsetof(struct ndp120_fw_state_s, match_producer) +
                sizeof(net_id) * net_id);
    s = syntiant_ndp120_read(ndp, 1, addr, &ndp120->match_producer[net_id]);
    return s;
}

/*
 * Used by the CLI, mostly for debugging
 */
int
syntiant_ndp120_do_mailbox_req(struct syntiant_ndp_device_s *ndp, uint8_t req, uint32_t *resp)
{
    int s, s0;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) {
	DEBUG_PRINTF("Error in syntiant_ndp120_do_mailbox_req\n");
	goto error;
    }

    s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, req, resp);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

error:
    return s;
}

static int watermarkint_recv(struct syntiant_ndp_device_s *ndp,
        ndp120_dsp_mailbox_msg_t *data) {
    int s;
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;

    s = ndp->iif.mbwait(ndp->iif.d);
    if (s) {
	DEBUG_PRINTF("Error in watermarkint_recv, %d\n", s);
	goto error;
    }
    MAYBE_POLL(ndp);
    *data = mb_state->watermarkint;

    error:
    return s;
}

/* MBOUT */
static int
mbout_recv(struct syntiant_ndp_device_s *ndp, uint8_t *data)
{
    int s;
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;

    mb_state->mbout = 0;
    s = ndp->iif.mbwait(ndp->iif.d);
    if (s) {
	DEBUG_PRINTF("Error in mbout_recv, %d\n", s);
	goto error;
    }
    MAYBE_POLL(ndp);
    *data = mb_state->mbout;

error:
    DEBUG_PRINTF("mbout_recv returning %d\n", s);
    return s;
}

static void
mbout_send_resp(struct syntiant_ndp_device_s *ndp, uint8_t data)
{
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;

    data = (uint8_t)((data & 0x7f) | mb_state->mbout_seq);
#if NDP120_DEBUG_MB
    {
        char buf[32] = "";
        syntiant_ndp120_mcu_mb_op_decoder(data, buf, sizeof(buf));
        DEBUG_PRINTF("0x%02X (%s) --> mbout_resp", data, buf);
    }
#endif
    ndp_spi_write(NDP120_SPI_MBOUT_RESP, data);
}

static uint8_t mb_resp_is_data(uint8_t result) {
    return result & SYNTIANT_NDP120_MB_MCU_DATA_MASK;
}

static uint8_t mb_resp_is_error(uint8_t result, uint8_t * error) {
    if (mb_resp_is_data(result)) return 0;

    if(result & SYNTIANT_NDP120_MB_MCU_ERROR_MASK) {
        if(error) *error = result & (SYNTIANT_NDP120_MB_MCU_ERROR_MASK - 1);
        return 1;
    }
    return 0;
}

static int
watermarkint_get(struct syntiant_ndp_device_s *ndp, ndp120_dsp_mailbox_msg_t *data)
{
    int s;
    s = ndp_mcu_read(NDP120_CHIP_CONFIG_WATERMARKINT, data);
    if (s) goto error;

error:
    return s;
}

static int mbout_get(struct syntiant_ndp_device_s *ndp, uint8_t *data) {
    int s;
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;

    s = ndp_spi_read(NDP120_SPI_MBOUT, data);
    if (s) goto error;


#if NDP120_DEBUG_MB
    DEBUG_PRINTF("mbout raw: 0x%02X", *data);
    {
        char buf[32] = "";
        syntiant_ndp120_mcu_mb_op_decoder(*data, buf, sizeof(buf));
        DEBUG_PRINTF("0x%02X (%s) <-- mbout", *data, buf);
    }
#endif
    mb_state->mbout_seq = (*data & 0x80);
    *data &= 0x7f;

error:
    return s;
}


static int
mbout_processor(struct syntiant_ndp_device_s *ndp, uint32_t *notify)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t data;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    *notify = 0;
    s = mbout_get(ndp, &data);
    if (s) goto error;

    ndp120->mb_state.mbout = data;

    switch (data) {
    case SYNTIANT_NDP120_MB_MCU_RUNNING:
        ndp120->mb_state.mbin_state = MB_STATE_SYNC;
        ndp120->mb_state.mbin_sync_count = 0;
        mailbox_reset_state(ndp);
        syntiant_ndp120_mbin_send(ndp, SYNTIANT_NDP120_MB_MCU_NOP);
        break;

    case SYNTIANT_NDP120_MB_MCU_RUNNING_TEST:
        ndp120->mb_state.mbin_state = MB_STATE_SYNC;
        ndp120->mb_state.mbin_sync_count = 0;
        mailbox_reset_state(ndp);
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT;
        break;

    case SYNTIANT_NDP120_MB_MCU_NOP:
        mbout_send_resp(ndp, SYNTIANT_NDP120_MB_MCU_NOP);
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT;
        break;

    case SYNTIANT_NDP120_MB_MCU_BOOTING:
        /* Nothing to do here, arrives right after firmware load */
        DEBUG_PRINTF("Received BOOTING in mbout");
        break;

    default:
        /* check for a match */
        if (data & SYNTIANT_NDP120_MB_MCU_NETWORK_MASK) {
            ndp120->last_network_id =
                (data & ~SYNTIANT_NDP120_MB_MCU_NETWORK_MASK) & 0xFF;
            syntiant_read_match_producer(ndp);
            mbout_send_resp(ndp, SYNTIANT_NDP120_MB_MCU_NOP);
            ndp120->matches++;
            *notify = SYNTIANT_NDP_NOTIFICATION_MATCH;
        } else {
            DEBUG_PRINTF("POLL: got unknown mbout: 0x%02X", data);
        }
        break;
    }

error:
    return s;
}

static int
dsp_mb_processor(struct syntiant_ndp_device_s *ndp, uint32_t *notify)
{
    int s = 0;
#if NDP120_DEBUG || NDP120_DEBUG_POLL
    char buf[32];
    uint32_t data;
    static int extract_count = 0;
#endif
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    s = watermarkint_get(ndp, &ndp120->mb_state.watermarkint);
    if (s) goto error;

#if NDP120_DEBUG
    syntiant_ndp120_dsp_mb_op_decoder(ndp120->mb_state.watermarkint, buf,
            sizeof(buf));
    if (NDP120_DSP_MB_GET_MESSAGE(ndp120->mb_state.watermarkint) !=
            NDP120_DSP_MB_D2H_DEBUG || !ndp120_debug_callback) {
        DEBUG_PRINTF("DSP sent: %s", buf);
    }
#endif

    switch (NDP120_DSP_MB_GET_MESSAGE(ndp120->mb_state.watermarkint)) {
    case NDP120_DSP_MB_D2H_WATERMARK:
        *notify = SYNTIANT_NDP_NOTIFICATION_WATER_MARK;
#if NDP120_DEBUG || NDP120_DEBUG_POLL
        ndp_mcu_read(NDP120_DSP_CONFIG_BUFFILLLEVEL(4), &data);
        DEBUG_PRINTF("FILL LEVEL: 0x%X\n", data);
        DEBUG_PRINTF("sending watermark ack");
#endif
        syntiant_ndp120_mbin_send(ndp, NDP120_DSP_MB_H2D_WATERMARK);
        break;

    case NDP120_DSP_MB_D2H_EXTRACT_READY:
        *notify |= SYNTIANT_NDP_NOTIFICATION_EXTRACT_READY;
       /* printf("extract_ready num=0x%x\n", mb_state.watermarkint.msg.payload); */
#if NDP120_DEBUG || NDP120_DEBUG_POLL
        ndp_mcu_read(NDP120_DSP_CONFIG_BUFFILLLEVEL(4), &data);
        DEBUG_PRINTF("FILL LEVEL: 0x%X\n", data);
        DEBUG_PRINTF("Sending extract ack\n");
#endif
        syntiant_ndp120_mbin_send(ndp, NDP120_DSP_MB_H2D_EXTRACT_READY);
#if NDP120_DEBUG || NDP120_DEBUG_POLL
        extract_count += 1;
        DEBUG_PRINTF("extract cnt: %d, payload: %d\n", extract_count,
                NDP120_DSP_MB_GET_PAYLOAD(ndp120->mb_state.watermarkint));
#endif
        break;

    case NDP120_DSP_MB_D2H_PONG:
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        break;

    case NDP120_DSP_MB_D2H_TEST:
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        break;

    case NDP120_DSP_MB_D2H_DEBUG: {
        uint32_t fw_st = syntiant_ndp120_get_dsp_fw_pointer(ndp);
        char buf[128];
        if (fw_st) {
            uint32_t to_read =
                (umin(NDP120_DSP_MB_GET_PAYLOAD(ndp120->mb_state.watermarkint),
                      sizeof(buf)) + 3) / 4 * 4;
            ndp_mcu_read_block(fw_st + (uint32_t) offsetof(ndp120_dsp_fw_base_t, debug_buf),
                    buf, to_read);
            ndp_mcu_write(fw_st + (uint32_t) offsetof(ndp120_dsp_fw_base_t, debug_buf), 0); /* ack */
            buf[sizeof(buf)-1] = 0;
            if (ndp120_debug_callback != NULL) {
                ndp120_debug_callback(buf);
            }
        }

    } break;

    case NDP120_DSP_MB_D2H_ADX_LOWER:
        ndp120->mb_state.watermarkint_data =
            NDP120_DSP_MB_GET_PAYLOAD(ndp120->mb_state.watermarkint);
        syntiant_ndp120_mbin_send(ndp, NDP120_DSP_MB_H2D_ADX_UPPER);
        break;

    case NDP120_DSP_MB_D2H_ADX_UPPER:
        ndp120->dsp_fw_state_addr = (ndp120->dsp_fw_state_addr & 0xFFFF) |
            (uint32_t)(NDP120_DSP_MB_GET_PAYLOAD(ndp120->mb_state.watermarkint) << 16);
        DEBUG_PRINTF("UPPER: DSP FW PTR: 0x%x", ndp120->dsp_fw_state_addr);
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        break;

    case NDP120_DSP_MB_D2H_RUNNING:
        /* fall through */
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        DEBUG_PRINTF("got RUNNING, mailbox notify\n");
        break;

    default:
        DEBUG_PRINTF("Invalid DSP mailbox value: 0x%X\n", ndp120->mb_state.watermarkint);
        break;
    }

error:
    return s;
}

static int
mbin_processor(struct syntiant_ndp_device_s *ndp, uint32_t *notify)
{
    int s;
    uint8_t data;
    syntiant_ndp120_mb_state_s *mb_state = &ndp->d.ndp120.mb_state;

    s = syntiant_ndp120_mbin_resp_get(ndp, &data);
    if (s) goto error;

    mb_state->mbin_resp = data;

    /* post fw load sync code */
    if (mb_resp_is_error(data, NULL) && mb_state->mbin_state == MB_STATE_SYNC) {
        ++mb_state->mbin_sync_count;
        if (mb_state->mbin_sync_count < 3) {
            syntiant_ndp120_mbin_send(ndp, SYNTIANT_NDP120_MB_MCU_NOP);
        } else {
            mb_state->mbin_state = MB_STATE_NONE;
            *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
        }
        goto error;
    }

    if (data == SYNTIANT_NDP120_MB_MCU_NOP) {
        if (mb_state->mbin_state == MB_STATE_SYNC) {
            mb_state->mbin_state = MB_STATE_NONE; 
        }
        *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
    } else if (mb_resp_is_data(data)) {
        if (mb_state->mbin_state == MB_STATE_DATA) {
            int last, addr_shift, mb_shift;
            ++mb_state->mbin_data_count;
            if (mb_state->mbin_data_count < 6) {
                /* TODO plz use literrals in place of hard coded values */
                last = 0;
                addr_shift = 6;
                mb_shift = 26;
            } else {
                last = 1;
                addr_shift = 2;
                mb_shift = 30;
            }
            mb_state->mbin_data >>= addr_shift;
            mb_state->mbin_data |= (uint32_t)((data & 0x3F) << mb_shift);
            if (last) {
                mb_state->mbin_state = MB_STATE_NONE;
                *notify = SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN;
            } else {
                syntiant_ndp120_mbin_send(ndp, SYNTIANT_NDP120_MB_MCU_DATA);
        }
        }
    }
error:
    return s;
}

static int
syntiant_ndp120_poll(struct syntiant_ndp_device_s *ndp,
                     uint32_t *notifications, int clear)
{
#if NDP120_DEBUG_POLL
    static struct timeval last_debug_print = { 0 };
    struct timeval start_time;
    struct timeval tmp_time;
    int debug_print = 0;
#endif
    int s;
    uint32_t notify = 0;
    uint8_t intsts;

    *notifications = 0;

    retry:

    s = syntiant_ndp120_read(ndp, 0, NDP120_SPI_INTSTS, &intsts);
    if (s) goto error;
#if NDP120_DEBUG_POLL
    {
        uint8_t intctl;
        gettimeofday(&tmp_time, NULL);
        timersub(&tmp_time, &last_debug_print, &tmp_time);
        if ((tmp_time.tv_sec >= 2) || intsts) {
            gettimeofday(&last_debug_print, NULL);
            debug_print = 1;
            syntiant_ndp120_read(ndp, 0, NDP120_SPI_INTCTL, &intctl);
            DEBUG_PRINTF( "poll() enter intsts: 0x%02X, intctl: 0x%02X", intsts, intctl);
        }
    }
#endif

    /* this is unlikely, and is probably a result of a reset.
       ignoring this causes false notifications that often
       result in a host crash */

    if (intsts == 0xFF) {
        uint8_t id;
        syntiant_ms_time timeout;
        syntiant_get_ms_time(&timeout);
#if NDP120_DEBUG_POLL
        gettimeofday(&start_time, NULL);
#endif
#if NDP120_DEBUG_POLL
        DEBUG_PRINTF("poll(): likely reset, waiting for recovery");
#endif
        do {
            s = syntiant_ndp120_read(ndp, 0, NDP120_SPI_ID0, &id);
            if (s) goto error;
            if (id != 0xFF) { 
#if NDP120_DEBUG_POLL
                gettimeofday(&tmp_time, NULL);
                timersub(&tmp_time, &start_time, &tmp_time);
                DEBUG_PRINTF("elapsed recovery time: %dus", tmp_time.tv_usec);
#endif
                goto retry;
            }
        } while (syntiant_get_ms_elapsed(&timeout) < 20);

#if NDP120_DEBUG_POLL
        DEBUG_PRINTF("poll(): reset did not recover");
#endif
        *notifications = SYNTIANT_NDP_NOTIFICATION_ERROR;
        goto error;
    }


    if (clear) {
        s = syntiant_ndp120_write(ndp, 0, NDP120_SPI_INTSTS, intsts);
    }

    /* H2M response interrupt */
    if (intsts & NDP120_SPI_INTSTS_MBIN_INT(1)) {
        s = mbin_processor(ndp, &notify);
        if (s) goto error;
        *notifications |= notify;
    }

    if (intsts & NDP120_SPI_INTSTS_DNN_INT(1)) {
        *notifications |= SYNTIANT_NDP_NOTIFICATION_DNN;
    }

    /* M2H request interrupt */
    /* Match comes via mailbox out event */
    if (intsts & NDP120_SPI_INTSTS_MBOUT_INT(1)) {
#if NDP120_DEBUG_POLL
        DEBUG_PRINTF("poll(): mbout interrupt");
#endif
        s = mbout_processor(ndp, &notify);
        if (s) goto error;

#if NDP120_DEBUG_POLL || NDP120_DEBUG_POLL_MATCH
        if (notify & SYNTIANT_NDP_NOTIFICATION_MATCH) {
            DEBUG_PRINTF("poll(): match message from mbout");
        }
#endif
        *notifications |= notify;
    }

    /* M2H Frequency domain completion (filter bank) interrupt */
    if (intsts & NDP120_SPI_INTSTS_FEATURE_INT(1)) {
        *notifications |= SYNTIANT_NDP_NOTIFICATION_FEATURE;
    }

    if (intsts & NDP120_SPI_INTSTS_AE_INT(1)) {
        *notifications |= SYNTIANT_NDP_NOTIFICATION_ERROR;
    }

    if (intsts & NDP120_SPI_INTSTS_RF_INT(1)) {
        *notifications |= SYNTIANT_NDP_NOTIFICATION_ERROR;
    }

    if (intsts & NDP120_SPI_INTSTS_WM_INT(1)) {
        s = dsp_mb_processor(ndp, &notify);
        if (s) goto error;
#if NDP120_DEBUG_POLL
        DEBUG_PRINTF("updating DSP notifications with %d s: %d",notify, s);
#endif
        *notifications |= notify;
    }
error:
#if NDP120_DEBUG_POLL
    if (debug_print) {
        DEBUG_PRINTF("poll() exiting with code %d, notificatons: %d", s, *notifications);
    }
#endif
    return s;
}

#ifdef SYNTIANT_NDP120_MAILBOX_TRACE
char *mailbox_dir[2] = { "h2m", "m2h" };

void
syntiant_ndp120_mb_request_trace(int m2h, uint8_t r)
{
    static char *reqs[] = SYNTIANT_NDP120_MB_MCU_REQUEST_DECODER;

    if (syntiant_ndp120_mailbox_trace) {
        if (1 < syntiant_ndp120_mailbox_trace
            || (r != SYNTIANT_NDP120_MB_MCU_REQUEST_CONT
                   && r != SYNTIANT_NDP120_MB_MCU_REQUEST_EXTOP
                   && (r & ~0x3) != SYNTIANT_NDP120_MB_MCU_REQUEST_DATA)) {
            SYNTIANT_PRINTF("%s req  %d: ", mailbox_dir[m2h],
                                syntiant_ndp120_mailbox_req_number[m2h]);
            if (r < sizeof(reqs) / sizeof(reqs[0])) {
                SYNTIANT_PRINTF("%s\n", reqs[r]);
            } else {
                SYNTIANT_PRINTF("ILLEGAL(%d)\n", r);
            }
        }
    }
    if (r < 8) {
        /*
         * only bump req count for 'primitive' (3-bit) requests
         * versus composite (extended/6-bit) requests
         */
        syntiant_ndp120_mailbox_req_number[m2h]++;
    }
}

void
syntiant_ndp120_mb_response_trace(int m2h, uint8_t r)
{
    static char *rsps[] = SYNTIANT_NDP120_MB_MCU_RESPONSE_DECODER;

    if (syntiant_ndp120_mailbox_trace) {
        if (1 < syntiant_ndp120_mailbox_trace
            || (r != SYNTIANT_NDP120_MB_MCU_RESPONSE_CONT
                   && r != SYNTIANT_NDP120_MB_MCU_RESPONSE_ERROR
                   && (r & ~0x3) != SYNTIANT_NDP120_MB_MCU_RESPONSE_DATA)) {
            SYNTIANT_PRINTF("%s rsp  %d: ", mailbox_dir[m2h],
                syntiant_ndp120_mailbox_rsp_number[m2h]);
            if (r < sizeof(rsps) / sizeof(rsps[0])) {
                SYNTIANT_PRINTF("%s\n", rsps[r]);
            } else {
                SYNTIANT_PRINTF("ILLEGAL(%d)\n", r);
            }
        }
    }
    syntiant_ndp120_mailbox_rsp_number[m2h]++;
}

void
syntiant_ndp120_mb_data_trace(int m2h, uint8_t *data, int bits)
{
    uint32_t d = 0;
    int i;

    if (syntiant_ndp120_mailbox_trace) {
        SYNTIANT_PRINTFi"%s data %d: ", mailbox_dir[m2h],
            syntiant_ndp120_mailbox_rsp_number[m2h]);
        if (bits <= sizeof(d) * 8) {
            memmove(&d, data, (bits + 7) / 8);
            SYNTIANT_PRINTF("0x%x", d);
        } else {
            for (i = 0; i < (bits + 7) / 8; i++) {
                SYNTIANT_PRINTF("0x%02x ", data[i]);
            }
        }
        SYNTIANT_PRINTF("\n");
    }
}

void
syntiant_ndp120_mb_unexpected_trace(int m2h, uint8_t r, uint8_t bits)
{
    int n;

    if (syntiant_ndp120_mailbox_trace) {
        n = m2h ? syntiant_ndp120_mailbox_req_number[m2h]
                : syntiant_ndp120_mailbox_rsp_number[m2h];
        SYNTIANT_PRINTF("%s unex %d: %d message unexpected"
                            ", %d data bits remaining\n",
            mailbox_dir[m2h], n, r, bits);
    }
    syntiant_ndp120_mailbox_unexpected[m2h]++;
}

void
syntiant_ndp120_mb_error_trace(int m2h, uint8_t e)
{
    static char *errors[] = SYNTIANT_NDP120_MB_MCU_ERROR_DECODER;

    if (syntiant_ndp120_mailbox_trace) {
        SYNTIANT_PRINTF("%s err  %d: ", mailbox_dir[m2h],
            syntiant_ndp120_mailbox_rsp_number[m2h]);
        if (e < sizeof(errors) / sizeof(errors[0])) {
            SYNTIANT_PRINTF("%s\n", errors[e]);
        } else {
            SYNTIANT_PRINTF("ILLEGAL(%d)\n", e);
        }
    }
    syntiant_ndp120_mailbox_error[m2h]++;
}
#else
#define syntiant_ndp120_mb_request_trace(m2h, r)                               \
    do {                                                                       \
    } while (0)
#define syntiant_ndp120_mb_response_trace(m2h, r)                              \
    do {                                                                       \
    } while (0)
#define syntiant_ndp120_mb_data_trace(m2h, d, b)                               \
    do {                                                                       \
    } while (0)
#define syntiant_ndp120_mb_unexpected_trace(m2h, r, bits)                      \
    do {                                                                       \
    } while (0)
#define syntiant_ndp120_mb_error_trace(m2h, e)                                 \
    do {                                                                       \
    } while (0)
#endif


#if 0
static void
syntiant_ndp120_reset_mailbox_state(struct syntiant_ndp_device_s *ndp)
{

    /*
    int i;
    for (i = 0; i < SYNTIANT_NDP120_DEVICE_MAILBOX_DIRECTIONS; i++) {
        memset(&ndp->d.ndp120.mb[i], 0,
            sizeof(struct syntiant_ndp120_device_mb_state_s));
    }
    */
    ndp->d.ndp120.mbin = 0;
    ndp->d.ndp120.mbin_resp = 0;
    ndp->d.ndp120.fw_pointers_addr = 0;
    ndp->d.ndp120.fw_state_addr = 0;
}
#endif


/* ================== */
/* INTERNAL FUNCTIONS */
/* ================== */


static int 
syntiant_ndp120_pkg_parse_nn_labels_v1_v3_tlv(struct syntiant_ndp_device_s *ndp)
{
    /* v2 was abandoned */
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    uint32_t valid;
    uint32_t offset;
    uint32_t size;
    uint32_t daddr = NDP120_ILIB_SCRATCH_ORIGIN +
        offsetof(syntiant_ndp120_scratch_t, label_size);

    if(pstate->mode == PACKAGE_MODE_VALUE_START){
        /*set valid bit and write length into config region*/
        s = syntiant_ndp120_write(ndp, 1, daddr, *(uint32_t*)pstate->length);
        if (s) goto error;

        ndp120->labels_len = *(uint32_t*)pstate->length;

    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE){
        offset = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
        pstate->partially_read_length -
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            pstate->partially_read_length - PARSER_SCRATCH_SIZE;

        size = pstate->partially_read_length % PARSER_SCRATCH_SIZE ?
            (pstate->partially_read_length % PARSER_SCRATCH_SIZE) :
            PARSER_SCRATCH_SIZE;

        daddr = NDP120_ILIB_SCRATCH_ORIGIN +
            ((uint32_t)offsetof(syntiant_ndp120_scratch_t,labels) + offset);
        s = syntiant_ndp120_write_block(ndp, 1, daddr, pstate->data.labels,
                size);
        if(s) goto error;
    }

    /* post-load actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        s = syntiant_ndp120_scratch_get_valid_skip_crc(ndp, &valid);
        if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
        s = 0;
        valid = valid | SYNTIANT_CONFIG_LABELS_VALID;
        s = syntiant_ndp120_scratch_set_valid(ndp, valid);
        if (s) goto error;
    }

 error:
    return s;
}

/**
 * @brief This function parses php_collection_v1 TLV and directly copies
 *  the condent to MCU ph worspace.
 * @param ndp device object
 */
static int
syntiant_ndp120_pkg_parse_php_collection_v1_tlv
(struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_ndp120_posterior_config_t ph_config;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint32_t i;

#if 0
    printf("\nParser mode: %d\n", pstate->mode);
    printf("Struct content\n");
    printf("\tCollection_params\n");
    printf("\t\tNum ph: %d\n", pstate->data.phc_params.collection.num_ph);
    printf("\tPh_params\n");
    printf("\t\tNum classes: %d\n", pstate->data.phc_params.ph.num_classes);
    printf("\t\tNum states: %d\n", pstate->data.phc_params.ph.num_states);
    printf("\t\tPh type: %d\n", pstate->data.phc_params.ph.ph_type);
    printf("\tState_params:\n");
    printf("\t\ttimeout: %d\n", pstate->data.phc_params.state.timeout);
    printf("\t\ttimeout_action: %d\n", pstate->data.phc_params.state.timeout_action);
    printf("\t\ttimeout_action_arg0: %d\n", pstate->data.phc_params.state.timeout_action_arg0);
    printf("\t\ttimeout_action_arg0: %d\n", pstate->data.phc_params.state.timeout_action_arg1);
    printf("\tClass_params:\n");
    printf("\t\twindow: %d\n", pstate->data.phc_params.class_.window);
    printf("\t\tthreshold: %d\n", pstate->data.phc_params.class_.threshold);
    printf("\t\tbackoff: %d\n", pstate->data.phc_params.class_.backoff);
    printf("\t\tsmooth Q size: %d\n", pstate->data.phc_params.class_.queuesize);
    printf("\t\taction: %d\n", pstate->data.phc_params.class_.action);
    printf("\t\taction_arg0: %d\n", pstate->data.phc_params.class_.action_arg0);
    printf("\t\taction_arg1: %d\n", pstate->data.phc_params.class_.action_arg1);
    printf("\tParser_params:\n");
    printf("\t\tCurrent ph: %d\n", pstate->data.phc_params.parser.cur_ph);
    printf("\t\tCurrent state: %d\n", pstate->data.phc_params.parser.cur_state);
    printf("\t\tCurrent class: %d\n", pstate->data.phc_params.parser.cur_class);
    printf("\t\tParsing Next: %d\n", pstate->data.phc_params.parser.parsing);
    printf("\t\tParsed Before: %d\n", pstate->data.phc_params.parser.parsed);
#endif

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* pre-write actions */
        memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
        ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ENABLE;
        ph_config.enable = 0x0;
        s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;
        goto got_value_start;
    };

    memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
    switch (pstate->data.phc_params.parser.parsed) {
        case PHC_PARSE_COLLECTION_PARAMS:
            memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
            ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_NUM_PH;
            ph_config.ph_num = pstate->data.phc_params.collection.num_ph;
            s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
            break;

        case PHC_PARSE_PH_PARAMS:
            ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_STATES
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_CLASSES
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_PH_TYPE;
            ph_config.states = pstate->data.phc_params.ph.num_states;
            ph_config.classes = pstate->data.phc_params.ph.num_classes;
            ph_config.ph_type = pstate->data.phc_params.ph.ph_type;
            ph_config.ph_idx = pstate->data.phc_params.parser.cur_ph - 1;
            s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
            break;

        case PHC_PARSE_STATE_PARAMS:
            ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION;
            ph_config.timeout = pstate->data.phc_params.state.timeout;
            ph_config.timeout_action_type =
                pstate->data.phc_params.state.timeout_action;
            ph_config.timeout_action_arg0 =
                pstate->data.phc_params.state.timeout_action_arg0;
            ph_config.timeout_action_arg1 =
                pstate->data.phc_params.state.timeout_action_arg1;
            ph_config.ph_idx = pstate->data.phc_params.parser.cur_ph - 1;
            ph_config.state = pstate->data.phc_params.parser.cur_state - 1;
            s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
            break;

        case PHC_PARSE_CLASS_PARAMS:
            ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_THRESHOLD
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_WINDOW
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_BACKOFF
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ACTION
                | NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE;
            ph_config.threshold = pstate->data.phc_params.class_.threshold;
            ph_config.window = pstate->data.phc_params.class_.window;
            ph_config.backoff = pstate->data.phc_params.class_.backoff;
            ph_config.action_type = pstate->data.phc_params.class_.action;
            ph_config.action_arg0 = pstate->data.phc_params.class_.action_arg0;
            ph_config.action_arg1 = pstate->data.phc_params.class_.action_arg1;
            ph_config.smoothing_queue_size =
                pstate->data.phc_params.class_.queuesize;
            ph_config.ph_idx = pstate->data.phc_params.parser.cur_ph - 1;
            ph_config.state = pstate->data.phc_params.parser.cur_state - 1;
            ph_config.class_index = pstate->data.phc_params.parser.cur_class - 1;
            s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
            break;
    }

    if (pstate->mode == PACKAGE_MODE_TAG_START) {
        /* posterior params load done */
        memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
        ph_config.set = NDP120_CONFIG_SET_POSTERIOR_CONFIG_SET_ENABLE;
        ph_config.enable = 0x1;
        s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;

        /* set the number of networks (assuming one ph per network) */
        memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
        s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
        if (s) goto error;
        ndp120->num_networks = ph_config.ph_num;

        /* set the number of classes per network*/
        memset(ndp120->classes, 0, sizeof(ndp120->classes));
        for (i = 0; i < pstate->data.phc_params.collection.num_ph; i++) {
            memset(&ph_config, 0, sizeof(syntiant_ndp120_posterior_config_t));
            ph_config.ph_idx = i;
            s = syntiant_ndp120_posterior_config_no_sync(ndp, &ph_config);
            if (s) goto error;
            ndp120->classes[i] = ph_config.classes;
        }
    };

got_value_start:
error:
    return s;
}

/**
 * @brief function to parse version strings
 * @param ndp device object
 */
static int
syntiant_ndp120_pkg_parse_version_string_v1_tlv
(struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t scratch_value_adx;
    uint32_t scratch_len_adx;
    uint8_t *value_adx;
    uint32_t *len_adx;

    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    uint32_t valid;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    if (pstate->mode == PACKAGE_MODE_TAG_START && 
        pstate->partially_read_length == *(uint32_t*)pstate->length) { 
        
        s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
        if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
        s = 0;

        if (*(uint32_t*)pstate->tag == TAG_FIRMWARE_VERSION_STRING_V1){
            valid |= SYNTIANT_CONFIG_FW_VERSION_VALID;
        } else if (*(uint32_t*)pstate->tag == TAG_NN_VERSION_STRING_V1){
            valid |= SYNTIANT_CONFIG_NN_VERSION_VALID;
        } else if (*(uint32_t*)pstate->tag == TAG_DSP_FIRMWARE_VERSION_STRING_V1) {
            valid |= SYNTIANT_CONFIG_DSP_FW_VERSION_VALID;
        } else if (*(uint32_t*)pstate->tag == TAG_PACKAGE_VERSION_STRING){
            valid |= SYNTIANT_CONFIG_PKG_VERSION_VALID;
        }

        switch(*(uint32_t *)pstate->tag){

        case TAG_FIRMWARE_VERSION_STRING_V1:
            scratch_value_adx = SCRATCH_VARIABLE_ADX(fw_version);
            scratch_len_adx = SCRATCH_VARIABLE_ADX(fw_version_size);
            value_adx = pstate->data.fw_version;
            len_adx = &ndp120->fwver_len;
            break;

        case TAG_NN_VERSION_STRING_V1:
            scratch_value_adx = SCRATCH_VARIABLE_ADX(params_version);
            scratch_len_adx = SCRATCH_VARIABLE_ADX(params_version_size);
            value_adx = pstate->data.params_version;
            len_adx = &ndp120->paramver_len;
            break;

        case TAG_DSP_FIRMWARE_VERSION_STRING_V1:
            scratch_value_adx = SCRATCH_VARIABLE_ADX(dsp_fw_version);
            scratch_len_adx = SCRATCH_VARIABLE_ADX(dsp_fw_version_size);
            value_adx = pstate->data.dsp_fw_version;
            len_adx = &ndp120->dspfwver_len;
            break;

        case TAG_PACKAGE_VERSION_STRING:
            scratch_value_adx = SCRATCH_VARIABLE_ADX(pkg_version);
            scratch_len_adx = SCRATCH_VARIABLE_ADX(pkg_version_size);
            value_adx = pstate->data.pkg_version;
            len_adx = &ndp120->pkgver_len;
            break;
        default:
            s = SYNTIANT_NDP_ERROR_UNSUP;
            goto error;
        }

        DEBUG_PRINTF("config: writing length: 0x%X to 0x%X", *(uint32_t *)pstate->length, scratch_len_adx);
        s = syntiant_ndp120_write(ndp, 1, scratch_len_adx, *(uint32_t*)pstate->length);
        if (s) goto error;

        DEBUG_PRINTF("config: writing value: '%*s' to 0x%X", -*(uint32_t *)pstate->length, value_adx, scratch_value_adx);
        s = syntiant_ndp120_write_block(ndp, 1, scratch_value_adx, value_adx, *(uint32_t*)pstate->length);
        if (s) goto error;

        s = syntiant_ndp120_scratch_set_valid(ndp, valid);
        if (s) goto error;
        syntiant_ndp120_scratch_get_valid(ndp, &valid);
        *len_adx = *(uint32_t*)pstate->length;

    }
 error:
    return s;
}


static int
syntiant_ndp120_pkg_parse_firmware_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint32_t offset, size;
    uint8_t data;

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        uint32_t clkctl0;

        /* enable mbin and mbout interrupts */
        s = ndp_spi_read(NDP120_SPI_INTCTL, &data);
        if(s) goto error;
        data = NDP120_SPI_INTCTL_MBIN_INTEN_INSERT(data,1);
        data = NDP120_SPI_INTCTL_MBOUT_INTEN_INSERT(data,1);
        ndp_spi_write(NDP120_SPI_INTCTL, data);

        /* enable interrupt pin */
        s = ndp_spi_read(NDP120_SPI_CFG, &data);
        if(s) goto error;
        data = NDP120_SPI_CFG_INTEN_INSERT(data,1);
        ndp_spi_write(NDP120_SPI_CFG, data);

        /* halt the CPU */
        s = ndp_spi_read(NDP120_SPI_CTL, &data);
        if(s) goto error;
        data = NDP120_SPI_CTL_MCUHALT_INSERT(data,1);
        ndp_spi_write(NDP120_SPI_CTL, data);

        /* put MCU into reset state */
        s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL0, &clkctl0);
        if (s) goto error;
        clkctl0 = NDP120_CHIP_CONFIG_CLKCTL0_MCURSTB_MASK_INSERT(clkctl0, 0x0);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL0, clkctl0);
        if (s) goto error;

        /* disable bootrom */
        s = ndp_mcu_write(NDP120_SYSCTL_MEMCTRL, 0);
        if (s) goto error;

        /* clear interrupts */
        ndp_spi_write(NDP120_SPI_INTSTS, 0xff);
        memset(ndp120->match_producer, 0, sizeof(ndp120->match_producer));
        memset(ndp120->match_consumer, 0, sizeof(ndp120->match_consumer));
    }
    /* copy the firmware in BOOTRAM */
    else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        offset = (pstate->partially_read_length % PARSER_SCRATCH_SIZE) ?
        (pstate->partially_read_length -
        (pstate->partially_read_length % PARSER_SCRATCH_SIZE)):
        (pstate->partially_read_length - PARSER_SCRATCH_SIZE);

        size = (pstate->partially_read_length % PARSER_SCRATCH_SIZE) ?
        (pstate->partially_read_length % PARSER_SCRATCH_SIZE):
        PARSER_SCRATCH_SIZE;

        s = ndp_mcu_write_block(
            NDP120_BOOTRAM_REMAP + offset, pstate->data.fw, size);
    }

    /* post-load actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        uint32_t clkctl0;

        /* reset the MCU */
        s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL0, &clkctl0);
        if (s) goto error;
        clkctl0 = NDP120_CHIP_CONFIG_CLKCTL0_MCURSTB_MASK_INSERT(clkctl0, 0x1);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL0, clkctl0);
        if (s) goto error;

        /* unhalt the CPU */
        s = ndp_spi_read(NDP120_SPI_CTL, &data);
        if(s) goto error;
        data = (uint8_t)NDP120_SPI_CTL_MCUHALT_MASK_INSERT(data, 0);
        ndp_spi_write(NDP120_SPI_CTL, data);

#ifdef SKIP_FW_HANDSHAKE
        goto error;
#endif

        DEBUG_PRINTF("mbout_recv()");
        s = mbout_recv(ndp, &data);
        DEBUG_PRINTF("got s:%d data: %d", s, data);
        if(s) goto error;

        switch (data) {
        case SYNTIANT_NDP120_MB_MCU_RUNNING:
            /* do nothing, intentionally */
            break;
        case SYNTIANT_NDP120_MB_MCU_RUNNING_TEST:
            /* skip handshake */
            goto error;
        default:
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("received unknown message 0x%X from MCU", data);
            goto error;
        }
        s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_MIADDR, NULL);
        if (s) goto error;
    }

 error:
    return s;
}

#if NDP120_DEBUG 
static void hexDump(uint8_t *buf, unsigned int size) {
    unsigned int i;
    unsigned int chunk_size;
    while(size) {
        chunk_size = umin(size,8);
        for(i = 0; i < chunk_size; ++i) {
            DEBUG_PRINTF("%02X ", *buf++);
        }
        DEBUG_PRINTF("\n");
        size -= chunk_size;
    }
}
#endif

static int
process_multisegment(struct syntiant_ndp_device_s *ndp)
{
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    syntiant_ndp120_multisegment_state_t *ms_state = &ndp120->ms_state;
    uint32_t size;
    int s = SYNTIANT_NDP_ERROR_NONE;

    #if NDP120_DEBUG 
        (void)hexDump;
    #endif

    if (pstate->is_multisegment) {
        uint8_t *cur = pstate->data.fw;

        size = (pstate->partially_read_length % PARSER_SCRATCH_SIZE)
            ? (pstate->partially_read_length % PARSER_SCRATCH_SIZE)
            : PARSER_SCRATCH_SIZE;

        while (size) {
#define DEBUG_MULTISEGMENT 0

            if (ms_state->bytes_rem_segment == 0) {
                /* new header */
                /* need to be careful, the parser chunk might
                    cut the header in half */

                if (ms_state->hdr_idx < sizeof(ms_state->hdr)) {
                    uint32_t bytes_to_read = MIN( (uint32_t)sizeof(ms_state->hdr) - ms_state->hdr_idx, size);
                    memcpy( ms_state->hdr + ms_state->hdr_idx, cur, bytes_to_read);
                    ms_state->hdr_idx += bytes_to_read;
                    size -= bytes_to_read;
                    cur += bytes_to_read;
                    /*
                    DEBUG_PRINTF("Added %u bytes to header, idx: %d, sizeof
                    header: "
                           "%lu",
                        bytes_to_read, ms_state->hdr_idx,
                    sizeof(ms_state->hdr)); printf("Header:\n");
                    printHex(ms_state->hdr, sizeof(ms_state->hdr));
                    */
                }

                if (ms_state->hdr_idx == sizeof(ms_state->hdr)) {
                    ms_state->adx = *(uint32_t *)ms_state->hdr;
                    ms_state->bytes_rem_segment = *(uint32_t *)(ms_state->hdr + sizeof(uint32_t));
                    ms_state->hdr_idx = 0;
#if DEBUG_MULTISEGMENT
                    DEBUG_PRINTF("Have full header. adx: 0x%X, size: %u",
                        ms_state->adx, ms_state->bytes_rem_segment);
#endif
                }
            } else {
                uint32_t bytes_to_write = MIN(size, ms_state->bytes_rem_segment);
                s = ndp_mcu_write_block(ms_state->adx, cur, bytes_to_write);
#if DEBUG_MULTISEGMENT
                if (ms_state->adx == 0x600800E0){
                    DEBUG_PRINTF("multiseg adx: %p", ms_state->adx);
                    hexDump(cur, bytes_to_write);
                }
#endif
                if (s)  {
                    DEBUG_PRINTF( "%s:%d ndp_mcu_write_block returned error %d\n", __FILE__, __LINE__, s);
                    goto error;
                }
                ms_state->adx += bytes_to_write;
                ms_state->bytes_rem_segment -= bytes_to_write;
                size -= bytes_to_write;
                cur += bytes_to_write;

#if DEBUG_MULTISEGMENT
                DEBUG_PRINTF("processing %u bytes, segment remaining: %u, chunk "
                       "remaining: %u",
                    bytes_to_write, ms_state->bytes_rem_segment, size);
#endif
            }
        }
    } else {
        s = SYNTIANT_NDP_ERROR_PACKAGE;
        goto error;
    }
error:
    return s;
}

static int
syntiant_ndp120_pkg_parse_dsp_firmware_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_multisegment_state_t *ms_state = &ndp->d.ndp120.ms_state;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;


    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* reset the DSP */
        uint32_t data_32 = 0;
        uint8_t data_8 = 0;

        /* if the normal MCU fw isn't loaded,
           we need to halt the MCU so the 
           bootloader doesn't respond */

        if (!ndp->d.ndp120.mcu_fw_state_addr) {
            halt_mcu(ndp);
            ndp_spi_write(NDP120_SPI_INTSTS, 0xff);
        }

        /* enable watermark interrupts */
        s = ndp_spi_read(NDP120_SPI_INTCTL, &data_8);
        if(s) goto error;
        data_8 = NDP120_SPI_INTCTL_WM_INTEN_INSERT(data_8,1);
        ndp_spi_write(NDP120_SPI_INTCTL, data_8);

        /* enable interrupt pin */
        s = ndp_spi_read(NDP120_SPI_CFG, &data_8);
        if(s) goto error;
        data_8 = NDP120_SPI_CFG_INTEN_INSERT(data_8, 1);
        ndp_spi_write(NDP120_SPI_CFG, data_8);


        memset(ms_state, 0, sizeof(syntiant_ndp120_multisegment_state_t));
        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_EN_DSP_CLK_INSERT(data_32,1); /* enable DSP clock */
        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_RUN_STALL_INSERT(data_32,1);
        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_BRESET_INSERT(data_32,1);
        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_DRESET_INSERT(data_32,1);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_DSP_CFG, data_32);
        if (s) goto error;

        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_DRESET_MASK_INSERT(data_32,0);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_DSP_CFG, data_32);
        if (s) goto error;

        data_32 = NDP120_CHIP_CONFIG_DSP_CFG_BRESET_MASK_INSERT(data_32,0);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_DSP_CFG, data_32);
        if (s) goto error;

        s = ndp_mcu_write(NDP120_CHIP_CONFIG_DSP_MEM_PGEN, 0x00); /* turn on DSP memories */
        if (s) goto error;

    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        s = process_multisegment(ndp);
    }

    /* post-read actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {

        uint32_t data = 0;
        ndp120_dsp_mailbox_msg_t msg;

        s = ndp_mcu_read(NDP120_CHIP_CONFIG_DSP_CFG, &data);
        data = NDP120_CHIP_CONFIG_DSP_CFG_RUN_STALL_MASK_INSERT(data, 0);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_DSP_CFG, data);
        if (s) goto error;

        ndp120->dsp_pcm_audio_sample_last_ptr = 0;
        ndp120->dsp_pcm_audio_annotation_last_ptr = 0;
        ndp120->dsp_function_sample_last_ptr = 0;

        DEBUG_PRINTF("waiting for RUNNING");
        s = watermarkint_recv(ndp, &msg);
        if(s) goto error;
        if (NDP120_DSP_MB_GET_MESSAGE(msg) != NDP120_DSP_MB_D2H_RUNNING) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("Did not receive 'running' from DSP");
            goto error;
        }
        DEBUG_PRINTF("getting dsp_fw_state_addr lower\n");
        syntiant_ndp120_do_mailbox_req_no_sync(ndp, NDP120_DSP_MB_H2D_ADX_LOWER, NULL);
        if (s) goto error;
    }

 error:
    return s;
}

static int ndp120_read_write_algo_config(struct syntiant_ndp_device_s *ndp, unsigned int index, void *data, unsigned int size, int read) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t adx;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    if (index >= NDP120_DSP_ALGO_CONFIG_MAX_COUNT) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (size > NDP120_DSP_ALGO_CONFIG_LEN_BYTES) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (!ndp120->dsp_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    adx = ndp120->dsp_fw_state_addr + (uint32_t)offsetof(ndp120_dsp_fw_base_t, algo_config);
    adx += index * NDP120_DSP_ALGO_CONFIG_LEN_BYTES;

    if (read) {
        s = ndp_mcu_read_block(adx, data, size);
    } else {
        s = ndp_mcu_write_block(adx, data, size);
    }
    if (s) goto error;

error:
    return s;
}

int
syntiant_ndp120_write_algo_config(struct syntiant_ndp_device_s *ndp,
        unsigned int index, void *data, unsigned int size)
{
    return ndp120_read_write_algo_config(ndp, index, data, size, 0);
}

int
syntiant_ndp120_read_algo_config(struct syntiant_ndp_device_s *ndp,
        unsigned int index, void *data, unsigned int size)
{
    return ndp120_read_write_algo_config(ndp, index, data, size, 1);
}

static int ndp120_read_write_audio_sync_config(struct syntiant_ndp_device_s *ndp, ndp120_dsp_audio_sync_config_t *config, int read) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t adx;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    if (!ndp120->dsp_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    adx = ndp120->dsp_fw_state_addr + (uint32_t)offsetof(ndp120_dsp_fw_state_t, audio_sync_config);

    if (read) {
        s = ndp_mcu_read_block(adx, config, sizeof(*config));
    } else {
        s = ndp_mcu_write_block(adx, config, sizeof(*config));
    }
    if (s) goto error;

error:
    return s;
}

int
syntiant_ndp120_write_audio_sync_config(struct syntiant_ndp_device_s *ndp, ndp120_dsp_audio_sync_config_t *config) {
    return ndp120_read_write_audio_sync_config(ndp, config, 0);
}

int
syntiant_ndp120_read_audio_sync_config(struct syntiant_ndp_device_s *ndp, ndp120_dsp_audio_sync_config_t *config) {
    return ndp120_read_write_audio_sync_config(ndp, config, 1);
}

static int
syntiant_ndp120_pkg_parse_dsp_fe_config_v1 (struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* nothing */
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        s = syntiant_ndp120_write_algo_config(ndp, pstate->data.dsp_fe_config.index,
            &pstate->data.dsp_fe_config.config, NDP120_DSP_ALGO_CONFIG_LEN_BYTES);
        if (s) goto error;
    }

    /* post-read actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
    }

    error:
    return s;
}

static int
syntiant_ndp120_pkg_parse_dsp_flow_collection_v1
(struct syntiant_ndp_device_s *ndp)
{
    int s = NDP_MB_ERROR_NONE;
    ndp120_dsp_data_flow_rule_t rule;
    syntiant_dsp_flow_collection_v1_t * parsed_rule;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    ndp120_dsp_data_flow_setup_t *setup =
        (ndp120_dsp_data_flow_setup_t *) pstate->metadata.scratch_metadata;

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        s = syntiant_ndp120_dsp_flow_setup_reset(setup);
        if(s) goto error;
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {

        parsed_rule = &(pstate->data.dsp_flow_collection.v1);

#if NDP120_DEBUG
        DEBUG_PRINTF("\nRule:\n");
        DEBUG_PRINTF("\tSet id: %d\n", parsed_rule->set_id);
        DEBUG_PRINTF("\tSrc type: %d\n", parsed_rule->src_type);
        DEBUG_PRINTF("\tSrc param: %d\n", parsed_rule->src_param);
        DEBUG_PRINTF("\tDest type: %d\n", parsed_rule->dst_type);
        DEBUG_PRINTF("\tDest param: %d\n", parsed_rule->dst_param);
        DEBUG_PRINTF("\tAlgo Config Index: %d\n", parsed_rule->algo_config_index);
#endif

        rule.set_id             = parsed_rule->set_id;
        rule.src_param          = parsed_rule->src_param;
        rule.dst_param          = parsed_rule->dst_param;
        rule.dst_type           = parsed_rule->dst_type;
        rule.algo_config_index  = parsed_rule->algo_config_index;
        s = syntiant_ndp120_dsp_flow_setup_add_rule(setup,
            &rule, parsed_rule->src_type);
        if(s) goto error;
    }

    /* post-read actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        s = syntiant_ndp120_dsp_flow_setup_apply_no_sync(ndp, setup);
        if(s) goto error;
    }

error:
    return s;
}

static int maybe_send_prepare(struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE; 

    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    if (ndp120->nn_metadata_loaded && ndp120->nn_params_loaded) {

        /* alerts DSP to init dnn state */
        s = syntiant_ndp120_do_mailbox_req_no_sync(
            ndp, NDP120_DSP_MB_H2D_PING, NULL);
        if (s) goto error;

        /* alerts MCU to init dnn state */
        s = syntiant_ndp120_do_mailbox_req_no_sync(
            ndp, SYNTIANT_NDP120_MB_MCU_PREPARE, NULL);
        if (s) goto error;
    }

error:
    return s;
}

static int
syntiant_ndp120_pkg_parse_nn_metadata(struct syntiant_ndp_device_s *ndp)
{
    int s = NDP_MB_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_nn_metadata_t config;
    syntiant_nn_metadata_v1_t *nn_metadata = &pstate->data.nn_metadata.v1;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

#if NDP120_DEBUG
    DEBUG_PRINTF("\nParser mode: %d\n", pstate->mode);
    DEBUG_PRINTF("\tStruct content:\n");
    DEBUG_PRINTF("\t\tNum NN: %d\n", nn_metadata->nn_num);
    DEBUG_PRINTF("\tBase Meta Content:\n");
    DEBUG_PRINTF("\t\tNum layers: %d\n", nn_metadata->base_meta.num_layers);
    DEBUG_PRINTF("\t\tIs cached: %d\n", nn_metadata->base_meta.is_nn_cached);
    DEBUG_PRINTF("\t\tInput ISA index: %d\n", nn_metadata->base_meta.nn_input_isa_idx);
    DEBUG_PRINTF("\t\tOutput ISA index: %d\n", nn_metadata->base_meta.nn_output_isa_idx);
    DEBUG_PRINTF("\t\tInput layer type: %d\n", nn_metadata->base_meta.nn_input_layer_type);
    DEBUG_PRINTF("\tInput Size Content:\n");
    DEBUG_PRINTF("\t\tx: %d, y: %d, z: %d\n",
        nn_metadata->inp_size.x, nn_metadata->inp_size.y, nn_metadata->inp_size.z);
    DEBUG_PRINTF("\tCoordinates content:\n");
    DEBUG_PRINTF("\t\tInput: 0x%X, Output: 0x%X\n",
        nn_metadata->coords.input_coord, nn_metadata->coords.output_coord);
    DEBUG_PRINTF("\tCache instructions content:\n");
    DEBUG_PRINTF("\t\tinput_base_coord_max: %d\n", nn_metadata->cache_params.input_base_coord_max);
    DEBUG_PRINTF("\t\toutput_base_coord_max: %d\n", nn_metadata->cache_params.output_base_coord_max);
    DEBUG_PRINTF("\t\tinput_base_coord_add: %d\n", nn_metadata->cache_params.input_base_coord_add);
    DEBUG_PRINTF("\t\tinput_offset_add: %d\n", nn_metadata->cache_params.input_offset_add);
    DEBUG_PRINTF("\t\tinput_offset_max: %d\n", nn_metadata->cache_params.input_offset_max);
    DEBUG_PRINTF("\t\toutput_base_coord_add: %d\n", nn_metadata->cache_params.output_base_coord_add);
    DEBUG_PRINTF("\t\toutput_base_coord_stride: %d\n", nn_metadata->cache_params.output_base_coord_stride);
    DEBUG_PRINTF("\tParser_params:\n");
    DEBUG_PRINTF("\t\tcurrent network: %d\n", nn_metadata->parser.cur_nn);
    DEBUG_PRINTF("\t\tcurrent layer: %d\n", nn_metadata->parser.cur_layer);
    DEBUG_PRINTF("\t\tparsing: %d\n", nn_metadata->parser.parsing);
    DEBUG_PRINTF("\t\tparsed: %d\n", nn_metadata->parser.parsed);
#endif

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* pre-write actions */
    }

    memset(&config, 0, sizeof(syntiant_ndp120_nn_metadata_t));
    switch (nn_metadata->parser.parsed) {
        case NNM_PARSE_NN_NUM:
            config.set = SYNTIANT_NDP120_SET_NN_NUM;
            config.nn_num = nn_metadata->nn_num;
            s = syntiant_ndp120_config_nn_metadata_no_sync(ndp, &config);
            if (s) goto error;
            break;

        case NNM_PARSE_BASE_META:
            config.set = SYNTIANT_NDP120_SET_LAYER_PER_NN |
                SYNTIANT_NDP120_SET_IS_NN_CACHED |
                SYNTIANT_NDP120_SET_INPUT_ISA_IDX |
                SYNTIANT_NDP120_SET_OUTPUT_ISA_IDX |
                SYNTIANT_NDP120_SET_INPUT_LAYER_TYPE;
            config.nn_idx = nn_metadata->parser.cur_nn - 1;
            config.layers_per_nn = nn_metadata->base_meta.num_layers;
            config.is_nn_cached = nn_metadata->base_meta.is_nn_cached;
            config.nn_input_isa_idx = nn_metadata->base_meta.nn_input_isa_idx;
            config.nn_output_isa_idx = nn_metadata->base_meta.nn_output_isa_idx;
            config.nn_input_layer_type = nn_metadata->base_meta.nn_input_layer_type;
            s = syntiant_ndp120_config_nn_metadata_no_sync(ndp, &config);
            if (s) goto error;
            break;

        case NNM_PARSE_INP_SIZE:
            config.set = SYNTIANT_NDP120_SET_INPUT_SIZE;
            config.nn_idx = nn_metadata->parser.cur_nn - 1;
            memcpy(&config.nn_input_layer_size, &nn_metadata->inp_size,
                sizeof(nn_metadata->inp_size));
            s = syntiant_ndp120_config_nn_metadata_no_sync(ndp, &config);
            if (s) goto error;
            break;

        case NNM_PARSE_COORD:
            config.set = SYNTIANT_NDP120_SET_INPUT_COORD |
                SYNTIANT_NDP120_SET_OUTPUT_COORD;
            config.nn_idx = nn_metadata->parser.cur_nn - 1;
            config.layer_idx = nn_metadata->parser.cur_layer - 1;
            config.input_coord = nn_metadata->coords.input_coord;
            config.output_coord = nn_metadata->coords.output_coord;
            s = syntiant_ndp120_config_nn_metadata_no_sync(ndp, &config);
            if (s) goto error;
            break;

        case NNM_PARSE_CACHE_INST:
            config.set = SYNTIANT_NDP120_SET_CACHE_INST;
            config.nn_idx = nn_metadata->parser.cur_nn - 1;
            config.layer_idx = nn_metadata->parser.cur_layer - 1;
            memcpy(&config.cache_inst, &nn_metadata->cache_params,
                sizeof(nn_metadata->cache_params));
            s = syntiant_ndp120_config_nn_metadata_no_sync(ndp, &config);
            if (s) goto error;
            break;
    }

    if (pstate->mode == PACKAGE_MODE_TAG_START) {
        ndp120->nn_metadata_loaded = 1;
        maybe_send_prepare(ndp);
    }

error:
    return s;
}

static int
syntiant_ndp120_pkg_parse_mcu_orch_params(struct syntiant_ndp_device_s *ndp) {
    int s = NDP_MB_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint32_t *node_count = pstate->metadata.scratch_metadata;
    syntiant_mcu_orch_v1_t * parsed_node;
    syntiant_ndp120_mcu_orch_t config;

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* pre-read actions */
        *node_count = 0;
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {

        parsed_node = &(pstate->data.mcu_orchestrator.v1);
        (*node_count) += 1;

        memset(&config, 0, sizeof(config));
        config.set = SYNTIANT_NDP120_SET_NODE;
        config.node_idx = *node_count - 1;
        config.id = parsed_node->id;
        config.flow_id = parsed_node->flow_id;
        config.status = parsed_node->status;
        config.type = parsed_node->type;
        config.action = parsed_node->action;
        config.num_inputs = parsed_node->num_inputs;
        config.num_outputs = parsed_node->num_outputs;
        memcpy(&config.input_edges, &parsed_node->input_edges,
            sizeof(parsed_node->input_edges));
        memcpy(&config.next_ids, &parsed_node->next_ids,
            sizeof(parsed_node->next_ids));

        s = syntiant_ndp120_config_mcu_orchestrator_no_sync(ndp, &config);
        if (s) goto error;

#if NDP120_DEBUG
        DEBUG_PRINTF("\nNode:\n");
        DEBUG_PRINTF("\tid: %d\n", parsed_node->id);
        DEBUG_PRINTF("\tflow id: %d\n", parsed_node->flow_id);
        DEBUG_PRINTF("\tstatus: %d\n", parsed_node->status);
        DEBUG_PRINTF("\ttype: %d\n", parsed_node->type);
        DEBUG_PRINTF("\taction: %d\n", parsed_node->action);
        DEBUG_PRINTF("\tnum inputs: %d\n", parsed_node->num_inputs);
        DEBUG_PRINTF("\tnum outputs: %d\n", parsed_node->num_outputs);
#endif
    }

    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        /* post-read actions */
        memset(&config, 0, sizeof(config));
        config.set = SYNTIANT_NDP120_SET_NUM_NODES | SYNTIANT_NDP120_SET_FLOWMAP;
        config.num_nodes = *node_count;
        config.flowmap = 0x1;    /* SET0 is the starting set */
        /* DEBUG_PRINTF("\nNum nodes: %d\n", *node_count); */

        s = syntiant_ndp120_config_mcu_orchestrator_no_sync(ndp, &config);
        if (s) goto error;
    }

error:
    return s;
}

static char * syntiant_ndp120_get_config_devtype(uint32_t device_type) {
    char *s = "unknown";

    switch (device_type) {
        case 0x30: /* fall through */
        case 0x31: /* fall through */
        case 0x32: /* fall through */
        case 0x33:
            s = "NDP120-A0 ES";
            break;

        case 0x34: /* fall through */
        case 0x35: /* fall through */
        case 0x36: /* fall through */
        case 0x37: /* fall through */
        case 0x38: /* fall through */
        case 0x39: /* fall through */
        case 0x3A: /* fall through */
        case 0x3B:
            s = "NDP120-B0";
            break;
    }
    return s;
}

static int 
syntiant_ndp120_access_config(struct syntiant_ndp_device_s *ndp, 
                              struct syntiant_ndp_config_s *config) {
    uint32_t length_in_scratch, len, valid, labels_len, firmware_version_len,
        parameters_version_len, pkg_version_len, dsp_firmware_version_len;
    int s;
    char *buffer = NULL;

    /* check valid bits */
    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
    s = SYNTIANT_NDP_ERROR_NONE;


    labels_len = config->labels_len;
    firmware_version_len = config->firmware_version_len;
    parameters_version_len = config->parameters_version_len;
    pkg_version_len = config->pkg_version_len;
    dsp_firmware_version_len = config->dsp_firmware_version_len;

    config->labels_len = 0;
    config->firmware_version_len = 0;
    config->parameters_version_len = 0;
    config->pkg_version_len = 0;
    config->dsp_firmware_version_len = 0;
    
    if (labels_len & 3) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (firmware_version_len & 3) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (parameters_version_len & 3) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (pkg_version_len & 3) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    if (dsp_firmware_version_len & 3) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    /* labels */
    if (valid & SYNTIANT_CONFIG_LABELS_VALID) {
        s = ndp_mcu_read(SCRATCH_VARIABLE_ADX(label_size), &length_in_scratch);
        if (s) goto error;

        buffer = config->labels;
        len = umin(labels_len, length_in_scratch);
        if (buffer && len > 0) {
            s = ndp_mcu_read_block(SCRATCH_VARIABLE_ADX(labels), buffer, len);
            if (s) goto error;
        }
        config->labels_len = length_in_scratch;
    }

    /*fw version*/

    if (valid & SYNTIANT_CONFIG_FW_VERSION_VALID) {
        s = syntiant_ndp120_read(ndp, 1, SCRATCH_VARIABLE_ADX(fw_version_size), &length_in_scratch);
        if (s) goto error;
        if (length_in_scratch < NDP120_ILIB_SCRATCH_LENGTH) {
            buffer = config->firmware_version;
            len = umin(firmware_version_len, length_in_scratch);
            if (buffer && 0 < len) {
                s = ndp_mcu_read_block(SCRATCH_VARIABLE_ADX(fw_version) , buffer, len);
                if (s) goto error;
            }
            config->firmware_version_len = length_in_scratch;
        }
    }

    /*dsp fw version*/
    if (valid & SYNTIANT_CONFIG_DSP_FW_VERSION_VALID) {
        s = ndp_mcu_read(SCRATCH_VARIABLE_ADX(dsp_fw_version_size), &length_in_scratch);
        if (s) goto error;
        if (length_in_scratch < NDP120_ILIB_SCRATCH_LENGTH) {
            buffer = config->dsp_firmware_version;
            len = umin(dsp_firmware_version_len, length_in_scratch);
            if (buffer && 0 < len) {
                s = ndp_mcu_read_block(SCRATCH_VARIABLE_ADX(dsp_fw_version), buffer, len);
                if (s) goto error;
            }
            config->dsp_firmware_version_len = length_in_scratch;
        }
    }

    /*nn version*/
    if (valid & SYNTIANT_CONFIG_NN_VERSION_VALID) {
        s = ndp_mcu_read( SCRATCH_VARIABLE_ADX(params_version_size), &length_in_scratch);
        if (s) goto error;
        if (length_in_scratch < NDP120_ILIB_SCRATCH_LENGTH) {
            buffer = config->parameters_version;
            len = umin(parameters_version_len, length_in_scratch);
            if (buffer && 0 < len) {
                s = ndp_mcu_read_block(SCRATCH_VARIABLE_ADX(params_version), buffer, len);
                if (s) goto error;
            }
            config->parameters_version_len = length_in_scratch;
        }
    }

    /*pkg version*/
    if (valid & SYNTIANT_CONFIG_PKG_VERSION_VALID) {
        s = ndp_mcu_read(SCRATCH_VARIABLE_ADX(pkg_version_size), &length_in_scratch);
        if (s) goto error;
        if (length_in_scratch < NDP120_ILIB_SCRATCH_LENGTH) {
            buffer = config->pkg_version;

            len = umin(pkg_version_len, length_in_scratch);
            if (buffer && 0 < len) {
                s = ndp_mcu_read_block (SCRATCH_VARIABLE_ADX(pkg_version), buffer, len);
                if (s) goto error;
            }
            config->pkg_version_len = length_in_scratch;
        }
    }

error:
    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    return s;
}

static int
syntiant_ndp120_verify_configuration(struct syntiant_ndp_device_s *ndp){

    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    int s;
    struct syntiant_ndp_config_s config;
    memset(&config, 0, sizeof(struct syntiant_ndp_config_s));
    s = syntiant_ndp120_access_config(ndp, &config);

    /* label length*/
    if (config.labels_len > 0){
        if (config.labels_len != ndp120->labels_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("labels config len: %d, internal_len: %d", config.labels_len, ndp120->labels_len);
            goto error;
        }
    }

    /* firmware version length */
    if (config.firmware_version_len > 0){
        if (config.firmware_version_len != ndp120->fwver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("fw_ver");
            goto error;
        }
    }

    /* dsp firmware version length */
    if (config.dsp_firmware_version_len > 0){
        if (config.dsp_firmware_version_len != ndp120->dspfwver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("dsp_fw_ver");
            goto error;
        }
    }

    /* params version length */
    if (config.parameters_version_len > 0){
        if (config.parameters_version_len != ndp120->paramver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("param_ver");
            goto error;
        }
    }

    /* pkg version length */
    if (config.pkg_version_len > 0){
        if (config.pkg_version_len != ndp120->pkgver_len){
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("pkg_ver");
            goto error;
        }
    }

error:
    if (s) DEBUG_PRINTF("syntiant_ndp_120_verify_config failed");
    return s;

}

static int
syntiant_ndp120_pkg_parse_board_calibration_params_v1_v2_tlv(
    struct syntiant_ndp_device_s *ndp, int padded)
{

    int s = SYNTIANT_NDP_ERROR_NONE;
    (void)ndp;
    (void)padded;
#if 0
    syntiant_ndp120_config_t ndp120_config;
    int i = 0;

    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    uint8_t *dnn_input = padded ?
                        &pstate->data.board_params.board_params_v2.dnn_input :
                        &pstate->data.board_params.board_params_v1.dnn_input;

    if (pstate->mode == PACKAGE_MODE_TAG_START &&
        pstate->partially_read_length == *(uint32_t*)pstate->length) {

        /* translate dnn inputs */
        switch (*dnn_input) {
        case SYNTIANT_PACKAGE_DNN_INPUT_NONE:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_NONE;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM0:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_PDM0;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM1:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_PDM1;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_PDM_SUM:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_PDM_SUM;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_LEFT:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_I2S_LEFT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_RIGHT:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_I2S_RIGHT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_SUM:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_I2S_SUM;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_MONO:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_I2S_MONO;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_I2S_DIRECT:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_I2S_DIRECT;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_SPI:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_SPI;
            break;

        case SYNTIANT_PACKAGE_DNN_INPUT_SPI_DIRECT:
            *dnn_input = NDP120_CONFIG_SET_DNN_INPUT_SPI_DIRECT;
            break;

        default:
            s = SYNTIANT_NDP_ERROR_PACKAGE;
            goto error;
        }
        memset(&ndp120_config, 0, sizeof(syntiant_ndp120_config_misc_t));
        ndp120_config.dnn_input = *dnn_input;

        ndp120_config.input_clock_rate = padded ?
            pstate->data.board_params.board_params_v2.input_clock_rate:
            pstate->data.board_params.board_params_v1.input_clock_rate;

        ndp120_config.pdm_clock_rate = padded ?
                pstate->data.board_params.board_params_v2.pdm_clock_rate:
                pstate->data.board_params.board_params_v1.pdm_clock_rate;

        for (i = 0 ; i < 2; i++){
            ndp120_config.pdm_dc_offset[i] = (padded ?
                pstate->data.board_params.board_params_v2.pdm_dc_offset[i]:
                pstate->data.board_params.board_params_v1.pdm_dc_offset[i]);
        }

        ndp120_config.pdm_clock_ndp = padded ?
            pstate->data.board_params.board_params_v2.pdm_clock_ndp:
            pstate->data.board_params.board_params_v1.pdm_clock_ndp;

        ndp120_config.power_offset = padded ?
            pstate->data.board_params.board_params_v2.power_offset:
            pstate->data.board_params.board_params_v1.power_offset;

        ndp120_config.preemphasis_exponent = padded ?
            pstate->data.board_params.board_params_v2.preemphasis_exponent:
            pstate->data.board_params.board_params_v1.preemphasis_exponent;

        for (i = 0 ; i < 2; i++){
            ndp120_config.pdm_in_shift[i] = (padded ?
                pstate->data.board_params.board_params_v2.cal_pdm_in_shift[i]:
                pstate->data.board_params.board_params_v1.cal_pdm_in_shift[i]);
        }

        for (i = 0 ; i < 2; i++){
            ndp120_config.pdm_out_shift[i] = (padded ?
                pstate->data.board_params.board_params_v2.cal_pdm_out_shift[i]:
                pstate->data.board_params.board_params_v1.cal_pdm_out_shift[i]);
        }

        ndp120_config.power_scale_exponent = padded ?
            pstate->data.board_params.board_params_v2.power_scale_exponent:
            pstate->data.board_params.board_params_v1.power_scale_exponent;

        ndp120_config.agc_on = padded ?
            pstate->data.board_params.board_params_v2.agc:
            pstate->data.board_params.board_params_v1.agc;

        for (i = 0 ; i < NDP120_CONFIG_SET_MISC_MAX_BINS ; i++){
            ndp120_config.filter_eq[i] = padded ?
                pstate->data.board_params.board_params_v2.equalizer[i]:
                pstate->data.board_params.board_params_v1.equalizer[i];
        }

        ndp120_config.set = (NDP120_CONFIG_SET_MISC_SET_INPUT |
            NDP120_CONFIG_SET_MISC_INPUT_CLOCK_RATE |
            NDP120_CONFIG_SET_MISC_PDM_CLOCK_RATE |
            NDP120_CONFIG_SET_MISC_PDM_DC_OFFSET |
            NDP120_CONFIG_SET_MISC_PDM_CLOCK_NDP |
            NDP120_CONFIG_SET_MISC_POWER_OFFSET |
            NDP120_CONFIG_SET_MISC_PREEMPHASIS_EXPONENT |
            NDP120_CONFIG_SET_MISC_PDM_IN_SHIFT |
            NDP120_CONFIG_SET_MISC_PDM_OUT_SHIFT |
            NDP120_CONFIG_SET_MISC_POWER_SCALE_EXPONENT |
            NDP120_CONFIG_SET_MISC_AGC_ON |
            NDP120_CONFIG_SET_MISC_FILTER_EQ );
        s = syntiant_ndp120_config_no_sync(ndp, &ndp120_config);
        if (s) goto error;
    }
error:
#endif
    return s;
}


static int
syntiant_ndp120_pkg_parse_nn_params_v1_tlv(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_multisegment_state_t *ms_state = &ndp->d.ndp120.ms_state;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;

    if (pstate->mode == PACKAGE_MODE_VALUE_START) {
        /* NN pre-write code */
        memset(ms_state, 0, sizeof(syntiant_ndp120_multisegment_state_t));
    } else if (pstate->value_mode == PACKAGE_MODE_VALID_PARTIAL_VALUE) {
        s = process_multisegment(ndp);
    }

    /* post-write actions */
    if (pstate->partially_read_length == *(uint32_t *)pstate->length) {
        ndp120->nn_params_loaded = 1;
        maybe_send_prepare(ndp);
    }
    return s;
}

static int
syntiant_ndp120_parse_tag_values(struct syntiant_ndp_device_s *ndp)
{
#if NDP120_DEBUG
    static uint32_t last_tag = 0xFFFFFFFF;
#endif
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    int aligned_value = 0;

#if NDP120_DEBUG
    if (last_tag != *(uint32_t *)pstate->tag) {
        DEBUG_PRINTF("processing tag %d", *(uint32_t *)pstate->tag);
        last_tag = *(uint32_t *)pstate->tag;
    }
#endif

    switch (*(uint32_t *)pstate->tag) {

    case TAG_FIRMWARE_VERSION_STRING_V1:
    case TAG_DSP_FIRMWARE_VERSION_STRING_V1:
    case TAG_NN_VERSION_STRING_V1:
    case TAG_PACKAGE_VERSION_STRING:
        s = syntiant_ndp120_pkg_parse_version_string_v1_tlv(ndp);
        break;

    case TAG_BOARD_CALIBRATION_PARAMS_V2:
        aligned_value = 1;
        /* fallthrough */
    case TAG_BOARD_CALIBRATION_PARAMS_V1:
        s = syntiant_ndp120_pkg_parse_board_calibration_params_v1_v2_tlv(
            ndp, aligned_value);
        break;

    case TAG_NN_LABELS_V1:
    case TAG_NN_LABELS_V3:
        s = syntiant_ndp120_pkg_parse_nn_labels_v1_v3_tlv(ndp);
        break;

#if 0
    case TAG_NN_PH_PARAMETERS_V4:
        /*TODO: remove*/
        s = syntiant_ndp120_pkg_parse_ph_params_v4_tlv(ndp);
        break;

    case TAG_NN_PH_PARAMETERS_V5:
        /*TODO: remove*/
        s = syntiant_ndp120_pkg_parse_ph_params_v5_tlv(ndp);
        break;
#endif

    case TAG_NN_PHP_COLLECTION_V1:
        s = syntiant_ndp120_pkg_parse_php_collection_v1_tlv(ndp);
        break;

    case TAG_NDP120_B0_NN_PARAMETERS_V1:
        s = syntiant_ndp120_pkg_parse_nn_params_v1_tlv(ndp);
        break;

    case TAG_NDP120_B0_MCU_ORCHESTRATOR_V1:
        s = syntiant_ndp120_pkg_parse_mcu_orch_params(ndp);
        break;

    /* Firmware */
    case TAG_FIRMWARE:
        ERROR_PRINTF(
            "warning: found 'TAG_FIRMWARE' tag, treating as if it is a "
            "TAG_NDP120_A0_FIRMWARE_V1 tag");
        /* fallthrough */

    case TAG_NDP120_B0_FIRMWARE_V1:
        s = syntiant_ndp120_pkg_parse_firmware_tlv(ndp);
        break;

    case TAG_NDP120_B0_DSP_FIRMWARE_V1:
        s = syntiant_ndp120_pkg_parse_dsp_firmware_tlv(ndp);
        break;

    case TAG_NDP120_B0_NN_METADATA:
        s = syntiant_ndp120_pkg_parse_nn_metadata(ndp);
        break;

    case TAG_NDP120_B0_DSP_FLOW_COLLECTION_V1:
        s = syntiant_ndp120_pkg_parse_dsp_flow_collection_v1(ndp);
        break;

    case TAG_NDP120_B0_DSP_FE_CONFIG_V1:
        s = syntiant_ndp120_pkg_parse_dsp_fe_config_v1(ndp);
        break;

    case TAG_PACKAGERLIB_VERSION_STRING:
        /* irrelevant to ilib. Used for determining packager version. */
        break;

    case TAG_HEADER:
        /* intentionally ignore */
        break;

    case TAG_CHECKSUM:
        /* intentionally ignore */
        break;

    default:
        s = SYNTIANT_PACKAGE_ERROR_UNKNOWN_TLV;
        DEBUG_PRINTF("%s:%d Unknown TLV %d", __FILE__, __LINE__,
            *(uint32_t *)pstate->tag);
        break;
    }
    return s;
}

static void erase_scratch(struct syntiant_ndp_device_s *ndp) {
#if NDP120_DEBUG_SIMULATOR
    uint8_t zeroes[512]; /* sim runs much faster with large chunk sizes */
#else
    uint8_t zeroes[32];
#endif
    uint32_t cur = NDP120_ILIB_SCRATCH_ORIGIN;
    memset(zeroes, 0, sizeof(zeroes));
    while (cur < NDP120_ILIB_SCRATCH_ORIGIN + NDP120_ILIB_SCRATCH_LENGTH) {
        ndp_mcu_write_block(cur, zeroes, sizeof(zeroes));
        cur += (uint32_t)sizeof(zeroes);
    }
}

int syntiant_ndp120_test_function(struct syntiant_ndp_device_s *ndp) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t valid;

    syntiant_ndp120_scratch_get_valid(ndp, &valid);

    DEBUG_PRINTF("valid: 0x%08X", valid);

    return s;
}

/* ========================= */
/* EXPORTED DRIVER FUNCTIONS */
/* ========================= */

static int
syntiant_ndp120_init(struct syntiant_ndp_device_s *ndp,
                     enum syntiant_ndp_init_mode_e init_mode)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t data;
    uint8_t spi_data;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    memset(ndp120, 0, sizeof(*ndp120));

    switch (init_mode) {
    case SYNTIANT_NDP_INIT_MODE_NO_TOUCH:
        break;

    case SYNTIANT_NDP_INIT_MODE_RESTART:
        ndp_spi_read(NDP120_SPI_INTCTL, &spi_data);
        spi_data = NDP120_SPI_INTCTL_WM_INTEN_INSERT(spi_data, 1);
        spi_data = NDP120_SPI_INTCTL_MBOUT_INTEN_INSERT(spi_data, 1);
        spi_data = NDP120_SPI_INTCTL_MBIN_INTEN_INSERT(spi_data, 1);
        ndp_spi_write(NDP120_SPI_INTCTL, spi_data);

        ndp_spi_read(NDP120_SPI_CFG, &spi_data);
        spi_data = NDP120_SPI_CFG_INTEN_INSERT(spi_data, 1);
        ndp_spi_write(NDP120_SPI_CFG, spi_data);


        /* hacky stuff to allow things to "work"
         
           It's arguable that init -N restart shouldn't 
           call sync() or set ndp->init to 0 
           ¯\_(ツ)_/¯ */

        ndp->iif.unsync(ndp->iif.d); /* allow processing */
        ndp->init = 1;

        /* end hacky stuff */

        /* clear any outstanding notifications */
        /* 
           intentionally not checking return codes here, 
           it's probably ok if these fail */

        s = syntiant_ndp120_poll(ndp, &data, 1);
        mailbox_reset_state(ndp);
        syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_MIADDR, NULL);
        syntiant_ndp120_do_mailbox_req_no_sync(ndp, NDP120_DSP_MB_H2D_ADX_LOWER, NULL);

        if (ndp120->dsp_fw_state_addr) {
            uint32_t adx, data;
            adx = ndp120->mcu_fw_state_addr +
                (uint32_t) offsetof(ndp120_dsp_fw_base_t, config.func_samp_size_bytes);
            s = ndp_mcu_read(adx, &data);
            if (s) goto error;
            ndp120->max_feature_len = data;
            /* DEBUG_PRINTF("syntiant_ndp120_init: max_feature_len=%d\n", ndp120->max_feature_len); */
        }
        else {
            /* default to ndp120 feature extractor length */
            ndp120->max_feature_len = 1028;
        }

        if (ndp120->mcu_fw_state_addr) {
            uint32_t addr;
            addr = ndp120->mcu_fw_state_addr + (uint32_t) offsetof(struct ndp120_fw_state_s, match_producer);
            s = ndp_mcu_read(addr, &ndp120->match_producer);
            if (s) goto error;
            memcpy(ndp120->match_consumer, ndp120->match_producer,
                    sizeof(ndp120->match_consumer));
        }
        
        break;

    case SYNTIANT_NDP_INIT_MODE_RESET:
        /* full POR */
        s = ndp_spi_read(NDP120_SPI_CTL, &data);
        if (s) goto error;
        data = NDP120_SPI_CTL_PORSTN_MASK_INSERT(data, 0);
        s = ndp_spi_write(NDP120_SPI_CTL, data);
        if (s) goto error;
        while(1) {
            ndp_spi_read(NDP120_SPI_ID0, &spi_data);
            if (spi_data != 0xff) break;
        }
        /* Drive the interrupt line output active high interrupts */
        spi_data = NDP120_SPI_CFG_INTEN(1) | NDP120_SPI_CFG_INTNEG(0);
        s = ndp_spi_write(NDP120_SPI_CFG, spi_data);
        if (s) goto error;

        ndp120->nn_params_loaded = 0;
        ndp120->nn_metadata_loaded = 0;

        erase_scratch(ndp);
        syntiant_ndp120_scratch_set_valid(ndp, 0);
        break;

    default:
        s = SYNTIANT_NDP_ERROR_ARG;
        break;
    }

    goto error;
error:
    ndp->init = 0;
    return s;
}

static int
syntiant_ndp120_uninit(
    struct syntiant_ndp_device_s *ndp, enum syntiant_ndp_init_mode_e init_mode)
{
    (void)*ndp;
    (void)init_mode;
    return SYNTIANT_NDP_ERROR_NONE;
}

static int
syntiant_ndp120_op_size(
    struct syntiant_ndp_device_s *ndp, int mcu, unsigned int *size)
{
    (void)*ndp;
    *size = OP_SIZE(mcu);
    return SYNTIANT_NDP_ERROR_NONE;
}

static int
syntiant_ndp120_interrupts(struct syntiant_ndp_device_s *ndp, int *causes)
{
    int s;
    uint8_t intctl, intctl_old;
    int cs = *causes;

    if (cs > SYNTIANT_NDP_INTERRUPT_ALL
        && cs != SYNTIANT_NDP_INTERRUPT_DEFAULT) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    s = syntiant_ndp120_read(ndp, 0, NDP120_SPI_INTCTL, &intctl);
    if (s) goto error;

    intctl_old = intctl;

    if (cs >= 0) {
        if (cs == SYNTIANT_NDP_INTERRUPT_DEFAULT) {
            intctl = NDP120_SPI_INTCTL_MBIN_INTEN(1)
                | NDP120_SPI_INTCTL_WM_INTEN(1)
                | NDP120_SPI_INTCTL_MBOUT_INTEN(1)
                | NDP120_SPI_INTCTL_AE_INTEN(1);
        } else {
            intctl = (uint8_t)
                (NDP120_SPI_INTCTL_MBIN_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_MAILBOX_IN))
                | NDP120_SPI_INTCTL_MBOUT_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_MAILBOX_OUT))
                | NDP120_SPI_INTCTL_DNN_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_DNN_FRAME))
                | NDP120_SPI_INTCTL_FEATURE_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_FEATURE))
                | NDP120_SPI_INTCTL_AE_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_ADDRESS_ERROR))
                | NDP120_SPI_INTCTL_WM_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_WATER_MARK))
                | NDP120_SPI_INTCTL_RF_INTEN
                (!!(cs & SYNTIANT_NDP_INTERRUPT_SPI_READ_FAILURE)));
        }
        s = syntiant_ndp120_write(ndp, 0, NDP120_SPI_INTCTL, intctl);
        if (s) goto error;
    }

    cs =
        ((NDP120_SPI_INTCTL_MBIN_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_MAILBOX_IN : 0)
        | ((NDP120_SPI_INTCTL_MBOUT_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_MAILBOX_OUT : 0)
        | ((NDP120_SPI_INTCTL_DNN_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_DNN_FRAME : 0)
        | ((NDP120_SPI_INTCTL_FEATURE_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_FEATURE : 0)
        | ((NDP120_SPI_INTCTL_AE_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_ADDRESS_ERROR : 0)
        | ((NDP120_SPI_INTCTL_WM_INTEN(1) & intctl_old)
           ? SYNTIANT_NDP_INTERRUPT_WATER_MARK : 0)
        | ((NDP120_SPI_INTCTL_RF_INTEN(1) & intctl_old) 
           ? SYNTIANT_NDP_INTERRUPT_SPI_READ_FAILURE : 0);

    *causes = cs;

error:
    return s;
}


static int
syntiant_ndp120_load_via_bootloader(
    struct syntiant_ndp_device_s *ndp, uint8_t *chunk_ptr, uint32_t chunk_len)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t data, error_code, is_error;
    uint32_t msg, window_size, bytes_to_copy; 
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    int i; 
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    syntiant_ndp120_bootloader_state_t *bl_state = &ndp120->bl_state;

    if (bl_state->mode == SYNTIANT_NDP120_BOOTLOADER_MODE_START) {
        /* Step 0: Reset the chip, wait for BOOTING on MBOUT */

        /* clear interrupts */
        ndp_spi_write(NDP120_SPI_INTSTS, 0xff);

        /* enable interrupts */
        s = ndp_spi_read(NDP120_SPI_CFG, &data);
        if (s) goto error;

        data = NDP120_SPI_CFG_INTEN_INSERT(data, 1);
        s = ndp_spi_write(NDP120_SPI_CFG, data);
        if (s) goto error;

        /* enable MBIN & MBOUT interrupts */
        data = 0;
        data = NDP120_SPI_INTCTL_MBIN_INTEN_INSERT(data, 1);
        data = NDP120_SPI_INTCTL_MBOUT_INTEN_INSERT(data, 1);
        ndp_spi_write(NDP120_SPI_INTCTL, data);
        mailbox_reset_state(ndp);

        /* Reset the chip */
        data = 0;
        data = NDP120_SPI_CTL_RESETN_INSERT(data, 0);
        data = NDP120_SPI_CTL_FLASHCTL_INSERT(data, NDP120_SPI_CTL_FLASHCTL_DISABLE);
        data = NDP120_SPI_CTL_PORSTN_INSERT(data, 1);
        data = NDP120_SPI_CTL_CLKEN_INSERT(data, 1);
        ndp_spi_write(NDP120_SPI_CTL, data);

        /* deassert reset */
        data = NDP120_SPI_CTL_RESETN_INSERT(data, 1);
        ndp_spi_write(NDP120_SPI_CTL, data);

        s = mbout_recv(ndp, &data);
        if (s) goto error;
        mbout_send_resp(ndp, SYNTIANT_NDP120_MB_MCU_ACK);

        if (data != SYNTIANT_NDP120_MB_MCU_BOOTING) {
            DEBUG_PRINTF("expecting BOOTING on mbout; received: 0x%02X", data);
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }

        /* Step 1: Handshake with bootloader */
        /* Handshake 0 */

        for (i = 0; i < 2; ++i) {
            s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_NOP, NULL);
            if (s) goto error;
        }
        DEBUG_PRINTF("handhake complete");

        /* Step 2:
               send LOAD
               await CONT
        */
        s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_LOAD, &msg);
        data = (uint8_t)msg;
        if (s) goto error;
        if (data != SYNTIANT_NDP120_MB_MCU_CONT) {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }

        /* Step 3: get "load area" from
                addrprotlo_0 and
                addrprothi_0
        */
        ndp_mcu_read_block(0x20007f80, &bl_state->window_lower,
            sizeof(bl_state->window_lower));
        ndp_mcu_read_block(0x20007f84, &bl_state->window_upper,
            sizeof(bl_state->window_upper));

        window_size = bl_state->window_upper - bl_state->window_lower;
        DEBUG_PRINTF("lower: 0x%X upper: 0x%X size: 0x%X",
            bl_state->window_lower, bl_state->window_upper, window_size);

        if (window_size > 8192) {
            ERROR_PRINTF("aborting, protected region size > 8192\n");
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }

        ndp120->bl_state.mode = SYNTIANT_NDP120_BOOTLOADER_MODE_IN_PROGRESS;
    }

    if (bl_state->mode == SYNTIANT_NDP120_BOOTLOADER_MODE_IN_PROGRESS) {
        window_size = bl_state->window_upper - bl_state->window_lower;

        /* MCU requires writes to be on 4-byte boundaries.  Oddball chunk sizes
         * remainders are stored in bl_state->remainder */
        if (bl_state->remainder_len > 0) {
            bytes_to_copy = MIN(4 - bl_state->remainder_len, chunk_len);
            if (bytes_to_copy > 0) {
                /* try to bring remainder up to 4 bytes */
                memcpy(bl_state->remainder + bl_state->remainder_len, chunk_ptr,
                    bytes_to_copy);
                chunk_len -= bytes_to_copy;
                chunk_ptr += bytes_to_copy;
                bl_state->remainder_len += bytes_to_copy;
            }
            /* cover edge case where chunks are < 4 bytes */
            if (bl_state->remainder_len == 4)  {
                s = ndp_mcu_write_block(
                    bl_state->window_lower + bl_state->window_idx,
                    bl_state->remainder, 4);
                bl_state->window_idx += 4;
                bl_state->remainder_len = 0;
            }
        }

        while (chunk_len) {
            if (chunk_len < 4) {
                memcpy(bl_state->remainder, chunk_ptr, chunk_len);
                bl_state->remainder_len = chunk_len;
                chunk_len = 0;
                break;
            }
            bytes_to_copy = MIN(window_size - bl_state->window_idx, chunk_len);
            bytes_to_copy -= bytes_to_copy % 4; /* trim to 4 byte boundary */

            /* this is sometimes 0 for odd chunk sizes*/
            if (bytes_to_copy > 0) {
                ndp_mcu_write_block(
                    bl_state->window_lower + bl_state->window_idx, chunk_ptr,
                    bytes_to_copy);
                chunk_len -= bytes_to_copy;
                chunk_ptr += bytes_to_copy;
                bl_state->window_idx += bytes_to_copy;
            }

            if (bl_state->window_idx == window_size
                || pstate->mode == PACKAGE_MODE_DONE) {
                s = syntiant_ndp120_do_mailbox_req_no_sync(ndp, SYNTIANT_NDP120_MB_MCU_CONT, &msg);
                bl_state->window_idx = 0;
                data = (uint8_t)msg;

                if (data != SYNTIANT_NDP120_MB_MCU_NOP) {
                    DEBUG_PRINTF(
                        "received 0x%02X instead of expected NOP", data);
                    s = SYNTIANT_NDP_ERROR_FAIL;
                    goto error;
                }

                s = mbout_recv(ndp, &data);
                if (s) goto error;
                mbout_send_resp(ndp, SYNTIANT_NDP120_MB_MCU_NOP);

                is_error = mb_resp_is_error(data, &error_code);
                if (data == SYNTIANT_NDP120_MB_MCU_LOAD_DONE) {
                    DEBUG_PRINTF("load done");
                    bl_state->mode = SYNTIANT_NDP120_BOOTLOADER_MODE_COMPLETE;
                } else if (is_error) {
                    char buf[32];
                    syntiant_ndp120_mcu_mb_error_decoder(
                        error_code, buf, sizeof(buf));
                    DEBUG_PRINTF("load error: %s (0x%02X)", buf, error_code);
                    s = SYNTIANT_NDP_ERROR_FAIL;
                    goto error;
                } else if (data != SYNTIANT_NDP120_MB_MCU_CONT) {
                    DEBUG_PRINTF("Received 0x%02X, aborting", data);
                    s = SYNTIANT_NDP_ERROR_FAIL;
                    goto error;
                }
            } /* if ("window full") */
        }    /* while (chunk_len) */
    } else {
        s = SYNTIANT_NDP_ERROR_FAIL;
    }

error:
    return s;
}

static int
syntiant_ndp120_load(struct syntiant_ndp_device_s *ndp, void *chunk, int len)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t data;
    syntiant_pkg_parser_state_t *pstate = &ndp->pstate;
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    uint32_t ulen = (uint32_t)len;
    int use_bootloader = ndp120->secure_boot_required || ndp120->chip_is_locked;

    DEBUG_PRINTF("syntiant_ndp120_load: %d bytes\n", len);
    if (ulen == 0) {
        /* check if fw or NN params are already there, not board params*/
        int is_any_pkg_loaded = (ndp120->paramver_len || ndp120->fwver_len);
        memset(&ndp120->bl_state, 0, sizeof(ndp120->bl_state));

        /* check to see if the chip requires secure boot */
        s = ndp_spi_read(NDP120_SPI_ID0, &data);
        if (s) goto error;
        ndp120->secure_boot_required
            = NDP120_SPI_ID0_AUTH_FIRMWARE_EXTRACT(data);

        /* check to see if the chip is locked */
        s = ndp_spi_read(NDP120_SPI_CTL, &data);
        if (s) goto error;
        ndp120->chip_is_locked = NDP120_SPI_CTL_LOCKED_EXTRACT(data);

        syntiant_pkg_parser_init(pstate);
        syntiant_pkg_parser_reset(pstate);
        /* verify configuration and ndp120 matches */
        if (is_any_pkg_loaded) {
            s = syntiant_ndp120_verify_configuration(ndp);
            if ((s && s != SYNTIANT_NDP_ERROR_PACKAGE)
                || (s && s == SYNTIANT_NDP_ERROR_FAIL)) {
                    DEBUG_PRINTF("config verify error: %d", s);
                goto error;
            }
        }
        s = SYNTIANT_NDP_ERROR_MORE;
        goto error;
    } else {
        /* process package */
        syntiant_pkg_preprocess_chunk(pstate, chunk, len, 0);
        syntiant_pkg_parser_reset(pstate);
        while (pstate->ptr < pstate->open_ram_end
            && pstate->mode != PACKAGE_MODE_DONE) {
            s = syntiant_pkg_parse_chunk(pstate, 0);
            if (s) {
                s = SYNTIANT_NDP_ERROR_PACKAGE;
                goto error;
            }
            if (pstate->mode == PACKAGE_MODE_TAG_START
                || pstate->mode == PACKAGE_MODE_VALUE_START
                || pstate->mode == PACKAGE_MODE_VALUE_CONT) {
                if (!use_bootloader) {
                    s = syntiant_ndp120_parse_tag_values(ndp);
                }
                if (s) {
                    DEBUG_PRINTF("%s:%d Got error %d from "
                                 "syntiant_ndp120_parse_tag_values, tag: %d",
                        __FILE__, __LINE__, s, *(uint32_t *)pstate->tag);
                    goto error;
                }

                if (pstate->ptr == pstate->open_ram_end
                    && pstate->mode != PACKAGE_MODE_DONE) {
                    s = SYNTIANT_NDP_ERROR_MORE;
                    goto error;
                }
            }
        }
    }

error:
    /* not necessarily a error that terminates the process*/
    if (use_bootloader
        && (s == SYNTIANT_NDP_ERROR_MORE || s == SYNTIANT_NDP_ERROR_NONE)) {
        syntiant_ndp120_load_via_bootloader(ndp, chunk, (uint32_t)len);
    }
    return s;
}

static int syntiant_ndp120_get_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp_config_s *config)
{
    syntiant_ndp120_device_t *ndp120 = &ndp->d.ndp120;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t network_id = ndp120->last_network_id;

    config->device_type = syntiant_ndp120_get_config_devtype(ndp->device_type);
    config->classes = ndp120->classes[network_id];

    if (
           !config->labels_len 
        && !config->firmware_version_len
        && !config->parameters_version_len
        && !config->pkg_version_len
        && !config->dsp_firmware_version_len
    ) {
        /* simply retrieve all the version lengths */
        config->labels_len = ndp120->labels_len;
        config->firmware_version_len = ndp120->fwver_len;
        config->parameters_version_len = ndp120->paramver_len;
        config->pkg_version_len = ndp120->pkgver_len;
        config->dsp_firmware_version_len = ndp120->dspfwver_len;
        s = SYNTIANT_NDP_ERROR_NONE;
        goto error;
    }

    s = syntiant_ndp120_access_config(ndp, config);
error:
    return s;
}

static int
syntiant_ndp120_send_data(struct syntiant_ndp_device_s *ndp, uint8_t *data,
    unsigned int len, int type, uint32_t offset)
{
    int s;

    /* boundary check for DNN static feature */
    if ((type == SYNTIANT_NDP_SEND_DATA_TYPE_STREAMING) && offset) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    #if NDP120_DEBUG_SEND 
    DEBUG_PRINTF("ndp120_send_data, sending %d bytes", len);
    #endif

    s = syntiant_ndp120_write_block(ndp, 0, NDP120_SPI_SAMPLE, data, len);

error:
    return s;
}

#define BUFDELTA(CUR, LAST, BUFSIZE) ((CUR) + ((CUR) < (LAST) ? (BUFSIZE) : 0) - (LAST))
/*
static uint32_t bufdelta(uintptr_t cur, uintptr_t last, size_t bufsize) {
    cur += (cur < last : bufsize : 0);
    return cur-last;
}
*/

static int
syntiant_ndp120_extract_data(struct syntiant_ndp_device_s *ndp, int type,
                             int from, uint8_t *data, unsigned int *lenp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;

    /* transient variables */
    uint32_t adx;
    uint32_t read_len_bytes;

    /* sample size vars */
    uint32_t sample_size, effective_sample_size;

    /* dsp buffer info */
    uint32_t prod_ptr, cons_ptr, last_ptr;
    uint32_t buf_start, buf_end;
    uint32_t buf_size_bytes;

    uint32_t bytes_available, effective_bytes_available;
    uint32_t chunk_count;

    /* buf read location info */
    size_t offset;
    uint32_t read_start_ptr;

    /* device info & fw state info*/
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    ndp120_dsp_config_t  fw_config;
    ndp120_dsp_buffers_t fw_buffers;

    /* get fw state info */
    adx = ndp120->dsp_fw_state_addr +
            (uint32_t)offsetof(ndp120_dsp_fw_base_t, buffers);
    s = ndp_mcu_read_block((uint32_t)adx, &fw_buffers, sizeof(fw_buffers));
    if (s) goto error;

    adx = ndp120->dsp_fw_state_addr +
            (uint32_t)offsetof(ndp120_dsp_fw_base_t, config);
    s = ndp_mcu_read_block((uint32_t)adx, &fw_config, sizeof(fw_config));
    if (s) goto error;

    switch (type) {
    case SYNTIANT_NDP_EXTRACT_TYPE_INPUT:
    case SYNTIANT_NDP_EXTRACT_TYPE_INPUT_ANNOTATED:
        /* DEBUG_PRINTF("extract_data: audio\n"); */
        prod_ptr = fw_buffers.aud_samp_buf_prod;
        cons_ptr = fw_buffers.aud_samp_buf_cons;
        last_ptr = ndp120->dsp_pcm_audio_sample_last_ptr;
        sample_size = fw_config.aud_samp_size_bytes;
        buf_start = fw_buffers.aud_samp_buf_ptr;
        buf_size_bytes = sample_size * fw_config.aud_samp_cap;
        buf_end   = buf_start + buf_size_bytes;
        break;

    case SYNTIANT_NDP_EXTRACT_TYPE_FEATURES:
        /* DEBUG_PRINTF("extract_data: feature\n"); */
        prod_ptr = fw_buffers.func_samp_buf_prod;
        cons_ptr = fw_buffers.func_samp_buf_cons;
        last_ptr = ndp120->dsp_function_sample_last_ptr;
        sample_size = fw_config.func_samp_size_bytes;
        buf_start = fw_buffers.func_samp_buf_ptr;
        buf_size_bytes = sample_size * fw_config.func_samp_cap;
        buf_end   = buf_start + buf_size_bytes;
        break;

    default:
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
    DEBUG_PRINTF("buf_start: 0x%X\n", buf_start);
    DEBUG_PRINTF("  buf_end: 0x%X\n", buf_end);
    DEBUG_PRINTF(" prod_ptr: 0x%X\n", prod_ptr);
    DEBUG_PRINTF(" cons_ptr: 0x%X\n", cons_ptr);

    effective_sample_size = sample_size;
    if (type == SYNTIANT_NDP_EXTRACT_TYPE_INPUT_ANNOTATED) {
        effective_sample_size += (uint32_t)sizeof(ndp120_dsp_audio_sample_annotation_t);
    }

    if (last_ptr == 0) { /* will be 0 on system start */
        last_ptr = cons_ptr;
    }

    /* round *lenp to even multiple of sample size. */
    *lenp = *lenp / effective_sample_size * effective_sample_size;

    /* calculate data available */

    switch(from) {
        case SYNTIANT_NDP_EXTRACT_FROM_MATCH: {
            bytes_available = (uint32_t)BUFDELTA(prod_ptr, buf_start + ndp120->tankptr_match, buf_size_bytes);
            break;
        }

        case SYNTIANT_NDP_EXTRACT_FROM_NEWEST:
        case SYNTIANT_NDP_EXTRACT_FROM_OLDEST:
            bytes_available = (uint32_t)BUFDELTA(prod_ptr, cons_ptr, buf_size_bytes);
            break;

        /* from the oldest unread */
        case SYNTIANT_NDP_EXTRACT_FROM_UNREAD:
            bytes_available = (uint32_t)BUFDELTA(prod_ptr, last_ptr, buf_size_bytes);
            break;

        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
    }

    chunk_count = bytes_available / sample_size;
    effective_bytes_available = chunk_count * effective_sample_size;
    DEBUG_PRINTF("bytes available:%d\n", bytes_available);
    DEBUG_PRINTF("*lenp interim: %d\n", *lenp);

    *lenp = MIN(*lenp, effective_bytes_available);
    /* just requesting size */
    if (data == NULL) goto error;

    switch (from) {
        case SYNTIANT_NDP_EXTRACT_FROM_MATCH: {
            read_start_ptr = buf_start + ndp120->tankptr_match;
            break;
        }

        /* from the oldest unread */
        case SYNTIANT_NDP_EXTRACT_FROM_UNREAD:
            read_start_ptr = last_ptr;
            DEBUG_PRINTF("using last_ptr of 0x%x\n", last_ptr);
            break;

        /* from the oldest recorded */
        case SYNTIANT_NDP_EXTRACT_FROM_OLDEST:
            read_start_ptr = cons_ptr;
            break;

        case SYNTIANT_NDP_EXTRACT_FROM_NEWEST:
            read_start_ptr = prod_ptr - chunk_count * sample_size;
            break;

        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
    }

    /* read out the actual data */

    chunk_count = *lenp / effective_sample_size;
    offset = 0;
    read_len_bytes = chunk_count * sample_size;
    adx = read_start_ptr;

    DEBUG_PRINTF("adx: 0x%X\n", adx);
    DEBUG_PRINTF("read_len_bytes: 0x%X\n", read_len_bytes);
    if (adx + read_len_bytes > buf_end) {
        read_len_bytes = buf_end - adx;
        s = ndp_mcu_read_block(adx, data, read_len_bytes);
        if (s) goto error;
        offset = read_len_bytes;
        read_len_bytes = (chunk_count * sample_size) - read_len_bytes;
        adx = buf_start;
    }
    s = ndp_mcu_read_block(adx, data + offset, read_len_bytes);
    if (s) goto error;

    last_ptr = adx + read_len_bytes;
    DEBUG_PRINTF("setting last_ptr to 0x%x\n", last_ptr);

    /* Annotation extraction and interleaving */
    if (type == SYNTIANT_NDP_EXTRACT_TYPE_INPUT_ANNOTATED) {
        uint32_t chunk_start_index = (read_start_ptr - buf_start) / sample_size;

        uint32_t ann_buf_sample_size = (uint32_t) sizeof(ndp120_dsp_audio_sample_annotation_t);
        uint32_t ann_buf_start = fw_buffers.aud_annotation_buf_ptr;
        uint32_t ann_buf_end = ann_buf_start + ann_buf_sample_size * fw_config.aud_samp_cap;

        uint32_t src_adx = ann_buf_start + chunk_start_index *
            (uint32_t) sizeof(ndp120_dsp_audio_sample_annotation_t);
        uint8_t   *dst_adx = data + chunk_count * sample_size;
        DEBUG_PRINTF("annotate ...\n");
        DEBUG_PRINTF("read_start_ptr: 0x%x\n", read_start_ptr);
        DEBUG_PRINTF("chunk_count: %u chunk_start_index: %u\n", chunk_count, chunk_start_index);
        DEBUG_PRINTF("ann_buf_start: 0x%x, ann_buf_end: 0x%x\n", ann_buf_start, ann_buf_end);
        DEBUG_PRINTF("src_adx: 0x%x dst_adx: %p\n", src_adx, dst_adx);

        /* extract */

        read_len_bytes = (uint32_t)(chunk_count * sizeof(ndp120_dsp_audio_sample_annotation_t));

        if (src_adx + read_len_bytes > ann_buf_end) {
            uint32_t read_len_bytes_temp = ann_buf_end - src_adx;
            s = ndp_mcu_read_block(src_adx, dst_adx, read_len_bytes_temp);
            if (s) goto error;
            read_len_bytes -= read_len_bytes_temp;
            src_adx += read_len_bytes_temp;
            dst_adx += read_len_bytes_temp;
        }
        s = ndp_mcu_read_block(src_adx, dst_adx, read_len_bytes);
        if (s) goto error;

        /* interleave */

        if (chunk_count > 1) {
            ndp120_dsp_audio_sample_annotation_t *ann_ptr =
                (ndp120_dsp_audio_sample_annotation_t *)(data + chunk_count * sample_size);
            ndp120_dsp_audio_sample_annotation_t ann;
            uint8_t *src, *dst;
            uint32_t i;
            uint32_t len_bytes;
            DEBUG_PRINTF("data ptr: %p\n", data);
#if 0
            for (i = 0; i < chunk_count; ++i) {
                ann = *ann_ptr;
                src = data + (i+1) * sample_size;
                dst = data + (i+1) * effective_sample_size;
                len_bytes = (chunk_count - (i+1)) * sample_size;
                DEBUG_PRINTF("src: %p dst: %p cnt: %d\n", src, dst, len_bytes);
                memmove(dst, src, len_bytes);
                *(ndp120_dsp_audio_sample_annotation_t *)(dst-sizeof(ann)) = ann;
                ++ann_ptr;
            }
#endif
            for (i = 1; i < chunk_count; ++i) {
                ann = *ann_ptr;
                src = data + i * sample_size;
                dst = data + i * effective_sample_size;
                len_bytes = (chunk_count - i) * sample_size;
                DEBUG_PRINTF("src: %p dst: %p cnt: %d\n", src, dst, len_bytes);
                memmove(dst, src, len_bytes);
                *(ndp120_dsp_audio_sample_annotation_t *)(dst-sizeof(ann)) = ann;
                ++ann_ptr;
            }
        }
    }



    if (last_ptr >= buf_end) { last_ptr -= buf_size_bytes; }

    /* update last ptr */
    switch (type) {
        case SYNTIANT_NDP_EXTRACT_TYPE_INPUT:
        case SYNTIANT_NDP_EXTRACT_TYPE_INPUT_ANNOTATED:
            ndp120->dsp_pcm_audio_sample_last_ptr = last_ptr;
            break;

        case SYNTIANT_NDP_EXTRACT_TYPE_FEATURES:
            ndp120->dsp_function_sample_last_ptr = last_ptr;
            break;
    }

error:
    if (*lenp == 0) {
        s = SYNTIANT_NDP_ERROR_DATA_REREAD;
        DEBUG_PRINTF("syntiant_ndp120_extract_data: reread error: \n");
    }
    return s;
}


static void
syntiant_ndp120_get_result_offset(uint32_t *addr, uint32_t network_id)
{
    if (*addr) {
        *addr += (uint32_t) offsetof(struct ndp120_fw_state_s, result);
        *addr += (uint32_t) sizeof(struct ndp120_result_s) * network_id;
    }
}

static int
syntiant_ndp120_get_match_summary(
    struct syntiant_ndp_device_s *ndp, uint32_t *summary)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    struct ndp120_fw_match_s match;
    uint32_t addr = ndp120->mcu_fw_state_addr;
    uint32_t network_id = ndp120->last_network_id;
    uint32_t cons;
    *summary = 0;

    if (addr) {
        cons = ndp120->match_consumer[network_id];
        if (ndp120->match_producer[network_id] != cons) {
            addr += (uint32_t) offsetof(struct ndp120_fw_state_s, match_ring);
            addr += (uint32_t) sizeof(match) * (network_id * NDP120_MATCH_RING_SIZE);
            addr += (uint32_t) sizeof(match) * cons;
            s = syntiant_ndp120_read_block(ndp, 1, addr, &match, sizeof(match));
            if (s) {
                DEBUG_PRINTF("Match results read error, %d\n", s);
                goto error;
            }
            ndp120->tankptr_match = match.tankptr & ~0x3U;
            DEBUG_PRINTF("tankptr at match: 0x%x\n", match.tankptr);
            *summary = match.summary;
            cons++;
            cons = cons == NDP120_MATCH_RING_SIZE ? 0 : cons;
            ndp120->match_consumer[network_id] = cons;
        }

    } else {
        s = ndp_spi_read(NDP120_SPI_MATCHSTS, summary);
        if(s) goto error;
   }

 error:
    return s;
}

static int
syntiant_ndp120_get_match_binary(
    struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len)
{
    int s;
    uint32_t addr = ndp->d.ndp120.mcu_fw_state_addr;
    uint32_t network_id = ndp->d.ndp120.last_network_id;

    if (8 < len || len % 4 != 0) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (addr) {
        syntiant_ndp120_get_result_offset(&addr, network_id);
        addr += (uint32_t) offsetof(struct ndp120_result_s, winner_one_hot);
        s = syntiant_ndp120_read_block(ndp, 1, addr, matches, len);
    } else {
        /* FAIL instead of UNINIT to match 10x ilib code */
        s = SYNTIANT_NDP_ERROR_FAIL;
        goto error;
    }

error:
    return s;
}

static int syntiant_ndp120_get_activation_size(
        struct syntiant_ndp_device_s *ndp, uint32_t network_id, uint8_t *act)
{
    int s;
    struct ndp120_nn_output_cache_s nn_cache;
    uint32_t addr = ndp->d.ndp120.mcu_fw_state_addr +
                (uint32_t) offsetof(struct ndp120_fw_state_s, nn_output_cache) +
                (uint32_t) sizeof(nn_cache) * network_id;

    s = syntiant_ndp120_read_block(ndp, 1, addr, (void *) &nn_cache,
            sizeof(nn_cache));
    if (s) goto error;

    switch (nn_cache.activation_type) {
    case NDP120_DNN_ISA_COMP0_ACTIVATION_RELU:
        *act = sizeof(((struct ndp120_result_s *)0)->raw_strengths.relu[0]);
        break;
    case NDP120_DNN_ISA_COMP0_ACTIVATION_LINEAR:
        *act = sizeof(((struct ndp120_result_s *)0)->raw_strengths.linear[0]);
        break;
    case NDP120_DNN_ISA_COMP0_ACTIVATION_LINEAR_16:
        *act = sizeof(((struct ndp120_result_s *)0)->raw_strengths.linear16[0]);
        break;
    default:
        DEBUG_PRINTF("Unknown activation type: %d\n",
                nn_cache.activation_type);
        s = SYNTIANT_NDP_ERROR_FAIL;
        break;
    }
error:
    return s;
}

static int
syntiant_ndp120_get_match_strength(struct syntiant_ndp_device_s *ndp, 
    uint8_t *strengths, unsigned int len, int type)

{
    int s;
    uint8_t act_size = 0;
    uint32_t addr = ndp->d.ndp120.mcu_fw_state_addr;
    uint32_t network_id = ndp->d.ndp120.last_network_id;

    if (type == SYNTIANT_NDP_STRENGTH_RAW) {
        s = syntiant_ndp120_get_activation_size(ndp, network_id, &act_size);
        if (s) goto error;
        len = len * act_size;
        if (len > 64 || len % 4 != 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
    } else if (type == SYNTIANT_NDP_STRENGTH_SOFTMAX) {
        if (len > 64 * (int) sizeof(uint32_t) || len % 4 != 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
    } else {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (type == SYNTIANT_NDP_STRENGTH_RAW) {

        if (addr) {
            syntiant_ndp120_get_result_offset(&addr, network_id);
            addr += (uint32_t) offsetof(struct ndp120_result_s, raw_strengths);
            s = syntiant_ndp120_read_block(ndp, 1, addr, strengths, len);
        } else {
        /* FAIL instead of UNINIT to match 10x ilib code */
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }

    } else {
        if (addr) {
            syntiant_ndp120_get_result_offset(&addr, network_id);
            addr += (uint32_t) offsetof(struct ndp120_result_s, softmax_strengths);
            s = syntiant_ndp120_read_block(ndp, 1, addr, strengths, len);
        } else {
        /* FAIL instead of UNINIT to match 10x ilib code */
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

error:
    return s;
}


int
syntiant_ndp120_init_ring_buffer_pointers(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    uint32_t data [3];
    uint32_t adx;

    adx = ndp120->dsp_fw_state_addr +
        (uint32_t) offsetof(ndp120_dsp_fw_base_t, buffers) +
        (uint32_t) offsetof(ndp120_dsp_buffers_t, aud_samp_buf_prod);
    s = ndp_mcu_read_block(adx, &data[0], sizeof(data[0]));
    if (s) {
        DEBUG_PRINTF("Unable to read func_samp_buf_prod\n");
        goto error;
    }

    adx = ndp120->dsp_fw_state_addr +
        (uint32_t) offsetof(ndp120_dsp_fw_base_t, buffers) +
        (uint32_t) offsetof(ndp120_dsp_buffers_t, func_samp_buf_prod);
    s = ndp_mcu_read_block(adx, &data[1], sizeof(data[1]));
    if (s) {
        DEBUG_PRINTF("Unable to read func_samp_buf_prod\n");
        goto error;
    }
    
    adx = ndp120->dsp_fw_state_addr +
        (uint32_t) offsetof(ndp120_dsp_fw_base_t, buffers) +
        (uint32_t) offsetof(ndp120_dsp_buffers_t, aud_annotation_buf_prod);
    s = ndp_mcu_read_block(adx, &data[2], sizeof(data[2]));
    if (s) {
        DEBUG_PRINTF("Unable to read aud_annotation_buf_prod\n");
        goto error;
    }
    

    /* reinitialize last_ptr to be equal to producer pointer to discard prior
       data, if any */
    ndp120->dsp_pcm_audio_sample_last_ptr = data[0];
    ndp120->dsp_function_sample_last_ptr = data[1];
    ndp120->dsp_pcm_audio_annotation_last_ptr = data[2];

error:
    return s;
}

int
syntiant_ndp120_read_block(struct syntiant_ndp_device_s *ndp, int mcu,
                           uint32_t address, void *value, unsigned int count)
{
    int s;
    const uint32_t MAX_BLOCK_SIZE = 2048;

    do {
        uint32_t max_bytes_allowed = 0;
        uint32_t bytes_to_read;

        if (address % MAX_BLOCK_SIZE == 0) {
            max_bytes_allowed = MAX_BLOCK_SIZE;
        } else {
            max_bytes_allowed = MAX_BLOCK_SIZE - (address % MAX_BLOCK_SIZE);
        }

        bytes_to_read = (count > max_bytes_allowed) ? max_bytes_allowed : count;

        s = ndp->iif.transfer(
            ndp->iif.d, mcu, address, NULL, value, bytes_to_read);
        if (s) goto error;

        address += bytes_to_read;
        value = (uint8_t*)value + bytes_to_read;
        count -= bytes_to_read;

    } while (count != 0);

error:
    return s;
    return SYNTIANT_NDP_ERROR_NONE;
}

int
syntiant_ndp120_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count)
{
    int s;
    const uint32_t MAX_BLOCK_SIZE = 2048;
    do {
        uint32_t max_bytes_allowed = 0;
        uint32_t bytes_to_write;
        if (address % MAX_BLOCK_SIZE == 0) {
            max_bytes_allowed = MAX_BLOCK_SIZE;
        } else {
            max_bytes_allowed = MAX_BLOCK_SIZE - (address % MAX_BLOCK_SIZE);
        }

        bytes_to_write
            = (count > max_bytes_allowed) ? max_bytes_allowed : count;

        s = ndp->iif.transfer(
            ndp->iif.d, mcu, address, value, NULL, bytes_to_write);

        if (s) goto error;

        if (mcu || address != NDP120_SPI_SAMPLE) {
            address += bytes_to_write;
        }
        value = (uint8_t*)value + bytes_to_write;
        count -= bytes_to_write;

    } while (count != 0);

error:
    return s;
}


int syntiant_ndp120_config_gpio(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_gpio_t *config)
{
    uint32_t data, data_old;
    uint32_t mask, mask_lower, mask_upper;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (!config->set) goto error;

    mask = (uint32_t) (1u << config->gpio_num);
    mask_lower = mask & 0xffff;
    mask_upper = (mask >> 16) & 0xffff;

    s = ndp_mcu_read(NDP120_CHIP_CONFIG_GPIOSEL, &data);
    if (s) goto error;
    data |= mask;
    s = ndp_mcu_write(NDP120_CHIP_CONFIG_GPIOSEL, data);
    if (s) goto error;

    if (config->set & NDP120_CONFIG_SET_GPIO_DIR) {
        if (config->dir == NDP120_CONFIG_VALUE_GPIO_DIR_OUT) {
            if (mask_lower) {
                s = ndp_mcu_write(NDP120_GPIO_OUTSET, mask_lower);
                if (s) goto error;
            } else {
                s = ndp_mcu_write(NDP120_GPIO1_OUTSET, mask_upper);
                if (s) goto error;
            }
        } else {
            if (mask_lower) {
                s = ndp_mcu_write(NDP120_GPIO_OUTCLR, mask_lower);
                if (s) goto error;
            } else {

                s = ndp_mcu_write(NDP120_GPIO_OUTCLR, mask_upper);
                if (s) goto error;
            }
        }
    }

    if (config->set & NDP120_CONFIG_SET_GPIO_VALUE) {

        s = ndp_mcu_read(
            mask_lower ? NDP120_GPIO_DATAOUT : NDP120_GPIO1_DATAOUT, &data);
        if (s) goto error;
        data_old = data;

        if (config->value) {
            data |= (mask_lower | mask_upper);
        } else {
            data &= ~(mask_lower | mask_upper);
        }

        if (data != data_old) {
            s = ndp_mcu_write(
                mask_lower ? NDP120_GPIO_DATAOUT : NDP120_GPIO1_DATAOUT, data);
            if (s) goto error;
        }
    }

error:
    return s;
}
static int compute_crc_mcu(struct syntiant_ndp_device_s *ndp, uint32_t adx, uint32_t len, uint32_t *crc_out) {
    uint32_t chunk_size;
#if NDP120_DEBUG_SIMULATOR
    uint8_t data[1024]; /* sim runs much faster with large chunk sizes */
#else
    uint8_t data[128];
#endif
    int s = 0;

    *crc_out = crc32_no_lib_init();

    while(len) {
        chunk_size = umin(sizeof(data), len);
        chunk_size = (chunk_size + 3) / 4 * 4;
        s = ndp_mcu_read_block(adx, data, chunk_size);
        if(s) goto error;
        *crc_out = crc32_no_lib_update(*crc_out, data, chunk_size);
        adx += chunk_size;
        len -= chunk_size;
    }
    *crc_out = crc32_no_lib_finalize(*crc_out);
    error:
    return s;
}

static int scratch_get_valid(struct syntiant_ndp_device_s *ndp, uint32_t *valid, int check_crc) {
    uint32_t stored_crc;
    uint32_t calc_crc;
    int s;
    *valid = 0;

    if (check_crc) {
        s = ndp_mcu_read(SCRATCH_CHECKSUM_ADX, &stored_crc);
        if (s) goto error;
        s = compute_crc_mcu(ndp, SCRATCH_CRC_REGION_BEGIN_ADX,
            SCRATCH_CRC_REGION_LENGTH, &calc_crc);
        if (s) goto error;
        if (stored_crc != calc_crc) {
            s = SYNTIANT_NDP_ERROR_CRC;
            DEBUG_PRINTF("CRC mismatch in scratch stored: 0x%08X calc: 0x%08X",
                stored_crc, calc_crc);
            goto error;
        }
    }
    s = ndp_mcu_read(SCRATCH_VALID_ADX, valid);
    if(s) goto error;

error:
    return s;
}
int syntiant_ndp120_scratch_get_valid(struct syntiant_ndp_device_s *ndp, uint32_t *valid) {
    return scratch_get_valid(ndp, valid, 1);
}

int syntiant_ndp120_scratch_get_valid_skip_crc(struct syntiant_ndp_device_s *ndp, uint32_t *valid) {
    return scratch_get_valid(ndp, valid, 0);
}

int syntiant_ndp120_scratch_set_valid(struct syntiant_ndp_device_s *ndp, uint32_t valid) {
    int s;
    uint32_t calc_crc;
    s = ndp_mcu_write(SCRATCH_VALID_ADX, valid);
    if(s) goto error;
    s = compute_crc_mcu(ndp, SCRATCH_CRC_REGION_BEGIN_ADX, SCRATCH_CRC_REGION_LENGTH, &calc_crc);
    if(s) goto error;
    s = ndp_mcu_write(SCRATCH_CHECKSUM_ADX, calc_crc);
    if(s) goto error;

error:
    return s;
}

int
syntiant_ndp120_debug_extract(struct syntiant_ndp_device_s *ndp, int type,
        void *data, unsigned int *len)
{

    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    unsigned int l = *len;
    uint32_t addr;
    int s = SYNTIANT_NDP_ERROR_ARG;

    if (!ndp120->mcu_fw_state_addr || !ndp120->dsp_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }
    /* pr_info("mcu fw addr: 0x%x\n", ndp120->mcu_fw_state_addr);
    pr_info("dsp fw addr: 0x%x\n", ndp120->dsp_fw_state_addr);
    */
    switch (type) {
    case SYNTIANT_NDP120_DEBUG_EXTRACT_TYPE_FW_STATE:
        if (l != sizeof(struct ndp120_fw_state_s)) {
            goto error;
        }
        addr = ndp120->mcu_fw_state_addr;
        break;
    case SYNTIANT_NDP120_DEBUG_EXTRACT_TYPE_PH_STATE:
        if (l != sizeof(struct ndp120_ph_state_s)) {
            goto error;
        }
        /* TODO: this address needs to be adjusted */
        addr = ndp120->mcu_fw_posterior_state_addr;
        break;

    case SYNTIANT_NDP120_DEBUG_EXTRACT_NN_NUM:
        *(uint32_t *) data = ndp120->last_network_id;
        goto error;

    case SYNTIANT_NDP120_DEBUG_EXTRACT_TYPE_DSP_FW_STATE:
        if (l != sizeof(ndp120_dsp_fw_state_t)) {
            goto error;
        }
        addr = ndp120->dsp_fw_state_addr;
        break;
    default:
        goto error;
    }

    s = syntiant_ndp_read_block(ndp, 1, addr, data, l);

error:
    return s;
}

/* Fetch number of networks present in device memory */
int
syntiant_ndp120_num_networks(struct syntiant_ndp_device_s *ndp,
        uint8_t *num_networks)
{
    uint32_t data;
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;

    uint32_t addr = ndp120->dsp_fw_state_addr +
        (uint32_t) offsetof(ndp120_dsp_fw_base_t, metadata) +
        (uint32_t) offsetof(ndp120_metadata_t, nn_cnt);
    s = syntiant_ndp_read_block(ndp, 1, addr, &data,
            sizeof(data));
    *num_networks = data & 0xff;

    return s;
}

/* Return last network ran (fetched from mailbox out event) */
uint32_t
syntiant_get_last_network_id(struct syntiant_ndp_device_s *ndp)
{
    return ndp->d.ndp120.last_network_id;
}

uint32_t
syntiant_ndp120_get_nn_graph(struct syntiant_ndp_device_s *ndp)
{
    return ndp->d.ndp120.mcu_fw_orchestrator_graph_addr;
}


int syntiant_ndp120_get_func_sample_size(struct syntiant_ndp_device_s *ndp, uint32_t *sample_size_bytes)
{
    uint32_t adx;
    int s;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    if (sample_size_bytes == NULL) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    if (!ndp120->dsp_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    adx = ndp120->dsp_fw_state_addr + (uint32_t) offsetof(ndp120_dsp_fw_base_t,
            config.func_samp_size_bytes);
    s = ndp_mcu_read(adx, sample_size_bytes);
    if (s) goto error;

error:
    return s;
}

int syntiant_ndp120_set_fifo_threshold(struct syntiant_ndp_device_s *ndp,
        unsigned int fifo_index, uint32_t threshold)
{
    uint32_t addr;
    int s = SYNTIANT_NDP_ERROR_NONE;
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;

    if (!ndp120->dsp_fw_state_addr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }
    addr = ndp120->dsp_fw_state_addr + (uint32_t) offsetof(ndp120_dsp_fw_base_t,
            config.fifo_threshold_bytes);
    addr += (uint32_t) sizeof(((ndp120_dsp_config_t *)0)->fifo_threshold_bytes[0]) *
        fifo_index;

    s = ndp_mcu_write(addr, threshold);
    if (s) goto error;

    /* sync DSP data structures */
    s = syntiant_ndp120_do_mailbox_req(ndp, NDP120_DSP_MB_H2D_PING, NULL);
    if (s) goto error;

error:
    return s;
}

int
syntiant_ndp120_status(struct syntiant_ndp_device_s *ndp,
                       struct syntiant_ndp120_status_s *status)
{
    struct syntiant_ndp120_device_s *ndp120 = &ndp->d.ndp120;
    uint32_t a, v;
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (status->set & ~((unsigned int) SYNTIANT_NDP120_STATUS_SET_ALL_M)) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    a = ndp120->mcu_fw_state_addr
        + (unsigned int) offsetof(struct ndp120_fw_state_s,
                                  mb_state.m2h_match_skipped);

    if (status->set & SYNTIANT_NDP120_STATUS_SET_MAILBOX_TRACE) {
        syntiant_ndp120_mailbox_trace = status->mailbox_trace;
    }

    status->mailbox_trace = syntiant_ndp120_mailbox_trace;
    status->h2m_mailbox_req
        = syntiant_ndp120_mailbox_req_number[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_HOST_TO_MCU];
    status->h2m_mailbox_rsp
        = syntiant_ndp120_mailbox_rsp_number[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_HOST_TO_MCU];
    status->h2m_mailbox_unexpected
        = syntiant_ndp120_mailbox_unexpected[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_HOST_TO_MCU];
    status->h2m_mailbox_error
        = syntiant_ndp120_mailbox_error[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_HOST_TO_MCU];
    status->m2h_mailbox_req
        = syntiant_ndp120_mailbox_req_number[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST];
    status->m2h_mailbox_rsp
        = syntiant_ndp120_mailbox_rsp_number[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST];
    status->m2h_mailbox_unexpected
        = syntiant_ndp120_mailbox_unexpected[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST];
    status->m2h_mailbox_error
        = syntiant_ndp120_mailbox_error[
            SYNTIANT_NDP120_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST];

    if (ndp120->mcu_fw_state_addr) {
        s = syntiant_ndp_read(ndp, 1, a, &v);
        if (s)
            goto error;
        status->missed_frames = v;
    } else {
        status->missed_frames = 0;
    }

    if (status->set & SYNTIANT_NDP120_STATUS_SET_CLEAR) {
        memset(syntiant_ndp120_mailbox_req_number, 0,
               sizeof(syntiant_ndp120_mailbox_req_number));
        memset(syntiant_ndp120_mailbox_rsp_number, 0,
               sizeof(syntiant_ndp120_mailbox_rsp_number));
        memset(syntiant_ndp120_mailbox_unexpected, 0,
               sizeof(syntiant_ndp120_mailbox_unexpected));
        memset(syntiant_ndp120_mailbox_error, 0,
               sizeof(syntiant_ndp120_mailbox_error));
        if (ndp120->mcu_fw_state_addr) {
            s = syntiant_ndp_write(ndp, 1, a, 0);
            if (s)
                goto error;
        }
    }

 error:
    return s;
}

struct syntiant_ndp_driver_s syntiant_ndp120_driver = {
    syntiant_ndp120_device_types,
    syntiant_ndp120_init,
    syntiant_ndp120_uninit,
    syntiant_ndp120_op_size,
    syntiant_ndp120_interrupts,
    syntiant_ndp120_poll,
    syntiant_ndp120_load,
    syntiant_ndp120_get_config,
    syntiant_ndp120_send_data,
    syntiant_ndp120_extract_data,
    syntiant_ndp120_get_match_summary,
    syntiant_ndp120_get_match_binary,
    syntiant_ndp120_get_match_strength,
    syntiant_ndp120_read_block,
    syntiant_ndp120_write_block
};

struct syntiant_ndp_driver_s* syntiant_ndp120_get_driver(void)
{
    return &syntiant_ndp120_driver;
}

