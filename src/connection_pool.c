/*
 * Connection Pool Statistics for mod_live_call
 */

#include "connection_pool.h"
#include "ws_client.h"

/* Get pool statistics */
void get_pool_stats(ws_pool_t *pool, pool_stats_t *stats)
{
    if (!pool || !stats) return;

    memset(stats, 0, sizeof(*stats));

    switch_mutex_lock(pool->mutex);

    stats->total_connections = pool->size;
    stats->available_connections = pool->available;
    stats->active_connections = pool->size - pool->available;
    stats->total_acquires = pool->total_acquires;
    stats->total_releases = pool->total_releases;

    switch_mutex_unlock(pool->mutex);
}
