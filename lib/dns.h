/* ------------------------------------------------------------------
 * LibDNS - Portable DNS Client
 * ------------------------------------------------------------------ */

#include <arpa/inet.h>
#include <stddef.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef DNS_H
#define DNS_H

/**
 * DNS resource records types list
 */
#define T_A         1   /* IPv4 address */
#define T_NS        2   /* Nameserver */
#define T_CNAME     5   /* Canonical name */
#define T_SOA       6   /* Start of authority zone */
#define T_PTR       12  /* Domain name pointer */
#define T_MX        15  /* Mail server */

/**
 * DNS socket timeouts
 */
#define DNS_SEND_TIMEOUT_SEC 3
#define DNS_SEND_TIMEOUT_USEC 0
#define DNS_RECV_TIMEOUT_SEC 3
#define DNS_RECV_TIMEOUT_USEC 0

/**
 * Maximum size of an UDP packet
 */
#define UDP_PKT_LEN_MAX 65536

/**
 * DNS resolve settings
 */
#define DNS_QUERY_LIMIT 48
#define DNS_NAME_SIZE_MAX 256

/**
 * DNS header structure
 */
struct dns_header_t
{
    uint16_t id;                /* identification number */

#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t rd:1;               /* recursion desired */
    uint8_t tc:1;               /* truncated message */
    uint8_t aa:1;               /* authoritive answer */
    uint8_t opcode:4;           /* purpose of message */
    uint8_t qr:1;               /* query/response flag */
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t qr:1;               /* query/response flag */
    uint8_t opcode:4;           /* purpose of message */
    uint8_t aa:1;               /* authoritive answer */
    uint8_t tc:1;               /* truncated message */
    uint8_t rd:1;               /* recursion desired */
#else
#error "Endian not set"
#endif

#if __BYTE_ORDER == __LITTLE_ENDIAN
    uint8_t rcode:4;            /* response code */
    uint8_t cd:1;               /* checking disabled */
    uint8_t ad:1;               /* authenticated data */
    uint8_t z:1;                /* reserved for future use */
    uint8_t ra:1;               /* recursion available */
#elif __BYTE_ORDER == __BIG_ENDIAN
    uint8_t ra:1;               /* recursion available */
    uint8_t z:1;                /* reserved for future use */
    uint8_t ad:1;               /* authenticated data */
    uint8_t cd:1;               /* checking disabled */
    uint8_t rcode:4;            /* response code */
#else
#error "Endian not set"
#endif

    uint16_t q_count;           /* number of question entries */
    uint16_t ans_count;         /* number of answer entries */
    uint16_t auth_count;        /* number of authority entries */
    uint16_t add_count;         /* number of resource entries */
} __attribute__( ( packed ) );

/**
 * DNS query structure
 */
struct dns_question_t
{
    uint16_t qtype;
    uint16_t qclass;
} __attribute__( ( packed ) );

/**
 * DNS answer structure
 */
struct dns_answer_t
{
    /* name */
    uint16_t type;
    uint16_t _class;
    uint32_t ttl;
    uint16_t rd_length;
    /* rdata */
} __attribute__( ( packed ) );

/**
 * Resolve hostname into IPv4 address
 */
extern int nsaddr ( const char *hostname, uint32_t * addr );

#endif
