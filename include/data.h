/*
 * ..
 */

#ifndef __DAV1D_DATA_H__
#define __DAV1D_DATA_H__

#include <stddef.h>
#include <stdint.h>

#include "ref.h"

#ifndef DAV1D_API
    #if defined _WIN32 || defined __CYGWIN__
      #define DAV1D_API __declspec(dllexport)
    #else
      #if __GNUC__ >= 4
        #define DAV1D_API __attribute__ ((visibility ("default")))
      #else
        #define DAV1D_API
      #endif
    #endif
#endif

typedef struct Dav1dData {
    uint8_t *data; ///< data pointer
    size_t sz; ///< data size
    Dav1dRef *ref; ///< allocation origin
} Dav1dData;

/**
 * Allocate data.
 */
DAV1D_API int dav1d_data_create(Dav1dData *data, size_t sz);

#endif /* __DAV1D_DATA_H__ */
