/*
 * ..
 */

#ifndef __DAV1D_SRC_THREAD_TASK_H__
#define __DAV1D_SRC_THREAD_TASK_H__

#include "src/internal.h"

int decode_frame(Dav1dFrameContext *f);
void *dav1d_frame_task(void *data);

int decode_tile_sbrow(Dav1dTileContext *t);
void *dav1d_tile_task(void *data);

#endif /* __DAV1D_SRC_THREAD_TASK_H__ */
