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
#include <unistd.h>
#include <sys/time.h>
#endif

#include <syntiant_ilib/syntiant_portability.h>
#include <syntiant_ilib/ndp120_regs.h>
#include <syntiant_ilib/ndp120_spi_regs.h>
#include <syntiant_ilib/syntiant_ndp.h>
#include <syntiant_ilib/syntiant_ndp120.h>
#include <syntiant_ilib/syntiant_ndp_error.h>
#include <syntiant_ilib/syntiant_ndp_driver.h>

#include <syntiant_ilib/ndp120_regs.h>
#include <syntiant_ilib/ndp120_spi_regs.h>
#include <syntiant_ilib/syntiant_ndp120_driver.h>
#include <syntiant_ilib/syntiant_ndp120_mailbox.h>
#include <syntiant_packager/syntiant_package_consts.h>
#include <syntiant-dsp-firmware/ndp120_dsp_fw_state.h>

#include <syntiant-firmware/ndp120_firmware.h>
#include <syntiant-firmware/ndp120_result.h>
#include <syntiant-firmware/ndp120_orchestrator.h>
#include <syntiant-dsp-firmware/ndp120_dsp_sample.h>
#include <syntiant-firmware/ndp120_ph.h>
#include "syntiant_ndp120_ph.h"

#define SPI 0
#define MCU 1
#define FLL_LOCK_WAIT_IN_MSEC   500
#define PLL_LOCK_WAIT_IN_MSEC   500

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

#define member_size(type, member) sizeof(((type *)0)->member)

enum syntiant_ndp120_internal_constants_e {
    /* TODO merge these elsewhere */
    SYNTIANT_NDP120_TARGET_CLOCK_RATE = 16000000,
    SYNTIANT_NDP120_TARGET_OVERDRIVE_CLOCK_RATE = 24000000,
    SYNTIANT_NDP120_MAX_MCU_RATE = 19200000,
    SYNTIANT_NDP120_CLKCTL0_MCUCLKDIV_NO_DIV = 1,
    SYNTIANT_NDP120_CLKCTL0_MCUCLKDIV_DIV2 = 2
};

/* mode_0p9v_15p360MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_0p9v_15p360MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x1d4},
    {0x3,0x6},
    {0x4,0xd94},
    {0x5,0x14bc},
    {0x7,0xc94},
    {0x8,0x1285},
    {0xa,0x1fe},
    {0xb,0xe10},
    {0xc,0x445},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_0p9v_21p504MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_0p9v_21p504MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x290},
    {0x3,0x2},
    {0x4,0xd94},
    {0x5,0x14bc},
    {0x7,0xc94},
    {0x8,0x1285},
    {0xa,0x1fe},
    {0xb,0x13b0},
    {0xc,0x445},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_0p9v_30p720MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_0p9v_30p720MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x3a9},
    {0x3,0x4},
    {0x4,0xd94},
    {0x5,0x14bc},
    {0x7,0xc94},
    {0x8,0x1285},
    {0xa,0x1fe},
    {0xb,0x1c20},
    {0xc,0x445},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_0p9v_15p360MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_0p9v_15p360MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0xf},
    {0x2,0x38},
    {0x3,0x2},
    {0x4,0x9cc},
    {0x5,0xf80},
    {0x7,0x9fa},
    {0x8,0xfaf},
    {0xa,0x1fe},
    {0xb,0x1b0},
    {0xc,0x32},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_0p9v_21p504MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_0p9v_21p504MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0xf},
    {0x2,0x4e},
    {0x3,0x6},
    {0x4,0x992},
    {0x5,0x10b7},
    {0x7,0x9b3},
    {0x8,0x10fa},
    {0xa,0x1fe},
    {0xb,0x25c},
    {0xc,0x46},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_0p9v_30p720MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_0p9v_30p720MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0xf},
    {0x2,0x70},
    {0x3,0x4},
    {0x4,0xacc},
    {0x5,0x1080},
    {0x7,0xafa},
    {0x8,0x10af},
    {0xa,0x1fe},
    {0xb,0x360},
    {0xc,0x64},
    {0x17,0x0},
    {0x18,0x0},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x6},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p0v_49p152MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_1p0v_49p152MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x5dc},
    {0x3,0x0},
    {0x4,0xfc8},
    {0x5,0x1580},
    {0x7,0xec8},
    {0x8,0x14b5},
    {0xa,0x1fe},
    {0xb,0x2d00},
    {0xc,0xc83},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p0v_61p440MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_1p0v_61p440MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x753},
    {0x3,0x0},
    {0x4,0xfc8},
    {0x5,0x1580},
    {0x7,0xec8},
    {0x8,0x14b5},
    {0xa,0x1fe},
    {0xb,0x3840},
    {0xc,0xc83},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p0v_49p152MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_1p0v_49p152MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0x8},
    {0x2,0x60},
    {0x3,0x0},
    {0x4,0xbc7},
    {0x5,0x12fa},
    {0x7,0xbf4},
    {0x8,0x11aa},
    {0xa,0x1fe},
    {0xb,0x2e1},
    {0xc,0xcd},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p0v_61p440MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_1p0v_61p440MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0x8},
    {0x2,0x78},
    {0x3,0x0},
    {0x4,0xbc7},
    {0x5,0x12fa},
    {0x7,0xbf4},
    {0x8,0x11aa},
    {0xa,0x1fe},
    {0xb,0x399},
    {0xc,0xcd},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p1v_76p800MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_1p1v_76p800MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0x927},
    {0x3,0x6},
    {0x4,0xf88},
    {0x5,0x16ac},
    {0x7,0xe88},
    {0x8,0x15f4},
    {0xa,0x1fe},
    {0xb,0x4650},
    {0xc,0xfff},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p1v_98p304MHz_32p768kHz */
static ndp120_pll_preset_value_t mode_1p1v_98p304MHz_32p768kHz[] = {
    {0x0,0x1f8},
    {0x1,0x1},
    {0x2,0xbb8},
    {0x3,0x0},
    {0x4,0xf88},
    {0x5,0x16ac},
    {0x7,0xe88},
    {0x8,0x15f4},
    {0xa,0x1fe},
    {0xb,0x5a00},
    {0xc,0xfff},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p1v_76p800MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_1p1v_76p800MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0x4},
    {0x2,0x4b},
    {0x3,0x0},
    {0x4,0xa86},
    {0x5,0x11a8},
    {0x7,0xaa4},
    {0x8,0x11e5},
    {0xa,0x1fe},
    {0xb,0x240},
    {0xc,0x98},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* mode_1p1v_98p304MHz_4p096MHz */
static ndp120_pll_preset_value_t mode_1p1v_98p304MHz_4p096MHz[] = {
    {0x0,0x1f8},
    {0x1,0x4},
    {0x2,0x60},
    {0x3,0x0},
    {0x4,0xa86},
    {0x5,0x11a8},
    {0x7,0xaa4},
    {0x8,0x11e5},
    {0xa,0x1fe},
    {0xb,0x2e1},
    {0xc,0x98},
    {0x17,0x0},
    {0x18,0x4},
    {0x19,0x0},
    {0x1a,0x0},
    {0x20,0x1},
    {0x23,0x0},
    {0x24,0x0},
    {0x27,0x14},
    {0x38,0x0},
    {0x0, 0x0}
};

