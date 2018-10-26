#ifndef CONN_H_
#define CONN_H_
#include "common.h"
#include "message.h"
#include "packet.h"

typedef struct _msg_parser_t {
    parse_result_t (*do_parse)(client_t *); /* parse packet list to discover message. */
    packet_state_t (*do_write_packet)(client_t *client); /*read socket and fill packet buffer. */
} msg_parser_t;

typedef enum _client_status_t{
    CLIENT_UNSTARTED,
    CLIENT_CLOSED
} client_status_t;

typedef struct _client_t {
    int fd;
    char identifier[UNIQUE_ID_SIZE];
    msg_parser_t *parser;//no need to free parser, see function "create_new_client"
    //buffer
    int packet_num;//must not exceed max num.
    int buf_avail_bytes;
    long int msg_nums;//for statistic
    long int bytes; //for flow control
    //msg must not be too frequent, for server safety.MSG_INTERVAL_MILLIS
    int64_t last_msg_time;
    int max_msg_fail_count;
    client_status_t status;

    packet_t *pkt_head;
    packet_t *pkt_tail;
    //msg
    msg_t *msg_pending;//currently reading.

    msg_t *msgQ_ready;
    msg_t *msgQ_ready_tail;

    msg_t *msgQ_to_write;
    msg_t *msgQ_to_write_tail;
} client_t;

PUBLIC client_t *create_new_client(int fd);

PUBLIC int _atom_accept_(int fd, struct sockaddr *addr, socklen_t *len);

PUBLIC int _atom_close_(int fd);

PUBLIC void add_event(client_t *client, int epollfd, int event);

PUBLIC void delete_event(client_t *client, int epollfd, int event);

PUBLIC void close_client(client_t *client);

PUBLIC void setnonblock(int sock);

PUBLIC void add_event(client_t *client, int epollfd, int event);
PUBLIC int socket_bind(char *ip, int port);

#endif /* CONN_H_ */
