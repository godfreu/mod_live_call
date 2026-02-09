/*
 * Jitter Buffer Implementation for mod_live_call
 * Provides stable audio playback despite network jitter
 */

#include "jitter_buffer.h"

/* Create jitter buffer */
jitter_buffer_t *jitter_buffer_create(size_t min_size, size_t max_size, 
                                      switch_memory_pool_t *pool)
{
    jitter_buffer_t *jb;

    jb = switch_core_alloc(pool, sizeof(*jb));
    if (!jb) {
        return NULL;
    }

    memset(jb, 0, sizeof(*jb));
    jb->min_size = min_size;
    jb->max_size = max_size;
    jb->target_size = (min_size + max_size) / 2;
    jb->pool = pool;

    /* Create mutex */
    if (switch_mutex_init(&jb->mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
        return NULL;
    }

    /* Create buffer */
    if (switch_buffer_create_dynamic(&jb->buffer, max_size, max_size * 2, 0) != SWITCH_STATUS_SUCCESS) {
        return NULL;
    }

    return jb;
}

/* Destroy jitter buffer */
void jitter_buffer_destroy(jitter_buffer_t *jb)
{
    if (!jb) return;

    if (jb->buffer) {
        switch_buffer_destroy(&jb->buffer);
        jb->buffer = NULL;
    }

    if (jb->mutex) {
        switch_mutex_destroy(jb->mutex);
        jb->mutex = NULL;
    }
}

/* Put data into jitter buffer */
switch_status_t jitter_buffer_put(jitter_buffer_t *jb, const void *data, size_t len)
{
    switch_status_t status = SWITCH_STATUS_FALSE;

    if (!jb || !data || len == 0) {
        return SWITCH_STATUS_FALSE;
    }

    switch_mutex_lock(jb->mutex);

    size_t current_size = switch_buffer_inuse(jb->buffer);
    
    /* Check for buffer overrun */
    if (current_size + len > jb->max_size) {
        jb->overrun_count++;
        /* Drop oldest data to make room */
        size_t to_drop = (current_size + len) - jb->max_size;
        uint8_t temp[4096];
        while (to_drop > 0) {
            size_t drop_chunk = (to_drop > sizeof(temp)) ? sizeof(temp) : to_drop;
            switch_buffer_read(jb->buffer, temp, drop_chunk);
            to_drop -= drop_chunk;
        }
    }

    /* Write data */
    size_t written = switch_buffer_write(jb->buffer, data, len);
    status = (written == len) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;

    switch_mutex_unlock(jb->mutex);

    return status;
}

/* Get data from jitter buffer */
switch_status_t jitter_buffer_get(jitter_buffer_t *jb, void *data, size_t len)
{
    switch_status_t status = SWITCH_STATUS_FALSE;

    if (!jb || !data || len == 0) {
        return SWITCH_STATUS_FALSE;
    }

    switch_mutex_lock(jb->mutex);

    size_t available = switch_buffer_inuse(jb->buffer);

    if (available >= len) {
        /* Sufficient data available */
        size_t read = switch_buffer_read(jb->buffer, data, len);
        status = (read == len) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
    } else if (available > 0 && available >= jb->min_size) {
        /* Some data available, but less than requested */
        size_t read = switch_buffer_read(jb->buffer, data, available);
        /* Pad with silence */
        memset((uint8_t*)data + read, 0, len - read);
        status = SWITCH_STATUS_SUCCESS;
    } else {
        /* Buffer underrun */
        jb->underrun_count++;
        memset(data, 0, len);
        status = SWITCH_STATUS_FALSE;
    }

    switch_mutex_unlock(jb->mutex);

    return status;
}

/* Check if jitter buffer is ready for playback */
int jitter_buffer_ready(jitter_buffer_t *jb)
{
    int ready = 0;

    if (!jb) return 0;

    switch_mutex_lock(jb->mutex);
    ready = (switch_buffer_inuse(jb->buffer) >= jb->min_size);
    switch_mutex_unlock(jb->mutex);

    return ready;
}

/* Get current buffer size */
size_t jitter_buffer_size(jitter_buffer_t *jb)
{
    size_t size = 0;

    if (!jb) return 0;

    switch_mutex_lock(jb->mutex);
    size = switch_buffer_inuse(jb->buffer);
    switch_mutex_unlock(jb->mutex);

    return size;
}

/* Reset jitter buffer */
void jitter_buffer_reset(jitter_buffer_t *jb)
{
    if (!jb) return;

    switch_mutex_lock(jb->mutex);
    switch_buffer_zero(jb->buffer);
    jb->underrun_count = 0;
    jb->overrun_count = 0;
    switch_mutex_unlock(jb->mutex);
}

/* Get jitter buffer statistics */
void jitter_buffer_get_stats(jitter_buffer_t *jb, int *underruns, int *overruns)
{
    if (!jb) return;

    switch_mutex_lock(jb->mutex);
    if (underruns) *underruns = jb->underrun_count;
    if (overruns) *overruns = jb->overrun_count;
    switch_mutex_unlock(jb->mutex);
}