/* Define the table of PLL settings */
ndp120_pll_preset_t ndp120_pll_presets[] = {
    {"mode_0p9v_15p360MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_0p9, 32768, 15360000, mode_0p9v_15p360MHz_32p768kHz},
    {"mode_0p9v_21p504MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_0p9, 32768, 21504000, mode_0p9v_21p504MHz_32p768kHz},
    {"mode_0p9v_30p720MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_0p9, 32768, 30720000, mode_0p9v_30p720MHz_32p768kHz},
    {"mode_0p9v_15p360MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_0p9, 4096000, 15360000, mode_0p9v_15p360MHz_4p096MHz},
    {"mode_0p9v_21p504MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_0p9, 4096000, 21504000, mode_0p9v_21p504MHz_4p096MHz},
    {"mode_0p9v_30p720MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_0p9, 4096000, 30720000, mode_0p9v_30p720MHz_4p096MHz},
    {"mode_1p0v_49p152MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_1p0, 32768, 49152000, mode_1p0v_49p152MHz_32p768kHz},
    {"mode_1p0v_61p440MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_1p0, 32768, 61440000, mode_1p0v_61p440MHz_32p768kHz},
    {"mode_1p0v_49p152MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_1p0, 4096000, 49152000, mode_1p0v_49p152MHz_4p096MHz},
    {"mode_1p0v_61p440MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_1p0, 4096000, 61440000, mode_1p0v_61p440MHz_4p096MHz},
    {"mode_1p1v_76p800MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_1p1, 32768, 76800000, mode_1p1v_76p800MHz_32p768kHz},
    {"mode_1p1v_98p304MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_1p1, 32768, 98304000, mode_1p1v_98p304MHz_32p768kHz},
    {"mode_1p1v_76p800MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_1p1, 4096000, 76800000, mode_1p1v_76p800MHz_4p096MHz},
    {"mode_1p1v_98p304MHz_4p096MHz", PLL_PRESET_OP_VOLTAGE_1p1, 4096000, 98304000, mode_1p1v_98p304MHz_4p096MHz},
    {  NULL, 0, 0, 0, NULL}
};

/* Define the table of FLL settings */
ndp120_fll_preset_t ndp120_fll_presets[] = {
    {"mode_fll_0p9v_15p360MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_0p9, 32768, 15360000, 768000},
    {"mode_fll_0p9v_16p896MHz_32p768kHz", PLL_PRESET_OP_VOLTAGE_0p9, 32768, 16896000, 768000},
    { NULL, 0, 0, 0, 0}
};

int syntiant_ndp120_config_clk_xtal(struct syntiant_ndp_device_s *ndp, 
        syntiant_ndp120_config_clk_xtal_t *config)
{
    uint32_t data, data_old;
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    syntiant_ndp120_config_clk_xtal_t config_out = { 0 };

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) goto error;

    if (!config->set && !config->get) goto error;
    
    s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL2, &data);
    if (s) goto error;

    data_old = data;
    config_out.out = NDP120_CHIP_CONFIG_CLKCTL2_XTAL_OUT_EXTRACT(data);
    config_out.osc = NDP120_CHIP_CONFIG_CLKCTL2_XTAL_OSC_EXTRACT(data);

    if(config->set & NDP120_CONFIG_SET_CLK_XTAL_OUT) {
        data = NDP120_CHIP_CONFIG_CLKCTL2_XTAL_OUT_MASK_INSERT(
                data, config->out);
    }
    if(config->set & NDP120_CONFIG_SET_CLK_XTAL_OSC) {
        data = NDP120_CHIP_CONFIG_CLKCTL2_XTAL_OSC_MASK_INSERT(
                data, config->osc);
    }
    if (data != data_old) {
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL2, data);
        if (s) goto error;
    }
    *config = config_out;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

static int
update_pll_registers(struct syntiant_ndp_device_s *ndp)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t data;

    data = NDP120_PLL_CONFIG_PLLCTL24_REG_UPDATE_INSERT(0, 0);
    s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL24, data);
    if (s) goto error;

    data = NDP120_PLL_CONFIG_PLLCTL24_REG_UPDATE_INSERT(0, 1);
    s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL24, data);

error:
    return s;
}

static int toggle_pdmctl_update(struct syntiant_ndp_device_s *ndp, uint32_t interface) {
    uint32_t data;
    int s = NDP_MB_ERROR_NONE;

    /* toggle update */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCTL(interface), &data);
    if (s) goto error;

    /* toggle=0 */
    data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_MASK_INSERT(data, 0);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(interface), data);
    if (s) goto error;

    /* toggle=1 */
    data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_INSERT(data, 1);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(interface), data);
    if (s) goto error;

    /* toggle=0 */
    data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_MASK_INSERT(data, 0);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(interface), data);
    if (s) goto error;

error:
    return s;
}

static int
config_calc_mult_mult(unsigned int* core_clock_rate,
            unsigned int input_clock_rate, unsigned int* clock_mult,
            unsigned int* clock_div)
{
    int s = SYNTIANT_NDP_ERROR_NONE;

    /*
     * compute clock multiplier with 3 frac bits, but note
     * the FLL target is actually core_clock_rate / 2
     * numerator factor is / 2 for core_clock_rate / 2,
     * and * 2 for 1 fractional bit with which to round up
     */
    *clock_mult = (uint32_t) (*core_clock_rate << (3 - 1 + 1)) / input_clock_rate;

    /* round up */
    *clock_mult = (*clock_mult / 2) + (*clock_mult & 0x1);

    /*
     * compute reciprocal of clock multiplier with (all) 14 frac bits
     * for tracking.
     * A simple multiplication by 2^14 would overflow the clock rate
     * in the worst case (4MHz), so we multiply by the maximum
     * 2^10 and divide the core clock by 2^4(+1-1) to ensure no overflow
     * again note the FLL target is actually core_clock_rate / 2
     * denominator factor is / 2 for core_clock_rate / 2,
     * and * 2 for 1 fractional bit with which to round up
     */
    *clock_div = (uint32_t) (input_clock_rate << 10)
                 / (*core_clock_rate >> (4 + 1 + 1));

    /* round up */
    *clock_div = (*clock_div / 2) + (*clock_div & 0x1);

    /*
     * Update core clock with real divisor
     * * 2 for core_clock_rate / 2,
     * * 2  for 1 fractional bit with which to round up
     */
    *core_clock_rate = (input_clock_rate * (*clock_mult))
                        >> (3 - 1 - 1);
    *core_clock_rate = (*core_clock_rate / 2) + (*core_clock_rate & 0x1);

    return s;
}

static int
syntiant_ndp120_config_clk_src_no_sync(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_clk_src_t *config)
{
    uint32_t data, data_old;
    syntiant_ndp120_config_clk_src_t config_out = { 0 };
    int s = SYNTIANT_NDP_ERROR_NONE;

    if (!config->set && !config->get) goto error;

    s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL1, &data);
    if (s) goto error;

    data_old = data;
    config_out.refsel = NDP120_CHIP_CONFIG_CLKCTL1_REFSEL_EXTRACT(data);
    config_out.clksel = NDP120_CHIP_CONFIG_CLKCTL1_CLKSEL_EXTRACT(data);
    config_out.extsel = NDP120_CHIP_CONFIG_CLKCTL1_EXTSEL_EXTRACT(data);

    if (config->set & NDP120_CONFIG_SET_CLK_SRC_REFSEL) {
        data = NDP120_CHIP_CONFIG_CLKCTL1_REFSEL_MASK_INSERT(
            data, config->refsel);
    }
    if (config->set & NDP120_CONFIG_SET_CLK_SRC_CLKSEL) {
        data = NDP120_CHIP_CONFIG_CLKCTL1_CLKSEL_MASK_INSERT(
            data, config->clksel);
    }
    if (config->set & NDP120_CONFIG_SET_CLK_SRC_EXTSEL) {
        data = NDP120_CHIP_CONFIG_CLKCTL1_EXTSEL_MASK_INSERT(
            data, config->extsel);
    }
    if (data != data_old) {
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL1, data);
        if (s) goto error;
    }

    s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL0, &data);
    if (s) goto error;

    data_old = data;
    config_out.force_dnn_clk
        = NDP120_CHIP_CONFIG_CLKCTL0_FORCE_DNN_CLK_EXTRACT(data);
    config_out.force_aesotp_clk
        = NDP120_CHIP_CONFIG_CLKCTL0_FORCE_AESOTP_CLK_EXTRACT(data);

    if (config->set & NDP120_CONFIG_SET_CLK_SRC_FORCE_DNN_CLK) {
        data = NDP120_CHIP_CONFIG_CLKCTL0_FORCE_DNN_CLK_MASK_INSERT(
            data, config->force_dnn_clk);
    }
    if (config->set & NDP120_CONFIG_SET_CLK_SRC_FORCE_AESOTP_CLK) {
        data = NDP120_CHIP_CONFIG_CLKCTL0_FORCE_AESOTP_CLK_MASK_INSERT(
            data, config->force_aesotp_clk);
    }
    if (data != data_old) {
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL0, data);
        if (s) goto error;
    }

    s = ndp_spi_read(NDP120_SPI_CTL, &data);
    if (s) goto error;

    data_old = data;

    config_out.force_ext = NDP120_SPI_CTL_EXTSEL_EXTRACT(data);
    config_out.force_int = NDP120_SPI_CTL_INTSEL_EXTRACT(data);

    if (config->set & NDP120_CONFIG_SET_CLK_SRC_FORCE_EXT) {
        data = NDP120_SPI_CTL_EXTSEL_MASK_INSERT(
            data, config->force_ext);
    }
    if (config->set & NDP120_CONFIG_SET_CLK_SRC_FORCE_INT) {
        if (config->force_int) {
            /* have to clear extsel for intsel to take effect */
            data = NDP120_SPI_CTL_EXTSEL_MASK_INSERT(data, 0);
        }
        data = NDP120_SPI_CTL_INTSEL_MASK_INSERT(
            data, config->force_int);
    }
    if (data != data_old) {
        s = ndp_spi_write(NDP120_SPI_CTL, data);
        if (s) goto error;
    }
    *config = config_out;

error:
    return s;
}

int
syntiant_ndp120_config_clk_src(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_clk_src_t *config)
{
    int s0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    s = syntiant_ndp120_config_clk_src_no_sync(ndp, config);
    if(s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

    return s;
}

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
static int
wait_for_pll_lock(struct syntiant_ndp_device_s *ndp, long int timeout_ms)
{
    int s;
    uint32_t data;
    syntiant_ms_time timeout;    
    syntiant_get_ms_time(&timeout);

    do {
        /* write to data_strobe */
        ndp_mcu_read(NDP120_PLL_CONFIG_PLLCTL15, &data);
        data = NDP120_PLL_CONFIG_PLLCTL15_SAMPLE_STROBE_MASK_INSERT(data, 1);
        ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL15, data);

        s = ndp_mcu_read(NDP120_PLL_CONFIG_PLLSTS7, &data);
        if (s) break;

        if (NDP120_PLL_CONFIG_PLLSTS7_LOCK_DETECT_EXTRACT(data)) {
            return 1;
        }
    } while (syntiant_get_ms_elapsed(&timeout) < (unsigned long)timeout_ms);
    return 0;
}

static int
wait_for_fll_lock(struct syntiant_ndp_device_s *ndp, long int timeout_ms)
{
    int s;
    uint32_t data;
    syntiant_ms_time timeout;
    syntiant_get_ms_time(&timeout);

    do {
        s = syntiant_ndp120_read(ndp, MCU, NDP120_CHIP_CONFIG_FLLSTS0, &data);
        if (s) break;
        if (NDP120_CHIP_CONFIG_FLLSTS0_LOCKED_EXTRACT(data)) {
            DEBUG_PRINTF("FLL locked! mode=%d\n",
                         NDP120_CHIP_CONFIG_FLLSTS0_MODE_EXTRACT(data));
            return 1;
        }
    } while (syntiant_get_ms_elapsed(&timeout) < (unsigned long)timeout_ms);

    return 0;
}

/* use PLL or else FLL */
static void select_fll_pll(struct syntiant_ndp_device_s *ndp, int pll) {
    syntiant_ndp120_config_clk_src_t src_config = { 0 };
    DEBUG_PRINTF("switching clock\n");
    src_config.set |= NDP120_CONFIG_SET_CLK_SRC_CLKSEL;
    src_config.set |= NDP120_CONFIG_SET_CLK_SRC_EXTSEL;
    src_config.clksel = !!pll; 
    src_config.extsel = 0; /* int */
    syntiant_ndp120_config_clk_src_no_sync(ndp, &src_config);
}

static int
get_put_int(struct syntiant_ndp_device_s *ndp, uint32_t adx, uint32_t mask, uint32_t *value)
{
    int s;
    uint32_t valid;

    if (value == NULL) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
    s = 0;

    if (*value == 0xFFFFFFFF) { /* invalidate */
        s = syntiant_ndp120_scratch_set_valid(ndp, valid & (uint32_t)(~mask));
        if (s) goto error;
        *value = 0;
    } else if (*value) { /* put */
        s = ndp_mcu_write(adx, *value);
        if (s) goto error;
        valid |= mask;
        s = syntiant_ndp120_scratch_set_valid(ndp, valid);
        if (s) goto error;
    } else if (valid & mask) { /* get */
        s = ndp_mcu_read(adx, value);
        if (s) goto error;
    } else { /* get, but not valid */
        *value = 0;
    }

error:
    return s;
}

int syntiant_ndp120_get_put_pll_clk_freq(struct syntiant_ndp_device_s *ndp,
        uint32_t *freq)
{
    int s;
    s = ndp->iif.sync(ndp->iif.d);
    if (s) return s;
    s = get_put_int(ndp, SCRATCH_PLL_CLK_FREQ_ADX,
            SYNTIANT_CONFIG_PLL_CLK_FREQ_VALID, freq);
    if (s) goto error;
error:
    s = ndp->iif.unsync(ndp->iif.d);
    return s;
}

static int syntiant_ndp120_get_put_pll_clk_freq_no_sync(
        struct syntiant_ndp_device_s *ndp, uint32_t *freq) {
    return get_put_int(ndp, SCRATCH_PLL_CLK_FREQ_ADX,
            SYNTIANT_CONFIG_PLL_CLK_FREQ_VALID, freq);
}

static int syntiant_ndp120_get_put_fll_clk_freq_no_sync(
        struct syntiant_ndp_device_s *ndp, uint32_t *freq) {
    return get_put_int(ndp, SCRATCH_FLL_CLK_FREQ_ADX,
            SYNTIANT_CONFIG_FLL_CLK_FREQ_VALID, freq);
}

static int syntiant_ndp120_get_put_ext_clk_freq_no_sync(
        struct syntiant_ndp_device_s *ndp, uint32_t *freq)
{
    return get_put_int(ndp, SCRATCH_EXT_CLK_FREQ_ADX,
            SYNTIANT_CONFIG_EXT_CLK_FREQ_VALID, freq);
}

int syntiant_ndp120_get_put_ext_clk_freq(struct syntiant_ndp_device_s *ndp,
        uint32_t *freq) {
    int s;

    s = ndp->iif.sync(ndp->iif.d);
    if (s) return s;
    s = syntiant_ndp120_get_put_ext_clk_freq_no_sync(ndp, freq);
    if (s) goto error;
    s = ndp->iif.unsync(ndp->iif.d);
error:
    return s;
}

int
syntiant_ndp120_config_clk_fll(struct syntiant_ndp_device_s *ndp,
                               syntiant_ndp120_config_clk_fll_t *config)
{
    ndp120_fll_preset_t *preset;
    int s0, s = SYNTIANT_NDP_ERROR_NONE;
    uint8_t ctl;
    unsigned int input_clock_rate = 0;
    unsigned int core_clock_rate = 0;
    unsigned int clock_mult, clock_div, module_clock_div;
    uint32_t clkctl0, fllctl0, fllctl1, fllctl2;

    /* this value index different for each config */
    uint32_t data;
    uint32_t valid;
    uint32_t adx;
    syntiant_ndp120_config_clk_fll_t config_out = { 0 };

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    /* get */
    if (!config->set && !config->get) goto error;

    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
    s = 0;

    if (config->get) {
        if (valid & SYNTIANT_CONFIG_FLL_PRESET_VALID) {
            adx = SCRATCH_FLL_PRESET;
            s = ndp_mcu_read(adx, &config_out.preset);
            if (s) goto error;
        } else {
            config_out.preset = 0xFFFFFFFF;
        }
        ndp_mcu_read(NDP120_CHIP_CONFIG_FLLSTS0, &data);
        config_out.locked = NDP120_CHIP_CONFIG_FLLSTS0_LOCKED_EXTRACT(data);
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: config_out.locked=%d\n",
                      config_out.locked);
    }

    if (config->set & NDP120_CONFIG_SET_CLK_FLL_PRESET) {

        if (config->preset > (ARRAY_LEN(ndp120_fll_presets) - 1)) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        preset = &ndp120_fll_presets[config->preset];
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: preset=%d\n",
                      config->preset);
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: input_freq=%d\n",
                      preset->input_freq);
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: output_freq=%d\n",
                      preset->output_freq);
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: pdm_freq=%d\n",
                      preset->pdm_freq);

        s = syntiant_ndp120_read(ndp, 0, NDP120_SPI_CTL, &ctl);
        if (s) goto error;

        /* retrieve input clock */
        syntiant_ndp120_get_put_ext_clk_freq_no_sync
                    (ndp, (uint32_t *)&input_clock_rate);
        DEBUG_PRINTF("syntiant_ndp120_config_clk_fll: input_clock_rate=%d\n",
                      input_clock_rate);

        /* Determine internal clock rate */
        /* Ensure what FLL plans to use as input freq is the same
           as previously set */
        if ((preset->input_freq != input_clock_rate) || !input_clock_rate) {
            s = SYNTIANT_NDP_ERROR_CONFIG;
            goto error;
        }

        /* -------------------------------------------------------
         * Configures the internal FLL
         * ------------------------------------------------------- */

        /* find out if external clock is already on and turn it off */
        if (NDP120_SPI_CTL_EXTSEL_EXTRACT(ctl)) {
            ctl = (uint8_t) NDP120_SPI_CTL_EXTSEL_MASK_INSERT(ctl, 0);
            s = syntiant_ndp120_write(ndp, 0, NDP120_SPI_CTL, ctl);
            if (s) goto error;
        }

        /* select FLL */
        select_fll_pll(ndp, 0);

        /* turn on FLL ref clock clockgate in fllctl0 */
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_DEFAULT;
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_CLK_ENABLE_INSERT(fllctl0, 1);
        s = syntiant_ndp120_write(ndp, 1, NDP120_CHIP_CONFIG_FLLCTL0,
                                  fllctl0);
        if (s) goto error;

        /* calculate targeted core clock rate */
        core_clock_rate = preset->output_freq;
        config_calc_mult_mult(&core_clock_rate, input_clock_rate,
                              &clock_mult, &clock_div);
        DEBUG_PRINTF("config_calc_mult_mult: core_clock_rate=%d, "
                     "input_clock_rate=%d, clock_mult=0x%x, clock_div=0x%x\n",
                      core_clock_rate, input_clock_rate, clock_mult,
                      clock_div);

        /* Forward write (no read/modify/write til clock rate is high */
        fllctl1 = NDP120_CHIP_CONFIG_FLLCTL1_DEFAULT;
        fllctl1 = NDP120_CHIP_CONFIG_FLLCTL1_FREQMULT_MASK_INSERT
                  (fllctl1, clock_mult);
        fllctl1 = NDP120_CHIP_CONFIG_FLLCTL1_FREQDIV_MASK_INSERT
                  (fllctl1, clock_div);
        s = syntiant_ndp120_write(ndp, 1, NDP120_CHIP_CONFIG_FLLCTL1,
                                  fllctl1);
        if (s) goto error;

        /* fix for #108 [HLS - ClockMult] FLL loop gain bug */
        fllctl2 = NDP120_CHIP_CONFIG_FLLCTL2_DEFAULT;
        fllctl2 = NDP120_CHIP_CONFIG_FLLCTL2_LOOPGAIN1_MASK_INSERT
                  (fllctl2, 0x7);
        s = syntiant_ndp120_write(ndp, 1, NDP120_CHIP_CONFIG_FLLCTL2,
                                  fllctl2);
        if (s) goto error;

        /* Turn on enable, enable tracking, and clockgate enable */
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_DEFAULT;
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_ENABLETRACKING_MASK_INSERT(fllctl0, 1);
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_ENABLE_MASK_INSERT(fllctl0, 1);
        fllctl0 = NDP120_CHIP_CONFIG_FLLCTL0_CLK_ENABLE_INSERT(fllctl0, 1);
        s = syntiant_ndp120_write(ndp, 1, NDP120_CHIP_CONFIG_FLLCTL0,
                                  fllctl0);
        if (s) goto error;

        /* wait for FLL lock */
        if (wait_for_fll_lock(ndp, FLL_LOCK_WAIT_IN_MSEC)) {
            DEBUG_PRINTF("FLL locked\n");
        } else {
            s = SYNTIANT_NDP_ERROR_FAIL;
            DEBUG_PRINTF("FLL unlocked\n");
            goto error;
        }

        /* -------------------------------------------------------
         *    This sections updates internal clock dividers
         * ------------------------------------------------------- */
        module_clock_div = SYNTIANT_NDP120_CLKCTL0_MCUCLKDIV_NO_DIV;
        if (core_clock_rate > SYNTIANT_NDP120_MAX_MCU_RATE) {
            module_clock_div = SYNTIANT_NDP120_CLKCTL0_MCUCLKDIV_DIV2;
        }

        clkctl0 = NDP120_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_MASK_INSERT
                  (NDP120_CHIP_CONFIG_CLKCTL0_DEFAULT, module_clock_div);
        s = syntiant_ndp120_write(ndp, 1, NDP120_CHIP_CONFIG_CLKCTL0, clkctl0);
        if (s) goto error;

        adx = SCRATCH_FLL_PRESET;
        s = ndp_mcu_write(adx, config->preset);
        if (s) goto error;
        valid |= SYNTIANT_CONFIG_FLL_PRESET_VALID;
        s = syntiant_ndp120_scratch_set_valid(ndp, valid);
        if (s) goto error;

        /* save core clock rate in scratch area */
        data = core_clock_rate;
        syntiant_ndp120_get_put_fll_clk_freq_no_sync(ndp, &data);
    }

    *config = config_out;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

    return s;
}

