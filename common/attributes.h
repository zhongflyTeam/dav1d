/*
 * ..
 */

#ifndef __DAV1D_COMMON_ATTRIBUTES_H__
#define __DAV1D_COMMON_ATTRIBUTES_H__

#include <stddef.h>

/*
 * API for variables, struct members (ALIGN()) like:
 * uint8_t var[1][2][3][4]
 * becomes:
 * ALIGN(uint8_t var[1][2][3][4], alignment).
 */
#define ALIGN(line, align) \
    line __attribute__((aligned(align)))

/*
 * API for stack alignment (ALIGN_STK_$align()) of variables like:
 * uint8_t var[1][2][3][4]
 * becomes:
 * ALIGN_STK_$align(uint8_t, var, 1, [2][3][4])
 */
#define ALIGN_STK_32(type, var, sz1d, sznd) \
    ALIGN(type var[sz1d]sznd, 32)
// as long as stack is itself 16-byte aligned, this works (win64, gcc)
#define ALIGN_STK_16(type, var, sz1d, sznd) \
    ALIGN(type var[sz1d]sznd, 16)

#endif /* __DAV1D_COMMON_ATTRIBUTES_H__ */
