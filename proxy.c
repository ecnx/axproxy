/* ------------------------------------------------------------------
 * AxProxy - Network Proxy Task
 * ------------------------------------------------------------------ */

#include "axproxy.h"

/**
 * Handle new stream creation
 */
static int handle_new_stream ( struct proxy_t *proxy, struct stream_t *stream )
{
    struct stream_t *util;

    if ( ~stream->revents & POLLIN )
    {
        return -1;
    }

    /* Accept incoming connection */
    if ( !( util = accept_new_stream ( proxy, stream->fd ) ) )
    {
        return -2;
    }

    /* Setup new stream */
    util->role = S_PORT_A;
    util->level = LEVEL_SOCKS_VER;
    util->events = POLLIN;

    return 0;
}

/**
 * Estabilish connection with endpoint
 */
static int setup_endpoint_stream ( struct proxy_t *proxy, struct stream_t *stream,
    const struct sockaddr_storage *saddr )
{
    int sock;
    struct stream_t *neighbour;

    /* Connect remote endpoint asynchronously */
    if ( ( sock = connect_async ( proxy, saddr ) ) < 0 )
    {
        return sock;
    }

    /* Try allocating neighbour stream */
    if ( !( neighbour = insert_stream ( proxy, sock ) ) )
    {
        force_cleanup ( proxy, stream );
        neighbour = insert_stream ( proxy, sock );
    }

    /* Check for neighbour stream */
    if ( !neighbour )
    {
        shutdown_then_close ( proxy, sock );
        return -2;
    }

    /* Set neighbour role */
    neighbour->role = S_PORT_B;
    neighbour->level = LEVEL_CONNECTING;
    neighbour->events = POLLIN | POLLOUT;

    /* Build up a new relation */
    neighbour->neighbour = stream;
    stream->neighbour = neighbour;

    verbose ( "new relation between socket:%i and socket:%i\n", stream->fd, sock );

    return 0;
}

/**
 * Handle stream socks handshake and request
 */
