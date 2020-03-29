/* ------------------------------------------------------------------
 * Ax-Proxy - Main Program File
 * ------------------------------------------------------------------ */

#include "ella.h"

/**
 * Show Ella Proxy program usage message
 */
static void show_usage ( void )
{
    N ( printf ( "[ella] usage: axproxy s-addr l-port\n"
            "\n" "       s-addr       Self address\n" "       l-port       Listen port\n" "\n" ) );
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    unsigned int port = 0;
    pid_t pid;
    int status;
    struct ella_params_t params;

    /* Show program version */
    N ( printf ( "[ella] Ella Proxy - ver. " ELLA_VERSION " [axproxy]\n" ) );

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }
#ifdef SILENT_MODE
    daemon ( 0, 0 );
#endif

    if ( inet_pton ( AF_INET, argv[1], &params.addr ) <= 0 )
    {
        show_usage (  );
        return 1;
    }

    if ( sscanf ( argv[2], "%u", &port ) <= 0 || port > 65535 )
    {
        show_usage (  );
        return 1;
    }

    params.port = port;

#ifdef SELF_RESTART_SEC

    /* Periodically create and destory process (ommited on failure). */
    while ( ( pid = fork (  ) ) > 0 )
    {
        sleep ( SELF_RESTART_SEC );
        /* Avoid defunct processes by reading exit status. */
        kill ( pid, SIGKILL );
        waitpid ( pid, &status, 0 );
    }

#endif

    if ( proxy_task ( &params ) < 0 )
    {
        N ( printf ( "[ella] exit status: %i\n", errno ) );
        return 1;
    }

    N ( printf ( "[ella] exit status: success\n" ) );
    return 0;
}
