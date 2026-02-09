#ifndef JITTER_BUFFER_H
#define JITTER_BUFFER_H

#include <switch.h>

/* Jitter buffer structure for audio stability */
typedef struct {
    switch_buffer_t *buffer;
    switch_mutex_t *mutex;
    size_t min_size;        /* Minimum buffering before playback */
    size_t max_size;        /* Maximum buffer size */
    size_t target_size;     /* Target buffer level */
    int underrun_count;     /* Track buffer underruns */
    int overrun_count;      /* Track buffer overruns */
    switch_memory_pool_t *pool;
} jitter_buffer_t;

/* Function prototypes */

/* Create/destroy jitter buffer */
jitter_buffer_t *jitter_buffer_create(size_t min_size, size_t max_size, 
                                      switch_memory_pool_t *pool);
void jitter_buffer_destroy(jitter_buffer_t *jb);

/* Data operations */
switch_status_t jitter_buffer_put(jitter_buffer_t *jb, const void *data, size_t len);
switch_status_t jitter_buffer_get(jitter_buffer_t *jb, void *data, size_t len);

/* Buffer status */
int jitter_buffer_ready(jitter_buffer_t *jb);
size_t jitter_buffer_size(jitter_buffer_t *jb);
void jitter_buffer_reset(jitter_buffer_t *jb);

/* Statistics */
void jitter_buffer_get_stats(jitter_buffer_t *jb, int *underruns, int *overruns);

#endif /* JITTER_BUFFER_H */
