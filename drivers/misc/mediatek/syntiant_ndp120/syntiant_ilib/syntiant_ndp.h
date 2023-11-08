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
#ifndef SYNTIANT_NDP_H
#define SYNTIANT_NDP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file syntiant_ndp.h
 * @date 2018-07-20
 * @brief Interface to Syntiant NDP chip interface library
 */


/*
 * Integration Interfaces
 */

/**
 * @brief handle/pointer to integration-specific device instance state
 *
 * An @c syntiant_ndp_handle_t typically points to a structure containing
 * all information required to access and manipulate an NDP device instance
 * in a particular environment.  For example, a Linux user mode integration
 * might require a Linux file descriptor to access the hardware, or a
 * 'flat, embedded' integration might require a hardware memory address.
 *
 * It is not used directly by the NDP ILib, but rather is passed unchanged to
 * integration interface functions.
 */
typedef void *syntiant_ndp_handle_t;

/**
 * @brief allocate ordinary memory
 *
 * The Syntiant ILib memory use is pseudo-static.  In other words, the use of
 * @c syntiant_ndp_malloc_f is restricted initialization time, and
 * only a modest amount of memory will ever be allocated, for control
 * structures.  All the memory allocated during initialization will be freed
 * during uninitialization in the reverse order in which it was allocated.
 *
 * The allocated memory is expected to be aligned to at least an @c int
 * boundary and @c size must be an @c int multiple.
 *
 * @param size bytes to allocate
 * @return the allocated memory or @c NULL
 */
typedef void *(*syntiant_ndp_malloc_f)(int size);

/**
 * @brief free ordinary memory
 *
 * Free memory allocated by @c syntiant_ndp_malloc_f.  This will only
 * happen during uninitlization and is in the reverse order of allocation.
 *
 * @param p memory to free
 */
typedef void (*syntiant_ndp_free_f)(void *p);

/**
 * @brief await NDP mailbox interrupt integration interface
 *
 * A provider of @c syntiant_ndp_mbwait must not return until the current
 * mailbox request is completed as indicated by @c syntiant_ndp_poll, or
 * an implementation-dependent timeout is reached.
 * The provider may yield control until an NDP interrupt is delivered, or
 * may poll by calling @c syntiant_ndp_poll periodically.
 *
 * If the provider of @c syntiant_ndp_mbwait yields control, it must
 * also perform the equivalent of a @c syntiant_ndp_unsync_f before
 * yielding and a @c syntiant_ndp_sync_f before returning.
 *
 * The client code may only call these functions during an mbwait:
 *   - @c syntiant_ndp_poll
 *   - @c syntiant_ndp_read
 *   - @c syntiant_ndp_write
 *   - @c syntiant_ndp_send_data
 *
 * @param d handle to the NDP device
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
typedef int (*syntiant_ndp_mbwait_f)(syntiant_ndp_handle_t d);

/**
 * @brief begin a critical section of NDP device access integration interface
 *
 * A provider of @c syntiant_ndp_sync should prevent any other device
 * to be selected, in the sense lf @c syntiant_ndp_select, and should only
 * allow a single thread of execution between a call of @c syntiant_ndp_sync
 * and @c syntiant_ndp_unsync.
 * In a single core execution enviroment with interrupts and a shared SPI
 * interface, this typically consists of blocking interrupts for
 * all devices sharing the SPI interface, including the NDP.
 * In a polled environment with the NDP on a point-to-point SPI
 * interface it would be a NOP.
 *
 * @param d handle to the NDP device
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
typedef int (*syntiant_ndp_sync_f)(syntiant_ndp_handle_t d);

/**
 * @brief end a critical section of NDP device access integration interface
 *
 * A provider of @c syntiant_ndp_unsync should lift the restrictions
 * applied by @c syntiant_ndp_sync.
 * In a single core execution enviroment with interrupts, and NDP on a
 * shared SPI bus, this typically consists of enabling interrupts for
 * all devices sharing the SPI interface, including the NDP.
 * In a polled environment it would be a NOP.
 *
 * @param d handle to the NDP device
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
typedef int (*syntiant_ndp_unsync_f)(syntiant_ndp_handle_t d);

/**
 * @brief retrieve the device type identifier
 *
 * A provider of @c syntiant_ndp_get_device_type will return the 32-bit
 * device type identifier.
 *
 * This interface will only be called before the device is initialized
 * so it can assume it is safe to manipulate the device in any way
 * required without regard for synchronization with other device activities.
 *
 * @param d handle to the NDP device
 * @param type the device type identifier
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
typedef int (*syntiant_ndp_get_device_type_f)(
    syntiant_ndp_handle_t d, unsigned int *type);

/**
 * @brief exchange bytes with NDP device integration interface
 *
 * A provider of @c syntiant_ndp_transfer will send @p count bytes
 * to the device and retrieve @p count bytes.
 *
 * This interface supports an underlying underlying SPI bus connection
 * to NDP where equal numbers of bytes are sent and received for cases
 * where full duplex transfer is relevant.  Simplex transfer is accomplished
 * by setting either @p out or @p in to NULL as appropriate.
 *
 * The interface provider may (should) support burst style efficiency
 * optimizations even across transfers where appropriate.  For example,
 * a multidrop SPI bus can continue to leave the interface selected
 * if subsequent transfers pick up where the previous left off and the
 * NDP device has not been deselected for other reasons.
 *
 * Transfers to the MCU space are generally expected to be simplex and
 * out == NULL -> read operation, in == NULL -> write operation.
 *
 * @param d handle to the NDP device
 * @param mcu for MCU address space (attachment bus space otherwise)
 * @param addr starting address
 * @param out bytes to send, or NULL if '0' bytes should be sent
 * @param in bytes to receive, or NULL if received bytes should be ignored
 * @param count number of bytes to exchange
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
typedef int (*syntiant_ndp_transfer_f)(syntiant_ndp_handle_t d, int mcu,
    uint32_t addr, void *out, void *in, unsigned int count);

/**
 * @brief integration interfaces
 */
