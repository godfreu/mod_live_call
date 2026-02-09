#ifndef WS_CLIENT_H
#define WS_CLIENT_H

#include <switch.h>
#include <libwebsockets.h>

/* WebSocket connection structure */
typedef struct {
    struct lws_context *context;
    struct lws *wsi;
    char *server_url;
    int port;
    char *path;
    int connected;
    switch_mutex_t *mutex;
    switch_buffer_t *send_buffer;
    switch_buffer_t *recv_buffer;
    void *user_data;
} ws_connection_t;

/* WebSocket connection pool */
typedef struct {
    ws_connection_t **connections;
    int size;
    int available;
    switch_mutex_t *mutex;
    switch_memory_pool_t *pool;
    /* Statistics tracking */
    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t total_reconnects;
} ws_pool_t;

/* Function prototypes */

/* Connection management */
ws_connection_t *ws_create_connection(const char *url, switch_memory_pool_t *pool);
void ws_destroy_connection(ws_connection_t *conn);
switch_status_t ws_connect(ws_connection_t *conn);
switch_status_t ws_reconnect(ws_connection_t *conn);
void ws_disconnect(ws_connection_t *conn);
int ws_is_connected(ws_connection_t *conn);

/* Data transfer */
switch_status_t ws_send_audio(ws_connection_t *conn, const char *uuid, 
                              const uint8_t *data, size_t len);
switch_status_t ws_receive_audio(ws_connection_t *conn, uint8_t **data, size_t *len);
int ws_has_incoming_audio(ws_connection_t *conn);

/* Connection pool */
ws_pool_t *ws_pool_create(int size, const char *server_url, switch_memory_pool_t *pool);
void ws_pool_destroy(ws_pool_t *pool);
ws_connection_t *ws_pool_acquire(ws_pool_t *pool);
void ws_pool_release(ws_pool_t *pool, ws_connection_t *conn);

/* Service */
void ws_service(ws_connection_t *conn, int timeout_ms);

#endif /* WS_CLIENT_H */