int
syntiant_ndp120_config_clk_pll(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_clk_pll_t *config)
{
    ndp120_pll_preset_t *preset;
    ndp120_pll_preset_value_t *preset_value;

    /* this value index different for each config */
    uint32_t data;
    uint32_t valid;
    uint32_t adx;
    syntiant_ndp120_config_clk_pll_t config_out = { 0 };
    int s0;
    int s = SYNTIANT_NDP_ERROR_NONE;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    /* get */
    if (!config->set && !config->get) goto error;

    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
    s = 0;

    if (config->get) {
        if (valid & SYNTIANT_CONFIG_PLL_PRESET_VALID) {
            adx = SCRATCH_PLL_PRESET;
            s = ndp_mcu_read(adx, &config_out.preset);
            if (s) goto error;
        } else {
            config_out.preset = 0xFFFFFFFF;
        }
        /* write to data_strobe */
        s = ndp_mcu_read(NDP120_PLL_CONFIG_PLLCTL15, &data);
        if (s) goto error;
        data = NDP120_PLL_CONFIG_PLLCTL15_SAMPLE_STROBE_MASK_INSERT(data, 1);
        s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL15, data);
        if (s) goto error;
        ndp_mcu_read(NDP120_PLL_CONFIG_PLLSTS7, &data);
        config_out.locked = NDP120_PLL_CONFIG_PLLSTS7_LOCK_DETECT_EXTRACT(data);
        DEBUG_PRINTF("config_out.locked = %d\n", config_out.locked);
    }

    if (config->set & NDP120_CONFIG_SET_CLK_PLL_PRESET) {

        if (config->preset > (ARRAY_LEN(ndp120_pll_presets) - 1)) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto error;
        }
        preset = &ndp120_pll_presets[config->preset];
        preset_value = preset->values;

        /* select FLL */
        select_fll_pll(ndp, 0);

        /* disable PLL */
        s = ndp_mcu_read(NDP120_PLL_CONFIG_PLLCTL0, &data);
        data = NDP120_PLL_CONFIG_PLLCTL0_PLL_ENABLE_MASK_INSERT(data, 0);
        s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL0, data);

        /* tell the pll to update the regs */
        s = update_pll_registers(ndp);
        if (s) goto error;

        /* updates for other regs*/
        /* Offsets are byte offsets, must multiply by 4 for proper reg */
        while (preset_value->offset != 0 || preset_value->value != 0) {
            adx = NDP120_PLL_CONFIG + (uint32_t) (preset_value->offset << 2);
            s = ndp_mcu_write(adx, preset_value->value);
            if (s) goto error;
            ++preset_value;
        }
        adx = SCRATCH_PLL_PRESET;
        s = ndp_mcu_write(adx, config->preset);
        if (s) goto error;
        valid |= SYNTIANT_CONFIG_PLL_PRESET_VALID;
        s = syntiant_ndp120_scratch_set_valid(ndp, valid);
        if (s) goto error;

        data = preset->output_freq;
        syntiant_ndp120_get_put_pll_clk_freq_no_sync(ndp, &data);

        /* tell the pll to update the regs */
        s = update_pll_registers(ndp);
        if (s) goto error;

        /* toggle the pll_config.pllctl0.dco_norm_enable 1 --> 0 --> 1 */
        s = ndp_mcu_read(NDP120_PLL_CONFIG_PLLCTL0, &data);
        if (s) goto error;
        data = NDP120_PLL_CONFIG_PLLCTL0_DCO_NORM_ENABLE_MASK_INSERT(data, 0);
        s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL0, data);
        if (s) goto error;

        /* tell the pll to update the regs */
        s = update_pll_registers(ndp);
        if (s) goto error;

        s = ndp_mcu_read(NDP120_PLL_CONFIG_PLLCTL0, &data);
        if (s) goto error;

        data = NDP120_PLL_CONFIG_PLLCTL0_DCO_NORM_ENABLE_MASK_INSERT(data, 1);
        s = ndp_mcu_write(NDP120_PLL_CONFIG_PLLCTL0, data);
        if (s) goto error;

        /* tell the pll to update the regs */
        s = update_pll_registers(ndp);
        if (s) goto error;

        if (wait_for_pll_lock(ndp, PLL_LOCK_WAIT_IN_MSEC)) {
            DEBUG_PRINTF("locked\n");
            if (!config->no_switch) {
                select_fll_pll(ndp, 1);
            }
        } else {
            s = SYNTIANT_NDP_ERROR_FAIL;
            goto error;
        }
    }

    *config = config_out;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

    return s;
}