struct syntiant_ndp_integration_interfaces_s {
    syntiant_ndp_handle_t d;                 /**< device handle */
    syntiant_ndp_malloc_f malloc;            /**< memory allocation function */
    syntiant_ndp_free_f free;                /**< memory free function */
    syntiant_ndp_mbwait_f mbwait;            /**< mailbox wait function */
    syntiant_ndp_get_device_type_f get_type; /**< get device type function */
    syntiant_ndp_sync_f sync;                /**< interrupt sync function */
    syntiant_ndp_unsync_f unsync;            /**< interrupt unsync function */
    syntiant_ndp_transfer_f transfer;        /**< data transfer function */
};

struct syntiant_ndp_device_s;
struct syntiant_ndp_driver_s;


/*
 * Control and Data Interfaces
 */

/**
 * @brief Get the NDP ILib version string
 *
 * @return a version string
 */
extern char *syntiant_ndp_ilib_version(void);

/**
 * @brief ndp10x initialization modes
 */
enum syntiant_ndp_init_mode_e {
    /**< reset the chip in init/uninit */
    SYNTIANT_NDP_INIT_MODE_RESET = 0, 

    /**< do not reset the chip but try to restore state */
    SYNTIANT_NDP_INIT_MODE_RESTART = 1,

    /** do not touch the chip at all */
    SYNTIANT_NDP_INIT_MODE_NO_TOUCH = 2,

    /** reset the chip and put it in secured mode (for development) */
    SYNTIANT_NDP_INIT_MODE_RESET_SECURED = 3
};

