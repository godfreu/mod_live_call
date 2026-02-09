/*
 * WebSocket Client Implementation for mod_live_call
 */

#include "ws_client.h"
#include <string.h>
#include <stdlib.h>

/* libwebsockets callback */
static int ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
                       void *user, void *in, size_t len)
{
    ws_connection_t *conn = (ws_connection_t *)user;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            if (conn) {
                conn->connected = 1;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                                "WebSocket connected\n");
            }
            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            if (conn && in && len > 0) {
                switch_mutex_lock(conn->mutex);
                switch_buffer_write(conn->recv_buffer, in, len);
                switch_mutex_unlock(conn->mutex);
            }
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            if (conn && switch_buffer_inuse(conn->send_buffer) > 0) {
                uint8_t buf[LWS_PRE + 8192];
                size_t to_send;

                switch_mutex_lock(conn->mutex);
                to_send = switch_buffer_read(conn->send_buffer, 
                                             buf + LWS_PRE, 
                                             sizeof(buf) - LWS_PRE);
                switch_mutex_unlock(conn->mutex);

                if (to_send > 0) {
                    lws_write(wsi, buf + LWS_PRE, to_send, LWS_WRITE_BINARY);
                }

                if (switch_buffer_inuse(conn->send_buffer) > 0) {
                    lws_callback_on_writable(wsi);
                }
            }
            break;

        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            if (conn) {
                conn->connected = 0;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                                "WebSocket connection error\n");
            }
            break;

        case LWS_CALLBACK_CLOSED:
            if (conn) {
                conn->connected = 0;
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                                "WebSocket closed\n");
            }
            break;

        default:
            break;
    }

    return 0;
}

/* Create WebSocket connection */
ws_connection_t *ws_create_connection(const char *url, switch_memory_pool_t *pool)
{
    ws_connection_t *conn;
    struct lws_context_creation_info info;
    static const struct lws_protocols protocols[] = {
        { "audio-stream", ws_callback, sizeof(ws_connection_t), 8192, 0, NULL, 0 },
        { NULL, NULL, 0, 0, 0, NULL, 0 }
    };

    conn = switch_core_alloc(pool, sizeof(*conn));
    memset(conn, 0, sizeof(*conn));

    /* Parse URL */
    conn->server_url = switch_core_strdup(pool, url);
    
    /* Simple URL parsing - assumes ws://host:port/path */
    char *proto = strstr(url, "://");
    if (proto) {
        proto += 3;
        char *path_start = strchr(proto, '/');
        if (path_start) {
            conn->path = switch_core_strdup(pool, path_start);
            char *host = switch_core_strndup(pool, proto, path_start - proto);
            
            char *port_str = strchr(host, ':');
            if (port_str) {
                *port_str = '\0';
                conn->port = atoi(port_str + 1);
            } else {
                conn->port = 80;
            }
        }
    }

    /* Create mutex */
    switch_mutex_init(&conn->mutex, SWITCH_MUTEX_NESTED, pool);

    /* Create buffers */
    switch_buffer_create_dynamic(&conn->send_buffer, 8192, 65536, 0);
    switch_buffer_create_dynamic(&conn->recv_buffer, 8192, 65536, 0);

    /* Create libwebsockets context */
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

    conn->context = lws_create_context(&info);
    if (!conn->context) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                        "Failed to create libwebsockets context\n");
        return NULL;
    }

    return conn;
}

/* Destroy WebSocket connection */
void ws_destroy_connection(ws_connection_t *conn)
{
    if (!conn) return;

    if (conn->wsi) {
        lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        conn->wsi = NULL;
    }

    if (conn->context) {
        lws_context_destroy(conn->context);
        conn->context = NULL;
    }

    if (conn->send_buffer) {
        switch_buffer_destroy(&conn->send_buffer);
    }

    if (conn->recv_buffer) {
        switch_buffer_destroy(&conn->recv_buffer);
    }
}

/* Connect WebSocket */
switch_status_t ws_connect(ws_connection_t *conn)
{
    struct lws_client_connect_info ccinfo;

    if (!conn || !conn->context) {
        return SWITCH_STATUS_FALSE;
    }

    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = conn->context;
    ccinfo.address = conn->server_url;
    ccinfo.port = conn->port;
    ccinfo.path = conn->path ? conn->path : "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = "audio-stream";
    ccinfo.userdata = conn;

    conn->wsi = lws_client_connect_via_info(&ccinfo);
    if (!conn->wsi) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                        "Failed to connect WebSocket\n");
        return SWITCH_STATUS_FALSE;
    }

    /* Wait for connection (with timeout) */
    int retry = 0;
    while (!conn->connected && retry < 50) {
        lws_service(conn->context, 100);
        retry++;
    }

    if (!conn->connected) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                        "WebSocket connection timeout\n");
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Reconnect WebSocket */
switch_status_t ws_reconnect(ws_connection_t *conn)
{
    if (!conn) return SWITCH_STATUS_FALSE;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
                    "Attempting to reconnect WebSocket\n");

    /* Disconnect first if needed */
    ws_disconnect(conn);

    /* Small delay before reconnect */
    switch_yield(500000); /* 500ms */

    /* Reconnect */
    return ws_connect(conn);
}

