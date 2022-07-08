/* ------------------------------------------------------------------
 * AxProxy - Shared Project Header
 * ------------------------------------------------------------------ */

#ifndef AXPROXY_H
#define AXPROXY_H

#include "defs.h"
#include "config.h"
#include "dns.h"

#define L_ACCEPT                    0

#define LEVEL_SOCKS_VER             1
#define LEVEL_SOCKS_AUTH            2
#define LEVEL_SOCKS_REQ             3
#define LEVEL_SOCKS_PASS            4

#define EPOLLREF                    ((struct pollfd*) -1)

/**
 * Utility data queue
 */
struct queue_t
{
    size_t len;
    uint8_t arr[DATA_QUEUE_CAPACITY];
};

/**
 * IP/TCP connection stream
 */
struct stream_t
{
    int role;
    int fd;
    int level;
    int allocated;
    int abandoned;
    short events;
    short levents;
    short revents;

    struct pollfd *pollref;
    struct stream_t *neighbour;
    struct stream_t *prev;
    struct stream_t *next;
    struct queue_t queue;
};

/**
 * AxProxy task context
 */
struct proxy_t
{
    size_t stream_size;
    int verbose;
    int epoll_fd;
    struct stream_t *stream_head;
    struct stream_t *stream_tail;
    struct stream_t stream_pool[POOL_SIZE];

    struct sockaddr_storage entrance;
};

/**
 * Proxy task entry point
 */
extern int proxy_task ( struct proxy_t *proxy );

/**
 * Resolve hostname into IPv4 address
 */
extern int nsaddr_cached ( const char *hostname, uint32_t * addr );

#include "util.h"

#endif