/**
 * @brief initialize NDP instance and associated state
 *
 * Reset the NDP device and initialize the device state object.
 *
 * If *ndp == NULL, a new NDP device state object will be allocated with
 * @c syntiant_ndp_malloc_f and the pointer returned in @c ndpp.  Otherwise
 * the NDP device state structure in *ndp will be used.
 *
 * The contents of @c iif are copied so need not be preserved following the
 * call.
 *
 * @param ndpp pointer to NDP state object pointer
 * @param iif integration interfaces for the NDP device
 * @param init_mode initialization mode to use
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_init(struct syntiant_ndp_device_s **ndpp,
    struct syntiant_ndp_integration_interfaces_s *iif,
    enum syntiant_ndp_init_mode_e init_mode);

/**
 * @brief uninitialize NDP instance and associated state
 *
 * Perform any clean-up actions required to cease using an NDP object.
 *
 * If @c free is set, the NDP device state object will be freed with
 * @c syntiant_ndp_free_f.  Otherwise, the NDP device state object
 * will not be freed.
 *
 * A device reset can be performed with a sequence of syntiant_ndp_uninit()
 * followed by syntiant_ndp_init().
 *
 * @param ndp NDP state object
 * @param free free the NDP state object
 * @param init_mode initialization mode to use
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_uninit(struct syntiant_ndp_device_s *ndp, int free,
    enum syntiant_ndp_init_mode_e init_mode);

/**
 * @brief report an NDP fundamental operation size.
 *
 * Report the fundamental size of an operation to an NDP for a bus or
 * MCU operation, for example 4 for MCU, 1 for SPI attachment bus.
 *
 * @param ndp NDP state object
 * @param mcu for MCU address space (attachment bus space otherwise)
 * @param size fundamental operation size
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_op_size(
    struct syntiant_ndp_device_s *ndp, int mcu, unsigned int *size);

/**
 * @brief read bytes from an NDP address
 *
 * Read data from an NDP device register or MCU address space.  This
 * function is primarily used for diagnostic purposes.  It performs
 * a call to the syntiant_ndp_transfer_f integration interface.
 *
 * @param ndp NDP state object
 * @param mcu read from MCU address space (attachment bus space otherwise)
 * @param address the address to read
 * @param value the value read
 * @param count the number of bytes to read
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_read_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count);

/**
 * @brief write bytes to a NDP address
 *
 * Write data from an NDP device register or MCU address space.  This
 * function is primarily used for diagnostic purposes.  It performs
 * a call to the syntiant_ndp_transfer_f integration interface.
 *
 * @param ndp NDP state object
 * @param mcu read from MCU address space (attachment bus space otherwise)
 * @param address the address to write
 * @param value pointer to the value to write
 * @param count the number of bytes to write
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_write_block(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, void *value, unsigned int count);

/**
 * @brief read word from an NDP address
 *
 * Read data from an NDP device register or MCU address space.  This
 * function is primarily used for diagnostic purposes.  It performs
 * a call to the syntiant_ndp_transfer_f integration interface.
 *
 * @param ndp NDP state object
 * @param mcu read from MCU address space (attachment bus space otherwise)
 * @param address the address to read
 * @param value the value read
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_read(
    struct syntiant_ndp_device_s *ndp, int mcu, uint32_t address, void *value);

/**
 * @brief writes word to a NDP address
 *
 * Write data from an NDP device register or MCU address space.  This
 * function is primarily used for diagnostic purposes.  It performs
 * a call to the syntiant_ndp_transfer_f integration interface.
 *
 * @param ndp NDP state object
 * @param mcu read from MCU address space (attachment bus space otherwise)
 * @param address the address to write
 * @param value value to write
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_write(struct syntiant_ndp_device_s *ndp, int mcu,
    uint32_t address, uint32_t value);

/**
 * @brief interrupt enable types
 */
enum syntiant_ndp_interrupt_e {
    SYNTIANT_NDP_INTERRUPT_MATCH = 0x01,      /**< external INT on match */
    SYNTIANT_NDP_INTERRUPT_MAILBOX_IN = 0x02, /**< mbox response */
    SYNTIANT_NDP_INTERRUPT_MAILBOX_OUT = 0x04,/**< mbox request */
    SYNTIANT_NDP_INTERRUPT_DNN_FRAME = 0x08,  /**< DNN frame */
    SYNTIANT_NDP_INTERRUPT_FEATURE = 0x10,    /**< filterbank completion */
    SYNTIANT_NDP_INTERRUPT_ADDRESS_ERROR = 0x20,/**< SPI address error */
    SYNTIANT_NDP_INTERRUPT_WATER_MARK = 0x40,
    /**< input buffer water mark */
    SYNTIANT_NDP_INTERRUPT_SPI_READ_FAILURE = 0x80,
    /**< spi read failure */
    SYNTIANT_NDP_INTERRUPT_ALL = 0xFF,
    SYNTIANT_NDP_INTERRUPT_DEFAULT = 0x100
    /**< enable the chip-specific default interrupts */
};


