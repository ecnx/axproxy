/* ------------------------------------------------------------------
 * Ax-Proxy - Config Header
 * ------------------------------------------------------------------ */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef ELLA_CONFIG_H
#define ELLA_CONFIG_H

#if !defined(ENABLE_HTTPS) && !defined(ENABLE_SOCKS5)
#error "No proxy protocol enabled"
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define FREE_SOCKET(S) \
    if ( S >= 0 ) \
    { \
        shutdown ( S, SHUT_RDWR ); \
        close ( S ); \
        S = -1; \
    }

#ifdef SILENT_MODE
#define N(X)
#else
#define N(X) X
#endif

#define SELF_RESTART_SEC 900

#define STATIC_POOL
#define STATIC_POOL_SIZE 256
#define FORWARD_PORT 443

#endif