static int handle_stream_socks ( struct proxy_t *proxy, struct stream_t *stream )
{
    int status;
    size_t len;
    size_t hostlen;
    struct sockaddr_storage saddr;
    struct sockaddr_in *saddr_in;
    struct sockaddr_in6 *saddr_in6;
    char straddr[STRADDR_SIZE];
    char hostname[256];
    uint8_t arr[DATA_QUEUE_CAPACITY];

    /* Expect socket ready to be read */
    if ( ~stream->revents & POLLIN )
    {
        return -1;
    }

    /* Receive data chunk */
    if ( ( ssize_t ) ( len = recv ( stream->fd, arr, sizeof ( arr ), 0 ) ) < 2 )
    {
        failure ( "cannot receive data (%i) from socket:%i\n", errno, stream->fd );
        return -1;
    }

    /* Print progress */
    verbose ( "received %i byte(s) in handshake from socket:%i\n", ( int ) len, stream->fd );

    /* Enqueue input data */
    if ( queue_push ( &stream->queue, arr, len ) < 0 )
    {
        return -1;
    }

    /* Choose the action */
    switch ( stream->level )
    {
    case LEVEL_SOCKS_VER:
        /* Print current stage */
        verbose ( "processing socks SERVER/VERSION stage on socket:%i...\n", stream->fd );

        /* Assert minimum data length */
        if ( check_enough_data ( proxy, stream, 1 ) < 0 )
        {
            return 0;
        }

        /* Check for SOCKS5 version */
        if ( stream->queue.arr[0] != 5 )
        {
            failure ( "invalid socks version (0x%.2x) from socket:%i\n", stream->queue.arr[0],
                stream->fd );
            return -1;
        }

        /* User - pass auth or no auth */
        if ( len > 2 && stream->queue.arr[1] == 1 && stream->queue.arr[2] == 2 )
        {
            arr[0] = 5; /* SOCKS5 version */
            arr[1] = 2; /* User - pass auth */
            stream->level = LEVEL_SOCKS_AUTH;

        } else
        {
            arr[0] = 5; /* SOCKS5 version */
            arr[1] = 0; /* No auth */
            stream->level = LEVEL_SOCKS_REQ;
        }

        /* Enqueue response */
        if ( queue_set ( &stream->queue, arr, 2 ) < 0 )
        {
            return -1;
        }

        /* Update levels and events flags */
        stream->events = POLLOUT;
        break;
    case LEVEL_SOCKS_AUTH:
        /* Print current stage */
        verbose ( "processing socks SERVER/AUTH stage on socket:%i...\n", stream->fd );

        /* Auth passed no matter what credentials */
        arr[0] = 5;     /* SOCKS5 version */
        arr[1] = 0;     /* Auth success */

        /* Enqueue response */
        if ( queue_set ( &stream->queue, arr, 2 ) < 0 )
        {
            return -1;
        }

        /* Update levels and events flags */
        stream->level = LEVEL_SOCKS_REQ;
        stream->events = POLLOUT;
        break;
    case LEVEL_SOCKS_REQ:
        /* Print current stage */
        verbose ( "processing socks SERVER/REQUEST stage on socket:%i...\n", stream->fd );

        /* Assert minimum data length */
        if ( check_enough_data ( proxy, stream, 4 ) < 0 )
        {
            return 0;
        }

        /* Expect SOCKS5 version + request opcode */
        if ( stream->queue.arr[0] != 5 || stream->queue.arr[1] != 1 || stream->queue.arr[2] != 0 )
        {
            failure ( "invalid socks request from socket:%i\n", stream->fd );
            return -1;
        }

        /* Clear socket address */
        memset ( &saddr, '\0', sizeof ( saddr ) );

        /* Direct connect or by hostname */
        if ( stream->queue.arr[3] == 1 )
        {
            /* Print progress */
            verbose ( "got connect by ipv4 address request from socket:%i\n", stream->fd );

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, 10 ) < 0 )
            {
                return 0;
            }

            /* Prepare socket address */
            saddr_in = ( struct sockaddr_in * ) &saddr;
            saddr_in->sin_family = AF_INET;

            /* Parse network address then port number */
            memcpy ( &saddr_in->sin_addr, stream->queue.arr + 4, 4 );
            saddr_in->sin_port = htons ( ( ( stream->queue.arr[8] ) << 8 ) | stream->queue.arr[9] );
            if ( proxy->verbose )
            {
                format_ip_port ( &saddr, straddr, sizeof ( straddr ) );
            }
            verbose ( "connect by ipv4 address to (%s) requested from socket:%i...\n", straddr,
                stream->fd );

        } else if ( stream->queue.arr[3] == 3 )
        {
            /* Print progress */
            verbose ( "got connect by hostname request from socket:%i\n", stream->fd );

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, 5 ) < 0 )
            {
                return 0;
            }

            /* Parse hostname length */
            hostlen = stream->queue.arr[4];

            /* Assert maximum data length */
            if ( hostlen >= sizeof ( hostname ) )
            {
                failure ( "hostname is too long by socket:%i\n", stream->fd );
                return -1;
            }

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, hostlen + 7 ) < 0 )
            {
                return 0;
            }

            /* Prepare socket address */
            saddr_in = ( struct sockaddr_in * ) &saddr;
            saddr_in->sin_family = AF_INET;

            /* Parse hostname then port number */
            saddr_in->sin_port =
                htons ( ( ( stream->queue.arr[5 + hostlen] ) << 8 ) | stream->queue.arr[6 +
                    hostlen] );
            memcpy ( hostname, stream->queue.arr + 5, hostlen );
            hostname[hostlen] = '\0';

            /* Print progress */
            verbose ( "connect by hostname to (%s) requested from socket:%i...\n", hostname,
                stream->fd );

            /* Resolve hostname */
            if ( nsaddr_cached ( hostname, &saddr_in->sin_addr.s_addr ) < 0 )
            {
                failure ( "failed to resolve address by hostname (%s)\n", hostname );
                return -1;
            }

            if ( proxy->verbose )
            {
                format_ip_port ( &saddr, straddr, sizeof ( straddr ) );
            }

            verbose ( "resolved address by hostname for socket:%i to %s\n", stream->fd, straddr );

        } else if ( stream->queue.arr[3] == 4 )
        {
            /* Print progress */
            verbose ( "got connect by ipv6 address request from socket:%i\n", stream->fd );

            /* Assert minimum data length */
            if ( check_enough_data ( proxy, stream, 22 ) < 0 )
            {
                return 0;
            }

            /* Prepare socket address */
            saddr_in6 = ( struct sockaddr_in6 * ) &saddr;
            saddr_in6->sin6_family = AF_INET6;

            /* Parse network address then port number */
            memcpy ( &saddr_in6->sin6_addr, stream->queue.arr + 4, 16 );
            saddr_in6->sin6_port =
                htons ( ( ( stream->queue.arr[20] ) << 8 ) | stream->queue.arr[21] );
            if ( proxy->verbose )
            {
                format_ip_port ( &saddr, straddr, sizeof ( straddr ) );
            }

            verbose ( "connect by ipv6 address to (%s) requested from socket:%i...\n", straddr,
                stream->fd );

        } else
        {
            verbose ( "unknown connect mode (0x%.2x) requested from socket:%i...\n",
                stream->queue.arr[3], stream->fd );
            return -1;
        }

        /* Connect endpoint */
        if ( ( status = setup_endpoint_stream ( proxy, stream, &saddr ) ) < 0 )
        {
            return status;
        }

        verbose ( "async connect successful for stream with socket:%i\n", stream->fd );

        /* Prepare response */
        arr[0] = 5;     /* SOCKS5 version */
        arr[1] = 0;     /* Request granted */
        arr[2] = 0;     /* Reserved */
        arr[3] = 1;     /* Address type: IPv4 */
        arr[4] = 0;     /* Address byte #1 */
        arr[5] = 0;     /* Address byte #2 */
        arr[6] = 0;     /* Address byte #3 */
        arr[7] = 0;     /* Address byte #4 */
        arr[8] = 0;     /* Port 1st byte */
        arr[9] = 0;     /* Port 2nd byte */

        /* Enqueue response */
        if ( queue_set ( &stream->queue, arr, 10 ) < 0 )
        {
            return -1;
        }

        /* Update levels and events flags */
        stream->level = LEVEL_SOCKS_PASS;
        stream->events = POLLOUT;
        break;
    default:
        return -1;
    }

    return 0;
}

