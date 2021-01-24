/* ------------------------------------------------------------------
 * AxProxy - Name Server Cache
 * ------------------------------------------------------------------ */

#include "axproxy.h"

/**
 * NS cache config
 */
#define CACHE_NAME_LENGTH 32
#define CACHE_RECORDS_LIMIT 1024

/**
 * NS cache variables
 */
static unsigned int cached_addrs[CACHE_RECORDS_LIMIT];
static unsigned char cached_names[CACHE_NAME_LENGTH * CACHE_RECORDS_LIMIT];
static size_t cache_len = 0;

/**
 * Resolve hostname into IPv4 address
 */
int nsaddr_cached ( const char *hostname, unsigned int *addr )
{
    int ret;
    size_t i;
    size_t len;
    char name[CACHE_NAME_LENGTH];

    if ( inet_pton ( AF_INET, hostname, addr ) > 0 )
    {
        return 0;
    }

    len = strlen ( hostname );
    if ( len >= sizeof ( name ) )
    {
        return nsaddr ( hostname, addr );
    }
    memset ( name, '\0', sizeof ( name ) );
    memcpy ( name, hostname, len );

    for ( i = 0; i < cache_len; i++ )
    {
        if ( !memcmp ( cached_names + i * CACHE_NAME_LENGTH, name, CACHE_NAME_LENGTH ) )
        {
            if (!(*addr = cached_addrs[i]))
            {
                return -1;
            }
            return 0;
        }
    }

    if ((ret = nsaddr ( hostname, addr )) < 0)
    {
        *addr = 0;
    }
    
    memcpy ( cached_names + cache_len * CACHE_NAME_LENGTH, name, CACHE_NAME_LENGTH );
    cached_addrs[cache_len] = *addr;
    cache_len++;
    cache_len %= CACHE_RECORDS_LIMIT;

    return ret;
}
