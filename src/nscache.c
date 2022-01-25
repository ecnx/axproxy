/* ------------------------------------------------------------------
 * AxProxy - Name server cache
 * ------------------------------------------------------------------ */

#include "axproxy.h"

/**
 * Name server cache config
 */
#define CACHE_NAME_LENGTH 32
#define CACHE_RECORDS_LIMIT 1024

/**
 * Name server cache record structure
 */
struct ns_record_t
{
    unsigned int addr;
    time_t expiry;
    char name[CACHE_NAME_LENGTH];
};

/**
 * Name server cache structure
 */
struct ns_cache_t
{
    struct ns_record_t records[CACHE_RECORDS_LIMIT];
};

/**
 * Name server cache variable
 */
static struct ns_cache_t ns_cache;

/**
 * Resolve hostname into IPv4 address
 */
int nsaddr_cached ( const char *hostname, unsigned int *addr )
{
    size_t i;
    size_t len;
    size_t count;
    time_t now;
    struct ns_record_t *record;
    struct ns_record_t *update;

    if ( inet_pton ( AF_INET, hostname, addr ) > 0 )
    {
        return 0;
    }

    len = strlen ( hostname );

    if ( len >= sizeof ( ns_cache.records[0].name ) )
    {
        return nsaddr ( hostname, addr );
    }

    now = time ( NULL );
    count = sizeof ( ns_cache.records ) / sizeof ( struct ns_record_t );

    for ( i = 0; i < count; i++ )
    {
        record = ns_cache.records + i;

        if ( record->expiry > now )
        {
            if ( !strcmp ( hostname, record->name ) )
            {
                *addr = record->addr;
                return 0;
            }
        }
    }

    if ( nsaddr ( hostname, addr ) < 0 )
    {
        return -1;
    }

    update = NULL;

    for ( i = 0; i < count; i++ )
    {
        record = ns_cache.records + i;

        if ( record->expiry <= now )
        {
            update = record;
            break;
        }
    }

    if ( !update )
    {
        update = ns_cache.records + ( now % count );
    }

    memcpy ( update->name, hostname, len + 1 );
    update->expiry = now + 900;
    update->addr = *addr;

    return 0;
}
