/*
 * ..
 */

#ifndef __DAV1D_SRC_GETBITS_H__
#define __DAV1D_SRC_GETBITS_H__

#include <stddef.h>
#include <stdint.h>

typedef struct GetBits {
    int error, eof;
    uint64_t state;
    unsigned bits_left;
    const uint8_t *ptr, *ptr_start, *ptr_end;
} GetBits;

void init_get_bits(GetBits *c, const uint8_t *data, size_t sz);
unsigned get_bits(GetBits *c, unsigned n);
int get_sbits(GetBits *c, unsigned n);
unsigned get_uniform(GetBits *c, unsigned range);
unsigned get_vlc(GetBits *c);
int get_bits_subexp(GetBits *c, int ref, unsigned n);
const uint8_t *flush_get_bits(GetBits *c);

#endif /* __DAV1D_SRC_GETBITS_H__ */