/**
 * @brief control NDP interrupt enables
 *
 * Call @c syntiant_ndp_interrupts to set and retrieve the NDP interrupt
 * enable.  @c on contains the new state when called, and returns the
 * prior state.  If @c on is supplied as < -1, the interrupt state is
 * not changed, and the current state is returned.
 *
 * This function will enable all interrupt causes expected for normal operation.
 * If a different set of interrupt enables is desired, the chip user
 * should manipulate the NDP chip interrupt control bits directly using
 * syntiant_ndp_{read,write} operations.  The ilib will never change the
 * interrupt mask state except in response to a call to
 * @c syntiant_ndp_interrupts.
 *
 * @param ndp NDP state object
 * @param on new state (< 0 -> do not change state), returns prior state
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_interrupts(struct syntiant_ndp_device_s *ndp, int *on);

/**
 * @brief notification causes
 */
enum syntiant_ndp_notification_e {
    SYNTIANT_NDP_NOTIFICATION_ERROR = 0x01,         /**< unexpected error */
    SYNTIANT_NDP_NOTIFICATION_MATCH = 0x02,         /**< algorithm match reported */
    SYNTIANT_NDP_NOTIFICATION_DNN = 0x04,           /**< Interrupt for DNN reported */
    SYNTIANT_NDP_NOTIFICATION_MAILBOX_IN = 0x08,    /**< host mailbox request complete */
    SYNTIANT_NDP_NOTIFICATION_MAILBOX_OUT = 0x10,   /**< ndp mailbox request start */
    SYNTIANT_NDP_NOTIFICATION_WATER_MARK = 0x20,    /**< input buffer water mark reached  */
    SYNTIANT_NDP_NOTIFICATION_FEATURE = 0x40,       /**< fequency domain processing completion event */
    SYNTIANT_NDP_NOTIFICATION_EXTRACT_READY = 0X80, /**< Data ready to be extracted from DSP */
    SYNTIANT_NDP_NOTIFICATION_ALL_M
    = ((SYNTIANT_NDP_NOTIFICATION_EXTRACT_READY  << 1) - 1)
};

/**
 * @brief poll and check NDP interrupts
 *
 * Call @c syntiant_ndp_poll either periodically or when an interrupt
 * has been reported.  @c syntiant_ndp_poll will progress any outstanding action
 * or communication with the NDP device as well as decoding and reporting
 * notification information.  Set @c clear to clear the notifications.
 *
 * @param ndp NDP state object
 * @param causes ored @c SYNTIANT_NDP_NOTIFICATION_*s
 * @param clear clear notifications if true
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_poll(
    struct syntiant_ndp_device_s *ndp, uint32_t *causes, int clear);

/**
 * @brief load or update NDP microcontroller firmware or network parameters. 
 *
 * The package will be parsed for microcontroller firmware and/or network
 * parameters which will be applied to the NDP device.  If both microcontroller
 * firmware and neural network parameters are present in a single package,
 * the microcontroller firmware will be updated first, followed by the
 * neural network parameters.
 * This function has the ability to load the packages in small chunks for the 
 * environments with limited memory. To start loading a package, this function 
 * should be called by passing len equal to zero, and as long as this function 
 * returns the SYNTIANT_NDP_ERROR_MORE code, more chunks should be loaded. This
 * function returns SYNTIANT_NDP_ERROR_NONE once the package is completely
 * and correctly loaded, otherwise, it will return an error code rather than 
 * SYNTIANT_NDP_ERROR_MORE.
 *
 * @param ndp NDP state object
 * @param package NDP binary package
 * @param len the whole package length or the length of a chunk
 * @return a @c SYNTIANT_NDP_ERROR_* code. 
 */
extern int syntiant_ndp_load(
    struct syntiant_ndp_device_s *ndp, void *package, int len);

/**
 * @brief NDP configuration data
 */
