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
 *  and its suppliers and may be covered by U.S. and Foreign Patents, patents
 *  in process, and are protected by trade secret or copyright law.
 *  Dissemination of this information or reproduction of this material is
 *  strictly forbidden unless prior written permission is obtained from
 *  Syntiant Corporation.
 */
#ifndef SYNTIANT_NDP10X_H
#define SYNTIANT_NDP10X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <syntiant_ilib/syntiant_ndp.h>
/**
 * @file syntiant_ndp10x.h
 * @date 2018-07-20
 * @brief Interface to Syntiant NDP10x chip-specific interface library functions
 */

/**
 *
 * Chip-independent interfaces are defined in syntiant_ndp.h, and NDP10x
 * chip-specific interfaces are defined in this file
 */

/** 
 * @brief chip-specific constants
 */
enum syntiant_ndp10x_constants_e {
    SYNTIANT_NDP10X_AUDIO_FREQUENCY = 16000,
    /** operating frequency of the audio section (samples/second) */
    SYNTIANT_NDP10X_MAX_FREQUENCY_BINS = 40
    /** maximum number of frequency domain bins */
};


/**
 * @brief DNN input sources
 */
enum syntiant_ndp10x_config_dnn_input_e {
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_NONE = 0,
    /**< DNN input disabled */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM0 = 1,
    /**< PDM microphone, falling clock */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM1 = 2,
    /**< PDM microphone, rising clock */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_PDM_SUM = 3,
    /**< Sum of both PDM microphones */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_LEFT = 4,
    /**< I2S left input */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_RIGHT = 5,
    /**< I2S right input */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_SUM = 6,
    /**< I2S left + right inputs */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_MONO = 7,
    /**< I2S 1 channel input */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_I2S_DIRECT = 8,
    /**< I2S interface direct feature input (bypass frequency block) */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI = 9,
    /**< SPI interface PCM */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT = 10,
    /**< SPI interface direct feature input (bypass frequency block) */
    SYNTIANT_NDP10X_CONFIG_DNN_INPUT_MAX
    = SYNTIANT_NDP10X_CONFIG_DNN_INPUT_SPI_DIRECT
};

/**
 * @brief holding tank memory input sources
 */
enum syntiant_ndp10x_config_tank_input_e {
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_NONE = 0,
    /**< Tank input disabled */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0 = 1,
    /**< PDM microphone, falling clock */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1 = 2,
    /**< PDM microphone, falling clock */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_SUM = 3,
    /**< Sum of both PDM microphones */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH = 4,
    /**< rising & falling clock PDM microphones (2 channel storage) */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM0_RAW = 5,
    /**< PDM microphone falling clock, raw, undecimated bitstream */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM1_RAW = 6,
    /**< PDM microphone rising clock, raw, undecimated bitstream */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_PDM_BOTH_RAW = 7,
    /**< rising & falling clock PDM microphones, raw undecimated bitstreams */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_LEFT = 8,
    /**< I2S left input */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_RIGHT = 9,
    /**< I2S right input */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_MONO = 10,
    /**< I2S mono input */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_SUM = 11,
    /**< I2S left + right inputs */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_I2S_BOTH = 12,
    /**< both I2S left and right inputs (2 channel storage) */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_SPI = 13,
    /**< SPI interface PCM */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_FILTER_BANK = 14,
    /**< filter bank output */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN = 15,
    /**< what ever the DNN is processing.  _DIRECT inputs will report an error */
    SYNTIANT_NDP10X_CONFIG_TANK_INPUT_MAX
    = SYNTIANT_NDP10X_CONFIG_TANK_INPUT_DNN
};

/**
 * @brief configuration variable set flags
 * @note this struct is sefined the same way in packager file board_TLVs.
 */