/* Disconnect WebSocket */
void ws_disconnect(ws_connection_t *conn)
{
    if (!conn) return;

    conn->connected = 0;

    if (conn->wsi) {
        lws_close_reason(conn->wsi, LWS_CLOSE_STATUS_NORMAL, NULL, 0);
        lws_callback_on_writable(conn->wsi);
        lws_service(conn->context, 100);
        conn->wsi = NULL;
    }
}

/* Check if connected */
int ws_is_connected(ws_connection_t *conn)
{
    return conn ? conn->connected : 0;
}

/* Send audio data */
switch_status_t ws_send_audio(ws_connection_t *conn, const char *uuid, 
                              const uint8_t *data, size_t len)
{
    if (!conn || !conn->connected || !data || len == 0) {
        return SWITCH_STATUS_FALSE;
    }

    /* Simple protocol: [UUID_LEN][UUID][DATA] */
    uint8_t header[256];
    size_t uuid_len = uuid ? strlen(uuid) : 0;
    size_t header_len = 0;

    header[header_len++] = (uint8_t)uuid_len;
    if (uuid_len > 0) {
        memcpy(header + header_len, uuid, uuid_len);
        header_len += uuid_len;
    }

    switch_mutex_lock(conn->mutex);
    switch_buffer_write(conn->send_buffer, header, header_len);
    switch_buffer_write(conn->send_buffer, data, len);
    switch_mutex_unlock(conn->mutex);

    if (conn->wsi) {
        lws_callback_on_writable(conn->wsi);
    }

    return SWITCH_STATUS_SUCCESS;
}

/* Receive audio data */
switch_status_t ws_receive_audio(ws_connection_t *conn, uint8_t **data, size_t *len)
{
    if (!conn || !data || !len) {
        return SWITCH_STATUS_FALSE;
    }

    *data = NULL;
    *len = 0;

    switch_mutex_lock(conn->mutex);
    size_t available = switch_buffer_inuse(conn->recv_buffer);
    
    if (available > 0) {
        *data = malloc(available);
        if (*data) {
            *len = switch_buffer_read(conn->recv_buffer, *data, available);
        }
    }
    switch_mutex_unlock(conn->mutex);

    return (*data && *len > 0) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

/* Check if has incoming audio */
int ws_has_incoming_audio(ws_connection_t *conn)
{
    if (!conn) return 0;

    switch_mutex_lock(conn->mutex);
    int has_data = switch_buffer_inuse(conn->recv_buffer) > 0;
    switch_mutex_unlock(conn->mutex);

    return has_data;
}

/* Service WebSocket */
void ws_service(ws_connection_t *conn, int timeout_ms)
{
    if (conn && conn->context) {
        lws_service(conn->context, timeout_ms);
    }
}

/* Create connection pool */
ws_pool_t *ws_pool_create(int size, const char *server_url, switch_memory_pool_t *pool)
{
    ws_pool_t *ws_pool;
    int i;

    ws_pool = switch_core_alloc(pool, sizeof(*ws_pool));
    memset(ws_pool, 0, sizeof(*ws_pool));

    ws_pool->size = size;
    ws_pool->available = size;
    ws_pool->pool = pool;
    ws_pool->total_acquires = 0;
    ws_pool->total_releases = 0;
    ws_pool->total_reconnects = 0;

    switch_mutex_init(&ws_pool->mutex, SWITCH_MUTEX_NESTED, pool);

    ws_pool->connections = switch_core_alloc(pool, sizeof(ws_connection_t*) * size);

    for (i = 0; i < size; i++) {
        ws_pool->connections[i] = ws_create_connection(server_url, pool);
        if (!ws_pool->connections[i]) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
                            "Failed to create WebSocket connection %d\n", i);
        }
    }

    return ws_pool;
}

/* Destroy connection pool */
void ws_pool_destroy(ws_pool_t *pool)
{
    int i;

    if (!pool) return;

    for (i = 0; i < pool->size; i++) {
        if (pool->connections[i]) {
            ws_destroy_connection(pool->connections[i]);
        }
    }

    if (pool->mutex) {
        switch_mutex_destroy(pool->mutex);
    }
}

/* Acquire connection from pool */
ws_connection_t *ws_pool_acquire(ws_pool_t *pool)
{
    ws_connection_t *conn = NULL;
    int i;

    if (!pool) return NULL;

    switch_mutex_lock(pool->mutex);

    for (i = 0; i < pool->size; i++) {
        if (pool->connections[i] && !pool->connections[i]->user_data) {
            conn = pool->connections[i];
            conn->user_data = (void *)1; /* Mark as in use */
            pool->available--;
            pool->total_acquires++;
            break;
        }
    }

    switch_mutex_unlock(pool->mutex);

    return conn;
}

/* Release connection back to pool */
void ws_pool_release(ws_pool_t *pool, ws_connection_t *conn)
{
    if (!pool || !conn) return;

    switch_mutex_lock(pool->mutex);

    conn->user_data = NULL; /* Mark as available */
    pool->available++;
    pool->total_releases++;

    /* Disconnect if needed */
    if (conn->connected) {
        ws_disconnect(conn);
    }

    /* Clear buffers */
    switch_buffer_zero(conn->send_buffer);
    switch_buffer_zero(conn->recv_buffer);

    switch_mutex_unlock(pool->mutex);
}
