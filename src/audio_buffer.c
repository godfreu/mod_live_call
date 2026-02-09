/*
 * Audio Buffer Management for mod_live_call
 */

#include "audio_buffer.h"

/* Create audio buffer */
audio_buffer_t *audio_buffer_create(size_t size, switch_memory_pool_t *pool)
{
    audio_buffer_t *ab;

    ab = switch_core_alloc(pool, sizeof(*ab));
    if (!ab) {
        return NULL;
    }

    ab->max_size = size;
    ab->pool = pool;

    /* Create mutex */
    if (switch_mutex_init(&ab->mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
        return NULL;
    }

    /* Create buffer */
    if (switch_buffer_create_dynamic(&ab->buffer, size, size * 2, 0) != SWITCH_STATUS_SUCCESS) {
        return NULL;
    }

    return ab;
}

/* Destroy audio buffer */
void audio_buffer_destroy(audio_buffer_t *ab)
{
    if (!ab) return;

    if (ab->buffer) {
        switch_buffer_destroy(&ab->buffer);
        ab->buffer = NULL;
    }

    if (ab->mutex) {
        switch_mutex_destroy(ab->mutex);
        ab->mutex = NULL;
    }
}

/* Write to audio buffer */
switch_status_t audio_buffer_write(audio_buffer_t *ab, const void *data, size_t len)
{
    switch_status_t status = SWITCH_STATUS_FALSE;

    if (!ab || !data || len == 0) {
        return SWITCH_STATUS_FALSE;
    }

    switch_mutex_lock(ab->mutex);

    /* Check if buffer has space */
    if (switch_buffer_inuse(ab->buffer) + len <= ab->max_size) {
        size_t written = switch_buffer_write(ab->buffer, data, len);
        status = (written == len) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
    } else {
        /* Buffer full - could implement overflow strategy here */
        status = SWITCH_STATUS_FALSE;
    }

    switch_mutex_unlock(ab->mutex);

    return status;
}

/* Read from audio buffer */
switch_status_t audio_buffer_read(audio_buffer_t *ab, void *data, size_t len)
{
    switch_status_t status = SWITCH_STATUS_FALSE;

    if (!ab || !data || len == 0) {
        return SWITCH_STATUS_FALSE;
    }

    switch_mutex_lock(ab->mutex);

    size_t available = switch_buffer_inuse(ab->buffer);
    if (available >= len) {
        size_t read = switch_buffer_read(ab->buffer, data, len);
        status = (read == len) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
    } else if (available > 0) {
        /* Read what's available */
        size_t read = switch_buffer_read(ab->buffer, data, available);
        /* Zero out the rest */
        if (read < len) {
            memset((uint8_t*)data + read, 0, len - read);
        }
        status = SWITCH_STATUS_SUCCESS;
    } else {
        /* Buffer empty - return silence */
        memset(data, 0, len);
        status = SWITCH_STATUS_FALSE;
    }

    switch_mutex_unlock(ab->mutex);

    return status;
}

/* Get used space in buffer */
size_t audio_buffer_used(audio_buffer_t *ab)
{
    size_t used = 0;

    if (!ab) return 0;

    switch_mutex_lock(ab->mutex);
    used = switch_buffer_inuse(ab->buffer);
    switch_mutex_unlock(ab->mutex);

    return used;
}

/* Get available space in buffer */
size_t audio_buffer_available(audio_buffer_t *ab)
{
    size_t available = 0;

    if (!ab) return 0;

    switch_mutex_lock(ab->mutex);
    available = ab->max_size - switch_buffer_inuse(ab->buffer);
    switch_mutex_unlock(ab->mutex);

    return available;
}

/* Clear buffer */
void audio_buffer_clear(audio_buffer_t *ab)
{
    if (!ab) return;

    switch_mutex_lock(ab->mutex);
    switch_buffer_zero(ab->buffer);
    switch_mutex_unlock(ab->mutex);
}

/* Check if buffer is empty */
int audio_buffer_is_empty(audio_buffer_t *ab)
{
    int empty = 1;

    if (!ab) return 1;

    switch_mutex_lock(ab->mutex);
    empty = (switch_buffer_inuse(ab->buffer) == 0);
    switch_mutex_unlock(ab->mutex);

    return empty;
}