struct syntiant_ndp_config_s {
    unsigned int device_id;   /**< device type identifier */
    char *device_type;        /**< device type identifier string */
    unsigned int classes;     /**< number of active classes */
    char *firmware_version;   /**< firmware version string */
    unsigned int firmware_version_len;
                              /**< length of supplied firmware version string */
    char *parameters_version; /**< parameters version string */
    unsigned int parameters_version_len;
                              /**< length of supplied parameters version
                                 string */
    char *labels;               /**< class labels strings array.  Each
                                   label is a C (NUL-terminated) string followed
                                   immediately by the next label.  The label
                                   strings will be 0 length if label strings
                                   are not available */
    unsigned int labels_len;  /**< length of supplied labels string array */
    char *pkg_version;        /**< package version string */
    unsigned int pkg_version_len;
                              /**< length of supplied package version string */
    char *dsp_firmware_version;
    unsigned int dsp_firmware_version_len;
};

/**
 * @brief retrieve NDP configuration data
 *
 * The supplied length values in @c config are the space available to copy
 * the strings.  The returned length values in @c config are the actual
 * lengths of the data.  As such, the returned lengths may exceed the
 * supplied lengths.  The caller is responsible for addressing this situation
 * (the copied data is less than the entire data).
 *
 * A call with all lengths of 0 can be used to retrieve the lengths of
 * each to prepare properly sized buffers for a subsequent call to
 * receive the actual data.
 *
 * @param ndp NDP state object
 * @param config configuration data object
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_get_config(
    struct syntiant_ndp_device_s *ndp, struct syntiant_ndp_config_s *config);


enum syntiant_ndp_send_data_type_e {
    SYNTIANT_NDP_SEND_DATA_TYPE_STREAMING       = 0x00,
    /**< streaming vector input features */
    SYNTIANT_NDP_SEND_DATA_TYPE_FEATURE_STATIC  = 0x01,
    /**< send to static features */
    SYNTIANT_NDP_NDP_SEND_DATA_TYPE_LAST
    = SYNTIANT_NDP_SEND_DATA_TYPE_FEATURE_STATIC
};

/**
 * @brief send input data to the neural network
 *
 * The format of the data depends on the configuration of the NDP device
 * datapath which is controlled by the contents of the neural network parameters
 * package.  For example, the data might be a stream of bytes, or a stream
 * of 12-bit quantities stored zero-padded in pairs of bytes.
 *
 * Whether an NDP device is configured to process data input from the
 * processor or other inputs (e.g. a hardware microphone input) is
 * controlled by a chip-specific configuration API
 * (e.g. syntiant_ndp10x_config)
 *
 * This function may or may not return an error code when unexpected data
 * is provided.
 *
 * @param ndp NDP state object
 * @param data array of data values
 * @param len @p data length
 * @param type send to memory type, e.g. streaming or static
 * @param address offset of the static section
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_send_data(
    struct syntiant_ndp_device_s *ndp, uint8_t *data, unsigned int len, int type,
    uint32_t address);

/**
 * @brief data extraction types
 */
enum syntiant_ndp_extract_type_e {
    SYNTIANT_NDP_EXTRACT_TYPE_INPUT = 0x00,   /**< input data */
    SYNTIANT_NDP_EXTRACT_TYPE_FEATURES = 0x01,/**< neural net input features */
    SYNTIANT_NDP_EXTRACT_TYPE_FEATURE_STATIC = 0x03,/**< dnn static features */
    SYNTIANT_NDP_EXTRACT_TYPE_INPUT_ANNOTATED = 0x04,
      /**< 'raw' input data  + 2 extra bytes about input_src and input_param*/
    SYNTIANT_NDP_EXTRACT_TYPE_RAW_ACCEL = 0x05,
      /**< raw sensor accelerometer data */
    SYNTIANT_NDP_EXTRACT_TYPE_LAST = SYNTIANT_NDP_EXTRACT_TYPE_RAW_ACCEL
};

/**
 * @brief data extraction ranges
 */
