#ifndef PACKET_H_
#define PACKET_H_

#include "common.h"

typedef enum _packet_state_t{
    PKT_STATE_FULL,// packet is full, need allocate new packet.
    PKT_STATE_ALLOC_FAIL, //allocate new packet fail
    PKT_STATE_DISORDER, //packet state including disorder unknown.
    PKT_STATE_CLIENT_ERR,//read socket return error, but not EAGAIN
    PKT_STATE_TRY_AGAIN, //read socket return EAGAIN, no data to read, try again later.
    PKT_STATE_CLIENT_CLOSE//read socket return 0, client close socket.
} packet_state_t;
/**
 * return packet available data size.
 */
#define get_pkt_avail(pkt) ((pkt)->used_size - (pkt)->parse_pos)
/**
 * return packet free buffer size
 */
#define get_pkt_free(pkt) (PACKET_BUF_SIZE - (pkt)->used_size)
/**
 * return the packet write start address
 */
#define pkt_write_start_addr(pkt) ((pkt)->buf + (pkt)->used_size)

/**
 * return the packet read start address
 */
#define pkt_read_start_addr(pkt) ((pkt)->buf + (pkt)->parse_pos)
/**
 * recycle the target used packet.
 */
PUBLIC void recycle_packet(packet_t *pkt);

/**
 * recycle the target used packet.
 */
PUBLIC void recycle_all_packet(client_t *client);

/**
 * allocate a new packet zeroed.
 */
PUBLIC packet_t* new_packet();

/**
 * allocate a new packet zeroed.
 */
PUBLIC packet_t* append_new_packet(client_t *client);

/**
 * if packet is in the list, remove it.
 * return the next one.
 */
PUBLIC packet_t *remove_head_packet(packet_t *pkt);

/**
 * read socket data to packet.
 */
PUBLIC packet_state_t write_to_packet(client_t *client);

/**
 * if packet used up, return 1, else return 0;
 */
PUBLIC int packet_full(packet_t *pkt);


#endif /* PACKET_H_ */
