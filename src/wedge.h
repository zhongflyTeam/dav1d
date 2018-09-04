/*
 * ..
 */

#ifndef __DAV1D_SRC_WEDGE_H__
#define __DAV1D_SRC_WEDGE_H__

#include "src/levels.h"

void av1_init_wedge_masks(void);
extern const uint8_t *wedge_masks[N_BS_SIZES][3 /* 444/luma, 422, 420 */]
                                 [2 /* sign */][16 /* wedge_idx */];

void av1_init_interintra_masks(void);
extern const uint8_t *const ii_masks[N_BS_SIZES][3 /* 444/luma, 422, 420 */]
                                    [N_INTER_INTRA_PRED_MODES];

#endif /* __DAV1D_SRC_WEDGE_H__ */
