/*
 * mod_live_call - FreeSWITCH Bi-directional Audio Streaming Module
 * Unlimited Channel Support
 */

#include "mod_live_call.h"

/* Module interface */
SWITCH_MODULE_LOAD_FUNCTION(mod_live_call_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_live_call_shutdown);
SWITCH_MODULE_DEFINITION(mod_live_call, mod_live_call_load, mod_live_call_shutdown, NULL);

/* Global state */
live_call_globals_t globals;

/* Application function */
SWITCH_STANDARD_APP(live_call_app_function)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    char *command = (char *) data;

    if (!zstr(command) && !strcasecmp(command, "start")) {
        if (start_live_call(session) == SWITCH_STATUS_SUCCESS) {
            switch_channel_set_variable(channel, "live_call_status", "active");
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, 
                            "Live call streaming started\n");
        } else {
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                            "Failed to start live call streaming\n");
        }
    } else if (!zstr(command) && !strcasecmp(command, "stop")) {
        if (stop_live_call(session) == SWITCH_STATUS_SUCCESS) {
            switch_channel_set_variable(channel, "live_call_status", "inactive");
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, 
                            "Live call streaming stopped\n");
        }
    } else {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Invalid command. Use 'start' or 'stop'\n");
    }
}

/* API function */
SWITCH_STANDARD_API(live_call_api_function)
{
    if (zstr(cmd)) {
        stream->write_function(stream, "-USAGE: live_call <status|stats>\n");
        return SWITCH_STATUS_SUCCESS;
    }

    if (!strcasecmp(cmd, "status")) {
        return get_live_call_status(stream);
    } else if (!strcasecmp(cmd, "stats")) {
        return get_live_call_stats(stream);
    } else {
        stream->write_function(stream, "-ERR Invalid command\n");
        return SWITCH_STATUS_SUCCESS;
    }
}

/* Audio callback - handles bi-directional audio */
static switch_bool_t audio_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
    live_call_context_t *context = (live_call_context_t *)user_data;
    switch_frame_t *frame;

    if (!context || !context->running) {
        return SWITCH_FALSE;
    }

    switch (type) {
        case SWITCH_ABC_TYPE_INIT:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                            "Audio bug initialized for session %s\n", context->uuid);
            break;

        case SWITCH_ABC_TYPE_READ_FRAME:
            /* Capture outgoing audio (what caller says) */
            frame = switch_core_media_bug_get_read_replace_frame(bug);
            if (frame && frame->data && frame->datalen) {
                /* Write to buffer untuk dikirim ke WebSocket */
                audio_buffer_write(context->read_buffer, frame->data, frame->datalen);
            }
            break;

        case SWITCH_ABC_TYPE_WRITE_FRAME:
            /* Inject audio from WebSocket into call */
            frame = switch_core_media_bug_get_write_replace_frame(bug);
            if (frame && frame->data && frame->datalen) {
                /* Use jitter buffer for stable playback */
                if (jitter_buffer_ready(context->jitter_buffer)) {
                    if (jitter_buffer_get(context->jitter_buffer, frame->data, frame->datalen) == SWITCH_STATUS_SUCCESS) {
                        switch_core_media_bug_set_write_replace_frame(bug, frame);
                    }
                } else {
                    /* Jitter buffer not ready, output silence */
                    memset(frame->data, 0, frame->datalen);
                }
            }
            break;

        case SWITCH_ABC_TYPE_CLOSE:
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                            "Audio bug closing for session %s\n", context->uuid);
            context->running = 0;
            break;

        default:
            break;
    }

    return SWITCH_TRUE;
}

