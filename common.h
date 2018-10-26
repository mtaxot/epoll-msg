/*
 * common.h
 *
 *  Created on: Jul 5, 2018
 *      Author: zsm
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>

#define DEBUG

#define PUBLIC
#define PRIVATE

//#define MAXSIZE     1024
//#define IPADDRESS   "127.0.0.1"
//#define THREAD_NAME_LEN 128
#define ACCEPT_MAX_CONNS_ONCE 128
#define SERV_PORT   8080
#define EPOLLEVENTS 20
#define MSG_INTERVAL_MILLIS 32
#define MAX_FD 65500
#define MAX_EVENTS MAX_FD
#define HOST_IP "127.0.0.1"
#define PORT 8080
#define PACKET_BUF_SIZE (4*1024)
#define UNIQUE_ID_SIZE 32
#define START_CODE ((unsigned int)(0x02010301))
#define BODY_AND_VERIFIER_LEN (256*1024)
#define SUM_BYTE(A_INT) \
        (((0xff) & ((guide_t)(A_INT))) + \
        ((0xff) &(((guide_t)(A_INT)) >> 8))+ \
        ((0xff) &(((guide_t)(A_INT)) >> 16)) + \
        ((0xff) &(((guide_t)(A_INT)) >> 24)))

extern int malloc_msg;
extern int free_msg;
extern int malloc_pkt;
extern int free_pkt;
extern pthread_mutex_t statistic_lock;


typedef struct _msg_t msg_t;
typedef struct _client_t client_t;
typedef struct _packet_t packet_t;
typedef struct _msg_header_t msg_header_t;
typedef struct _msg_body_header_t msg_body_header_t;
typedef struct _msg_parser_t msg_parser_t;
typedef unsigned int msg_id_t;
typedef unsigned int guide_t;
typedef struct _msg_crclen_t msg_crclen_t;
typedef enum _packet_type_t packet_type_t;
typedef unsigned short checksum_t;

packet_type_t parse_packet(msg_t *msg, packet_t *packet);


typedef enum _packet_type_t{
    PKT_NEED_MORE        = 0,      /*need more data*/
    PKT_BEGIN       = 1 << 0, /*begin code + body(>=0)*/
    PKT_BODY        = 1 << 1, /*no begin code, more data please*/
    PKT_DIVIDER     = 1 << 2, /*body(>=0) + begin code + body(>=0)*/
    PKT_BIND_BEGIN  = 1 << 3, /*2 packet, need bind together to get begin code*/
    PKT_BIND_END    = 1 << 4, /*BIND = SHARE*/
    PKT_ERR         = 1 << 5
} packet_type_t;

typedef enum _msg_type_t{
    MSG_
} msg_type_t;

typedef struct _packet_t {
    int parse_pos;
    int used_size;
    packet_t *prev;
    packet_t *next;
    msg_t *msg;
    char buf[PACKET_BUF_SIZE];
} packet_t;

typedef struct _msg_header_t {
    guide_t start_code;
    int body_len;
    checksum_t header_cksm;
    checksum_t body_cksm;
    int data_vrfy;
} msg_header_t;

typedef struct _msg_crclen_t {
    int body_len;
    int header_cksm;
    int body_cksm;
    int data_vrfy;
} msg_crclen_t;

typedef struct _msg_body_header_t {
    char msg_from[UNIQUE_ID_SIZE];
    char msg_to[UNIQUE_ID_SIZE];
    int to_type;//personal or group.
    msg_id_t msg_id;
    int cmd;
} msg_body_header_t;

/** MSG Header:
 *  START CODE 4 bytes   guide 0x02010201
 *  BODY LEN   4 bytes
 *  HEADER CRC 4 bytes
 *  DATA   CRC 4 bytes
 *  VERIFIER   4 bytes
 ** MSG Body:
 *  MSG FROM   32 bytes UNIQUE_ID_SIZE
 *  MSG TO     32 bytes UNIQUE_ID_SIZE
 *  TO TYPE     4 bytes
 *  MSG ID      4 bytes
 *  CMD         4 bytes
 *  REAL DATA   0-512 KB BODY_LEN
 */
typedef struct _msg_t {
    char *buffer;
    int msg_len;//include begin code
    int pos;
    //int next_action;
    client_t *client;
    msg_t *prev;
    msg_t *next;
} msg_t;




#ifdef DEBUG
    #define LOG(format,args...) fprintf(stderr, format, ##args)
#else
    #define LOG(format,args...)
#endif


#endif /* COMMON_H_ */
