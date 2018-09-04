/*
 * ..
 */

#include <assert.h>

#include "common/intops.h"

#include "src/getbits.h"

void init_get_bits(GetBits *const c,
                   const uint8_t *const data, const size_t sz)
{
    c->ptr = c->ptr_start = data;
    c->ptr_end = &c->ptr_start[sz];
    c->bits_left = 0;
    c->state = 0;
    c->error = 0;
    c->eof = 0;
}

static void refill(GetBits *const c, const unsigned n) {
    assert(c->bits_left <= 56);
    uint64_t state = 0;
    do {
        state <<= 8;
        c->bits_left += 8;
        if (!c->eof)
            state |= *c->ptr++;
        if (c->ptr >= c->ptr_end) {
            c->error = c->eof;
            c->eof = 1;
        }
    } while (n > c->bits_left);
    c->state |= state << (64 - c->bits_left);
}

unsigned get_bits(GetBits *const c, const unsigned n) {
    assert(n <= 32 /* can go up to 57 if we change return type */);

    if (n > c->bits_left) refill(c, n);

    const uint64_t state = c->state;
    c->bits_left -= n;
    c->state <<= n;

    return state >> (64 - n);
}

int get_sbits(GetBits *const c, const unsigned n) {
    const int shift = 31 - n;
    const int res = get_bits(c, n + 1) << shift;
    return res >> shift;
}

unsigned get_uniform(GetBits *const c, const unsigned n) {
    assert(n > 0);
    const int l = ulog2(n) + 1;
    assert(l > 0);
    const int m = (1 << l) - n;
    const int v = get_bits(c, l - 1);
    return v < m ? v : (v << 1) - m + get_bits(c, 1);
}

unsigned get_vlc(GetBits *const c) {
    int n_bits = 0;
    while (!get_bits(c, 1)) n_bits++;
    if (n_bits >= 32) return 0xFFFFFFFFU;
    return ((1 << n_bits) - 1) + get_bits(c, n_bits);
}

static unsigned get_bits_subexp_u(GetBits *const c, const unsigned ref,
                                  const unsigned n)
{
    unsigned v = 0;

    for (int i = 0;; i++) {
        const int b = i ? 3 + i - 1 : 3;

        if (n < v + 3 * (1 << b)) {
            v += get_uniform(c, n - v);
            break;
        }

        if (!get_bits(c, 1)) {
            v += get_bits(c, b);
            break;
        }

        v += 1 << b;
    }

    return ref * 2 <= n ? inv_recenter(ref, v) : n - inv_recenter(n - ref, v);
}

int get_bits_subexp(GetBits *const c, const int ref, const unsigned n) {
    return (int) get_bits_subexp_u(c, ref + (1 << n), 2 << n) - (1 << n);
}

const uint8_t *flush_get_bits(GetBits *c) {
    c->bits_left = 0;
    c->state = 0;
    return c->ptr;
}
