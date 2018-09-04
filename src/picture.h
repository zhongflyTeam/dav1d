/*
 * ..
 */

#ifndef __DAV1D_SRC_PICTURE_H__
#define __DAV1D_SRC_PICTURE_H__

#include <pthread.h>
#include <stdatomic.h>

#include "include/picture.h"

enum PlaneType {
    PLANE_TYPE_Y,
    PLANE_TYPE_UV,
    PLANE_TYPE_BLOCK,
    N_PLANE_TYPES,
};

struct thread_data {
    pthread_t thread;
    pthread_cond_t cond;
    pthread_mutex_t lock;
};

typedef struct Dav1dThreadPicture {
    Dav1dPicture p;
    struct thread_data *t;
    atomic_int *progress;
} Dav1dThreadPicture;

/*
 * Allocate a picture with custom border size.
 */
int dav1d_thread_picture_alloc(Dav1dThreadPicture *p, int w, int h,
                               enum Dav1dPixelLayout layout, int bpc,
                               struct thread_data *t);

/**
 * Create a copy of a picture.
 */
void dav1d_picture_ref(Dav1dPicture *dst, const Dav1dPicture *src);
void dav1d_thread_picture_ref(Dav1dThreadPicture *dst,
                              const Dav1dThreadPicture *src);
void dav1d_thread_picture_unref(Dav1dThreadPicture *p);

/**
 * Wait for picture to reach a certain stage.
 *
 * y is in full-pixel units. If pt is not UV, this is in luma
 * units, else it is in chroma units.
 * PLANE_TYPE defines how many pixels delay are introduced by
 * loopfilter processes.
 */
void dav1d_thread_picture_wait(const Dav1dThreadPicture *p, int y,
                               enum PlaneType plane_type);

#endif /* __DAV1D_SRC_PICTURE_H__ */