int
syntiant_ndp120_config_clk_div(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_clk_div_t *config)
{
    uint32_t data;
    int s0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    syntiant_ndp120_config_clk_div_t  config_out = {0};

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    if (!config->set && !config->get) goto error;

    /* mcu clkdiv */
    s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL0, &data);
    if(s) goto error;
    config_out.mcuclkdiv
        = NDP120_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_EXTRACT(data);
    if (config->set & NDP120_CONFIG_SET_CLK_DIV_MCUCLKDIV) {
        data = NDP120_CHIP_CONFIG_CLKCTL0_MCUCLKDIV_MASK_INSERT(data, config->mcuclkdiv);
        s = ndp_mcu_write(NDP120_CHIP_CONFIG_CLKCTL0, data);
        if (s) goto error;
    }

error:
    *config = config_out;
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

    return s;
}

static int determine_main_clk_src(struct syntiant_ndp_device_s *ndp, uint32_t *src)
{
    int s;

    /* raw reg values */
    uint8_t spi_ctl;
    uint32_t mcu_chip_config_clkctl1;

    /* extracted fields */
    uint32_t spi_ctl_extsel;
    uint32_t spi_ctl_intsel;

    uint32_t mcu_chip_config_clksel;
    uint32_t mcu_chip_config_refsel;
    uint32_t mcu_chip_config_extsel;

    /* mux logic */
    uint32_t extsel;
    uint32_t clksel;
    uint32_t refsel;

    *src = 0;

    s = ndp_spi_read(NDP120_SPI_CTL, &spi_ctl);
    if (s) goto error;

    s = ndp_mcu_read(NDP120_CHIP_CONFIG_CLKCTL1, &mcu_chip_config_clkctl1);
    if (s) goto error;

    spi_ctl_extsel = NDP120_SPI_CTL_EXTSEL_EXTRACT(spi_ctl);
    spi_ctl_intsel = NDP120_SPI_CTL_INTSEL_EXTRACT(spi_ctl);

    mcu_chip_config_clksel = NDP120_CHIP_CONFIG_CLKCTL1_CLKSEL_EXTRACT(mcu_chip_config_clkctl1);
    mcu_chip_config_refsel = NDP120_CHIP_CONFIG_CLKCTL1_REFSEL_EXTRACT(mcu_chip_config_clkctl1);
    mcu_chip_config_extsel = NDP120_CHIP_CONFIG_CLKCTL1_EXTSEL_EXTRACT(mcu_chip_config_clkctl1);

    extsel = spi_ctl_extsel    || ((!spi_ctl_intsel) && mcu_chip_config_extsel);
    clksel = (!spi_ctl_intsel) && mcu_chip_config_clksel;
    refsel = (!spi_ctl_extsel) && mcu_chip_config_refsel; 

    /* FIXME we should deal with refsel */
    /* also, we need to store the xtal freq somewhere */

    (void)refsel; /* not using this right now */

    if (extsel) {
        *src = NDP120_MAIN_CLK_SRC_EXT;
    } else {
        *src = clksel ? NDP120_MAIN_CLK_SRC_PLL : NDP120_MAIN_CLK_SRC_FLL;
    }

    error:
    return s;
}

int
syntiant_ndp120_get_main_clk_freq_no_sync(
    struct syntiant_ndp_device_s *ndp, uint32_t *freq)
{
    int s;
    uint32_t src;

    s = determine_main_clk_src(ndp, &src);
    if (s) goto error;

    *freq = 0; /* "get" operation */

    switch (src) {
    case NDP120_MAIN_CLK_SRC_EXT:
        return syntiant_ndp120_get_put_ext_clk_freq_no_sync(ndp, freq);
    case NDP120_MAIN_CLK_SRC_PLL:
        return syntiant_ndp120_get_put_pll_clk_freq_no_sync(ndp, freq);
    case NDP120_MAIN_CLK_SRC_FLL:
        return syntiant_ndp120_get_put_fll_clk_freq_no_sync(ndp, freq);
    }

error:
    return s;
}

int
syntiant_ndp120_get_main_clk_freq(
    struct syntiant_ndp_device_s *ndp, uint32_t *freq)
{
    int s;
    int s0;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    s = syntiant_ndp120_get_main_clk_freq_no_sync(ndp, freq);

    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;

    return s;
}

int
syntiant_ndp120_config_notify_on_sample_ready(struct syntiant_ndp_device_s *ndp, uint32_t enable)
{
    int s0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t fw_st_ptr;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    fw_st_ptr = syntiant_ndp120_get_dsp_fw_pointer(ndp);
    if (!fw_st_ptr) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    fw_st_ptr += (uint32_t) offsetof(ndp120_dsp_fw_base_t, config.notify_on_sample_ready);
    s = ndp_mcu_write(fw_st_ptr, enable);
    if (s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int syntiant_ndp120_config_decimation(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_decimation_t *config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    uint32_t data, old_data;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    if (!config->set && !config->get) {
        DEBUG_PRINTF("not get or set, returning\n");
        goto error;
    }

    s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_B(config->mic), &data);
    if (s) goto error;
    old_data = data;

    if (config->set & NDP120_CONFIG_SET_DECIMATION_INSHIFT) {
        data = NDP120_DSP_CONFIG_PDMCFG_B_INSHIFT_MASK_INSERT(data, config->inshift);
    } else {
        config->inshift = NDP120_DSP_CONFIG_PDMCFG_B_INSHIFT_EXTRACT(data);
    }

    if (config->set & NDP120_CONFIG_SET_DECIMATION_OUTSHIFT) {
        data = NDP120_DSP_CONFIG_PDMCFG_B_OUTSHIFT_MASK_INSERT(data, config->outshift);
    } else {
        config->outshift = NDP120_DSP_CONFIG_PDMCFG_B_OUTSHIFT_EXTRACT(data);
    }

    if (old_data != data) {
        s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_B(config->mic), data);
        if (s) goto error;
        s = toggle_pdmctl_update(ndp, config->mic >> 1);
        if (s) goto error;
    }

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int syntiant_ndp120_config_gain(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_gain_t *config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    int updated = 0;
    uint32_t data, old_data;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    if (!config->set && !config->get) {
        DEBUG_PRINTF("not get or set, returning\n");
        goto error;
    }

    /* pdmcfg_b */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_B(config->mic), &data);
    if (s) goto error;
    old_data = data;

    if (config->set & NDP120_CONFIG_SET_GAIN_DCREMOVALMODE) {
        data = NDP120_DSP_CONFIG_PDMCFG_B_DCREMOVALMODE_MASK_INSERT(data, config->dcremovalmode);
    } else {
        config->dcremovalmode = NDP120_DSP_CONFIG_PDMCFG_B_DCREMOVALMODE_EXTRACT(data);
    }

    if (old_data != data) {
        s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_B(config->mic), data);
        if (s) goto error;
        updated = 1;
    }

    /* misccfg */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_MISCCFG(config->mic), &data);
    if (s) goto error;
    old_data = data;

    if (config->set & NDP120_CONFIG_SET_GAIN_AGCSHIFTDIR) {
        data = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTDIR_MASK_INSERT(data, config->agcshiftdir);
    } else {
        config->agcshiftdir = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTDIR_EXTRACT(data);
    }

    if (config->set & NDP120_CONFIG_SET_GAIN_AGCSHIFTCNT) {
        data = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTCNT_MASK_INSERT(data, config->agcshiftcnt);
    } else {
        config->agcshiftcnt = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTCNT_EXTRACT(data);
    }

    if(config->set & NDP120_CONFIG_SET_GAIN_AGCFINEGRAINMUL) {
        data = NDP120_DSP_CONFIG_MISCCFG_AGCFINEGRAINMUL_MASK_INSERT(data, config->agcfinegrainmul);
    } else {
        config->agcfinegrainmul = NDP120_DSP_CONFIG_MISCCFG_AGCFINEGRAINMUL_EXTRACT(data);
    }

    if (config->set & NDP120_CONFIG_SET_GAIN_ZCGAINCHANGE) {
	    data = NDP120_DSP_CONFIG_MISCCFG_ZCGAINCHANGEENABLE_MASK_INSERT(data,
		            config->zcgainchange);
    } else {
	    config->zcgainchange =
            NDP120_DSP_CONFIG_MISCCFG_ZCGAINCHANGEENABLE_EXTRACT(data);
    }

    if (old_data != data) {
        s = ndp_mcu_write(NDP120_DSP_CONFIG_MISCCFG(config->mic), data);
        if (s) goto error;
        updated = 1;
    }

    if (updated) {
        s = toggle_pdmctl_update(ndp, config->mic >> 1);
        if (s) goto error;
    }

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int syntiant_ndp120_config_farrow(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_farrow_t *config){
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    uint32_t data, old_data;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    if (!config->set && !config->get) {
        DEBUG_PRINTF("not get or set, returning\n");
        goto error;
    }

    /* pdmcfg_b */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_FARROWCFG(config->interface), &data);
    if (s) goto error;
    old_data = data;

    if (config->set & NDP120_CONFIG_SET_FARROW_BYPASS) {
        data = NDP120_DSP_CONFIG_FARROWCFG_BYPASS_FARROW_MASK_INSERT(data, config->bypass);
    } else {
        config->bypass = NDP120_DSP_CONFIG_FARROWCFG_BYPASS_FARROW_EXTRACT(data);
    }

    if (old_data != data) {
        s = ndp_mcu_write(NDP120_DSP_CONFIG_FARROWCFG(config->interface), data);
        if (s) goto error;
        s = toggle_pdmctl_update(ndp, config->interface);
        if (s) goto error;
    }

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