enum syntiant_ndp10x_config_set_e {
    SYNTIANT_NDP10X_CONFIG_SET_TANK_BITS = 0x00000001,
    /**< set holding tank sample size -- 8 or 16 */
    SYNTIANT_NDP10X_CONFIG_SET_TANK_INPUT = 0x00000002,
    /**< set the input source for sample tank storage */
    SYNTIANT_NDP10X_CONFIG_SET_TANK_SIZE = 0x00000004,
    /**< set the size of sample tank storage */
    SYNTIANT_NDP10X_CONFIG_SET_FREQ_FRAME_SIZE = 0x00000008,
    /**< set the filter bank feature extraction frame size */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUT = 0x00000010,
    /**< set the input source for dnn processing */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_FRAME_SIZE = 0x00000020,
    /**< set number of streaming inputs per DNN frame */
    SYNTIANT_NDP10X_CONFIG_SET_INPUT_CLOCK_RATE = 0x00000040,
    /**< set the chip main clock rate */
    SYNTIANT_NDP10X_CONFIG_SET_MCU_CLOCK_RATE = 0x00000080,
    /**< set the MCU clock rate */
    SYNTIANT_NDP10X_CONFIG_SET_SPI_WORD_BITS = 0x00000100,
    /**< set the SPI input word bit width (typ. 16 for PCM, 8 for direct) */
    SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_RATE = 0x00000200,
    /**< set the PDM clock rate */
    SYNTIANT_NDP10X_CONFIG_SET_PDM_CLOCK_NDP = 0x00000400,
    /**< set the PDM clock source to NDP (versus external) */
    SYNTIANT_NDP10X_CONFIG_SET_PDM_IN_SHIFT = 0x000000800,
    /**< set the PDM second stage shift (volume control) */
    SYNTIANT_NDP10X_CONFIG_SET_PDM_OUT_SHIFT = 0x00001000,
    /**< set the PDM first stage shift (volume control) */
    SYNTIANT_NDP10X_CONFIG_SET_PDM_DC_OFFSET = 0x00002000,
    /**< set the PDM input DC offset */
    SYNTIANT_NDP10X_CONFIG_SET_I2S_FRAME_SIZE = 0x00004000,
    /**< set the I2S input frame size in bits -- the number of bits sent
       between transitions of the word clock.  Typical values are 16 & 24 */
    SYNTIANT_NDP10X_CONFIG_SET_I2S_SAMPLE_SIZE = 0x00008000,
    /**< set the I2S input sample size in bits -- the number of bits used
       for audio must be <= the frame size.  The actual PCM sample will be
       padded with 0s to a width of 16 bits */
    SYNTIANT_NDP10X_CONFIG_SET_I2S_SAMPLE_MSBIT = 0x00010000,
    /**< set the I2S input sample most signficant bit index, counted from
       the earliest bit to arrive.  This allows the sample portion to
       begin at any bit within the frame.  It is typically set to
       framesize - 1 assuming the I2S device uses uses the full numerical range
       afforded by the frame size. */
    SYNTIANT_NDP10X_CONFIG_SET_FREQ_CLOCK_RATE = 0x00020000,
    /**< set the frequency domain clock rate */
    SYNTIANT_NDP10X_CONFIG_SET_PREEMPHASIS_EXPONENT = 0x00040000,
    /**< set the preemphasis filter decay exponent */
    SYNTIANT_NDP10X_CONFIG_SET_WATER_MARK_ON = 0x00080000,
    /**< enable audio input buffer low water mark notification */
    SYNTIANT_NDP10X_CONFIG_SET_POWER_OFFSET = 0x00100000,
    /**< set the filter bank power offset */
    SYNTIANT_NDP10X_CONFIG_SET_POWER_SCALE_EXPONENT = 0x00200000,
    /**< set the filter bank power scale exponent */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_CLOCK_RATE = 0x00400000,
    /**< set the DNN clock rate */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_SIGNED = 0x00800000,
    /**< set the input source for dnn processing */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_MINIMUM_THRESHOLD = 0x01000000,
    /**< set DNN input vector minimum threshold */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_RUN_THRESHOLD = 0x02000000,
    /**< set minimum DNN run threshold */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_INPUTS = 0x04000000,
    /**< set total number of input vector elements */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_STATIC_INPUTS = 0x08000000,
    /**< set number of static input vector elements */
    SYNTIANT_NDP10X_CONFIG_SET_DNN_OUTPUTS = 0x10000000,
    /**< set total number of output vector elements */
    SYNTIANT_NDP10X_CONFIG_SET_AGC_ON = 0x20000000,
    /**< enable or disable automatic gain control */
    SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON = 0x40000000,
    /**< enable or disable match interrupt for every frame */
    SYNTIANT_NDP10X_CONFIG_SET_ALL_M
    = ((((unsigned int) SYNTIANT_NDP10X_CONFIG_SET_MATCH_PER_FRAME_ON) << 1U)
       - 1U),

