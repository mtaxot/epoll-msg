#ifndef MESSAGE_H_
#define MESSAGE_H_
#include "common.h"
#include "packet.h"

typedef enum _parse_result_t{
    PARSE_MSG_HDR_FAIL,
    PARSE_MSG_HDR_SUCCESS, //got message header. no need to handle
    PARSE_MSG_HDR_NO_PKT, //no packet to parse
    PARSE_MSG_HDR_INVALID_GUIDE, //wrong message guide code.
    PARSE_MSG_HDR_NEED_MORE_DATA, //data not enough to find message header.
    PARSE_MSG_HDR_CRC_FAIL,
    PARSE_MSG_HDR_BODYLEN_INVALID,

    PARSE_MSG_READY_TO_DELIVER,
    PARSE_MSG_BODY_NEED_MORE_DATA,
    PARSE_MSG_BODY_VERIFIER_INVALID,
    PARSE_MSG_BODY_SUCCESS,
    PARSE_MSG_COPY_NO_MEMORY,
    PARSE_MSG_BODY_CRC_FAIL,
    PARSE_MSG_TRY_AGAIN /*usually you got wrong msg,drop and parse again*/

} parse_result_t;



/**
 * calculate the checksum of given data.
 */
PUBLIC checksum_t calc_checksum(char *head, int len);

/**
 * move pending message to messageQ ready, and marshal reference.
 */
PRIVATE void message_ready(client_t *client);


/**
 * copy from packet unsafe, you need to ensure copy length.
 */
PRIVATE void copy_buffer_unsafe(msg_t *msg, packet_t *pkt, int len);

/**
 * allocate a new message
 */
PUBLIC msg_t *new_message(int msg_len);

/**
 * release message.
 */
PUBLIC void recycle_msg(msg_t *msg);


/**
 * gen header checksum given header info. but not fill in.
 */
PUBLIC checksum_t mk_header_checksum(msg_header_t *header);

/**
 * gen header checksum given header info. but not fill in.
 */
PUBLIC checksum_t mk_body_checksum(msg_t *msg);

/**
 * validate a message body,
 */
PUBLIC int validate_body(msg_t *msg);

/**
 * validate a message header,
 * if return PARSE_MSG_HDR_SUCCESS body length will be fill in body_len
 */
PUBLIC parse_result_t validate_header(msg_header_t *hdr, int *body_len);

/**
 * parse client message given packets, it has different from strict mode.
 * it allows uncomplete data in buffer, we just ignore it and find start code
 * to parse.
 */
PUBLIC parse_result_t parse_message(client_t *client);

/**
 * check strictly, must assume packet buffer begin with start code,
 *  or return PARSE_MSG_HDR_INVALID_GUIDE
 */
PUBLIC parse_result_t check_msg_header_strict(packet_t *pkt, int *body_len);


/**
 * copy packet buffer data to pending message,
 * if return PARSE_MSG_READY_TO_DELIVER,
 * then it indicate message is ready ,you could do next action.
 * message has valid header info
 */
PRIVATE parse_result_t copy_packet(packet_t *list, msg_t *msg_pending);


/**
 * when close socket fd, we should tackle these message.
 * to store, to deliver, or just discard.
 */
PUBLIC void handle_undelivered_msg(client_t *client);


/**
 * put msg into message binary
 */
PUBLIC msg_t *make_string_message(char *msg, int msg_len);

/**
 * make a fix len message with a fix buffer in msg.
 * body_len is not include verifier.
 */
PUBLIC int make_fixlen_message(msg_t *msg, int body_len);

PUBLIC void dump_msg(msg_t *msg);

/**
  * insert msg at tail.
 */
PUBLIC void queue_msg_r(client_t *client, msg_t *msg);

/**
 * pull msg from head.
 */
PUBLIC msg_t *pull_head_msg_r(client_t *client);

/**
 * pull msg from tail.
 */
PUBLIC msg_t *pull_tail_msg_r(client_t *client);



PUBLIC int validate_msg_timeinterval(client_t *client);














#endif /* MESSAGE_H_ */