static int
syntiant_ndp120_config_i2s_disable(struct syntiant_ndp_device_s *ndp,
        syntiant_ndp120_config_i2s_t *config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t i2sctl, audctrl2;

    s = ndp_mcu_read(NDP120_DSP_CONFIG_I2SCTL(config->interface), &i2sctl);
    if (s) goto out;

    /* disable i2sctl */
    i2sctl = NDP120_DSP_CONFIG_I2SCTL_ENABLE_MASK_INSERT(i2sctl, 0);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), i2sctl);
    if (s) goto out;

    /* disable aud2ctl */
    s = ndp_mcu_read(NDP120_CHIP_CONFIG_AUDCTRL2, &audctrl2);
    if (s) goto out;
    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_OE_MASK_INSERT(audctrl2,
            NDP120_CHIP_CONFIG_AUDCTRL2_OE_DISABLE);
    audctrl2 =  NDP120_CHIP_CONFIG_AUDCTRL2_IE_MASK_INSERT(audctrl2,
            NDP120_CHIP_CONFIG_AUDCTRL2_IE_DISABLE);
    /* reset aud2 out block */
    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_RSTB_MASK_INSERT(audctrl2, 0);
    s = ndp_mcu_write(NDP120_CHIP_CONFIG_AUDCTRL2, audctrl2);
    if (s) goto out;

out:
    return s;
}

int
syntiant_ndp120_config_i2s(
    struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_i2s_t *config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int i;
    uint32_t temp;
    uint32_t data;
    uint32_t data_old;
    uint32_t dsp_fw_state_ptr;

    uint32_t frphasestep = 0;
    ndp120_dsp_fw_aud2_config_t aud2_config, aud2_config_old;

    if (config->interface > 2) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto done;
    }
    if (!config->set && !config->get) {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto done;
    }

    if (config->set & NDP120_CONFIG_SET_I2S_DISABLE) {
        return syntiant_ndp120_config_i2s_disable(ndp, config);
    }

    s = ndp_mcu_read(NDP120_DSP_CONFIG_I2SCTL(config->interface), &data);
    if (s) goto done;

    data_old = data;

    if (config-> set) {
        /* enable = 0 */
        data = NDP120_DSP_CONFIG_I2SCTL_ENABLE_MASK_INSERT(data, 0);
        s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), data);
        if (s) goto done;

        /* init = 1 */
        /* init = 0 */
        for (i = 1; i >= 0; --i) {
            data = NDP120_DSP_CONFIG_I2SCTL_INIT_MASK_INSERT(data, (uint32_t)i);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), data);
            if (s) goto done;
        }
    }

    if (config->set & NDP120_CONFIG_SET_I2S_MODE) {
        switch (config->mode) {
        case NDP120_CONFIG_VALUE_I2S_MODE_STANDARD:
        /* fall-through */
        case NDP120_CONFIG_VALUE_I2S_MODE_TDM:
            data = NDP120_DSP_CONFIG_I2SCTL_MODE_MASK_INSERT(data, config->mode);
            break;
        default:
            s = SYNTIANT_NDP_ERROR_ARG;
            goto done;
        }
    }

    if (config->set) {
        if (config->freq < 16000 || config->freq > 48000) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto done;
        }
    }

    if (config->set & NDP120_CONFIG_SET_I2S_FRAMESIZE) {
        if (config->framesize > 0x20) {
            s = SYNTIANT_NDP_ERROR_ARG;;
            goto done;
        }
        data = NDP120_DSP_CONFIG_I2SCTL_FRAMESIZE_MASK_INSERT(data, config->framesize);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_SAMPLESIZE) {
        if (config->samplesize > 0x20) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto done;
        }

        data = NDP120_DSP_CONFIG_I2SCTL_SAMPLESIZE_MASK_INSERT(data, config->samplesize);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_MSB_INDEX) {
        if (config->msb_index > 0x1e) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto done;
        }
        data = NDP120_DSP_CONFIG_I2SCTL_MSBINDEX_MASK_INSERT(data, config->msb_index);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_PACKED) {
        data = NDP120_DSP_CONFIG_I2SCTL_PACKED_MASK_INSERT(data, (uint32_t)!!config->packed);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_EDGE) {
        data = NDP120_DSP_CONFIG_I2SCTL_NEGEDGEENABLE_MASK_INSERT(data, (uint32_t)!!config->edge);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_DELAYED_FLOP_SENSITIVITY) {
        data = NDP120_DSP_CONFIG_I2SCTL_DELAYEDFLOPSENSITIVITY_MASK_INSERT(data, (uint32_t)!!config->delayed_flop_sensitivity);
    }

    if (config->interface == NDP120_DSP_AUDIO_SYNC_AUD2) {
        uint32_t audctrl2, audctrl2_old;
        uint32_t main_clk;
        uint32_t aud2clkoutdiv, aud2clkdiv, framesize, packed;

        framesize = NDP120_DSP_CONFIG_I2SCTL_FRAMESIZE_EXTRACT(data);
        packed = NDP120_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(data);

        /* is this only for aud2? ¯\_(ツ)_/¯ */
        if (config->set & NDP120_CONFIG_SET_I2S_AUDIO_OUT_FS_EXT_ENABLE) {
            data = NDP120_DSP_CONFIG_I2SCTL_AUDIO_OUT_FS_EXT_ENABLE_MASK_INSERT(data,
                    (uint32_t) !!config->audio_out_fs_ext_enable);
        }

        syntiant_ndp120_get_main_clk_freq_no_sync(ndp, &main_clk);
        if (main_clk == 0) {
            s = SYNTIANT_NDP_ERROR_CONFIG;
            goto done;
        } 

        DEBUG_PRINTF("using main clock of %d\n", main_clk);

        /* audctrl2 */
        s = ndp_mcu_read(NDP120_CHIP_CONFIG_AUDCTRL2, &audctrl2);
        if (s) goto done;
        audctrl2_old = audctrl2;

        if (config->set & NDP120_CONFIG_SET_I2S_AUD2CLKOUTNEEDED) {
            audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_AUD2CLKOUTNEEDED_MASK_INSERT(audctrl2, (uint32_t) !!config->aud2clkoutneeded);
        }

        temp = (4 * config->freq * framesize * (packed ? 2 : 1));
        if (temp != 0 && config->set) {
            aud2clkoutdiv = main_clk / temp;
            aud2clkdiv = aud2clkoutdiv / 2;

            audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_AUD2CLKOUTDIV_MASK_INSERT(audctrl2, aud2clkoutdiv);
            audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_AUD2CLKDIV_MASK_INSERT(audctrl2, aud2clkdiv);
        }

        if (config->set & NDP120_CONFIG_SET_I2S_AUD2_OUT_MODE) {
            switch (config->aud2_out_mode) {
                case NDP120_CONFIG_VALUE_I2S_AUD2_OUT_MODE_MASTER:
                    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_OE_MASK_INSERT(audctrl2, NDP120_CHIP_CONFIG_AUDCTRL2_OE_I2S_MASTER);
                    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_IE_MASK_INSERT(audctrl2, NDP120_CHIP_CONFIG_AUDCTRL2_IE_I2S_MASTER);
                    break;
                case NDP120_CONFIG_VALUE_I2S_AUD2_OUT_MODE_SLAVE:
                    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_OE_MASK_INSERT(audctrl2, NDP120_CHIP_CONFIG_AUDCTRL2_OE_I2S_SLAVE);
                    audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_IE_MASK_INSERT(audctrl2, NDP120_CHIP_CONFIG_AUDCTRL2_IE_I2S_SLAVE);
                    break;
            }
        }

        audctrl2 = NDP120_CHIP_CONFIG_AUDCTRL2_RSTB_MASK_INSERT(audctrl2, 1);

        if (config->set && audctrl2 != audctrl2_old) {
            s = ndp_mcu_write(NDP120_CHIP_CONFIG_AUDCTRL2, audctrl2);
            if (s) goto done;
        }
        dsp_fw_state_ptr = syntiant_ndp120_get_dsp_fw_pointer(ndp);
        if (!dsp_fw_state_ptr) {
            s = SYNTIANT_NDP_ERROR_UNINIT;
            goto done;
        }

        dsp_fw_state_ptr += (uint32_t)offsetof(ndp120_dsp_fw_state_t, aud2_config);
        s = ndp_mcu_read_block(dsp_fw_state_ptr, &aud2_config, sizeof(aud2_config));
        if (s) goto done;

        memcpy(&aud2_config_old, &aud2_config, sizeof(aud2_config));

        if (config->set & NDP120_CONFIG_SET_I2S_AUD2_SRC_TYPE) {
            aud2_config.src_type = (uint8_t)config->aud2_src_type;
        }

        if (config->set & NDP120_CONFIG_SET_I2S_AUD2_SRC_PARAM) {
            aud2_config.src_param = (uint8_t)config->aud2_src_param;
        }

        if (memcmp(&aud2_config, &aud2_config_old, sizeof(aud2_config))) {
            s = ndp_mcu_write_block(dsp_fw_state_ptr, &aud2_config, sizeof(aud2_config));
            if (s) goto done;
        }

        if (config->get) {
            config->aud2clkoutneeded = NDP120_CHIP_CONFIG_AUDCTRL2_AUD2CLKOUTNEEDED_EXTRACT(audctrl2_old);
            config->aud2_out_mode = NDP120_CHIP_CONFIG_AUDCTRL2_OE_EXTRACT(audctrl2_old) == NDP120_CHIP_CONFIG_AUDCTRL2_OE_I2S_MASTER ?
                NDP120_CONFIG_VALUE_I2S_AUD2_OUT_MODE_MASTER :
                NDP120_CONFIG_VALUE_I2S_AUD2_OUT_MODE_SLAVE;

            config->aud2_src_type = aud2_config_old.src_type;
            config->aud2_src_param = aud2_config_old.src_param;
            config->packed = NDP120_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(data_old);

            aud2clkoutdiv = NDP120_CHIP_CONFIG_AUDCTRL2_AUD2CLKOUTDIV_EXTRACT(audctrl2_old);
            config->freq = main_clk  / aud2clkoutdiv / 4  / 16 / (config->packed ? 2 : 1);
        }
    }

    if (config->set & NDP120_CONFIG_SET_I2S_DUAL_CHANNEL_TDM) {
        if (config->interface != 0) {
            s = SYNTIANT_NDP_ERROR_ARG;
            goto done;
        }
        data = NDP120_DSP_CONFIG_I2SCTL_DUALCHANTDM_MASK_INSERT(data, (uint32_t)!!config->dual_channel_tdm);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_LEFTCHENABLE) {
        data = NDP120_DSP_CONFIG_I2SCTL_LEFTCHENABLE_MASK_INSERT(data, (uint32_t)!!config->leftchenable);
    }

    if (config->set & NDP120_CONFIG_SET_I2S_RIGHTCHENABLE) {
        data = NDP120_DSP_CONFIG_I2SCTL_RIGHTCHENABLE_MASK_INSERT(data, (uint32_t)!!config->rightchenable);
    }

    if (config->get) {
        uint32_t farrowcfg;
        config->mode = NDP120_DSP_CONFIG_I2SCTL_MODE_EXTRACT(data_old);
        config->framesize = NDP120_DSP_CONFIG_I2SCTL_FRAMESIZE_EXTRACT(data_old);
        config->samplesize = NDP120_DSP_CONFIG_I2SCTL_SAMPLESIZE_EXTRACT(data_old);
        config->msb_index = NDP120_DSP_CONFIG_I2SCTL_MSBINDEX_EXTRACT(data_old);
        config->packed = NDP120_DSP_CONFIG_I2SCTL_PACKED_EXTRACT(data_old);
        config->edge = NDP120_DSP_CONFIG_I2SCTL_NEGEDGEENABLE_EXTRACT(data_old);
        config->delayed_flop_sensitivity = NDP120_DSP_CONFIG_I2SCTL_DELAYEDFLOPSENSITIVITY_EXTRACT(data_old);
        config->audio_out_fs_ext_enable = NDP120_DSP_CONFIG_I2SCTL_AUDIO_OUT_FS_EXT_ENABLE_EXTRACT(data_old);
        config->dual_channel_tdm = NDP120_DSP_CONFIG_I2SCTL_DUALCHANTDM_EXTRACT(data_old);
        config->leftchenable = NDP120_DSP_CONFIG_I2SCTL_LEFTCHENABLE_EXTRACT(data_old);
        config->rightchenable = NDP120_DSP_CONFIG_I2SCTL_RIGHTCHENABLE_EXTRACT(data_old);
        s = ndp_mcu_read(NDP120_DSP_CONFIG_FARROWCFG(config->interface), &farrowcfg);
    }

    if (config->set) {
        /* write initial i2sctl */
        if (data != data_old) {
            s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), data);
            if (s) goto done;
        }

        if (config->interface < 2) {
            /* update audctrl */
            s = ndp_mcu_read(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), &data);
            if (s) goto done;
            data = NDP120_CHIP_CONFIG_AUDCTRL_MODE_MASK_INSERT(data, NDP120_CHIP_CONFIG_AUDCTRL_MODE_I2S_SLAVE);
            data = NDP120_CHIP_CONFIG_AUDCTRL_IE_MASK_INSERT(data, 0x7);
            data = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_MASK_INSERT(data, 0x1);
            data = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_MASK_INSERT(data, 0x1);
            s = ndp_mcu_write(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), data);
            if (s) goto done;

            /* pdmctl.rstb --> 1 */
            data = 0;
            data = NDP120_DSP_CONFIG_PDMCTL_RSTB_INSERT(data, 1);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
            if (s) goto done;

            /* enable pdmfiltstage2bypass */
            s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), &data);
            if (s) goto done;
            data = NDP120_DSP_CONFIG_PDMCFG_A_PDMFILTSTAGE2BYPASS_MASK_INSERT(data, (uint32_t)!frphasestep);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), data);
            if (s) goto done;
        }

        /* agc, pdmcfg_b: */
        for (i = 0; i <= 1; ++i) {
            s = ndp_mcu_read(NDP120_DSP_CONFIG_MISCCFG((uint32_t) (config->interface << 1) + (uint32_t)i), &data);
            if (s) goto done;
            data = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTDIR_MASK_INSERT(data, 1);
            data = NDP120_DSP_CONFIG_MISCCFG_AGCSHIFTCNT_MASK_INSERT(data, 3);

            data = NDP120_DSP_CONFIG_MISCCFG_AGCFINEGRAINMUL_MASK_INSERT(data, (uint32_t) 1<<13);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_MISCCFG((uint32_t) (config->interface << 1) + (uint32_t)i), data);
            if (s) goto done;

            if (config->interface < 2) {
                /* disable dc removal mode */
                s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_B((uint32_t) (config->interface << 1) + (uint32_t)i), &data);
                if (s) goto done;
                data = NDP120_DSP_CONFIG_PDMCFG_B_DCREMOVALMODE_MASK_INSERT(data, NDP120_DSP_CONFIG_PDMCFG_B_DCREMOVALMODE_OFF);
                s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_B((uint32_t) (config->interface << 1) + (uint32_t)i), data);
                if (s) goto done;
            }
        }

        /* bypass_farrow=1 */
        s = ndp_mcu_read(NDP120_DSP_CONFIG_FARROWCFG(config->interface), &data);
        if (s) goto done;
        data = NDP120_DSP_CONFIG_FARROWCFG_BYPASS_FARROW_MASK_INSERT(data, (uint32_t)!frphasestep);

        /* freq / 64 , frphasestep = 1.27 unsigned*/
        data = NDP120_DSP_CONFIG_FARROWCFG_FRPHASESTEP_MASK_INSERT(data, frphasestep);
        s = ndp_mcu_write(NDP120_DSP_CONFIG_FARROWCFG(config->interface), data);
        if (s) goto done;


        if (config->interface < 2) {
            /* pdmctl.update 0,1,0 */
            s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCTL(config->interface), &data);
            if (s) goto done;

            data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_MASK_INSERT(data, 0);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
            if (s) goto done;
            data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_MASK_INSERT(data, 1);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
            if (s) goto done;
            data = NDP120_DSP_CONFIG_PDMCTL_UPDATE_MASK_INSERT(data, 0);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
            if (s) goto done;
        }

        /* i2sctl.enable = 1 */
        s = ndp_mcu_read(NDP120_DSP_CONFIG_I2SCTL(config->interface), &data);
        if (s) goto done;
        data = NDP120_DSP_CONFIG_I2SCTL_ENABLE_INSERT(data, 1);
        s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), data);
        if (s) goto done;
    }