    /* below flags apply for flag set1 */
    SYNTIANT_NDP10X_CONFIG_SET1_FILTER_EQ = 0x00000001,
    /**< enable or disable match interrupt for every frame */
    /**< Set max agc adjust level pos/neg */
    SYNTIANT_NDP10X_CONFIG_SET1_AGC_NOM_SPEECH_TARGET  = 0x00000002,
    /**< Set AGC target quiet speech level default 58*/
    SYNTIANT_NDP10X_CONFIG_SET1_AGC_MAX_ADJ = 0x00000004,
    /**< Set AGC max adjustable for positive and negative side */
    SYNTIANT_NDP10X_CONFIG_SET1_MEMORY_POWER = 0x00000008,
    /**< Set memory power modes */
    SYNTIANT_NDP10X_CONFIG_SET1_NOISE_THRESHOLD = 0x00000010,
    /**< Set noise threshold */
    SYNTIANT_NDP10X_CONFIG_SET1_NOISE_THRESHOLD_WIN = 0x00000020,
    /**< Set noise threshold window size */
    SYNTIANT_NDP10X_CONFIG_SET1_SENSOR = 0x00000040,
    /**< Set sensor configuration */
    SYNTIANT_NDP10X_CONFIG_SET1_ALL_M
    = ((((unsigned int) SYNTIANT_NDP10X_CONFIG_SET1_SENSOR) << 1U) - 1U)
};


/**
 * @brief memory identifier
 */
enum syntiant_ndp10x_config_memory_e {
    SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTROM = 0,
    SYNTIANT_NDP10X_CONFIG_MEMORY_RAM = 1,
    SYNTIANT_NDP10X_CONFIG_MEMORY_BOOTRAM = 2,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNNFEATURE = 3,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN00 = 4,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN01 = 5,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN02 = 6,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN03 = 7,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN04 = 8,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN05 = 9,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN06 = 10,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN07 = 11,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN08 = 12,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN09 = 13,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN10 = 14,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN11 = 15,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN12 = 16,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN13 = 17,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN14 = 18,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN15 = 19,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN16 = 20,
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN17 = 21,
    SYNTIANT_NDP10X_CONFIG_MEMORY_MAX =
    SYNTIANT_NDP10X_CONFIG_MEMORY_DNN17,
    SYNTIANT_NDP10X_CONFIG_MEMORY_SIZE =
    (SYNTIANT_NDP10X_CONFIG_MEMORY_MAX + 3) / 4 * 4
};

/**
 * @brief memory power state
 */
enum syntiant_ndp10x_config_memory_power_e {
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_NO_CHANGE   = 0,
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_OFF         = 1,
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_RETAIN      = 2,
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON          = 3,
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_MAX         =
    SYNTIANT_NDP10X_CONFIG_MEMORY_POWER_ON
};
    
/**
 * @brief serial interface type
 */
enum syntiant_ndp10x_serial_interface_e {
    SYNTIANT_NDP10X_SERIAL_INTERFACE_I2C = 0,   /** I2C */
    SYNTIANT_NDP10X_SERIAL_INTERFACE_SPI0 = 1, /* SPI mode 0 */
    SYNTIANT_NDP10X_SERIAL_INTERFACE_SPI1 = 2, /* SPI mode 1 */
    SYNTIANT_NDP10X_SERIAL_INTERFACE_SPI2 = 3, /* SPI mode 2 */
    SYNTIANT_NDP10X_SERIAL_INTERFACE_SPI3 = 4, /* SPI mode 3 */
    SYNTIANT_NDP10X_SERIAL_INTERFACE_MAX =
    SYNTIANT_NDP10X_SERIAL_INTERFACE_SPI3
};

/**
 * @brief sensor gpio role
 */
