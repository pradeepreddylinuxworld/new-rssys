/*
 * SYNTIANT CONFIDENTIAL
 *
 * _____________________
 *
 * Copyright (c) 2017-2021 Syntiant Corporation
 * All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains the property of
 * Syntiant Corporation and its suppliers, if any.  The intellectual and
 * technical concepts contained herein are proprietary to Syntiant Corporation
 * and its suppliers and may be covered by U.S. and Foreign Patents, patents in
 * process, and are protected by trade secret or copyright law.  Dissemination
 * of this information or reproduction of this material is strictly forbidden
 * unless prior written permission is obtained from Syntiant Corporation.
 *
 */

/*
 * NOTE:
 * 1. this header file is used in the syntiant-ilib repo as an interface file
 * 2. Some compilers do not pack the structs which means they are always 32 bit
 * aligned so they are explicitly made 32 bit aligned so please follow that.
 */

#ifndef NDP10X_FIRMWARE_H
#define NDP10X_FIRMWARE_H

#include <syntiant-firmware/ndp10x_smap.h>

#ifdef __cplusplus
extern "C" {
#endif
    
/**********************************************************************
 **********************************************************************
 * 
 * Firmware host <-> MCU interface state
 * 
 **********************************************************************
 **********************************************************************/

#define FW_VER "##FIRMWARE_VERSION##"

#define FW_VER_SIZE 24

/**
 * @brief defines constants used by ilib code to store configuration in
 * ndp memory. This constants define beginning and length of free memory
 * location.
 *
 */
enum ndp10x_memory_region_constants_e {
    NDP10X_ILIB_SCRATCH_ORIGIN = 0x20017000,
    NDP10X_ILIB_SCRATCH_LENGTH = 0xc00
};

/**
 * @brief indexes for addresses of various components of firmware. The
 * addresses are stored in fw_state.addresses and this constants are used
 * as indices for addresses array.
 *
 */
enum ndp10x_fw_state_address_indices_e {
    NDP10X_FW_STATE_ADDRESS_INDEX_FW_STATE = 0x0,
    NDP10X_FW_STATE_ADDRESS_INDEX_TANK_START = 0x5,
    NDP10X_FW_STATE_ADDRESS_INDEX_TANK_END = 0x6
};
    
/**
 * @brief addresses for various firmware data structures to be located
 * by the ILib.  This structure is placed immediately preceeding the
 * first instruction to be executed (thus can be located by following
 * the starting address vector and working 'backwards').  The population is
 * done in startup.s and its size MUST NOT CHANGE without changing
 * the code in startup.s.
 */
struct ndp10x_fw_state_pointers_s {
    uint32_t addresses[8];
};

/* this structure is defined in startup.s */
extern struct ndp10x_fw_state_pointers_s fw_state_pointers;
    
/**
 * @brief defines the states of MCU to HOST mailbox exchange
 *
 */
enum ndp10x_m2h_state_e {
    NDP10X_M2H_STATE_IDLE = 0,
    NDP10X_M2H_STATE_MATCH = 1
};

/**
 * @brief defines the states of HOST to MCU mailbox exchange
 *
 */
enum ndp10x_h2m_state_e {
    NDP10X_H2M_STATE_IDLE = 0,
    NDP10X_H2M_STATE_EXTOP = 1,
    NDP10X_H2M_STATE_DATA_OUT = 2
};

/**
 * @brief define constants to extract information from mailbox.
 *
 */
enum ndp10x_mb_protcol_e {
    NDP10X_MB_HOST_TO_MCU_OWNER = 0x08,
    NDP10X_MB_HOST_TO_MCU_M = 0x07,
    NDP10X_MB_HOST_TO_MCU_S = 0,
    NDP10X_MB_MCU_TO_HOST_OWNER = 0x80,
    NDP10X_MB_MCU_TO_HOST_M = 0x70,
    NDP10X_MB_MCU_TO_HOST_S = 4
};

#define NDP10X_MB_HOST_TO_MCU_INSERT(m, r)                                     \
    ((((m) & ~NDP10X_MB_HOST_TO_MCU_M) ^ NDP10X_MB_HOST_TO_MCU_OWNER)          \
        | ((r) << NDP10X_MB_HOST_TO_MCU_S))

#define NDP10X_MB_MCU_TO_HOST_INSERT(m, r)                                     \
    ((((m) & ~NDP10X_MB_MCU_TO_HOST_M) ^ NDP10X_MB_MCU_TO_HOST_OWNER)          \
        | ((r) << NDP10X_MB_MCU_TO_HOST_S))

/* short and long request codes are numbered contiguously (for now) */
enum ndp10x_mb_request_e {
    NDP10X_MB_REQUEST_NOP = 0x0,
    NDP10X_MB_REQUEST_CONT = 0x1,
    NDP10X_MB_REQUEST_MATCH = 0x2, /* m2h */
    NDP10X_MB_REQUEST_EXTOP = 0x3,
    NDP10X_MB_REQUEST_DATA = 0x4,
    NDP10X_MB_REQUEST_MIADDR = 0x8
};
#define NDP10X_MB_M2H_REQUEST_DECODER                                          \
    {                                                                          \
        "nop", "cont", "match", "extop", "data(0)", "data(1)", "data(2)",      \
            "data(3)"                                                          \
    }
#define NDP10X_MB_H2M_REQUEST_DECODER                                          \
    {                                                                          \
        "nop", "cont", "ILLEGAL(2)", "extop", "data(0)", "data(1)", "data(2)", \
            "data(3)", "miaddr"                                                \
    }

/* response codes (3-bit) */
enum ndp10x_mb_response_e {
    NDP10X_MB_RESPONSE_SUCCESS = 0x0,
    NDP10X_MB_RESPONSE_CONT = NDP10X_MB_REQUEST_CONT,
    NDP10X_MB_RESPONSE_ERROR = 0x2,
    NDP10X_MB_RESPONSE_DATA = NDP10X_MB_REQUEST_DATA
};
#define NDP10X_MB_RESPONSE_DECODER                                             \
    {                                                                          \
        "success", "cont", "error", "ILLEGAL(3)", "data(0)", "data(1)",        \
            "data(2)", "data(3)"                                               \
    }

/* error codes (6-bit) */
enum ndp10x_mb_error_e {
    NDP10X_MB_ERROR_FAIL = 0x0,
    NDP10X_MB_ERROR_UNEXPECTED = 0x1
};
#define NDP10X_MB_ERROR_DECODER                                                \
    {                                                                          \
        "fail", "unexpected"                                                   \
    }

enum ndp10x_mb_match_offsets_e {
    NDP10X_MB_MATCH_SUMMARY_O = 0x00,
    NDP10X_MB_MATCH_BINARY_O = 0x04,
    NDP10X_MB_MATCH_STRENGTH_O = 0x0c
};



/**
 * @brief enum values to enable/disable various components of firmware. This
 * constants will also be used by ilib code to enable/disable the components.
 *
 */
enum ndp10x_fw_state_address_enable_e {
    NDP10X_FW_STATE_ENABLE_AGC = 0x1,
    NDP10X_FW_STATE_ENABLE_POSTERIOR = 0x2,
    NDP10X_FW_STATE_ENABLE_SMAX_SMOOTHER = 0x4
};

/**
 * @brief GPIO bit roles
 */
enum ndp10x_fw_gpio_role_e {
    NDP10X_FW_GPIO_ROLE_NONE = 0,    /** firmware will not touch */
    NDP10X_FW_GPIO_ROLE_IDATA = 1,   /** I2C data */
    NDP10X_FW_GPIO_ROLE_ICLK = 2,    /** I2C clock */
    NDP10X_FW_GPIO_ROLE_MMISO = 3,   /** SPI master MISO */
    NDP10X_FW_GPIO_ROLE_MMOSI = 4,   /** SPI master MOSI */
    NDP10X_FW_GPIO_ROLE_MSCLK = 5,   /** SPI master clock */
    NDP10X_FW_GPIO_ROLE_MSSEL = 6,   /** SPI slave select */
    NDP10X_FW_GPIO_ROLE_I2SCLK = 7,  /** I2S emulator bit clock */
    NDP10X_FW_GPIO_ROLE_INTEF = 8,   /** falling edge interrupt */
    NDP10X_FW_GPIO_ROLE_INTER = 9,   /** rising edge interrupt */
    NDP10X_FW_GPIO_ROLE_INTLL = 10,  /** low level interrupt */
    NDP10X_FW_GPIO_ROLE_INTLH = 11   /** high level interrupt */
};
    
/**
 * @brief sensor driver device type identifier
 */
enum ndp10x_fw_sensor_id_e {
    NDP10X_FW_SENSOR_ID_NONE = 0,   /* no sensor */
    NDP10X_FW_SENSOR_ID_BMI160 = 1, /* Bosch BMI160 (6 or 10 axis) */
    NDP10X_FW_SENSOR_ID_VM3011 = 2,    /* VM3011 energy detecting microphone */
    NDP10X_FW_SENSOR_ID_EDGE_INT = 3,  /* Generic edge interrupt sensor such as LiteOn LTR559 */
    NDP10X_FW_SENSOR_ID_DA217 = 4,     /* Miramems DA217 motion sensor */
    NDP10X_FW_SENSOR_ID_KX120 = 5,     /* Kionix KX120 motion sensor */
    NDP10X_FW_SENSOR_ID_MC3419 = 6     /* Kionix MC3419 motion sensor */
};
    
enum ndp10x_firmware_constants_e {
    NDP10X_MATCH_RING_SIZE = 10,
    NDP10X_MATCH_SNR_SHIFT = 8,
    NDP10X_MATCH_SNR_MASK  = 0xff << NDP10X_MATCH_SNR_SHIFT,
    NDP10X_MATCH_CONFIDENCE_SHIFT = 16,
    NDP10X_MATCH_CONFIDENCE_MASK  = 0xff << NDP10X_MATCH_CONFIDENCE_SHIFT,
    NDP10X_MATCH_NOISE_THRESHOLD_SHIFT = 24,
    NDP10X_MATCH_NOISE_THRESHOLD_MASK =
    0xff << NDP10X_MATCH_NOISE_THRESHOLD_SHIFT,
    NDP10X_FW_SERIAL_BYTE_MAX = 12,
    NDP10X_FW_SERIAL_CONTROL_RUN_SHIFT = 0,
    NDP10X_FW_SERIAL_CONTROL_RUN_MASK =
    0x1 << NDP10X_FW_SERIAL_CONTROL_RUN_SHIFT,
    NDP10X_FW_SERIAL_CONTROL_CONTINUE_SHIFT = 1,
    NDP10X_FW_SERIAL_CONTROL_CONTINUE_MASK =
    0x1 << NDP10X_FW_SERIAL_CONTROL_CONTINUE_SHIFT,
    NDP10X_FW_SERIAL_CONTROL_STATUS_SHIFT = 6,
    NDP10X_FW_SERIAL_CONTROL_STATUS_MASK =
    0x3 << NDP10X_FW_SERIAL_CONTROL_STATUS_SHIFT,
    NDP10X_FW_SERIAL_CONTROL_STATUS_SUCCESS = 0,
    NDP10X_FW_SERIAL_CONTROL_STATUS_TIMEOUT = 1,
    NDP10X_FW_SERIAL_CONTROL_STATUS_ERROR = 2,
    NDP10X_FW_SERIAL_CONTROL_ADDRESS_SHIFT = 8,
    NDP10X_FW_SERIAL_CONTROL_ADDRESS_MASK =
    0xff << NDP10X_FW_SERIAL_CONTROL_ADDRESS_SHIFT,
    NDP10X_FW_SERIAL_CONTROL_OUTLEN_SHIFT = 16,
    NDP10X_FW_SERIAL_CONTROL_OUTLEN_MASK =
    0x3f << NDP10X_FW_SERIAL_CONTROL_OUTLEN_SHIFT,
    NDP10X_FW_SERIAL_CONTROL_INLEN_SHIFT = 22,
    NDP10X_FW_SERIAL_CONTROL_INLEN_MASK =
    0x3f << NDP10X_FW_SERIAL_CONTROL_INLEN_SHIFT,
    NDP10X_FW_SERIAL_ADDRESS_I2C_ADDRESS_MASK = 0x7f,
    NDP10X_FW_SERIAL_ADDRESS_I2C_ADDRESS_SHIFT = 0,
    NDP10X_FW_SERIAL_ADDRESS_SPI_SELECT_GPIO_MASK = 0x1f,
    NDP10X_FW_SERIAL_ADDRESS_SPI_SELECT_GPIO_SHIFT = 0,
    NDP10X_FW_SERIAL_ADDRESS_SPI_MODE_MASK = 0x60,
    NDP10X_FW_SERIAL_ADDRESS_SPI_MODE_SHIFT = 5,
    NDP10X_FW_SERIAL_ADDRESS_I2C_MASK = 0x80,
    NDP10X_FW_SERIAL_ADDRESS_I2C_SHIFT = 7,
    NDP10X_FW_SENSOR_MAX = 4,
    NDP10X_FW_GPIO_MAX = 8,
    NDP10X_FW_SENSOR_AXIS_MAX = 15,
    NDP10X_FW_SENSOR_CONTROL_ID_MASK = 0xff,
    NDP10X_FW_SENSOR_CONTROL_ID_SHIFT = 0,
    NDP10X_FW_SENSOR_CONTROL_ADDRESS_MASK = 0xff00,
    NDP10X_FW_SENSOR_CONTROL_ADDRESS_SHIFT = 8,
    NDP10X_FW_SENSOR_CONTROL_INT_GPIO_MASK = 0xf0000,
    NDP10X_FW_SENSOR_CONTROL_INT_GPIO_SHIFT = 16,
    NDP10X_FW_SENSOR_CONTROL_AXES_MASK = 0xf00000,
    NDP10X_FW_SENSOR_CONTROL_AXES_SHIFT = 20,
    NDP10X_FW_SENSOR_ENABLE_TANK_SHIFT = 0,
    NDP10X_FW_SENSOR_ENABLE_TANK_MASK = 0x7fff,
    NDP10X_FW_SENSOR_ENABLE_INPUT_MASK = 0x7fff0000,
    NDP10X_FW_SENSOR_ENABLE_INPUT_SHIFT = 16,
    NDP10X_FW_BMI160_PARAMETERS_TEST_ISIZE = 12,
    NDP10X_FW_BMI160_PARAMETERS_TEST_ISIZE_SCALE = 16,
    NDP10X_FW_BMI160_PARAMETERS_INT_PASSTHROUGH = 13,
    NDP10X_FW_BMI160_PARAMETERS_INT_PASSTHROUGH_TEST = 0xff,
    NDP10X_FW_BMI160_PARAMETERS_FLUSH_THRESHOLD = 14,
    NDP10X_FW_BMI160_PARAMETERS_DNN_INPUT_THRESHOLD = 15,
    NDP10X_FW_BMI160_TEST_BASE = 0x20012800,
    NDP10X_FW_VM3011_PARAMETERS_NORMAL_CLOCK_DIV = 0,
    NDP10X_FW_VM3011_PARAMETERS_TIMEOUT_COUNT = 1,
    NDP10X_FW_VM3011_PARAMETERS_CURRENT_THRESHOLD = 2,
    NDP10X_FW_VM3011_PARAMETERS_PGA_MAX_GAIN = 3,
    NDP10X_FW_VM3011_PARAMETERS_MATCH_MIN_SPL = 4,
    NDP10X_FW_VM3011_PARAMETERS_TEST_SPL = 5,
    NDP10X_FW_NDP10X_PARAMETERS_EDGE_INT_ID = 0,
    NDP10X_FW_DA217_PARAMETERS_PICK_UP_ID = 0,
    NDP10X_FW_DA217_PARAMETERS_SIGNIFICANT_MOTION_ID = 1,
    NDP10X_FW_DA217_PARAMETERS_TILT_ID = 2,
    NDP10X_FW_DA217_PARAMETERS_DOUBLE_TAP_ID = 3,
    NDP10X_FW_DA217_PARAMETERS_FREE_FALL_ID = 4,
    NDP10X_FW_DA217_PARAMETERS_STEP_DETECT_ID = 5,
    NDP10X_FW_DA217_EVENT_TYPES = 6,
    NDP10X_FW_DA217_PARAMETERS_TEST_TRIGGER = 15,
    NDP10X_FW_DA217_NUM_AXES = 3,
    NDP10X_FW_DA217_BYTES_PER_AXIS = 2,
    NDP10X_FW_DA217_BYTES_PER_READ =
        NDP10X_FW_DA217_NUM_AXES * NDP10X_FW_DA217_BYTES_PER_AXIS + 2,
    NDP10X_FW_DA217_MAIN_BUF_ENTRIES = 128,
    NDP10X_FW_DA217_RAW_BUF_ENTRIES = 64,
    NDP10X_FW_DA217_RAW_BUF_ENTRIES_IN_POWER_2 = 6,
    NDP10X_FW_DA217_RAW_SAMPLE_RING_BUF_SIZE = NDP10X_FW_DA217_RAW_BUF_ENTRIES
        * NDP10X_FW_DA217_BYTES_PER_READ,
    NDP10X_FW_ACCEL_PARAMETERS_PICK_UP_ID = 0,
    NDP10X_FW_ACCEL_PARAMETERS_SIGNIFICANT_MOTION_ID = 1,
    NDP10X_FW_ACCEL_PARAMETERS_TILT_ID = 2,
    NDP10X_FW_ACCEL_PARAMETERS_DOUBLE_TAP_ID = 3,
    NDP10X_FW_ACCEL_PARAMETERS_FREE_FALL_ID = 4,
    NDP10X_FW_ACCEL_PARAMETERS_STEP_DETECT_ID = 5,
    NDP10X_FW_ACCEL_EVENT_TYPES = 6,
    NDP10X_FW_ACCEL_PARAMETERS_TEST_TRIGGER = 15,
    NDP10X_FW_ACCEL_NUM_AXES = 3,
    NDP10X_FW_ACCEL_BYTES_PER_AXIS = 2,
    NDP10X_FW_ACCEL_BYTES_PER_READ =
        NDP10X_FW_ACCEL_NUM_AXES * NDP10X_FW_ACCEL_BYTES_PER_AXIS + 2,
    NDP10X_FW_ACCEL_MAIN_BUF_ENTRIES = 256,
    NDP10X_FW_ACCEL_RAW_BUF_ENTRIES = 8,
    NDP10X_FW_ACCEL_RAW_BUF_ENTRIES_IN_POWER_2 = 3,
    NDP10X_FW_ACCEL_RAW_SAMPLE_RING_BUF_SIZE = NDP10X_FW_ACCEL_RAW_BUF_ENTRIES
        * NDP10X_FW_ACCEL_BYTES_PER_READ,
    NDP10X_FW_SENSOR_MAX_INPUT_MEMBERS = 128,
    NDP10X_FW_POSTERIOR_PARAMETERS_SIZE = 4096
};

/* currently define KX120. Will need to unify buffer sizes
   across all motion sensors */
#define  KX120

#ifdef KX120
#define NDP10X_RAW_SAMPLE_RING_BUF_SIZE   NDP10X_FW_ACCEL_RAW_SAMPLE_RING_BUF_SIZE
#define NDP10X_MAIN_BUF_ENTRIES           NDP10X_FW_ACCEL_MAIN_BUF_ENTRIES
#define NDP10X_ACCEL_NUM_AXES             NDP10X_FW_ACCEL_NUM_AXES
#else
#define NDP10X_RAW_SAMPLE_RING_BUF_SIZE   NDP10X_FW_DA217_RAW_SAMPLE_RING_BUF_SIZE
#define NDP10X_MAIN_BUF_ENTRIES           NDP10X_FW_DA217_MAIN_BUF_ENTRIES
#define NDP10X_ACCEL_NUM_AXES             NDP10X_FW_DA217_NUM_AXES
#endif

/**
 * @brief serial communications control area
 */
struct ndp10x_fw_serial_s {
    uint32_t control; /** address, outlen, inlen, finish */
    uint32_t data[NDP10X_FW_SERIAL_BYTE_MAX / 4]; /** serial data buffer */
};

#define SERIAL_DATA8(s) ((uint8_t *) (s)->data)
#define SERIAL_DATA16(s) ((uint16_t *) (s)->data)
#define SERIAL_DATA32(s) ((s)->data)

/**
 * @brief attached sensor configuration
 */
struct ndp10x_fw_sensor_configuration_s {
    uint32_t control;   /** sensor control fields */
    uint32_t enable;    /** tank and input enable bits */
    uint8_t parameter[NDP10X_FW_SENSOR_AXIS_MAX + 1];
    /** per-driver configuration, e.g. axis input shift */
};

/* accumulated input vector member state */
struct ndp10x_dnn_input_state_s {
    uint32_t input_count;
    uint32_t flushed;
    uint32_t test_offset;
    uint8_t inputs[NDP10X_FW_SENSOR_MAX_INPUT_MEMBERS];
};

struct ndp10x_sensor_vm3011_state_s {
    uint32_t current_timeout;
    uint32_t current_state;
    uint32_t current_threshold;
    uint32_t pga_max_gain;
    uint32_t test_spl;
    int32_t clear_sum;
    uint32_t two_frame_sum;
    uint32_t max_avg;
    uint32_t previous_sum;
};
    
union ndp10x_fw_sensor_state_u {
    struct ndp10x_dnn_input_state_s input;
    struct ndp10x_sensor_vm3011_state_s sensor_vm3011;
};

/**
 * @brief enum values to be used by firmware to determine if dnn
 * match should be suppress based on other sensor input.
 *
 */
enum ndp10x_firmware_result_flags_e {
    NDP10X_FIRMWARE_RESULT_FLAGS_MATCH = 0x1,
    NDP10X_FIRMWARE_RESULT_FLAGS_SUPPRESS_DNN_MATCH = 0x2
};

struct ndp10x_fw_match_s {
    uint32_t summary;
    uint32_t tankptr;
};

struct max_gain_adj_t {
    uint8_t max_neg_adjust_gain;
    uint8_t max_pos_adjust_gain;
    uint8_t reserved[2];
};

enum ndp10x_results_constants_e {
    NDP10X_RESULT_NUM_CLASSES = 32,
    NDP10X_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE = 12
};

/**
 * @brief Stores all the results collected from hardware and firmware
 *
 */
struct ndp10x_result_s {
    uint32_t num_classes;
    uint32_t max_index;
    uint8_t raw_strengths[NDP10X_RESULT_NUM_CLASSES];
    uint32_t softmax_strengths[NDP10X_RESULT_NUM_CLASSES];
    uint32_t winner_one_hot[2];
    uint32_t summary; /* formatted the same as NDP10X_SPI_MATCH */
    uint32_t tankptr;
};

enum ndp10x_ph_action_e {
    NDP10X_PH_ACTION_STATE_M = 0x7f,
    NDP10X_PH_ACTION_STAY = NDP10X_PH_ACTION_STATE_M,
    NDP10X_PH_ACTION_MATCH_M = 0x80
};

enum ndp_ph_action_e {
    NDP_PH_ACTION_MATCH = 0,
    NDP_PH_ACTION_STATE = 1,
    NDP_PH_ACTION_STAY = 2
};

enum ndp10x_ph_threshold_flags_e {
    NDP10X_PH_THRESHOLD_ADAPTIVE_UPDATES_ENABLE_M = 0x00010000,
    NDP10X_PH_THRESHOLD_FLAGS_M = NDP10X_PH_THRESHOLD_ADAPTIVE_UPDATES_ENABLE_M
};

/**
 * @brief parameters for each class
 *
 */
struct ndp10x_ph_class_params_s {
    uint32_t window;
    uint32_t threshold;
    uint32_t backoff;
    uint32_t action;
    uint32_t smoothing_queue_size;
};

/**
 * @brief parameters for each state
 *
 */
struct ndp10x_ph_state_params_s {
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t class_params_offset;
};

/**
 * @brief All parameters used by posterior handler
 *
 */
struct ndp10x_ph_params_s {
    uint32_t num_classes;
    uint32_t num_states;
    /* the frame processing function to be used */
    uint32_t ph_type;
    uint32_t adaptive_frames;       /* interval between updates 0 -> adapt off */
    uint32_t adaptive_denominator;  /* denominator of update */
    uint32_t adaptive_max_updates;  /* maximum updates */
    /* place holder of 1 state (3 words) with max classes (5 words/class) */
    uint32_t params_memory[(sizeof(struct ndp10x_ph_state_params_s)
                            + NDP10X_RESULT_NUM_CLASSES
                            * sizeof(struct ndp10x_ph_class_params_s))
                           / sizeof(uint32_t)];
};

/*
 * This structure is used by the ILib to communicate with the firmware
 */
struct ndp10x_host_interface_s {
    /*
     * These members must be in fixed locations and never change.
     * The uILib relies on these members and does not include
     * the firmware header files.
     * ALL of the members of this structure must be sized integers to ensure
     * proper host <-> firmware communication.  No pointers or other values
     * with system-specific sizes are allowed
     */
    uint32_t tankptr;     /* tank pointer with all 17 bits */
    uint32_t match_ring_size;
    uint32_t match_producer;
    struct ndp10x_fw_match_s match_ring[NDP10X_MATCH_RING_SIZE];
    /*
     * frame results after posterior computations
     */
    struct ndp10x_result_s result;

    uint32_t enable;

    uint32_t tank_reset;              /* host->mcu set to 1 when tank reset */

    /* serial communication host-to-mcu state */
    volatile struct ndp10x_fw_serial_s serial;

    /* GPIO configuration host-to-mcu state */
    volatile uint32_t gpio_role[NDP10X_FW_GPIO_MAX / 4];

    /* Sensor configuration host-to-mcu state */
    volatile struct ndp10x_fw_sensor_configuration_s
                    sensor[NDP10X_FW_SENSOR_MAX];

    uint32_t enable_match_for_every_frame;
    uint32_t m2h_match_skipped;

    struct max_gain_adj_t max_adjustment_gain; /* Q0*/
    int32_t nom_speech_quiet; /* Reference level for speech in quiet (-90) */
    int32_t noise_threshold;       /* Noise threshold for host notification */
    uint32_t noise_threshold_win;  /* window size for noise level tracking */
    uint32_t noise_threshold_cntr_hi; /* high noise threshold breach */
    uint32_t noise_threshold_cntr_lo; /* low noise threshold breach */

    uint32_t sensor_raw_producer;
    uint32_t sensor_raw_sample_buf[NDP10X_RAW_SAMPLE_RING_BUF_SIZE
                                   / sizeof(uint32_t)];
    uint32_t posterior_parameters[NDP10X_FW_POSTERIOR_PARAMETERS_SIZE
                                  / sizeof(uint32_t)];
};
    
/**********************************************************************
 **********************************************************************
 * 
 * Firmware private state (ILib use is firmware debugging only)
 * 
 **********************************************************************
 **********************************************************************/


#ifdef CORTEX_M0
typedef uint32_t pointer_t;
#else
typedef void *pointer_t;
#endif

/**
 * @brief Posterior Handler state
 *
 */
struct ndp10x_ph_state_s {
    uint32_t current_class;
    uint32_t class_count;
    uint32_t adaptive_class_count;
    uint32_t current_state;
    uint32_t backoff_counter;
    uint32_t timeout;
    uint32_t timeout_action;
    uint32_t frame_counter;
    uint32_t winner;
    uint32_t winner_prob;
    uint32_t threshold;
    uint32_t match;
    uint32_t window;
    uint32_t tankptr;
    uint32_t class_tankptr;
    uint32_t adaptive_frames_remaining;
    uint32_t adaptive_updates_remaining;
    pointer_t params_addr;
};

    
/**
 * @brief Mailbox state object
 *
 */
struct ndp10x_mb_state_s {
    uint32_t m2h_state;
    uint32_t m2h_req;
    uint32_t m2h_rsp_success;
    uint32_t m2h_rsp_unknown;

    uint32_t h2m_state;
    uint32_t h2m_req_nop;
    uint32_t h2m_req_nop_serial;
    uint32_t h2m_req_nop_smap;
    uint32_t h2m_req_extop;
    uint32_t h2m_req_data;
    uint32_t h2m_req_cont;

    uint32_t h2m_unexpected_nop;
    uint32_t h2m_unexpected_extop;
    uint32_t h2m_unexpected_cont;
    uint32_t h2m_unexpected_data;
    uint32_t h2m_req_unknown;
    uint32_t h2m_extop_unknown;

    uint32_t h2m_data;
    uint32_t h2m_data_count;

    uint32_t previous_mbox;
};

#define NDP10X_NOISE_NOTIFICATION_DISABLE (0xFFFFFFFF)
#define NDP10X_NOISE_HI_THRESHOLD_MARK (0x1)
#define NDP10X_NOISE_LO_THRESHOLD_MARK (0x2)

enum ndp10x_agc_constants_e {
    NDP10X_AGC_MAX_FILTER_BANK = 38,
    NDP10X_AGC_MIN_FILTER_BANK = 1,
    NDP10X_AGC_FILTER_BANK_Q = 2 /* Filterbank output is Q2 */
};

struct ndp10x_agc_state_s {
    uint32_t frame_counter;
    int32_t reference_gain;                    /* Q0*/
    int32_t reference_noise_threshold;         /* Q23*/
    int32_t adjustment_gain_hysteresis;        /* Q23*/

    int32_t min_threshold;     /* Q23*/
    int32_t min_increase;      /* Q23*/
    int32_t when_to_reset_min; /* Q0*/

    int32_t shwd_reset_period; /* Q0 Reset the shadow tracker every so often*/
    int32_t shdw_tracker_confdnc;  /* Q0 Number of updates before shadow*/
                                   /* tracker is considered*/
    int32_t thr_reset_min_to_shdw; /* Q23 Reset only if the shadow tracker is
                                    at least thrResetMintoShdw above the
                                    min tracker*/

    int32_t timeout_after_adj_gain; /* Q0*/

    int8_t filterbanks[40];     /* Q0*/
    int32_t adjustment_gain;    /* Q0*/
    int32_t timeout_adj_gain;   /* Q0*/
    int32_t sum_logbank_energy; /* Q23*/

    int32_t min_tracker;            /* Q23*/
    int32_t consecutive_min_update; /* Q0*/

    int32_t min_tracker_shdw; /* Q23 Shadow min tracker that gets reset every
                               shdwReset and used to reset the minTracker*/
    int32_t
        shdw_reset_timer; /* Q0 Timer for periodic reset of the shadow timer*/
    int32_t shdw_tracker_upds; /* Q0 Counter of updates to the shadow tracker*/

    int32_t smoothed_min_tracker; /* Q23*/

    /*******************************************/
    /* Constants and state memory for Gen2 AGC */
    /*******************************************/

    /* Constant */
    /* Gen2 adaptive noise smoothing constant */
    int32_t alpha_noise_shift_base;     /* alpha_noise in the form of
                                          (1-1/2^alpha_noise_shift), e.g.
                                          (1-2^(-7)) */
    int32_t alpha_noise_tracking_shift; /* alpha_noise_tracking in the form of
                                          (1-1/2^alpha_noise_tracking_shift),
                                          e.g. (1-2^(-7))
                                          */

    /* Gen2 adaptive noise smoothing state memory */
    int32_t noise_level;          /* Smoothed noise level */
    int32_t noise_level_tracking; /* Smoothed tracking between estimated and */
                                  /* instant speech level */

    /*  Gen2 speech level estimation constants */
    int32_t max_thrs;
    int32_t max_decrease; /* Drift updownwards by 15dB per second in absense */
                          /* of max update */
    int32_t when_to_reset_max;  /* Reset max tracker after 2 seconds of no */
                                /* down-update */
    int32_t thr_to_clear_noise; /* Level above noise floor (10 dB) to allow */
                                /* max tracker to drift lower */
    int32_t alpha_speech_shift_base; /* AlphaSpeech in the form of */
                                     /* (1-1/2^AlphaSpeechShift), e.g. */
                                     /* (1-2^(-7)) */
    int32_t
        alpha_speech_tracking_shift; /* AlphaSpeechTracking in the form of */
                                     /* (1-1/2^alpha_speech_tracking_shift), */
                                     /* e.g. (1-2^(-7))    */

    /* Gen2 speech level estimation state memory */
    int32_t max_tracker;          /* Initialize max tracker to minimum  */
    int32_t max_tracker_shdw;     /* Initialize shadow max tracker to minimum */
    int32_t smoothed_max_tracker; /* Smoothed max tracker - initialized on */
                                  /* the "safe" side of nominal */
    int32_t shdw_max_tracker_upds; /* Counter of updates to the shadow max */
                                   /* tracker */
    int32_t consec_no_max_upd;     /* Number of consecutive frames without */
                                   /* up-update of the maximum tracker */
    int32_t speech_level;          /* Smoothed speech level */
    int32_t speech_level_tracking; /* Smoothed tracking between estimated and */
                                   /* instant speech level */
    int32_t noise_level_data;      /* Noise level at the time of breach */
};
    
struct ndp10x_result_softmax_smoother_s {
    uint32_t queue_curr_ptr[NDP10X_RESULT_NUM_CLASSES];
    uint32_t queues[NDP10X_RESULT_NUM_CLASSES]
                   [NDP10X_RESULT_SOFTMAX_SMOOTHER_MAX_QUEUE_SIZE];
};

/**
 * @brief Data Structure for storing firmware state.
 */
struct ndp10x_fw_state_s {
    struct ndp10x_host_interface_s host_intf;
    
    uint32_t debug;                   /* debug scratch area */
    uint32_t prev_enable;
    uint32_t result_fifo_full;
    
    /* interrupt counters*/
    uint32_t mb_int_count;
    uint32_t freq_int_count;
    uint32_t dnn_int_count;
    uint32_t unknown_int_count;
    uint32_t sensor_int_count[NDP10X_FW_SENSOR_MAX];
    uint32_t da217_int_count[NDP10X_FW_DA217_EVENT_TYPES];
    uint32_t accel_event_count[NDP10X_FW_ACCEL_EVENT_TYPES];
    uint32_t serial_not_configured;

    /* common accel data */
    int32_t  accel_running_sum[3];
    uint16_t initial_num_samples;
    uint16_t pickup_not_detected_count;
    uint16_t pickup_detected_count;

    /* da217 posterior handler related */
    uint16_t sensor_avg_sample_buf_index;
    int16_t  sensor_avg_sample_buf[NDP10X_MAIN_BUF_ENTRIES
                                  * NDP10X_ACCEL_NUM_AXES];
    /* flip flags to correct for accel raw data orientation */
    uint8_t swap_xy_axes;          /* 0: no change, 1: swap */
    uint8_t invert_x_axis;         /* 0: no change, 1: inverted */
    uint8_t invert_y_axis;         /* 0: no change, 1: inverted */
    uint8_t invert_z_axis;         /* 0: no change, 1: inverted */

    /* below are PH's window and backoff for all gestures */
    uint16_t phwin_pickup_count;
    uint16_t phbackoff_pickup_count;
    uint16_t phwin_flip_count;
    uint16_t phbackoff_flip_count;
    uint16_t phwin_freefall_count;
    uint16_t phbackoff_freefall_count;
    uint16_t phwin_elderly_fall_count;
    uint16_t phbackoff_elderly_fall_count;
    uint16_t phwin_shake_gesture_count;
    uint16_t phbackoff_shake_gesture_count;
    uint16_t phwin_tilt_up_count;
    uint16_t phbackoff_tilt_up_count;
    uint16_t phwin_tilt_down_count;
    uint16_t phbackoff_tilt_down_count;
    uint16_t phwin_tilt_left_count;
    uint16_t phbackoff_tilt_left_count;
    uint16_t phwin_tilt_right_count;
    uint16_t phbackoff_tilt_right_count;
    uint16_t phwin_tilt_count;
    uint16_t phbackoff_tilt_count;
    uint16_t phwin_significant_motion_count;
    uint16_t phbackoff_significant_motion_count;
    uint16_t phwin_double_tap_count;
    uint16_t phbackoff_double_tap_count;

    /* da217 step counters stored by mcu */
    uint64_t da217_mcu_step_cnt;
    uint32_t da217_step_prev_cnt;

    /* accel step counter stored by mcu */
    uint64_t accel_mcu_step_cnt;

    /* bit flag for all accel gestures detect */
    uint32_t match_detected_pre_filter;  /* pre posterior filter */
    uint32_t match_detected;             /* final, after posterior filter */

    /* step count variables */
    int16_t max_val;
    int16_t min_val;
    uint16_t sample_counter;
    int32_t dynamic_threshold;
    int32_t peak_to_peak;
    int16_t dynamic_max;
    int16_t dynamic_min;
    uint16_t dynamic_precision;
    int16_t prev_dynamic_threshold;
    uint16_t prev_sample_count;
    uint16_t pre_prev_sample_count;
    int16_t new_sample;
    int16_t old_sample;
    uint16_t pre_step_cnt;
    uint8_t false_detection;
    uint8_t change_flag;
    uint8_t interval;
    uint8_t true_interval;

    /* firmware version */
    char version[FW_VER_SIZE];
   
    /* serial interface internal state */
    uint32_t serial_active;

    /* GPIO configuration internal state */
    uint32_t gpio_role_saved[NDP10X_FW_GPIO_MAX / 4];
    uint32_t idata;
    uint32_t iclk;
    uint32_t mmiso;
    uint32_t mmosi;
    uint32_t msclk;
    uint32_t i2sclk;

    uint32_t i2sclk_init;
    uint32_t manual_dnn_running;

    /* neural package loader internal state */
    uint32_t nn_state;
    uint32_t nn_offset;
    uint32_t nn_left;
    
    union ndp10x_fw_sensor_state_u sensor_state[NDP10X_FW_SENSOR_MAX];
    
    struct ndp10x_mb_state_s mb_state;
    
    struct ndp10x_agc_state_s agc_state;

    struct ndp10x_ph_state_s ph_state;
    
    struct ndp10x_result_softmax_smoother_s smoother;
};

extern struct ndp10x_fw_state_s fw_state;

#ifdef __cplusplus
}
#endif
#endif /* NDP10X_FIRMWARE_H */