done:
    return s;
}

int
syntiant_ndp120_config_pdm(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_pdm_t *config) {
    int s = 0;
    int s0;
    int need_to_reconfigure;
    int missing_config = 0;
    uint32_t data;
    uint32_t adx;
    uint32_t valid;
    uint32_t frphasestep;
    uint32_t decimation;
    uint32_t main_clk;
    uint32_t main_clk_at_last_config = 0;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    if (!config->set && !config->get) { 
        DEBUG_PRINTF("not get or set, returning\n");
        goto error;
    }

    s = syntiant_ndp120_scratch_get_valid(ndp, &valid);
    if (s && s != SYNTIANT_NDP_ERROR_CRC) goto error;
    s = 0;

    /*                         */
    /* GET/SET CACHED PARAMS:  */
    /*                         */
    /* sample_rate             */
    /* rate                    */
    /* clk_mode                */
    /* mode                    */
    /*                         */

    /* main clock */
    syntiant_ndp120_get_main_clk_freq_no_sync(ndp, &main_clk);
    if (main_clk == 0) {
        missing_config = 1;
    } 
    DEBUG_PRINTF("using main clock of %d\n", main_clk);

    /* main clock last time we configured */

    /* This is the value of the main clock 
       from the last time we ran this config.
       if it changes, we need to re-do the config */

    /* this param not affected by interface number */
    if (valid & SYNTIANT_CONFIG_PDM_MAIN_CLK_AT_LAST_CONFIG) {
        s = ndp_mcu_read(SCRATCH_PDM_MAIN_CLK_AT_LAST_CONFIG_ADX, &main_clk_at_last_config);
        if (s) goto error;
    } 

    /* for the "valid" word in the scratch, we use two bits per PDM 
       setting, since there are two PDM audio channels, thus the shift
       right using config->interface */

    /* sample rate */
    adx = SCRATCH_PDM_SAMPLE_RATE_ADX(config->interface);
    if (config->set & NDP120_CONFIG_SET_PDM_SAMPLE_RATE) {
        s = ndp_mcu_write(adx, config->sample_rate);
        if (s) goto error;
        valid |= (uint32_t)(SYNTIANT_CONFIG_PDM_SAMPLE_RATE_VALID << config->interface);
    } else if (!(valid & (uint32_t)(SYNTIANT_CONFIG_PDM_SAMPLE_RATE_VALID << config->interface))) {
        missing_config = 1;
    } else {
        s = ndp_mcu_read(adx, &config->sample_rate);
        config->set |= NDP120_CONFIG_SET_PDM_SAMPLE_RATE;
    }

    /* pdm rate */
    adx = SCRATCH_PDM_RATE_ADX(config->interface);
    if (config->set & NDP120_CONFIG_SET_PDM_PDM_RATE) {
        s = ndp_mcu_write(adx, config->pdm_rate);
        if (s) goto error;
        valid |= (uint32_t)(SYNTIANT_CONFIG_PDM_RATE_VALID << config->interface);
    } else if (!(valid & (uint32_t)(SYNTIANT_CONFIG_PDM_RATE_VALID << config->interface))) {
        missing_config = 1;
    } else {
        s = ndp_mcu_read(adx, &config->pdm_rate);
        config->set |= NDP120_CONFIG_SET_PDM_PDM_RATE;
    }

    /* pdm clk_mode */
    adx = SCRATCH_PDM_CLK_MODE_ADX(config->interface);
    if (config->set & NDP120_CONFIG_SET_PDM_CLK_MODE) {
        s = ndp_mcu_write(adx, config->clk_mode);
        if (s) goto error;
        valid |= (uint32_t)(SYNTIANT_CONFIG_PDM_CLK_MODE_VALID << config->interface);
    } else if (!(valid & (uint32_t)(SYNTIANT_CONFIG_PDM_CLK_MODE_VALID << config->interface))) {
        missing_config = 1;
    } else {
        s = ndp_mcu_read(adx, &config->clk_mode);
        config->set |= NDP120_CONFIG_SET_PDM_CLK_MODE;
    }

    /* pdm mode */
    adx = SCRATCH_PDM_MODE_ADX(config->interface);
    if (config->set & NDP120_CONFIG_SET_PDM_MODE) {
        s = ndp_mcu_write(adx, config->mode);
        if (s) goto error;
        valid |= (uint32_t)(SYNTIANT_CONFIG_PDM_MODE_VALID << config->interface);
    } else if (!(valid & (uint32_t)(SYNTIANT_CONFIG_PDM_MODE_VALID << config->interface))) {
        missing_config = 1;
    } else {
        s = ndp_mcu_read(adx, &config->mode);
        config->set |= NDP120_CONFIG_SET_PDM_MODE;
    }

    s = syntiant_ndp120_scratch_set_valid(ndp, valid);
    if (s) goto error;


    need_to_reconfigure = (main_clk != main_clk_at_last_config) || config->set;
    if (!need_to_reconfigure) goto error;

    if (missing_config) {
        s = SYNTIANT_NDP_ERROR_CONFIG;
        goto error;
    }

    /*                         */
    /* ACTUAL CONFIGURATION    */
    /*                         */

    DEBUG_PRINTF("pdm, doing actual config!\n");
    decimation = config->pdm_rate / 16000;
    frphasestep = config->pdm_rate / (decimation * config->sample_rate);

    /* clear enable */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCTL(config->interface), &data);
    if (s) goto error;
    data = NDP120_DSP_CONFIG_PDMCTL_ENABLE_MASK_INSERT(data, 0);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
    if (s) goto error;

    /* set decimation */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), &data);
    if (s) goto error;
    data = NDP120_DSP_CONFIG_PDMCFG_A_DECIMATION_MASK_INSERT(data, decimation);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), data);
    if (s) goto error;

    /* set farrow */
    s = ndp_mcu_read(NDP120_DSP_CONFIG_FARROWCFG(config->interface), &data);
    if (s) goto error;
    data = NDP120_DSP_CONFIG_FARROWCFG_FRPHASESTEP_MASK_INSERT(data, frphasestep);
    data = NDP120_DSP_CONFIG_FARROWCFG_BYPASS_FARROW_MASK_INSERT(data, frphasestep == 1 ? 1U : 0U);
    s = ndp_mcu_write(NDP120_DSP_CONFIG_FARROWCFG(config->interface), data);


    if (config->set & NDP120_CONFIG_SET_PDM_MODE) {

        /* disable I2S */
        s = ndp_mcu_read(NDP120_DSP_CONFIG_I2SCTL(config->interface), &data);
        if (s) goto error;
        data = NDP120_DSP_CONFIG_I2SCTL_ENABLE_MASK_INSERT(data, 0);
        s = ndp_mcu_write(NDP120_DSP_CONFIG_I2SCTL(config->interface), data);
        if (s) goto error;

        if (config->mode == NDP120_CONFIG_VALUE_PDM_MODE_OFF) {

            s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCTL(config->interface), &data);
            if (s) goto error;
            /* disable edges */
            data = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_MASK_INSERT( data, 0);
            data = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_MASK_INSERT( data, 0);
            /* disable enable */
            data = NDP120_DSP_CONFIG_PDMCTL_ENABLE_MASK_INSERT(data, 0);

            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), data);
            if (s) goto error;

            s = ndp_mcu_read(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), &data);
            if (s) goto error;

            data = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_MASK_INSERT(data, 0);
            data = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_MASK_INSERT(data, 0);
            data = NDP120_CHIP_CONFIG_AUDCTRL_OE_MASK_INSERT(data, 0);
            data = NDP120_CHIP_CONFIG_AUDCTRL_IE_MASK_INSERT(data, 0);
            data = NDP120_CHIP_CONFIG_AUDCTRL_MODE_MASK_INSERT(data, 0);

            s = ndp_mcu_write(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), data);
            if (s) goto error;

        } else { /* PDM mode != OFF */

            uint32_t pdmctl;
            uint32_t audctrl;

            /* pdmcfg_a.pdm_bypass = 0 */
            /* pdmcfg_a.pdmsiltstage2bypass = 0 */
            s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), &data);
            if (s) goto error;
            data = NDP120_DSP_CONFIG_PDMCFG_A_PDMBYPASS_MASK_INSERT(data, 0);
            data = NDP120_DSP_CONFIG_PDMCFG_A_PDMFILTSTAGE2BYPASS_MASK_INSERT(data, 0);
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCFG_A(config->interface), data);
            if (s) goto error;

            /* read pdmctl and audctrl */
            s = ndp_mcu_read(NDP120_DSP_CONFIG_PDMCTL(config->interface), &pdmctl);
            if (s) goto error;
            s = ndp_mcu_read(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), &audctrl);
            if (s) goto error;

            /* pdmctl.enable = 1, pdmctl.rstb = 1*/
            pdmctl = NDP120_DSP_CONFIG_PDMCTL_ENABLE_INSERT(pdmctl, 1);
            pdmctl = NDP120_DSP_CONFIG_PDMCTL_RSTB_INSERT(pdmctl, 1);
            DEBUG_PRINTF("clk mode: %d\n", config->clk_mode);

        switch(config->clk_mode) {
            case NDP120_CONFIG_VALUE_PDM_CLK_MODE_EXTERNAL:

                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PDMCLKOUTNEEDED_MASK_INSERT(audctrl, 0);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_INSERT(audctrl, 1);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_MASK_INSERT(audctrl, 0);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_OE_MASK_INSERT(audctrl, 0);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_IE_MASK_INSERT(audctrl, 0x5);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_MODE_MASK_INSERT(audctrl, NDP120_CHIP_CONFIG_AUDCTRL_MODE_PDM_IN);

                switch (config->mode) {
                    case NDP120_CONFIG_VALUE_PDM_MODE_LEFT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_RIGHT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_STEREO:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        break;
                }
            break;
            case NDP120_CONFIG_VALUE_PDM_CLK_MODE_INTERNAL:

                /* audctrl */
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PDMCLKOUTNEEDED_INSERT(audctrl, 1);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_MODE_MASK_INSERT(audctrl, NDP120_CHIP_CONFIG_AUDCTRL_MODE_PDM_OUT);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PDMCLKOUTDIV_MASK_INSERT(audctrl, main_clk / 2 / config->pdm_rate);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_INSERT(audctrl, 1);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_MASK_INSERT(audctrl, 0);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_OE_MASK_INSERT(audctrl, 1);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_IE_MASK_INSERT(audctrl, 0x5);

                switch (config->mode) {
                    case NDP120_CONFIG_VALUE_PDM_MODE_LEFT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_RIGHT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_STEREO:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        break;
                }
                break;
            case NDP120_CONFIG_VALUE_PDM_CLK_MODE_DUAL_INTERNAL:
                /* audctrl */
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PDMCLKOUTNEEDED_INSERT(audctrl, 1);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_MODE_MASK_INSERT(audctrl, NDP120_CHIP_CONFIG_AUDCTRL_MODE_PDM_OUT);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PDMCLKOUTDIV_MASK_INSERT(audctrl, main_clk / 2 / config->pdm_rate);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_OE_MASK_INSERT(audctrl, 3);
                audctrl = NDP120_CHIP_CONFIG_AUDCTRL_IE_MASK_INSERT(audctrl, 0x7);

                switch (config->mode) {
                    case NDP120_CONFIG_VALUE_PDM_MODE_LEFT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_INSERT(audctrl, 1);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_MASK_INSERT(audctrl, 0);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_RIGHT:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_MASK_INSERT(pdmctl, 0);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_MASK_INSERT(audctrl, 0);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_INSERT(audctrl, 1);
                        break;
                    case NDP120_CONFIG_VALUE_PDM_MODE_STEREO:
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_POSEDGEENABLE_INSERT(pdmctl, 1);
                        pdmctl = NDP120_DSP_CONFIG_PDMCTL_NEGEDGEENABLE_INSERT(pdmctl, 1);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK0_EN_INSERT(audctrl, 1);
                        audctrl = NDP120_CHIP_CONFIG_AUDCTRL_PCLK1_EN_INSERT(audctrl, 1);
                        break;
                }
                break;
            }

            s = ndp_mcu_write(NDP120_CHIP_CONFIG_AUDCTRL(config->interface), audctrl);
            if (s) goto error;
            s = ndp_mcu_write(NDP120_DSP_CONFIG_PDMCTL(config->interface), pdmctl);
            if (s) goto error;
        } /* pdm_mode != off */
    }

    s = ndp_mcu_write(SCRATCH_PDM_MAIN_CLK_AT_LAST_CONFIG_ADX, main_clk);
    if (s) goto error;
    valid |= SYNTIANT_CONFIG_PDM_MAIN_CLK_AT_LAST_CONFIG;

    s = toggle_pdmctl_update(ndp, config->interface);
    if (s) goto error;

    s = syntiant_ndp120_scratch_set_valid(ndp, valid);
    if (s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int syntiant_ndp120_config_filter(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_config_filter_t *config) {
    /* FIXME */
    (void)ndp;
    (void)config;
    return 0;
}

int
syntiant_ndp120_config_nn_metadata_no_sync
(struct syntiant_ndp_device_s *ndp, syntiant_ndp120_nn_metadata_t * config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t metadata_addr = syntiant_ndp120_get_dsp_fw_pointer(ndp) +
        (uint32_t) offsetof(ndp120_dsp_fw_base_t, metadata);
    uint32_t nn_metadata_addr = metadata_addr + (uint32_t) member_size(ndp120_metadata_t, nn_cnt);
    uint32_t offset;

    offset = (uint32_t) offsetof(ndp120_metadata_t, nn_cnt);
    if (config->set & SYNTIANT_NDP120_SET_NN_NUM) {
        s = ndp_mcu_write(metadata_addr + offset, config->nn_num);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(metadata_addr + offset, &config->nn_num);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, layers_per_nn) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_LAYER_PER_NN) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->layers_per_nn);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->layers_per_nn);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, is_nn_cached) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_IS_NN_CACHED) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->is_nn_cached);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->is_nn_cached);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, input_layer_isa_idx) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_INPUT_ISA_IDX) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->nn_input_isa_idx);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->nn_input_isa_idx);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, output_layer_isa_idx) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_OUTPUT_ISA_IDX) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->nn_output_isa_idx);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->nn_output_isa_idx);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, input_layer_type) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_INPUT_LAYER_TYPE) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->nn_input_layer_type);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->nn_input_layer_type);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, input_layer_size) +
        config->nn_idx * sizeof(nn_metadata_t));
    if (config->set & SYNTIANT_NDP120_SET_INPUT_SIZE) {
        s = ndp_mcu_write_block(nn_metadata_addr + offset,
            &config->nn_input_layer_size, sizeof(config->nn_input_layer_size));
        if (s) goto error;
    } else {
        s = ndp_mcu_read_block(nn_metadata_addr + offset,
            &config->nn_input_layer_size, sizeof(config->nn_input_layer_size));
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, input_coords) +
        config->nn_idx * sizeof(nn_metadata_t) +
        config->layer_idx * member_size(nn_metadata_t, input_coords[0]));
    if (config->set & SYNTIANT_NDP120_SET_INPUT_COORD) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->input_coord);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->input_coord);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, output_coords) +
        config->nn_idx * sizeof(nn_metadata_t) +
        config->layer_idx * member_size(nn_metadata_t, output_coords[0]));
    if (config->set & SYNTIANT_NDP120_SET_OUTPUT_COORD) {
        s = ndp_mcu_write(nn_metadata_addr + offset, config->output_coord);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(nn_metadata_addr + offset, &config->output_coord);
        if (s) goto error;
    }

    offset = (uint32_t) (offsetof(nn_metadata_t, cache_instructions) +
        config->nn_idx * sizeof(nn_metadata_t) +
        config->layer_idx * member_size(nn_metadata_t, cache_instructions[0]));
    if (config->set & SYNTIANT_NDP120_SET_CACHE_INST) {
        s = ndp_mcu_write_block(nn_metadata_addr + offset, &config->cache_inst,
            sizeof(config->cache_inst));
        if (s) goto error;
    } else {
        s = ndp_mcu_read_block(nn_metadata_addr + offset, &config->cache_inst,
            sizeof(config->cache_inst));
        if (s) goto error;
    }