/**
 * Handle stream events
 */
int handle_stream_events ( struct proxy_t *proxy, struct stream_t *stream )
{
    int status;

    if ( handle_forward_data ( proxy, stream ) >= 0 )
    {
        return 0;
    }

    if ( stream->role == S_PORT_A && stream->queue.len && ( stream->revents & POLLOUT ) )
    {
        if ( queue_shift ( &stream->queue, stream->fd ) < 0 )
        {
            remove_relation ( stream );
            return 0;
        }
        if ( stream->queue.len == 0 )
        {
            stream->events = stream->level == LEVEL_SOCKS_PASS ? 0 : POLLIN;
        }
        return 0;
    }

    switch ( stream->role )
    {
    case L_ACCEPT:
        show_stats ( proxy );
        if ( handle_new_stream ( proxy, stream ) == -2 )
        {
            return -1;
        }
        return 0;
    case S_PORT_A:
        if ( ( status = handle_stream_socks ( proxy, stream ) ) >= 0 )
        {
            return 0;
        }
        if ( status == -2 )
        {
            failure ( "encountered a severe error!\n" );
            return -1;
        }
        break;
    case S_PORT_B:
        if ( stream->level == LEVEL_CONNECTING && stream->neighbour
            && ( stream->revents & ( POLLIN | POLLOUT ) ) )
        {
            verbose ( "async connect completed for socket:%i\n", stream->fd );
            stream->level = LEVEL_FORWARDING;
            stream->events = POLLIN;
            stream->neighbour->level = LEVEL_FORWARDING;
            stream->neighbour->events = POLLIN;
            return 0;
        }
        break;
    }

    remove_relation ( stream );

    return 0;
}

/**
 * Proxy task entry point
 */
int proxy_task ( struct proxy_t *proxy )
{
    int status = 0;
    int sock;
    struct stream_t *stream;

    /* Set stream size */
    proxy->stream_size = sizeof ( struct stream_t );

    /* Reset current state */
    proxy->stream_head = NULL;
    proxy->stream_tail = NULL;
    memset ( proxy->stream_pool, '\0', sizeof ( proxy->stream_pool ) );

    /* Proxy events setup */
    if ( proxy_events_setup ( proxy ) < 0 )
    {
        return -1;
    }

    /* Setup listen socket */
    if ( ( sock = listen_socket ( proxy, &proxy->entrance ) ) < 0 )
    {
        if ( proxy->epoll_fd >= 0 )
        {
            close ( proxy->epoll_fd );
        }
        return -1;
    }

    /* Allocate new stream */
    if ( !( stream = insert_stream ( proxy, sock ) ) )
    {
        shutdown_then_close ( proxy, sock );
        if ( proxy->epoll_fd >= 0 )
        {
            close ( proxy->epoll_fd );
        }
        return -1;
    }

    /* Update listen stream */
    stream->role = L_ACCEPT;
    stream->events = POLLIN;

    verbose ( "proxy setup was successful\n" );

    /* Run forward loop */
    while ( ( status = handle_streams_cycle ( proxy ) ) >= 0 );

    /* Remove all streams */
    remove_all_streams ( proxy );

    /* Close epoll fd if created */
    if ( proxy->epoll_fd >= 0 )
    {
        close ( proxy->epoll_fd );
    }

    verbose ( "done proxy uninitializing\n" );

    return status;
}
