/*
 * ..
 */

#ifndef __DAV1D_COMMON_VALIDATE_H__
#define __DAV1D_COMMON_VALIDATE_H__

#include <stdio.h>
#include <stdlib.h>

#if defined(NDEBUG)
#define debug_abort()
#else
#define debug_abort abort
#endif

#define validate_input_or_ret_with_msg(x, r, msg...) \
    if (!(x)) { \
        fprintf(stderr, "Input validation check \'%s\' failed in %s!\n", \
                #x, __PRETTY_FUNCTION__); \
        fprintf(stderr, msg); \
        debug_abort(); \
        return r; \
    }

#define validate_input_or_ret(x, r) \
    if (!(x)) { \
        fprintf(stderr, "Input validation check \'%s\' failed in %s!\n", \
                #x, __PRETTY_FUNCTION__); \
        debug_abort(); \
        return r; \
    }

#define validate_input(x) validate_input_or_ret(x, )

#endif /* __DAV1D_COMMON_VALIDATE_H__ */