error:
    return s;
}

int
syntiant_ndp120_config_mcu_orchestrator
(struct syntiant_ndp_device_s * ndp, syntiant_ndp120_mcu_orch_t * config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    s = syntiant_ndp120_config_mcu_orchestrator_no_sync(ndp, config);
    if(s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;

}

int
syntiant_ndp120_config_mcu_orchestrator_no_sync
(struct syntiant_ndp_device_s * ndp, syntiant_ndp120_mcu_orch_t * config)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t graph_addr = ndp->d.ndp120.mcu_fw_orchestrator_graph_addr;
    uint32_t nodes_addr = (uint32_t) (graph_addr + offsetof(struct ndp120_nn_graph, nn_graph));
    ndp120_nno_node_t node;
    uint32_t offset, value;

    if (config->set & SYNTIANT_NDP120_SET_NUM_NODES) {
        s = ndp_mcu_read(graph_addr, &value);
        if (s) goto error;
        /* Write to the first entry of the struct */
        value = value & 0xffffff00;
        value |= (config->num_nodes & 0x000000ff);
        s = ndp_mcu_write(graph_addr, value);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(graph_addr, &value);
        if (s) goto error;
        config->num_nodes = (value & 0x000000ff);
    }

    if (config->set & SYNTIANT_NDP120_SET_FLOWMAP) {
        s = ndp_mcu_read(graph_addr, &value);
        if (s) goto error;
        /* Write to the second entry of the struct */
        value = value & 0xffff00ff;
        value |= (uint32_t) (config->flowmap << 8) & 0x0000ff00;
        s = ndp_mcu_write(graph_addr, value);
        if (s) goto error;
    } else {
        s = ndp_mcu_read(graph_addr, &value);
        if (s) goto error;
        config->flowmap = (uint8_t)((value & 0x0000ff00) >> 8);
    }

    offset = (uint32_t) (config->node_idx * sizeof(ndp120_nno_node_t));
    if (config->set & SYNTIANT_NDP120_SET_NODE) {
        memset(&node, 0, sizeof (ndp120_nno_node_t));
        node.id = config->id;
        node.flowset_id = config->flow_id;
        node.status = config->status;
        node.type = config->type;
        node.action = config->action;
        node.num_inputs = config->num_inputs;
        node.num_outputs = config->num_outputs;
        memcpy(&node.input, &config->input_edges, sizeof(node.input));
        memcpy(&node.next_ids, &config->next_ids, sizeof(node.next_ids));
        s = ndp_mcu_write_block(nodes_addr + offset, &node, sizeof(node));
        if (s) goto error;
    } else {
        s = ndp_mcu_read_block(nodes_addr + offset, &node, sizeof(node));
        if (s) goto error;
        config->id = node.id;
        config->flow_id = node.flowset_id;
        config->status = node.status;
        config->type = node.type;
        config->action = node.action;
        memcpy(&config->input_edges, &node.input, sizeof(config->input_edges));
        memcpy(&config->next_ids, &node.next_ids, sizeof(config->next_ids));
    }

error:
    return s;
}


