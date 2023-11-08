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
#ifndef ES_CONVERSION_H
#define ES_CONVERSION_H

#include <syntiant_ilib/syntiant_ndp10x.h>
/**
 * @brief converts the dnn input enum to string
 *
 * @param e configured dnn input
 */
extern const char *
syntiant_ndp10x_config_dnn_input_s(enum syntiant_ndp10x_config_dnn_input_e e);

/**
 * @brief converts the tank input enum to string
 *
 * @param e configured tank input
 */
extern const char *
syntiant_ndp10x_config_tank_input_s(enum syntiant_ndp10x_config_tank_input_e e);

/**
 * @brief converts the memory type name enum to string
 *
 * @param e configured memory
 */
extern const char *
syntiant_ndp10x_memory_s(enum syntiant_ndp10x_config_memory_e e);

/**
 * @brief convert the memory state name enum to string
 *
 * @param e configured memory state name
 */
extern const char *
syntiant_ndp10x_memory_power_s(enum syntiant_ndp10x_config_memory_power_e e);

#ifdef __cplusplus
}
#endif

#endif
