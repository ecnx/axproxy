/* ------------------------------------------------------------------
 * Ax-Proxy - Proxy Task
 * ------------------------------------------------------------------ */

#include "ella.h"

/**
 * IP/TCP connection stream 
 */
struct stream_t
{
    int role;
    int fd;
    int polled;
    int level;

#ifdef STATIC_POOL
    int allocated;
#endif

    struct stream_t *neighbour;
    struct stream_t *prev;
    struct stream_t *next;
};

/**
 * Ella Proxy program variables
 */
static int exit_flag = 0;
static struct stream_t *stream_list;
static size_t poll_len = 0;
static int poll_modified = 1;
#ifndef STATIC_POOL
static size_t poll_size = POLL_BASE_SIZE;
static struct pollfd *poll_list;
#else
static size_t poll_size = STATIC_POOL_SIZE;
static struct pollfd poll_list[STATIC_POOL_SIZE];
static struct stream_t stream_pool[STATIC_POOL_SIZE];
#endif

/**
 * Expand poll event list
 */
static int poll_expand ( void )
{
#ifndef STATIC_POOL
    size_t size_backup = poll_size;
    struct pollfd *list_backup = poll_list;

    poll_size <<= 1;

    if ( !( poll_list =
            ( struct pollfd * ) realloc ( poll_list, poll_size * sizeof ( struct pollfd ) ) ) )
    {
        errno = ENOMEM;
        free ( list_backup );
        return -1;
    }

    while ( size_backup < poll_size )
    {
        poll_list[size_backup++].events = POLLIN;
    }

    return 0;
#else
    return -1;
#endif
}

/**
 * Allocate stream structure and insert into the list
 */
static struct stream_t *insert_stream ( int role, int sock )
{
    struct stream_t *stream;
#ifdef STATIC_POOL
    size_t i;
#endif

#ifndef STATIC_POOL
    if ( !( stream = malloc ( sizeof ( struct stream_t ) ) ) )
    {
        return NULL;
    }
#else
    for ( i = 0, stream = NULL; i < sizeof ( stream_pool ) / sizeof ( struct stream_t ); i++ )
    {
        if ( !stream_pool[i].allocated )
        {
            stream = &stream_pool[i];
            break;
        }
    }

    if ( !stream )
    {
        return NULL;
    }

    stream->allocated = 1;
#endif

    stream->role = role;
    stream->fd = sock;
    stream->neighbour = NULL;
    stream->level = LEVEL_NONE;
    stream->prev = NULL;
    stream->next = stream_list;

    if ( stream_list )
    {
        stream_list->prev = stream;
    }
    stream_list = stream;

    poll_modified = 1;
    return stream;
}

/**
 * Bind address to socket and set listen mode
 */