/* Worker thread - handles WebSocket communication */
static void *audio_worker_thread(switch_thread_t *thread, void *obj)
{
    live_call_context_t *context = (live_call_context_t *)obj;
    uint8_t audio_data[DEFAULT_BUFFER_SIZE];
    size_t len;
    int reconnect_attempts = 0;
    const int max_reconnect_attempts = 5;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                    "Worker thread started for session %s\n", context->uuid);

    while (context->running) {
        /* Check WebSocket connection health */
        if (!ws_is_connected(context->ws_conn)) {
            if (reconnect_attempts < max_reconnect_attempts) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
                                "WebSocket disconnected, attempting reconnect %d/%d\n", 
                                reconnect_attempts + 1, max_reconnect_attempts);
                
                if (ws_reconnect(context->ws_conn) == SWITCH_STATUS_SUCCESS) {
                    reconnect_attempts = 0;
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                                    "WebSocket reconnected successfully\n");
                } else {
                    reconnect_attempts++;
                    switch_yield(1000000); /* Wait 1 second before retry */
                }
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                                "Max reconnect attempts reached, stopping session %s\n", 
                                context->uuid);
                context->running = 0;
                break;
            }
        }

        /* Service WebSocket connection */
        ws_service(context->ws_conn, 50);

        /* Send outgoing audio to WebSocket */
        if (!audio_buffer_is_empty(context->read_buffer)) {
            len = audio_buffer_used(context->read_buffer);
            if (len > sizeof(audio_data)) {
                len = sizeof(audio_data);
            }
            
            if (audio_buffer_read(context->read_buffer, audio_data, len) == SWITCH_STATUS_SUCCESS) {
                if (ws_send_audio(context->ws_conn, context->uuid, audio_data, len) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
                                    "Failed to send audio, connection may be down\n");
                } else {
                    context->packets_sent++;
                    context->bytes_sent += len;
                }
            }
        }

        /* Receive incoming audio from WebSocket */
        if (ws_has_incoming_audio(context->ws_conn)) {
            uint8_t *recv_data = NULL;
            size_t recv_len = 0;
            
            if (ws_receive_audio(context->ws_conn, &recv_data, &recv_len) == SWITCH_STATUS_SUCCESS) {
                if (recv_data && recv_len > 0) {
                    /* Put received audio into jitter buffer */
                    if (jitter_buffer_put(context->jitter_buffer, recv_data, recv_len) != SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                                        "Jitter buffer full, audio dropped\n");
                    } else {
                        context->packets_received++;
                        context->bytes_received += recv_len;
                    }
                    switch_safe_free(recv_data);
                }
            }
        }

        /* Small sleep to prevent busy loop */
        switch_yield(1000);
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                    "Worker thread exiting for session %s\n", context->uuid);

    return NULL;
}

/* Cleanup context */
static void cleanup_context(live_call_context_t *context)
{
    if (!context) return;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
                    "Cleaning up context for session %s\n", context->uuid);

    context->running = 0;

    /* Wait for thread to finish */
    if (context->thread) {
        switch_status_t st;
        switch_thread_join(&st, context->thread);
        context->thread = NULL;
    }

    /* Release WebSocket connection back to pool */
    if (context->ws_conn && globals.ws_pool) {
        ws_pool_release(globals.ws_pool, context->ws_conn);
        context->ws_conn = NULL;
    }

    /* Destroy buffers */
    if (context->read_buffer) {
        audio_buffer_destroy(context->read_buffer);
        context->read_buffer = NULL;
    }

    if (context->write_buffer) {
        audio_buffer_destroy(context->write_buffer);
        context->write_buffer = NULL;
    }

    if (context->jitter_buffer) {
        jitter_buffer_destroy(context->jitter_buffer);
        context->jitter_buffer = NULL;
    }

    /* Decrement active sessions */
    switch_atomic_dec(&globals.active_sessions);
}