int syntiant_ndp120_dsp_flow_setup_reset(ndp120_dsp_data_flow_setup_t *setup)
{
    memset(setup, 0, sizeof(*setup));
    return SYNTIANT_NDP_ERROR_NONE;
}

static int flow_get_chan_and_len(ndp120_dsp_data_flow_setup_t *setup, int src, ndp120_dsp_data_flow_rule_t **src_chan, unsigned int *src_len) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    switch (src) {
    case NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO:
        *src_chan = setup->src_pcm_audio;
        *src_len = ARRAY_LEN(setup->src_pcm_audio);
        break;

    case NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION:
        *src_chan = setup->src_function;
        *src_len = ARRAY_LEN(setup->src_function);
        break;

    case NDP120_DSP_DATA_FLOW_SRC_TYPE_NN:
        *src_chan = setup->src_nn;
        *src_len = ARRAY_LEN(setup->src_nn);
        break;

    default:
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }

    error:
    return s;
}

static void compact(ndp120_dsp_data_flow_setup_t *setup) {
    ndp120_dsp_data_flow_setup_t temp;
    unsigned int i,j,src_len;
    int set_id;
    int s;
    ndp120_dsp_data_flow_rule_t *rule;
    ndp120_dsp_data_flow_rule_t *chan;
    const int sources[] = { NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO,
        NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION,
        NDP120_DSP_DATA_FLOW_SRC_TYPE_NN };

    memcpy(&temp, setup, sizeof(temp));
    syntiant_ndp120_dsp_flow_setup_reset(setup);
    
    /* do two things here:
     * 1) Remove gaps in rules
     * 2) Group together sets while not changing the rule order in those sets.
     */
    for (j = 0; j < ARRAY_LEN(sources); ++j) {
        s = flow_get_chan_and_len(&temp, sources[j], &chan, &src_len);
        if (!s) {
            do { 
                set_id = -1; 
                for (i = 0; i < src_len; i++) { 
                    rule = &chan[i]; 
                    if (!NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) continue; 
                    if (set_id == -1) set_id = rule->set_id; 
                    if (rule->set_id == set_id) { 
                        syntiant_ndp120_dsp_flow_setup_add_rule(setup, rule, sources[j]); 
                        NDP120_DSP_FLOW_RULE_INVALIDATE(*rule); 
                    } 
                } 
            } while(set_id !=-1); 
        }
    }
}

int
syntiant_ndp120_dsp_flow_setup_del_rule(ndp120_dsp_data_flow_setup_t *setup,
    const ndp120_dsp_data_flow_rule_t *rule, int src_type) {
    ndp120_dsp_data_flow_rule_t *src_chan;
    unsigned int i, src_len;
    int deleted = 0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    s = flow_get_chan_and_len(setup, src_type, &src_chan, &src_len);
    if (s) goto error;

    for(i = 0; i < src_len; ++i) {
        if (!NDP120_DSP_FLOW_RULE_IS_VALID(src_chan[i])) continue;
        /*DEBUG_PRINTF("i: %d src_param: %d, dst_type: %d, dst_param: %d\n", i, src_chan[i].src_param, src_chan[i].dst_type, src_chan[i].dst_param);*/
        if(src_chan[i].set_id == rule->set_id &&
           src_chan[i].src_param == rule->src_param &&
           src_chan[i].dst_type == rule->dst_type &&
           src_chan[i].dst_param == rule->dst_param ) {
               NDP120_DSP_FLOW_RULE_INVALIDATE(src_chan[i]);
               deleted = 1;
               break;
           }
    }

    if(deleted) {
        compact(setup);
    }


error:
    return s;
}

int
syntiant_ndp120_dsp_flow_setup_add_rule(ndp120_dsp_data_flow_setup_t *setup,
    const ndp120_dsp_data_flow_rule_t *rule, int src_type)
{
    ndp120_dsp_data_flow_rule_t *src_chan;
    unsigned int src_len, i;
    int s = SYNTIANT_NDP_ERROR_NONE;
    int added = 0;
    s = flow_get_chan_and_len(setup, src_type, &src_chan, &src_len);
    if (s) goto error;

    for(i = 0; i < src_len; ++i) {
        if (NDP120_DSP_FLOW_RULE_IS_VALID(src_chan[i])) continue;
        src_chan[i] = *rule;
        added = 1;
        break;
    }
    if (!added) {
        s = SYNTIANT_NDP_ERROR_NOMEM;
        goto error;
    }

error:
    return s;
}

int
syntiant_ndp120_dsp_flow_setup_apply(struct syntiant_ndp_device_s *ndp, ndp120_dsp_data_flow_setup_t *setup) {
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;

    s = syntiant_ndp120_dsp_flow_setup_apply_no_sync(ndp, setup);
    if(s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int
syntiant_ndp120_dsp_flow_setup_apply_no_sync(struct syntiant_ndp_device_s *ndp,
    ndp120_dsp_data_flow_setup_t *setup)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t fw_st_adx = ndp->d.ndp120.dsp_fw_state_addr;

    if (!fw_st_adx) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }
    compact(setup);

    s = ndp_mcu_write_block((uint32_t) (fw_st_adx +
            offsetof(ndp120_dsp_fw_base_t, data_flow)), (void*)setup, sizeof(*setup));
    if(s) goto error;

error:
    return s;
}

int
syntiant_ndp120_dsp_flow_setup_get(
    struct syntiant_ndp_device_s *ndp, ndp120_dsp_data_flow_setup_t *setup)
{
    int s0;
    int s = SYNTIANT_NDP_ERROR_NONE;
    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    s = syntiant_ndp120_dsp_flow_setup_get_no_sync(ndp, setup);
    if(s) goto error;

error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

int
syntiant_ndp120_dsp_flow_setup_get_no_sync(
    struct syntiant_ndp_device_s *ndp, ndp120_dsp_data_flow_setup_t *setup)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    uint32_t fw_st_adx = ndp->d.ndp120.dsp_fw_state_addr;

    if (!fw_st_adx) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    s = ndp_mcu_read_block((uint32_t) (fw_st_adx +
           offsetof(ndp120_dsp_fw_base_t, data_flow)), (void*)setup, sizeof(*setup));

error:
    return s;
}

int
syntiant_ndp120_dsp_flow_get_put_set_id(struct syntiant_ndp_device_s *ndp, int *set_id)
{
    int s = SYNTIANT_NDP_ERROR_NONE;
    int s0;
    uint32_t adx;

    s = (ndp->iif.sync)(ndp->iif.d);
    if (s) return s;


    adx = syntiant_ndp120_get_dsp_fw_pointer(ndp);
    if (!adx) {
        s = SYNTIANT_NDP_ERROR_UNINIT;
        goto error;
    }

    adx += (uint32_t) offsetof(ndp120_dsp_fw_base_t, data_flow_current_set_id);

    if (*set_id < 0) {
        ndp_mcu_read(adx, set_id);
    } else if (*set_id <= 255) {
        ndp_mcu_write(adx, (uint32_t)*set_id);
    } else {
        s = SYNTIANT_NDP_ERROR_ARG;
        goto error;
    }
error:
    s0 = (ndp->iif.unsync)(ndp->iif.d);
    s = s ? s : s0;
    return s;
}

#if NDP120_DEBUG 
#define COLUMN_DUMP(SRC_TYPE) \
    flow_get_chan_and_len(setup, SRC_TYPE, &src_chan, &src_len); \
    for(i=0;i<src_len;++i) { \
        rule = &src_chan[i]; \
        if (!NDP120_DSP_FLOW_RULE_IS_VALID(*rule)) continue; \
        DEBUG_PRINTF("[%d] %s%d-->%s%d\n", \
        rule->set_id, \
        NDP120_DSP_DATA_FLOW_SRC_TYPE_STR(SRC_TYPE), rule->src_param, \
        NDP120_DSP_DATA_FLOW_RULE_DST_STR(*rule), rule->dst_param \
        ); \
    }
void syntiant_ndp120_dsp_flow_setup_dump(ndp120_dsp_data_flow_setup_t *setup) {
    unsigned int i, src_len;
    ndp120_dsp_data_flow_rule_t *src_chan, *rule;

    COLUMN_DUMP(NDP120_DSP_DATA_FLOW_SRC_TYPE_PCM_AUDIO);
    COLUMN_DUMP(NDP120_DSP_DATA_FLOW_SRC_TYPE_FUNCTION);
    COLUMN_DUMP(NDP120_DSP_DATA_FLOW_SRC_TYPE_NN);
}
#else
void syntiant_ndp120_dsp_flow_setup_dump(ndp120_dsp_data_flow_setup_t *setup) {
    (void)setup;
}
#endif