enum syntiant_ndp10x_config_gpio_role_e {
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_NONE   = 0, /** firmware won't touch */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_IDATA  = 1, /** I2C data */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_ICLK   = 2, /** I2C clock */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_MMISO  = 3, /** SPI master MISO */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_MMOSI  = 4, /** SPI master MOSI */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_MSCLK  = 5, /** SPI master clock */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_MSSEL  = 6, /** SPI slave select */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_I2SCLK = 7, /** I2S emulator bit clock */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_INTEF  = 8, /** falling edge interrupt */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_INTER  = 9, /** rising edge interrupt */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_INTLL  = 10,/** low level interrupt */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_INTLH  = 11,/** high level interrupt */
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_MAX    =
    SYNTIANT_NDP10X_CONFIG_GPIO_ROLE_INTLH
};

/**
 * @brief sensor type
 */
enum syntiant_ndp10x_config_sensor_id_e {
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_NONE = 0,   /** no sensor */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_BMI160 = 1, /** Bosch BM?160 (<= 10 axis) */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_VM3011 = 2,    /** energy detecting mic */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_EDGE_INT = 3, /** generic edge interrupt such as LiteOn ltr559 */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_DA217 = 4, /** Miramems DA217 motion sensor */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_KX120 = 5, /** Kionix KX120 motion sensor */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_MC3419 = 6, /** Memsic MC3419 motion sensor */
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_MAX =
    SYNTIANT_NDP10X_CONFIG_SENSOR_ID_MC3419
};

/** 
 * @brief sensor configuration constants
 */
enum syntiant_ndp10x_config_sensor_constants_e {
    SYNTIANT_NDP10X_CONFIG_SENSOR_GPIOS = 8,
    /** maximum number of gpio bits */
    SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS = 4,
    /** maximum number of supported sensors */
    SYNTIANT_NDP10X_CONFIG_SENSOR_AXES = 15,
    /** maximum number of supported axes per sensor */
    SYNTIANT_NDP10X_CONFIG_SENSOR_PARAMETERS = 16
    /** maximum number of supported parameters per sensor */
};

/**
 * @brief configuration variable constants
 */

/**
 * @brief NDP10x configuration data
 */