/* Start live call streaming */
switch_status_t start_live_call(switch_core_session_t *session)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_codec_implementation_t read_impl = { 0 };
    live_call_context_t *context;
    switch_threadattr_t *thd_attr = NULL;
    switch_memory_pool_t *pool = switch_core_session_get_pool(session);

    /* Get codec info */
    switch_core_session_get_read_impl(session, &read_impl);

    /* Allocate context */
    context = switch_core_session_alloc(session, sizeof(*context));
    memset(context, 0, sizeof(*context));

    context->session = session;
    context->channel = channel;
    context->uuid = switch_core_session_get_uuid(session);
    context->sample_rate = read_impl.samples_per_second;
    context->running = 1;

    /* Create mutex */
    switch_mutex_init(&context->mutex, SWITCH_MUTEX_NESTED, pool);

    /* Create audio buffers */
    context->read_buffer = audio_buffer_create(globals.buffer_size, pool);
    context->write_buffer = audio_buffer_create(globals.buffer_size, pool);
    context->jitter_buffer = jitter_buffer_create(
        globals.buffer_size / 4,    /* min_size: 25% of buffer */
        globals.buffer_size * 2,     /* max_size: 2x buffer */
        pool
    );

    if (!context->read_buffer || !context->write_buffer || !context->jitter_buffer) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Failed to create audio buffers\n");
        cleanup_context(context);
        return SWITCH_STATUS_FALSE;
    }

    /* Initialize statistics */
    context->packets_sent = 0;
    context->packets_received = 0;
    context->bytes_sent = 0;
    context->bytes_received = 0;

    /* Acquire WebSocket connection from pool */
    context->ws_conn = ws_pool_acquire(globals.ws_pool);
    if (!context->ws_conn) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Failed to acquire WebSocket connection\n");
        cleanup_context(context);
        return SWITCH_STATUS_FALSE;
    }

    /* Connect WebSocket */
    if (ws_connect(context->ws_conn) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Failed to connect WebSocket\n");
        cleanup_context(context);
        return SWITCH_STATUS_FALSE;
    }

    /* Add media bug for audio capture */
    if (switch_core_media_bug_add(session, "live_call", NULL,
                                   audio_callback, context, 0,
                                   SMBF_READ_STREAM | SMBF_WRITE_STREAM | SMBF_READ_REPLACE | SMBF_WRITE_REPLACE,
                                   &context->bug) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Failed to add media bug\n");
        cleanup_context(context);
        return SWITCH_STATUS_FALSE;
    }

    /* Start worker thread */
    switch_threadattr_create(&thd_attr, pool);
    switch_threadattr_detach_set(thd_attr, 1);
    switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);

    if (switch_thread_create(&context->thread, thd_attr, audio_worker_thread, 
                            context, pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
                        "Failed to create worker thread\n");
        switch_core_media_bug_remove(session, &context->bug);
        cleanup_context(context);
        return SWITCH_STATUS_FALSE;
    }

    /* Store context in channel private data */
    switch_channel_set_private(channel, "live_call_context", context);

    /* Increment counters */
    switch_atomic_inc(&globals.active_sessions);
    switch_atomic_inc(&globals.total_sessions);

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, 
                    "Live call streaming started successfully\n");

    return SWITCH_STATUS_SUCCESS;
}

/* Stop live call streaming */
switch_status_t stop_live_call(switch_core_session_t *session)
{
    switch_channel_t *channel = switch_core_session_get_channel(session);
    live_call_context_t *context = switch_channel_get_private(channel, "live_call_context");

    if (!context) {
        return SWITCH_STATUS_FALSE;
    }

    /* Remove media bug */
    if (context->bug) {
        switch_core_media_bug_remove(session, &context->bug);
        context->bug = NULL;
    }

    /* Cleanup */
    cleanup_context(context);
    switch_channel_set_private(channel, "live_call_context", NULL);

    return SWITCH_STATUS_SUCCESS;
}

/* Get status */
switch_status_t get_live_call_status(switch_stream_handle_t *stream)
{
    stream->write_function(stream, "+OK\n");
    stream->write_function(stream, "Active Sessions: %d\n", 
                          switch_atomic_read(&globals.active_sessions));
    stream->write_function(stream, "Total Sessions: %d\n", 
                          switch_atomic_read(&globals.total_sessions));
    
    return SWITCH_STATUS_SUCCESS;
}

