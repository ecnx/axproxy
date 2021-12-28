/* ------------------------------------------------------------------
 * AxProxy - Main Program File
 * ------------------------------------------------------------------ */

#include "axproxy.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    N ( printf ( "[axpr] usage: axproxy addr:port\n" ) );
}

/**
 * Decode ip address and port number
 */
static int ip_port_decode ( const char *input, unsigned int *addr, unsigned short *port )
{
    unsigned int lport;
    size_t len;
    const char *ptr;
    char buffer[32];

    /* Find port number separator */
    if ( !( ptr = strchr ( input, ':' ) ) )
    {
        return -1;
    }

    /* Validate destination buffer size */
    if ( ( len = ptr - input ) >= sizeof ( buffer ) )
    {
        return -1;
    }

    /* Save address string */
    memcpy ( buffer, input, len );
    buffer[len] = '\0';

    /* Parse IP address */
    if ( inet_pton ( AF_INET, buffer, addr ) <= 0 )
    {
        return -1;
    }

    ptr++;

    /* Parse port b number */
    if ( sscanf ( ptr, "%u", &lport ) <= 0 || lport > 65535 )
    {
        return -1;
    }

    *port = lport;
    return 0;
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    struct proxy_t proxy;

    /* Show program version */
    N ( printf ( "[axpr] AxProxy - ver. " AXPROXY_VERSION "\n" ) );

    /* Validate arguments count */
    if ( argc != 2 )
    {
        show_usage (  );
        return 1;
    }
#ifndef VERBOSE_MODE
    if ( daemon ( 0, 0 ) < 0 )
    {
        return -1;
    }
#endif

    if ( ip_port_decode ( argv[1], &proxy.addr, &proxy.port ) < 0 )
    {
        show_usage (  );
        return 1;
    }

    for ( ;; )
    {
        if ( proxy_task ( &proxy ) < 0 )
        {
            if ( errno == EINTR || errno == ENOTCONN )
            {
                N ( printf ( "[axpr] retrying in 1 sec...\n" ) );
                sleep ( 1 );

            } else
            {
                N ( printf ( "[axpr] exit status: %i\n", errno ) );
                return 1;
            }
        }
    }

    N ( printf ( "[axpr] exit status: success\n" ) );
    return 0;
}