struct syntiant_ndp10x_config_s {
    unsigned int set;              /**< configuration variable set flags */
    unsigned int set1;             /**< more configuration variable set flags */
    int get_all; /**< read all configuration state (touching the chip) */
    /* fields returning valid info even when set == 0 && get_all == 0 */
    unsigned int tank_bits;        /**< holding tank bits 8 or 16 */
    unsigned int tank_input;       /**< input source for tank storage */
    unsigned int tank_size;        /**< holding tank size */
    unsigned int audio_frame_size;
    /**< samples per freq domain frame (read only) */
    unsigned int audio_frame_step;
    /**< samples per freq domain step (read only) */
    unsigned int freq_frame_size;  /**< feature extractor frame size (in bins) */
    unsigned int dnn_input;        /**< input source for dnn processing */
    unsigned int dnn_frame_size;   /**< streaming features per DNN frame */
    /* fields returning valid info when set != 0 || get_all != 0 */
    unsigned int input_clock_rate; /**< input clock rate in hz */
    unsigned int core_clock_rate;  /**< core clock rate in hz (read only) */
    unsigned int mcu_clock_rate;   /**< MCU clock rate in hz */
    unsigned int spi_max_pcm_input_rate;
    /**< maximum SPI speed for PCM data input in hz (read only) */
    unsigned int spi_word_bits;    /**< SPI input word bit width */
    unsigned int pdm_clock_rate;   /**< PDM clock rate in hz */
    unsigned int pdm_clock_ndp;    /**< PDM clock source is NDP */
    unsigned int pdm_in_shift[2];  /**< PDM input shift (~volume control) */
    unsigned int pdm_out_shift[2]; /**< PDM output shift (~volume control) */
    int pdm_dc_offset[2];          /**< PDM DC offset */
    unsigned int i2s_frame_size;   /**< I2S frame size in bits */
    unsigned int i2s_sample_size;  /**< I2S sample size in bits */
    unsigned int i2s_sample_msbit; /**< I2S sample most significant bit index */
    unsigned int freq_clock_rate;  /**< freq domain clock rate in hz */
    unsigned int preemphasis_exponent;
    /**< 'n' in preemphasis filter computation x[t] - x[t-1] * (1 - 2^(-n)) */
    unsigned int tank_max_size;
    /**< holding tank maximum allowable size (read only) */
    unsigned int audio_buffer_used;
    /**< audio input buffer used frames (read only) */
    unsigned int water_mark_on;
    /**< enable audio input buffer low water mark notification */
    unsigned int power_offset;
    /**< 'o' in FFT power computation (p + o) * 2^n */
    unsigned int power_scale_exponent;
    /**< 'n' in FFT power computation (p + o) * 2^n */
    unsigned int dnn_clock_rate;   /**< DNN clock rate in hz */
    unsigned int dnn_signed;
    /**< direct (spi or i2s) input to dnn is signed */
    unsigned int dnn_minimum_threshold;
    /**< input vector members <= threshold are set to '0' to reduce power */
    unsigned int dnn_run_threshold;
    /**< DNN will only run if threshold <= max(input vector) to reduce power */
    unsigned int dnn_inputs;       /**< DNN input vector size */
    unsigned int dnn_static_inputs;/**< DNN input vector static portion size */
    unsigned int dnn_outputs;      /**< DNN output vector size */
    unsigned int agc_on;           /**< automatic gain control on/off */
    unsigned int agc_max_adj[2];
    /**< AGC Max positive/negative adjustable AGC gain */
    unsigned int agc_nom_speech_quiet;
    /**< AGC reference levle for speech 0=100 */
    unsigned int match_per_frame_on;
    /**< match interrupt for every frame on/off */
    uint32_t fw_pointers_addr; /**< firmware state pointers addr (read only) */
    unsigned int secured;          /**< in secured mode (read only) */
    /* fields returning valid info when get_all != 0 */
    unsigned char memory_power[SYNTIANT_NDP10X_CONFIG_MEMORY_SIZE];
     /**< memory power state */
    unsigned int filter_frequency[SYNTIANT_NDP10X_MAX_FREQUENCY_BINS];
    /**< filter bank bin center frequencies
         ranging 0-511.  Hz can be computed:
         filter_frequency[i] / 512 * sample rate (read only) */
    int filter_eq[SYNTIANT_NDP10X_MAX_FREQUENCY_BINS];
    /**< filter bank bin equalizations -128-127 in 0.25 dB steps.
         the content of a bin 'b' = (strength[b] + eq[b]) / 4 dB */
    int32_t noise_threshold; /**< Noise threshold level */
    uint32_t noise_thresh_win; /***< Noise threshold window */
    unsigned char gpio_role[SYNTIANT_NDP10X_CONFIG_SENSOR_GPIOS];
    /** sensor interface GPIO bit roles */
    unsigned char sensor_id[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** sensor id */
    unsigned char sensor_interface[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** sensor interface type */
    unsigned char sensor_interface_address
    [SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** sensor i2c address or SPI select gpio # */
    unsigned char sensor_int[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** true if sensor presents an interrupt input */
    unsigned char sensor_int_gpio[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** sensor interrupt pin gpio or SYNTIANT_NDP10X_GPIO_NONE*/
    unsigned char sensor_axes[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS];
    /** sensor axis count */
    unsigned char sensor_axis_tank[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS]
    [SYNTIANT_NDP10X_CONFIG_SENSOR_AXES];
    /** store sensor axis in holding tank */
    unsigned char sensor_axis_input[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS]
    [SYNTIANT_NDP10X_CONFIG_SENSOR_AXES];
    /** store sensor axis in input vector */
    unsigned char sensor_parameter[SYNTIANT_NDP10X_CONFIG_SENSOR_SENSORS]
    [SYNTIANT_NDP10X_CONFIG_SENSOR_PARAMETERS];
    /** driver-specific typically per-axis sensor configuration parameter */
};

/**
 * @brief set and retrieve NDP10x configuration data
 *
 * Configuration variables with the corresponding
 * @c SYNTIANT_NDP10X_CONFIG_SET_* bit set in @c config->set will be applied to
 * the NDP10x device.  After performing any requested configuration, the current
 * configuration values will be returned in @c config.
 *
 * @param ndp NDP state object
 * @param config configuration data object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_config(
    struct syntiant_ndp_device_s *ndp, struct syntiant_ndp10x_config_s *config);


/**
 * @brief NDP10x firmware posterior handler set flags
 */
enum syntiant_ndp10x_posterior_config_set_e {
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_STATES = 0x0001,
    /**< set the number of states */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_CLASSES = 0x0002,
    /**< set the number of classes */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_SM_QUEUE_SIZE = 0x0004,
    /**< set the number of frames to smooth probabilities over */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT = 0x0008,
    /**< set the state timeout */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION = 0x0010,
    /**< set the state timeout action */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_THRESHOLD = 0x0020,
    /**< set the class active threshold */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_WINDOW = 0x0040,
    /**< set the class match window */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_BACKOFF = 0x0080,
    /**< set the class match backoff timer */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ENABLE = 0x0100,
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION = 0x0200,
    /**< set the class action*/
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_PH_TYPE = 0x0400,
    /**< set the posterior handler type */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_TIMEOUT_ACTION_V2 = 0x0800,
    /**< set the state timeout action V2*/
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ACTION_V2 = 0x1000,
    /**< set the class action V2*/
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ADAPTIVE = 0x2000,
    /**< set adaptive threshold parameters */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ALL_M
    = ((SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_ADAPTIVE << 1) - 1)
};

/**
 * @brief NDP10x firmware posterior handler set flags
 */
enum syntiant_ndp10x_posterior_config_action_type_e {
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_MATCH = 0,
    /**< report a match in the summary and go to state 0 */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STATE = 1,
    /**< go to state @c action_state */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STAY = 2,
    /**< stay in the current state and leave timer running */
    SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_MAX
    = ((SYNTIANT_NDP10X_POSTERIOR_CONFIG_ACTION_TYPE_STAY << 1) - 1)
};

/**
 * @brief NDP10x firmware posterior handler configuration data
 */
struct syntiant_ndp10x_posterior_config_s {
    unsigned int set;         /**< configuration variable set flags */
    unsigned int states;      /**< number of states */
    unsigned int classes;     /**< number of classes */
    unsigned int ph_type;     /**< posterior handler type */
    unsigned int adaptive_frames; /**< frames per adaptive update 0 -> off */
    unsigned int adaptive_denominator; /**< adaptive update denominator */
    unsigned int adaptive_max_updates; /**< max adaptive increase updates */
    unsigned int smoothing_queue_size;
    /**< number of frames to smooth probabilities over */
    unsigned int state;       /**< state number to configure */
    unsigned int timeout;     /**< state timeout in DNN frames */
    unsigned int timeout_action_type; /**< timeout action type (MATCH|STATE) */
    unsigned int timeout_action_arg; /*replaces the next two filed in phV5*/
    unsigned int timeout_action_match;
    /**< timeout action match summary code 0-63 */
    unsigned int timeout_action_state;/**< timeout action next state */
    unsigned int class_index;       /**< class_index number to configure */
    unsigned int threshold;   /**< class active threshold level 0-65535 */
    unsigned int window;      /**< class active window in DNN frames 0-255 */
    unsigned int backoff;     /**< match backoff timer in DNN frames 0-255 */
    unsigned int action_type; /**< match action type */
    unsigned int action_arg; /*replaces the next two filed in phV5*/
    unsigned int action_match;/**< match action match summary code 0-63 */
    unsigned int action_state;/**< match action next state */
    unsigned int adaptive_threshold_on; /**< class uses adaptive threshold */
};

/**
 * @brief set and retrieve NDP10x firmware posterior handler configuration
 *
 * Configuration variables with the corresponding
 * @c SYNTIANT_NDP10X_POSTERIOR_CONFIG_SET_* bit set in @c config->set will
 * be applied to the NDP10x device.  After performing any requested
 * configuration, the current configuration values will be returned in
 * @c config.
 *
 * Changing either @c config->states or @c config->classes will reset
 * the entire posterior handler configuration to an initial state
 * where every class has threshold 65535, window 0, backoff 0, match
 * action == MATCH, summary == class #.
 *
 * This function will return @c ERROR_ARG if any other configuration data
 * is requested to be set in the same call as either @c states or @c classes.
 *
 * This function will return @c ERROR_UNINIT if firmware is not presently
 * loaded.
 *
 * @param ndp NDP state object
 * @param config configuration data object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_posterior_config(struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp10x_posterior_config_s *config);

/**
 * @brief GPIO operational flags
 */
enum syntiant_ndp10x_gpio_flags_e {
    SYNTIANT_NDP10X_GPIO_FLAGS_GET_OUTS         = 0x01, /**< get in/out bits */
    SYNTIANT_NDP10X_GPIO_FLAGS_GET_VALUES       = 0x02, /**< get value bits */
    SYNTIANT_NDP10X_GPIO_FLAGS_SET_INPUT_ENABLE = 0x04, /**< set input enable */
    SYNTIANT_NDP10X_GPIO_FLAGS_GET_INPUT_ENABLE = 0x08, /**< get input enable */
    SYNTIANT_NDP10X_GPIO_FLAGS_ALL =
    (SYNTIANT_NDP10X_GPIO_FLAGS_GET_INPUT_ENABLE << 1) - 1
};

/**
 * @brief GPIO configuration data
 */
struct syntiant_ndp10x_gpio_s {
    unsigned int flags;           /**< operation flags */
    unsigned int set_outs_mask;   /**< per-gpio in/out mode setting mask */
    unsigned int outs;            /**< per-gpio output (set is masked) */
    unsigned int set_values_mask; /**< per-gpio value setting mask */
    unsigned int values;          /**< per-gpio value in/out (set is masked) */
    unsigned int input_enable;    /**< global input enable */
};

/**
 * @brief control NDP10x GPIO pins
 *
 * GPIO direction bits with the corresponding @c set_out_mask bits will be
 * set according to the values in @c out.
 * All GPIO direction bits will be read into @c out if @c flags 
 * contains @c SYNTIANT_NDP10X_GPIO_FLAGS_GET_OUT.
 * 
 * GPIO bits with the corresponding @c set_value_mask bits will be
 * set according to the values in @c value.
 * All GPIO direction bits will be loaded into @c out if @c flags 
 * contains @c SYNTIANT_NDP10X_GPIO_FLAGS_GET_VALUE.
 * 
 * The global GPIO input enable bit will be set to @c input_enable if @c flags 
 * contains @c SYNTIANT_NDP10X_GPIO_FLAGS_SET_INPUT_ENABLE.
 * The global GPIO input enable bit value will be read into @c input_enable
 * if @c flags contains @c SYNTIANT_NDP10X_GPIO_FLAGS_GET_INPUT_ENABLE.
 *
 * @param ndp NDP state object
 * @param gpio gpio data object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_gpio(struct syntiant_ndp_device_s *ndp,
                                struct syntiant_ndp10x_gpio_s *gpio);


/**
 * @brief NDP10x status and debugging set flags
 */
enum syntiant_ndp10x_serial_cmd_e {
    SYNTIANT_NDP10X_SERIAL_CMD_IN = 0x0,  /**< receive a byte */
    SYNTIANT_NDP10X_SERIAL_CMD_OUT = 0x1, /**< send a byte */
    SYNTIANT_NDP10X_SERIAL_CMD_MAX =
    SYNTIANT_NDP10X_SERIAL_CMD_OUT
};

/**
 * @brief perform a serial interface transfer from the NDP MCU
 *
 * This function sends and then receives a sequence of bytes on a serial
 * interface connected device attached to the NDP (I2C or SPI).
 *
 * The NDP pin assignments for the SPI and I2C must be configured through
 * `syntiant_ndp10x_config()` before performing any transfers.
 * The host will request the transfer using a firmware-defined
 * messaging protocol that includes synchronization with a mailbox exchange.
 * This function will block until the transfer is completed.
 *
 * Serially-connected devices typical support the notion of a transaction
 * that includes sending initial control information exchange (e.g. transfer
 * direction and internal resource addressing) followed by sending or receiving
 * payload bytes associated with the operation.  @c cont allows a transaction
 * to be extended across multiple calls to this function.
 *
 * @param ndp NDP state object
 * @param ifc_type interface type from syntiant_ndp10x_config_serial_interface_e
 * @param ifc_addr interface address (I2C or gpio # for SPI)
 * @param out data to send on the interface
 * @param outlen number of bytes to send on the interface
 * @param in data received on the interface
 * @param inlen number of bytes to receive on the interface
 * @param continue_ true if transaction should be continued
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_serial_transfer(struct syntiant_ndp_device_s *ndp,
                                           unsigned int ifc_type,
                                           unsigned int ifc_addr,
                                           uint8_t *out, unsigned int outlen,
                                           uint8_t *in, unsigned int inlen,
                                           int continue_);

/**
 * @brief NDP10x status and debugging set flags
 */
enum syntiant_ndp10x_status_set_e {
    SYNTIANT_NDP10X_STATUS_SET_CLEAR = 0x01,
    /**< clear status state (counters, etc.) */
    SYNTIANT_NDP10X_STATUS_SET_MAILBOX_TRACE = 0x02,
    /**< set or clear mailbox tracing */
    SYNTIANT_NDP10X_STATUS_SET_ALL_M
    = ((SYNTIANT_NDP10X_STATUS_SET_MAILBOX_TRACE << 1) - 1)
};

/**
 * @brief NDP10x status and debug mailbox directions
 */
enum syntiant_ndp10x_status_mailbox_direction_e {
    SYNTIANT_NDP10X_STATUS_MAILBOX_DIRECTION_HOST_TO_MCU = 0,
    SYNTIANT_NDP10X_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST = 1,
    SYNTIANT_NDP10X_STATUS_MAILBOX_DIRECTION_MAX
    = SYNTIANT_NDP10X_STATUS_MAILBOX_DIRECTION_MCU_TO_HOST
};

/**
 * @brief NDP10x status and debugging information
 */
struct syntiant_ndp10x_status_s {
    unsigned int set;               /**< set flags */
    int mailbox_trace;              /**< enable mailbox tracing */
    uint32_t h2m_mailbox_req;       /**< host to mcu mailbox requests */
    uint32_t h2m_mailbox_rsp;       /**< host to mcu mailbox responses */
    uint32_t h2m_mailbox_unexpected;
    /**< host to mcu mailbox unexpected messages */
    uint32_t h2m_mailbox_error;
    /**< host to mcu mailbox protocol errors */
    uint32_t m2h_mailbox_req;       /**< mcu to host mailbox requests */
    uint32_t m2h_mailbox_rsp;       /**< mcu to host mailbox responses */
    uint32_t m2h_mailbox_unexpected;
    /**< mcu to host mcu mailbox unexpected messages */
    uint32_t m2h_mailbox_error;
    /**< mcu to host mcu mailbox protocol errors */
    uint32_t missed_frames;         /**< missed MATCH reports */
};

/**
 * @brief retrieve NDP10x status and debugging information
 *
 * Configuration variables with the corresponding
 * @c SYNTIANT_NDP10X_STATUS_SET_* bit set in @c debug->set will
 * be updated.  After performing any requested updates, the current state
 * will be returned in @c status.
 *
 * @param ndp NDP state object
 * @param status status data object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_status(struct syntiant_ndp_device_s *ndp,
                                  struct syntiant_ndp10x_status_s *status);

/**
 * @brief mailbox NOP call to check operation of NDP10X
 *
 * Mailbox call
 *
 * @param ndp NDP state object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp10x_mb_nop(struct syntiant_ndp_device_s *ndp);

enum syntiant_ndp_debug_extract_type_e {
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_FW_STATE = 0x00,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_AGC_STATE = 0x01,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_PH_STATE = 0x02,
    SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_LAST
    = SYNTIANT_NDP10X_DEBUG_EXTRACT_TYPE_PH_STATE
};

extern int syntiant_ndp10x_debug_extract(struct syntiant_ndp_device_s *ndp,
                                         int type, void *data,
                                         unsigned int *len);

extern int syntiant_ndp10x_reset_fix(struct syntiant_ndp_device_s *ndp);

extern uint32_t syntiant_ndp10x_get_fw_agc_addr(
        struct syntiant_ndp_device_s *ndp);

extern int syntiant_ndp10x_read(struct syntiant_ndp_device_s *ndp, int mcu,
        uint32_t address, void *value);

extern int syntiant_ndp10x_read_block(struct syntiant_ndp_device_s *ndp,
        int mcu, uint32_t address, void *value, unsigned int size);

extern struct syntiant_ndp_driver_s* syntiant_ndp10x_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif
