/*
SYNTIANT CONFIDENTIAL
_____________________

Copyright (c) 2017-2020 Syntiant Corporation
All Rights Reserved.

NOTICE:  All information contained herein is, and remains the property of
Syntiant Corporation and its suppliers, if any.  The intellectual and
technical concepts contained herein are proprietary to Syntiant Corporation
and its suppliers and may be covered by U.S. and Foreign Patents, patents in
process, and are protected by trade secret or copyright law.  Dissemination of
this information or reproduction of this material is strictly forbidden unless
prior written permission is obtained from Syntiant Corporation.
*/

#include <syntiant_ilib/syntiant_portability.h>

#include <syntiant_packager/syntiant_package.h>

#define CRC_POLYNOMIAL 0xEDB88320U

uint32_t
crc32_no_lib(uint8_t *bytes, size_t len)
{
    size_t i, j;
    uint32_t byte, crc, mask;

    i = 0;
    crc = 0xFFFFFFFF;
    for (i = 0; i < len; i++) {
        byte = bytes[i];    /* Get next byte */
        crc = crc ^ byte;
        for(j = 0; j < 8; ++j) {
            mask = (unsigned int) -(((int) crc) & 1);
            crc = (crc >> 1) ^ (CRC_POLYNOMIAL & mask);
        }
    }
    return ~crc;
}

uint32_t
crc32_no_lib_init(void)
{
    return 0xFFFFFFFF;
}

uint32_t
crc32_no_lib_update(uint32_t crc, uint8_t *bytes, size_t len)
{
    size_t i, j;
    unsigned int byte, mask;

    i = 0;
    for (i = 0; i < len; i++) {
        byte = bytes[i];    /* Get next byte */
        crc = crc ^ byte;
        for(j = 0; j < 8; ++j) {
            mask = (unsigned int) -(((int) crc) & 1);
            crc = (crc >> 1) ^ (CRC_POLYNOMIAL & mask);
        }
    }
    return crc;
}

uint32_t
crc32_no_lib_finalize(uint32_t crc)
{
    return ~crc;
}
