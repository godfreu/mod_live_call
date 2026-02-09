#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include <switch.h>

/* Connection pool statistics */
typedef struct {
    int total_connections;
    int active_connections;
    int available_connections;
    uint64_t total_acquires;
    uint64_t total_releases;
} pool_stats_t;

/* Get pool statistics */
void get_pool_stats(ws_pool_t *pool, pool_stats_t *stats);

#endif /* CONNECTION_POOL_H */
