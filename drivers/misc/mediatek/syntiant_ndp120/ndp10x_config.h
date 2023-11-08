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
#ifndef NDP10X_CONFIG_H
#define NDP10X_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define STRING_LEN 256
#define MAX_LABELS 64

/* spi driver specific definitions */
#define SPI_DEVICE_NAME "ndp10x_spi_driver"

#ifndef SPI_MAX_SPEED
#define SPI_MAX_SPEED 20000000
#endif
#ifndef MAX_SPEED
#define MAX_SPEED 1000000
#endif
#ifndef SPI_SPLIT
#define SPI_SPLIT 0
#endif
#ifndef SPI_READ_DELAY
#define SPI_READ_DELAY 1
#endif
#define BUS_NUM 0
#define CHIP_SELECT 0
#define SPI_MODE 0 /* only SPI_MODE 0 is supported in B0 */
#define MAX_BUFFER_SIZE	2048

#ifdef __cplusplus
}
#endif

#endif