static int listen_socket ( unsigned int addr, unsigned short port, int role )
{
    int sock;
    int yes = 1;
    struct sockaddr_in saddr;
    struct stream_t *stream;

    /* Prepare socket address */
    memset ( &saddr, '\0', sizeof ( saddr ) );
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = addr;
    saddr.sin_port = htons ( port );

    /* Allocate socket */
    if ( ( sock = socket ( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        return -1;
    }

    /* Allow reusing socket address */
    if ( setsockopt ( sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof ( yes ) ) < 0 )
    {
        close ( sock );
        return -1;
    }

    /* Bind socket to address */
    if ( bind ( sock, ( struct sockaddr * ) &saddr, sizeof ( saddr ) ) < 0 )
    {
        close ( sock );
        return -1;
    }

    /* Put socket into listen mode */
    if ( listen ( sock, LISTEN_BACKLOG ) < 0 )
    {
        close ( sock );
        return -1;
    }

    /* Allocate new stream */
    if ( !( stream = insert_stream ( role, sock ) ) )
    {
        close ( sock );
        return -1;
    }

    return sock;
}

/**
 * Show relations statistics
 */
#ifndef SILENT_MODE
static void show_stats ( void )
{
    int assoc_count = 0;
    int total_count = 0;
    struct stream_t *iter = stream_list;

    while ( iter )
    {
        if ( iter->role == S_PORT_A || iter->role == S_PORT_B )
        {
            if ( iter->neighbour )
            {
                assoc_count++;
            }
            total_count++;
        }

        iter = iter->next;
    }

    N ( printf ( "[ella] relations: S %i A %i\n", assoc_count / 2,
            total_count - assoc_count + ( assoc_count / 2 ) ) );
}
#endif

/**
 * Free and remove a single stream from the list
 */
static void remove_stream ( struct stream_t *stream )
{
    FREE_SOCKET ( stream->fd );

    if ( stream == stream_list )
    {
        stream_list = stream->next;
    }

    if ( stream->next )
    {
        stream->next->prev = stream->prev;
    }

    if ( stream->prev )
    {
        stream->prev->next = stream->next;
    }
#ifndef STATIC_POOL
    free ( stream );
#else
    stream->allocated = 0;
#endif
    poll_modified = 1;
}

/*
 * Remove pair of associated streams
 */
static void remove_relation ( struct stream_t *stream )
{
    if ( stream->neighbour )
    {
        remove_stream ( stream->neighbour );
    }
    remove_stream ( stream );
    N ( printf ( "[ella] removed relation.\n" ) );
#ifndef SILENT_MODE
    show_stats (  );
#endif
}

/**
 * Remove all relations
 */
static void empty_rels ( void )
{
    struct stream_t *iter = stream_list;
    struct stream_t *next;

    while ( iter )
    {
        next = iter->next;
        remove_stream ( iter );
        iter = next;
    }

    stream_list = NULL;
}

/**
 * Remove unassociated relations
 */
static void clear_rels ( void )
{
    struct stream_t *iter = stream_list;

    while ( iter )
    {
        if ( !iter->neighbour && ( iter->role == S_PORT_A || iter->role == S_PORT_B ) )
        {
            remove_stream ( iter );
        }
        iter = iter->next;
    }

#ifndef SILENT_MODE
    show_stats (  );
#endif
}

/**
 * Remove all forwarding relations
 */
static void empty_forwarding_rels ( void )
{
    struct stream_t *iter = stream_list;
    struct stream_t *next;

    while ( iter )
    {
        next = iter->next;
        if ( iter->role == S_PORT_A || iter->role == S_PORT_B )
        {
            remove_stream ( iter );
        }
        iter = next;
    }

    N ( printf ( "[ella] reset relations.\n" ) );
}

/**
 * Create a new stream
 */
static int new_stream ( int lfd, int role )
{
    int sock;
    struct stream_t *stream;

    /* Accept incoming connection */
    if ( ( sock = accept ( lfd, NULL, NULL ) ) < 0 )
    {
        return -1;
    }

    /* Allocate new stream */
    if ( !( stream = insert_stream ( role, sock ) ) )
    {
        return -1;
    }

    /* Perform additional actions if needed */
    if ( role == S_PORT_A )
    {
        stream->level = LEVEL_AWAITING;
    }

    return 0;
}

/**
 * Rebuild poll event list
 */
static int poll_rebuild ( void )
{
    struct stream_t *iter = stream_list;

    /* Append file descriptors to the poll list */
    for ( poll_len = 0; iter; iter = iter->next )
    {
        /* Skip port A select on level connect */
        if ( iter->role == S_PORT_A && iter->level == LEVEL_CONNECT )
        {
            iter->polled = 0;
            continue;
        }

        /* Filter POLLOUT if endpoint connect is in progress */
        if ( iter->role == S_PORT_B && iter->level == LEVEL_CONNECT )
        {
            poll_list[poll_len].events = POLLOUT;

        } else if ( poll_list[poll_len].events != POLLIN )
        {
            poll_list[poll_len].events = POLLIN;
        }

        /* Expand poll event list if needed */
        if ( poll_len == poll_size && poll_expand (  ) < 0 )
        {
            return -1;
        }

        /* Append stream fd to poll event list if allowed */
        poll_list[poll_len++].fd = iter->fd;
        iter->polled = 1;
    }

    /* Poll event list is rebuilt by now */
    poll_modified = 0;
    return 0;
}

/**
 * Forward data from one socket to another
 */
static int move_data ( int srcfd, int dstfd )
{
    size_t tot;
    size_t len;
    size_t sum;
    unsigned char buffer[65536];

    /* Receive data from input socket to buffer */
    if ( ( ssize_t ) ( tot = recv ( srcfd, buffer, sizeof ( buffer ), 0 ) ) < 0 )
    {
        return -1;
    }

    /* Abort if receive length is zero */
    if ( !tot )
    {
        errno = EPIPE;
        return -1;
    }

    /* Send complete data from buffer to output socket */
    for ( sum = 0; sum < tot; sum += len )
    {
        if ( ( ssize_t ) ( len = send ( dstfd, buffer + sum, tot - sum, MSG_NOSIGNAL ) ) < 0 )
        {
            return -1;
        }
    }

    return 0;
}

/**
 * Perform HTTPS proxy handshake
 */
#ifdef ENABLE_HTTPS
static int proxy_handshake_https ( char *buffer, unsigned int *addr, unsigned short *port )
{
    char *start_ptr;
    char *end_ptr;
    const char *s_connect = "CONNECT ";

    if ( strstr ( buffer, s_connect ) != buffer )
    {
        return -1;
    }

    start_ptr = buffer + 8;
    end_ptr = start_ptr;

    while ( *end_ptr && *end_ptr != '\x20' && *end_ptr != ':' )
    {
        end_ptr++;
    }

    *end_ptr = '\0';

    /* Resolve hostname */
    if ( nsaddr ( start_ptr, addr ) < 0 )
    {
        return -1;
    }

    /* Use HTTPS forward port number */
    *port = FORWARD_PORT;

    return 0;
}
#endif

/**
 * Perform SOCKS5 proxy handshake
 */
#ifdef ENABLE_SOCKS5
static int proxy_handshake_socks5 ( int sock, unsigned char *buffer, size_t len, size_t size,
    unsigned int *addr, unsigned short *port )
{
    size_t hostlen;
    char hostname[256];

    /* Validate minumal handshake request length */
    if ( len < 3 )
    {
        return -1;
    }

    /* Expect SOCKS5 version */
    if ( buffer[0] != 5 )
    {
        return -1;
    }

    if ( buffer[1] == 1 && buffer[2] == 2 )
    {
        /* Username-Password authentication selected */
        buffer[1] = 2;

        /* Send message remote peer */
        if ( send ( sock, buffer, 2, MSG_NOSIGNAL ) < 0 )
        {
            return -1;
        }

        /* Receive authentication data */
        if ( ( ssize_t ) ( len = recv ( sock, buffer, size, 0 ) ) <= 0 )
        {
            return -1;
        }

        /* Authentication passed */
        buffer[1] = 0;

        /* Send message remote peer */
        if ( send ( sock, buffer, 2, MSG_NOSIGNAL ) < 0 )
        {
            return -1;
        }

    } else
    {
        /* No authentication needed */
        buffer[1] = 0;

        /* Send message remote peer */
        if ( send ( sock, buffer, 2, MSG_NOSIGNAL ) < 0 )
        {
            return -1;
        }
    }

    /* Receive request connect */
    if ( ( ssize_t ) ( len = recv ( sock, buffer, size, 0 ) ) <= 0 )
    {
        return -1;
    }

    /* Expect SOCKS5, TCP/IP stream request */
    if ( len < 8 || buffer[0] != 5 || buffer[1] != 1 || buffer[2] != 0 )
    {
        return -1;
    }

    /* Connect IPv4 if requested */
    if ( buffer[3] == 1 )
    {
        if ( len != 10 )
        {
            return -1;
        }

        memcpy ( addr, buffer + 4, 4 );
        *port = ( ( buffer[8] ) << 8 ) | buffer[9];
        return 0;
    }

    /* Otherwise hostname expected */
    if ( buffer[3] != 3 )
    {
        return -1;
    }

    /* Extract hostname and port numer */
    hostlen = buffer[4];

    /* Valdidate hostname string length */
    if ( len < 7 + hostlen || hostlen >= sizeof ( hostname ) )
    {
        return -1;
    }

    /* Assign port number */
    *port = ( ( buffer[5 + hostlen] ) << 8 ) | buffer[6 + hostlen];

    /* Extract hostname string */
    memcpy ( hostname, buffer + 5, hostlen );
    hostname[hostlen] = '\0';

    /* Resolve hostname */
    if ( nsaddr ( hostname, addr ) < 0 )
    {
        return -1;
    }

    return 0;
}
#endif

/**
 * Send HTTPS proxy success message
 */
#ifdef ENABLE_HTTPS
static int proxy_confirm_https ( int sock )
{
    const char *response = "HTTP/1.1 200 OK\r\n\r\n";

    if ( send ( sock, response, strlen ( response ), MSG_NOSIGNAL ) < 0 )
    {
        close ( sock );
        return -1;
    }

    return 0;
}
#endif

/**
 * Send SOCK5 proxy success message
 */
#ifdef ENABLE_SOCKS5
static int proxy_confirm_socks5 ( int sock )
{
    unsigned char response[] = {
        5,      /* SOCKS5 version */
        0,      /* request granted */
        0,      /* reserved */
        1,      /* address type: IPv4 */
        0,      /* address byte #1 */
        0,      /* address byte #2 */
        0,      /* address byte #3 */
        0,      /* address byte #4 */
        0,      /* port byte #1 */
        0       /* port byte #2 */
    };

    if ( send ( sock, response, sizeof ( response ), MSG_NOSIGNAL ) < 0 )
    {
        close ( sock );
        return -1;
    }

    return 0;
}
#endif

/**
 * Estabilish connection with destination host
 */
static int connect_endpoint ( struct stream_t *stream )
{
    int sock;
    int use_https_proxy;
    ssize_t len;
    unsigned short port = 0;
    long mode = 0;
    struct sockaddr_in saddr;
    struct stream_t *neighbour;
    unsigned int addr;
    char buffer[2048];

    /* Receive connect request content */
    if ( ( ssize_t ) ( len = recv ( stream->fd, buffer, sizeof ( buffer ) - 1, 0 ) ) <= 0 )
    {
        return -1;
    }

    /* Put proxy handshake string delimiter */
    buffer[len] = '\0';

    /* Detect proxy handshake type */
    use_https_proxy = buffer[0] == 'C';

    /* Perform proxy handshake */
    if ( use_https_proxy )
    {
#ifdef ENABLE_HTTPS
        if ( proxy_handshake_https ( buffer, &addr, &port ) < 0 )
        {
            return -1;
        }
#else
        errno = ENOSYS;
        return -1;
#endif
    } else
    {
#ifdef ENABLE_SOCKS5
        if ( proxy_handshake_socks5 ( stream->fd, ( unsigned char * ) buffer, len,
                sizeof ( buffer ), &addr, &port ) < 0 )
        {
            return -1;
        }
#else
        errno = ENOSYS;
        return -1;
#endif
    }

#ifdef ALLOWED_ONLY_PORT
    if (port != ALLOWED_ONLY_PORT) {
        errno = EINVAL;
        return -1;
    }
#endif

    /* Prepare socket address */
    memset ( &saddr, '\0', sizeof ( saddr ) );
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = addr;
    saddr.sin_port = htons ( port );

    /* Allocate new socket */
    if ( ( sock = socket ( AF_INET, SOCK_STREAM, 0 ) ) < 0 )
    {
        return -1;
    }

    /* Set non-blocking mode and connect socket  */
    if ( ( mode = fcntl ( sock, F_GETFL, 0 ) ) < 0
        || fcntl ( sock, F_SETFL, mode | O_NONBLOCK ) < 0 )
    {
        close ( sock );
        return -1;
    }

    /* Add new relation on success */
    if ( connect ( sock, ( struct sockaddr * ) &saddr, sizeof ( struct sockaddr_in ) ) >= 0
        || errno != EINPROGRESS )
    {
        close ( sock );
        return -1;
    }

    /* Send HTTP response to A peer */
    if ( use_https_proxy )
    {
#ifdef ENABLE_HTTPS
        if ( proxy_confirm_https ( stream->fd ) < 0 )
        {
            return -1;
        }
#else
        errno = ENOSYS;
        return -1;
#endif
    } else
    {
#ifdef ENABLE_SOCKS5
        if ( proxy_confirm_socks5 ( stream->fd ) < 0 )
        {
            return -1;
        }
#else
        errno = ENOSYS;
        return -1;
#endif
    }

    /* Allocate new neighbour structure */
    if ( !( neighbour = insert_stream ( S_PORT_B, sock ) ) )
    {
        close ( sock );
        return -1;
    }

    /* Build up a new relation */
    neighbour->neighbour = stream;
    stream->neighbour = neighbour;

    /* Shift relation level */
    neighbour->level = LEVEL_CONNECT;
    stream->level = LEVEL_CONNECT;

    return 0;
}

/**
 * Finish asynchronous connect process
 */
static int complete_connect ( struct stream_t *stream )
{
    int so_error = 0;
    long mode = 0;
    socklen_t len = sizeof ( so_error );

    /* Neighbour must be present */
    if ( !stream->neighbour )
    {
        return -1;
    }

    /* Read and analyze socket error */
    if ( getsockopt ( stream->fd, SOL_SOCKET, SO_ERROR, &so_error, &len ) < 0 || so_error )
    {
        return -1;
    }

    /* Restore blocking mode on socket */
    if ( ( mode = fcntl ( stream->fd, F_GETFL, 0 ) ) < 0
        || fcntl ( stream->fd, F_SETFL, mode & ( ~O_NONBLOCK ) ) < 0 )
    {
        return -1;
    }

    /* Shift relation level */
    stream->level = LEVEL_FORWARD;
    stream->neighbour->level = LEVEL_FORWARD;
    poll_modified = 1;

    return 0;
}

/**
 * Proxy data forward loop
 */
static void poll_loop ( void )
{
    int status;
    size_t pos = 0;
    struct stream_t *stream;

    /* Rebuild poll event list if needed */
    if ( poll_modified && poll_rebuild (  ) < 0 )
    {
        N ( printf ( "[ella] rebuild failed: %i\n", errno ) );
        exit_flag = 1;
        return;
    }

    /* Find streams ready to be read */
    if ( ( status = poll ( poll_list, poll_len, POLL_TIMEOUT_MSEC ) ) < 0 )
    {
        N ( printf ( "[ella] poll failed: %i\n", errno ) );
        exit_flag = 1;
        return;
    }

    /* Clear unassociated relations on timeout */
    if ( !status )
    {
        clear_rels (  );
        return;
    }

    /* Forward data or accept peers if needed */
    for ( stream = stream_list; stream; stream = stream->next )
    {
        if ( !stream->polled )
        {
            continue;
        }

        if ( poll_list[pos].revents & ( POLLHUP | POLLERR ) )
        {
            remove_relation ( stream );
            return;
        }

        if ( stream->level == LEVEL_CONNECT && ( poll_list[pos].revents & POLLOUT )
            && stream->role == S_PORT_B )
        {
            if ( complete_connect ( stream ) < 0 )
            {
                remove_relation ( stream );
            }
            return;
        }

        if ( ~poll_list[pos++].revents & POLLIN )
        {
            continue;
        }

        if ( stream->role == L_ACCEPT )
        {
            if ( new_stream ( stream->fd, S_PORT_A ) < 0 )
            {
                empty_forwarding_rels (  );
                return;
            }

        } else if ( stream->role == S_PORT_A && stream->level == LEVEL_AWAITING )
        {
            if ( connect_endpoint ( stream ) < 0 )
            {
                remove_relation ( stream );
                return;
            }

        } else if ( stream->level == LEVEL_FORWARD && stream->neighbour )
        {
            if ( move_data ( stream->fd, stream->neighbour->fd ) < 0 )
            {
                remove_relation ( stream );
                return;
            }
        }
    }
}

/**
 * Proxy task entry point
 */
int proxy_task ( const struct ella_params_t *params )
{
    size_t i;

    /* Setup listen sockets */
    if ( listen_socket ( params->addr, params->port, L_ACCEPT ) < 0 )
    {
        N ( printf ( "[ella] allocation failed: %i\n", errno ) );
        return -1;
    }

    /* Allocate poll event list */
#ifndef STATIC_POOL
    if ( !( poll_list = ( struct pollfd * ) malloc ( poll_size * sizeof ( struct pollfd ) ) ) )
    {
        return -1;
    }
#endif

    /* Prepare poll event list */
    for ( i = 0; i < poll_size; i++ )
    {
        poll_list[i].events = POLLIN;
    }

    N ( printf ( "[ella] allocation done.\n" ) );

    /* Perform select loop until exit flag is set */
    while ( !exit_flag )
    {
        poll_loop (  );
    }

    /* Remove all relations */
    empty_rels (  );

    /* Free poll event list */
#ifndef STATIC_POOL
    free ( poll_list );
#endif

    N ( printf ( "[ella] free done.\n" ) );

    return 0;
}
