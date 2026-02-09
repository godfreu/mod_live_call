#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <switch.h>

/* Audio buffer structure */
typedef struct {
    switch_buffer_t *buffer;
    switch_mutex_t *mutex;
    size_t max_size;
    switch_memory_pool_t *pool;
} audio_buffer_t;

/* Function prototypes */

/* Buffer management */
audio_buffer_t *audio_buffer_create(size_t size, switch_memory_pool_t *pool);
void audio_buffer_destroy(audio_buffer_t *ab);

/* Data operations */
switch_status_t audio_buffer_write(audio_buffer_t *ab, const void *data, size_t len);
switch_status_t audio_buffer_read(audio_buffer_t *ab, void *data, size_t len);
size_t audio_buffer_used(audio_buffer_t *ab);
size_t audio_buffer_available(audio_buffer_t *ab);
void audio_buffer_clear(audio_buffer_t *ab);
int audio_buffer_is_empty(audio_buffer_t *ab);

#endif /* AUDIO_BUFFER_H */
