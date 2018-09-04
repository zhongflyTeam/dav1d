#ifndef __DAV1D_H__
#define __DAV1D_H__

#include "picture.h"
#include "data.h"

typedef struct Dav1dContext Dav1dContext;

typedef struct Dav1dSettings {
    int n_frame_threads;
    int n_tile_threads;
} Dav1dSettings;

/*
 * Init the library.
 */
void dav1d_init(void);

/**
 * Get library version.
 */
const char *dav1d_version(void);

/**
 * Initialize settings to default values.
 */
void dav1d_default_settings(Dav1dSettings *s);

/**
 * Open/allocate decoder instance.
 *
 * The resulting instance context will be placed in $c_out and can be used in
 * iterative calls to dav1d_decode().
 *
 * You should free the context using dav1d_close() when you're done decoding.
 *
 * This returns < 0 (a negative errno code) on error, or 0 on success.
 */
int dav1d_open(Dav1dContext **c_out, const Dav1dSettings *s);

/**
 * Decode one input frame. Library takes ownership of the passed-in reference.
 * After that, it will return < 0 (a negative errno code, but not -EAGAIN) on
 * failure, or 0 on success. If any decoded output frames are available, they
 * will be placed in $out. The caller assumes ownership of the returned output
 * picture.
 *
 * To flush the decoder (i.e. all input is finished), feed it NULL input data
 * until it returns -EAGAIN.
 */
int dav1d_decode(Dav1dContext *c, Dav1dData *in, Dav1dPicture *out);

/**
 * Close decoder instance, free all associated memory.
 */
void dav1d_close(Dav1dContext *c);

#endif /* __DAV1D_H__ */
