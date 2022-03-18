/* ------------------------------------------------------------------
 * AxProxy - Includes and Definitions
 * ------------------------------------------------------------------ */

#ifndef AXPROXY_DEFS_H
#define AXPROXY_DEFS_H

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef VERBOSE_MODE
#define V(X) X
#else
#define V(X)
#endif

#endif
