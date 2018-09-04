/*
 * ..
 */

#ifndef __DAV1D_INPUT_INPUT_H__
#define __DAV1D_INPUT_INPUT_H__

#include "data.h"

typedef struct DemuxerContext DemuxerContext;

void init_demuxers(void);
int input_open(DemuxerContext **c, const char *filename,
               unsigned fps[2], unsigned *num_frames);
int input_read(DemuxerContext *ctx, Dav1dData *data);
void input_close(DemuxerContext *ctx);

#endif /* __DAV1D_INPUT_INPUT_H__ */
