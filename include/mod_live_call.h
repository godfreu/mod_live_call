#ifndef MOD_LIVE_CALL_H
#define MOD_LIVE_CALL_H

#include <switch.h>
#include "ws_client.h"
#include "audio_buffer.h"
#include "connection_pool.h"
#include "jitter_buffer.h"

/* Module version */
#define MOD_LIVE_CALL_VERSION "1.0.0"

/* Default configuration values */
#define DEFAULT_WS_SERVER "ws://localhost:8080/audio"
#define DEFAULT_MAX_THREADS 1000
#define DEFAULT_BUFFER_SIZE 8192
#define DEFAULT_SAMPLE_RATE 8000

/* Module globals */
typedef struct {
    char *ws_server;
    int max_threads;
    int buffer_size;
    int sample_rate;
    int debug_level;
    switch_memory_pool_t *pool;
    ws_pool_t *ws_pool;
    switch_mutex_t *mutex;
    switch_atomic_t active_sessions;
    switch_atomic_t total_sessions;
} live_call_globals_t;

extern live_call_globals_t globals;

/* Context for each session */
typedef struct {
    switch_core_session_t *session;
    switch_channel_t *channel;
    switch_media_bug_t *bug;
    switch_mutex_t *mutex;
    switch_thread_t *thread;
    audio_buffer_t *read_buffer;
    audio_buffer_t *write_buffer;
    jitter_buffer_t *jitter_buffer;
    ws_connection_t *ws_conn;
    int running;
    char *uuid;
    uint32_t sample_rate;
    /* Statistics */
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} live_call_context_t;

/* Function prototypes */
switch_status_t start_live_call(switch_core_session_t *session);
switch_status_t stop_live_call(switch_core_session_t *session);
switch_status_t get_live_call_status(switch_stream_handle_t *stream);
switch_status_t get_live_call_stats(switch_stream_handle_t *stream);

/* Internal functions */
static switch_bool_t audio_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type);
static void *audio_worker_thread(switch_thread_t *thread, void *obj);
static void cleanup_context(live_call_context_t *context);
static switch_status_t load_config(void);

#endif /* MOD_LIVE_CALL_H */