/* Get statistics */
switch_status_t get_live_call_stats(switch_stream_handle_t *stream)
{
    pool_stats_t stats;
    
    get_pool_stats(globals.ws_pool, &stats);
    
    stream->write_function(stream, "+OK\n");
    stream->write_function(stream, "=== Live Call Statistics ===\n");
    stream->write_function(stream, "Active Sessions: %d\n", 
                          switch_atomic_read(&globals.active_sessions));
    stream->write_function(stream, "Total Sessions: %d\n", 
                          switch_atomic_read(&globals.total_sessions));
    stream->write_function(stream, "\n=== Connection Pool ===\n");
    stream->write_function(stream, "Total Connections: %d\n", stats.total_connections);
    stream->write_function(stream, "Active Connections: %d\n", stats.active_connections);
    stream->write_function(stream, "Available Connections: %d\n", stats.available_connections);
    stream->write_function(stream, "Total Acquires: %llu\n", (unsigned long long)stats.total_acquires);
    stream->write_function(stream, "Total Releases: %llu\n", (unsigned long long)stats.total_releases);
    
    return SWITCH_STATUS_SUCCESS;
}

/* Load configuration */
static switch_status_t load_config(void)
{
    switch_xml_t cfg, xml, settings, param;
    
    memset(&globals, 0, sizeof(globals));
    
    /* Default values */
    globals.ws_server = DEFAULT_WS_SERVER;
    globals.max_threads = DEFAULT_MAX_THREADS;
    globals.buffer_size = DEFAULT_BUFFER_SIZE;
    globals.sample_rate = DEFAULT_SAMPLE_RATE;
    globals.debug_level = 0;

    if ((xml = switch_xml_open_cfg("live_call.conf", &cfg, NULL))) {
        if ((settings = switch_xml_child(cfg, "settings"))) {
            for (param = switch_xml_child(settings, "param"); param; 
                 param = param->next) {
                char *var = (char *)switch_xml_attr_soft(param, "name");
                char *val = (char *)switch_xml_attr_soft(param, "value");

                if (!strcasecmp(var, "ws-server")) {
                    globals.ws_server = switch_core_strdup(globals.pool, val);
                } else if (!strcasecmp(var, "max-threads")) {
                    globals.max_threads = atoi(val);
                } else if (!strcasecmp(var, "buffer-size")) {
                    globals.buffer_size = atoi(val);
                } else if (!strcasecmp(var, "sample-rate")) {
                    globals.sample_rate = atoi(val);
                } else if (!strcasecmp(var, "debug-level")) {
                    globals.debug_level = atoi(val);
                }
            }
        }
        switch_xml_free(xml);
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Module load */
SWITCH_MODULE_LOAD_FUNCTION(mod_live_call_load)
{
    switch_application_interface_t *app_interface;
    switch_api_interface_t *api_interface;

    /* Create module pool */
    switch_core_new_memory_pool(&globals.pool);
    
    /* Load config */
    load_config();

    /* Initialize mutex */
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

    /* Initialize atomic counters */
    switch_atomic_init(&globals.active_sessions, 0);
    switch_atomic_init(&globals.total_sessions, 0);

    /* Create WebSocket connection pool */
    globals.ws_pool = ws_pool_create(globals.max_threads, globals.ws_server, globals.pool);
    if (!globals.ws_pool) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                        "Failed to create WebSocket connection pool\n");
        return SWITCH_STATUS_FALSE;
    }

    /* Connect module interface */
    *module_interface = switch_loadable_module_create_module_interface(pool, modname);

    /* Register application */
    SWITCH_ADD_APP(app_interface, "live_call", "Live call audio streaming", 
                   "Start/stop live call streaming", live_call_app_function, 
                   "<start|stop>", SAF_SUPPORT_NOMEDIA);

    /* Register API */
    SWITCH_ADD_API(api_interface, "live_call", "Live call API", 
                   live_call_api_function, "<status|stats>");

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                    "mod_live_call loaded (version %s)\n", MOD_LIVE_CALL_VERSION);

    return SWITCH_STATUS_SUCCESS;
}

/* Module shutdown */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_live_call_shutdown)
{
    /* Destroy connection pool */
    if (globals.ws_pool) {
        ws_pool_destroy(globals.ws_pool);
        globals.ws_pool = NULL;
    }

    /* Cleanup mutex */
    if (globals.mutex) {
        switch_mutex_destroy(globals.mutex);
        globals.mutex = NULL;
    }

    /* Destroy pool */
    if (globals.pool) {
        switch_core_destroy_memory_pool(&globals.pool);
        globals.pool = NULL;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                    "mod_live_call unloaded\n");

    return SWITCH_STATUS_SUCCESS;
}
