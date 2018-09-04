/*
 * ..
 */

#ifndef __DAV1D_SRC_QM_H__
#define __DAV1D_SRC_QM_H__

#include "src/levels.h"

extern const uint8_t *av1_qm_tbl[16][2][N_RECT_TX_SIZES];

void av1_init_qm_tables(void);

#endif /* __DAV1D_SRC_QM_H__ */
