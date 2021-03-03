/* ------------------------------------------------------------------
 * AxProxy - Includes and Definitions
 * ------------------------------------------------------------------ */

#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
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

#ifndef MRELAY_DEFS_H
#define MRELAY_DEFS_H

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifdef VERBOSE_MODE
#define N(X) X
#else
#define N(X)
#endif

#define POOL_SIZE 256

#endif
