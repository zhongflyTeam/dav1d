/*
 * ..
 */

#ifndef __DAV1D_SRC_THREAD_DATA_H__
#define __DAV1D_SRC_THREAD_DATA_H__

struct thread_data {
    pthread_t thread;
    pthread_cond_t cond;
    pthread_mutex_t lock;
};

#endif /* __DAV1D_SRC_THREAD_DATA_H__ */
