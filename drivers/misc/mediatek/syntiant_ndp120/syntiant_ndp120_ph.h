
#ifndef SYNTIANT_NDP120_PH_H
#define SYNTIANT_NDP120_PH_H

#include <syntiant_ilib/syntiant_ndp_driver.h>

typedef struct syntiant_ndp120_posterior_config_s {
    unsigned int set;         /**< configuration variable set flags */
    unsigned int states;      /**< number of states */
    unsigned int classes;     /**< number of classes */
    unsigned int ph_type;     /**< posterior handler type used */
    unsigned int smoothing_queue_size;
    /**< number of frames to smooth probabilities over */
    unsigned int state;       /**< state number to configure */
    unsigned int timeout;     /**< state timeout in DNN frames */
    unsigned int timeout_action_type; /**< timeout action type (MATCH|STATE) */
    unsigned int timeout_action_arg0; /* first action argument */
    unsigned int timeout_action_arg1; /* second action argument */
    unsigned int class_index;       /**< class_index number to configure */
    unsigned int threshold;   /**< class active threshold level 0-65535 */
    unsigned int window;      /**< class active window in DNN frames 0-255 */
    unsigned int backoff;     /**< match backoff timer in DNN frames 0-255 */
    unsigned int action_type; /**< match action type */
    unsigned int action_arg0; /* first action argument */
    unsigned int action_arg1; /* second action argument */
    unsigned int ph_num; /**< number of activat phs */
    unsigned int ph_idx; /***< The index of posterior handler */
    int enable;
} syntiant_ndp120_posterior_config_t;

int syntiant_ndp120_posterior_config_no_sync(
    struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp120_posterior_config_s *config);

int syntiant_ndp120_posterior_config(
    struct syntiant_ndp_device_s *ndp,
    struct syntiant_ndp120_posterior_config_s *config);

#endif