enum syntiant_ndp_extract_from_e {
    SYNTIANT_NDP_EXTRACT_FROM_MATCH = 0x00,
    /**< from 'length' older than a match.  The matching extraction position
       is only updated when @c syntiant_ndp_get_match_summary() is called
       and reports a match has been detected */
    SYNTIANT_NDP_EXTRACT_FROM_UNREAD = 0x01,
    /**< from the oldest unread */
    SYNTIANT_NDP_EXTRACT_FROM_OLDEST = 0x02,
    /**< from the oldest recorded */
    SYNTIANT_NDP_EXTRACT_FROM_NEWEST = 0x03,
    /**< from the newest recorded */
    SYNTIANT_NDP_EXTRACT_FROM_LAST = SYNTIANT_NDP_EXTRACT_FROM_NEWEST 
};

/**
 * @brief extract stored data from the NDP device
 *
 * The format of the data depends on the type of data requested, the
 * and the configuration of the NDP device as controlled by the contents of
 * the neural network parameters package and other device configuration APIs.
 * For example, the data might 8-bit or 16-bit PCM data, or filter bank
 * frames with some particular number of bins, span and frame rate.
 *
 * @param ndp NDP state object
 * @param type the type of data to extract, e.g. input or extracted
 *        features
 * @param from point at which to start extracting
 * @param data array of data values.  NULL will 'discard' @len data 
 *        if @c from = @c _UNREAD and @c _OLDEST and set the next
 *        pointer start of the requested data if @c from = @c _MATCH
 * @param len @p supplies length of the data array and returns the amount
 *        of data that would have been extracted if len were unconstrained
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_extract_data(struct syntiant_ndp_device_s *ndp,
                                     int type, int from, uint8_t *data,
                                     unsigned int *len);

/**
 * @brief retrieve NDP match summary information
 *
 * The match summary information is no more than 32 bits in a device-dependent
 * format.
 *
 * This function also loads the tank pointer used by
 * @c syntiant_ndp_extract_data(SYNTIANT_NDP_EXTRACT_FROM_MATCH) in the event
 * that a match is sigalled.
 *
 * @param ndp NDP state object
 * @param summary match summary information
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_get_match_summary(struct syntiant_ndp_device_s *ndp,
                                          uint32_t *summary);

/**
* @brief retrieve NDP binary match indications
*
* The number of active classes is controlled by the contents of the neural
* network parameters package.
*
* @param ndp NDP state object
* @param matches array of 8 match bits per byte, little endian organization
* @param len @p matches length
* @return a @c SYNTIANT_NDP_ERROR_* code
*/
extern int syntiant_ndp_get_match_binary(
    struct syntiant_ndp_device_s *ndp, uint8_t *matches, unsigned int len);

/**
 * @brief strength types
 *
 */
enum syntiant_ndp_strength_type_e {
    SYNTIANT_NDP_STRENGTH_RAW = 0x0,     /* final layer activation */
    SYNTIANT_NDP_STRENGTH_SOFTMAX = 0x1 /* softmax based on final layer
                                            activation */
};

/**
 * @brief retrieve NDP match strengths
 *
 * The number of active classes is controlled by the contents of the neural
 * network parameters package.
 *
 * The format of the match strengths is controlled by the contents of the
 * neural network parameters package.  For example, a strength may be an
 * 8-bit value, or 12-bit value zero-padded into a pair of bytes.
 *
 * The strength to be extracted is choosen by providing type.
 *
 * @param ndp NDP state object
 * @param strengths array of match strengths
 * @param len @p strengths length
 * @param type type of strength to get e.g. final layer activation or softmax
 * @return a @c SYNTIANT_NDP_ERROR_* code
 */
extern int syntiant_ndp_get_match_strength(
    struct syntiant_ndp_device_s *ndp, uint8_t *strengths, unsigned int len,
    int type);

/**
 * @Syntiant device family
 */
typedef enum syntiant_ndp_device_type_e {
    SYNTIANT_NDP_INV_CORE = -1,
    SYNTIANT_NDP_CORE_1 = 1,
    SYNTIANT_NDP_CORE_2 = 2
} syntiant_ndp_device_type_t;

extern syntiant_ndp_device_type_t syntiant_ndp_get_device_family(
        unsigned int device_id);

#ifdef __cplusplus
}
#endif

#endif
