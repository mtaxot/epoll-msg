#include <pthread.h>
#include "common.h"
#include "message.h"
#include "connection.h"

//for memory leak check.
int malloc_msg = 0;
int free_msg = 0;
int malloc_pkt = 0;
int free_pkt = 0;
pthread_mutex_t statistic_lock = PTHREAD_MUTEX_INITIALIZER;


PUBLIC checksum_t calc_checksum(char *head, int len){
    unsigned int acc = 0;
    checksum_t src;
    unsigned char *octetptr = (unsigned char*)head;
    //octetptr = (unsigned char*)dataptr;
    while (len > 1) {
      src = (*octetptr) << 8;
      octetptr++;
      src |= (*octetptr);
      octetptr++;
      acc += src;
      len -= 2;
    }
    if (len > 0) {
      src = (*octetptr) << 8;
      acc += src;
    }

    acc = (acc >> 16) + (acc & (unsigned int)0x0000ffff);
    if ((acc & (unsigned int)0xffff0000) != 0) {
      acc = (acc >> 16) + (acc & (unsigned int)0x0000ffff);
    }

    src = (checksum_t)acc;
    return ~src;
}

PUBLIC
msg_t *new_message(int msg_len){
    pthread_mutex_lock(&statistic_lock);
    if(msg_len <= 0){
        return NULL;
    }
    msg_t *p = (msg_t*)malloc(sizeof(msg_t));
    if(p != NULL){
        memset(p, 0, sizeof(msg_t));
        p->buffer = (char*)malloc(msg_len);
        if(p->buffer == NULL){
            free(p);
            p = NULL;
        }else{
            memset(p->buffer, 0, msg_len);
            malloc_msg++;
        }
    }
    pthread_mutex_unlock(&statistic_lock);
    return p;
}


PUBLIC
void recycle_msg(msg_t *msg){
    //if(msg->next == RECYCLED)
    pthread_mutex_lock(&statistic_lock);
    if(msg != NULL){
        free(msg->buffer);
        free(msg);

        free_msg++;
    }
    pthread_mutex_unlock(&statistic_lock);
}

PUBLIC
checksum_t mk_header_checksum(msg_header_t *header){
    //sum crc.
    msg_header_t hdr;
    memcpy (&hdr, header, sizeof(msg_header_t));
    hdr.header_cksm = 0;
    return calc_checksum((char*)&hdr, sizeof(msg_header_t));
}

PUBLIC checksum_t mk_body_checksum(msg_t *msg){
    char *body = msg->buffer + sizeof(msg_header_t);
    return calc_checksum(body, msg->msg_len - sizeof(msg_header_t));
}

PUBLIC
parse_result_t validate_header(msg_header_t *hdr, int *body_len){

    if(hdr->start_code != START_CODE){
        return PARSE_MSG_HDR_INVALID_GUIDE;
    }
    if(hdr->header_cksm != mk_header_checksum(hdr)){
        return PARSE_MSG_HDR_CRC_FAIL;
    }
    if(hdr->body_len < 0 || hdr->body_len > BODY_AND_VERIFIER_LEN){
        return PARSE_MSG_HDR_BODYLEN_INVALID;
    }
    *body_len = hdr->body_len;
    return PARSE_MSG_HDR_SUCCESS;
}

PUBLIC
int validate_body(msg_t *msg){
    //int len = msg->msg_len;
    msg_header_t *hdr = (msg_header_t*)msg->buffer;
    int hdr_vrfy = hdr->data_vrfy;
    void *body_vrfy = (void*)(msg->buffer + msg->msg_len - sizeof(hdr->data_vrfy));
    if(*((int*)body_vrfy) != hdr_vrfy){
        return PARSE_MSG_BODY_VERIFIER_INVALID;
    }

    if(mk_body_checksum(msg) != hdr->body_cksm){
        return PARSE_MSG_BODY_CRC_FAIL;
    }

    //check msg body crc. if fail return PARSE_MSG_BODY_CRC_FAIL
    //dump_msg(msg);
    return PARSE_MSG_BODY_SUCCESS;
}

PUBLIC
parse_result_t check_msg_header_strict(packet_t *pkt, int *body_len){
    if(pkt == NULL){
        //return PARSE_MSG_HDR_NO_PKT;
        return PARSE_MSG_HDR_NEED_MORE_DATA;
    }
    int avail_size = get_pkt_avail(pkt);
    //printf("available size = %d\n", avail_size);
    if(avail_size >= sizeof(msg_header_t)){
        //message header in 1 packet
        msg_header_t *hdr = (msg_header_t*)(pkt->buf + pkt->parse_pos);
        return validate_header(hdr, body_len);
    }else{
        //message header span 2 packet.
        if(pkt->next != NULL){
            msg_header_t tmp_hdr;
            memset(&tmp_hdr, 0, sizeof(tmp_hdr));
            int needs = sizeof(msg_header_t) - avail_size;
            int next_avail_size = get_pkt_avail(pkt->next);
            if(next_avail_size >= needs){
                //copy and don't move pkt's parse_pos.
                memcpy(&tmp_hdr, pkt->buf + pkt->parse_pos, avail_size);
                memcpy(((void*)&tmp_hdr)/*must conv*/ + avail_size,
                        pkt->next->buf + pkt->next->parse_pos, needs);
                return validate_header(&tmp_hdr, body_len);
                //如果消息头是正确的，那么通过body_len 就能计算出需要拷贝的消息长度
                //同时，我们一定能从packet中拷贝出消息头
            }else{
                return PARSE_MSG_HDR_NEED_MORE_DATA;
            }
        }else{
            return PARSE_MSG_HDR_NEED_MORE_DATA;
        }
    }
}


PRIVATE
parse_result_t copy_packet_with_recovery(packet_t *list, msg_t *msg_pending){
    //there must be a full len msg. so we don't need to worry
    //about that packet is not enough
    //copy and please don't move the parse_pos
    //if validate msg success, we move parse_pos at one time.
    //and also release packet.
    int need_len = msg_pending->msg_len;
    parse_result_t state = PARSE_MSG_BODY_NEED_MORE_DATA;
    client_t *client = msg_pending->client;
    if(client->buf_avail_bytes < need_len){
        //buf data not enough
        return PARSE_MSG_BODY_NEED_MORE_DATA;
    }

    assert(list != NULL);
    //do copy with no parse_pos move.
    int current_parse_pos = list->parse_pos;
    packet_t *current_packet = list;
    //printf ("current packet addr = %ld\n", (unsigned long)current_packet);
    while(need_len > 0){
        int avail_len = current_packet->used_size - current_parse_pos;
        if(avail_len >= need_len){
            //copy need len and break.
            memcpy(msg_pending->buffer + msg_pending->pos,
                   current_packet->buf + current_parse_pos,
                   need_len);
            msg_pending->pos += need_len;
            current_parse_pos += need_len;
            need_len = 0;
            //message ready, check msg.
            //if ready really, release packet and consume the buffer.
            int vbres = validate_body(msg_pending);
            if(vbres == PARSE_MSG_BODY_SUCCESS){
                //consume the buffer at one time.
                client->buf_avail_bytes -= client->msg_pending->msg_len;
                message_ready(client);
                //recycle packet.
                while(list != current_packet){
                    packet_t *to_free = list;
                    list = list->next;
                    recycle_packet(to_free);
                }
                current_packet->prev = NULL;
                client->pkt_head = current_packet;
                current_packet->parse_pos = current_parse_pos;
                return PARSE_MSG_READY_TO_DELIVER;
            }else{
                // body crc fail or body verifier fail.
                return vbres;
            }
        }else{
            //copy available and move next packet.
            memcpy(msg_pending->buffer + msg_pending->pos,
                   current_packet->buf + current_parse_pos,
                   avail_len);
            msg_pending->pos += avail_len;
            need_len -= avail_len;
            current_parse_pos = 0;
            current_packet = current_packet->next;

        }
    }
    return state;
}


PRIVATE
int drop_one_byte(client_t *client){
    //if drop success return 1 else return 0
    packet_t *list = client->pkt_head;
    if(list == NULL){
        return 0;
    }
    int avail_size = get_pkt_avail(list);//available bytes.
    if(avail_size > 0){
        list->parse_pos++;
        client->buf_avail_bytes--;
        return 1;
    }else if(avail_size == 0){
        if(list->parse_pos == sizeof(list->buf)){
            packet_t *to_free = list;
            list = remove_head_packet(to_free);//return next
            client->pkt_head = list;
            //release packet.
            recycle_packet(to_free);
            if(list != NULL && list->parse_pos < list->used_size){
                return 1;
            }else{
                return 0;
            }
        }else{
            //this buffer is not fully used, then next buffer must have no data.
            //drop nothing.
            return 0;
        }
    }
    return 0;
}

PRIVATE
parse_result_t probe_message_header(client_t *client){
    int rres, i;
    int body_len = -1;
    int max_msg_len = BODY_AND_VERIFIER_LEN + sizeof(msg_header_t);    
    // if user do right work, then within max_msg_len,
    // there must be a start code.
    for(i = 0; i <= max_msg_len; i++){
        rres = check_msg_header_strict(client->pkt_head, &body_len);
        if(rres == PARSE_MSG_HDR_SUCCESS){
            //printf("################drop sucdc######\n");
            int msg_len = sizeof(msg_header_t) + body_len;
            msg_t *msg_pending = new_message(msg_len);
            if(msg_pending != NULL){
                client->msg_pending = msg_pending;
                msg_pending->client = client;
                msg_pending->msg_len = msg_len;
            }else{
                return PARSE_MSG_COPY_NO_MEMORY;
            }
            break;
        }else{
            //we drop 1 bytes from packet to find next start code.
            if(drop_one_byte(client)){
                continue;
            }else{
                rres = PARSE_MSG_HDR_NEED_MORE_DATA;
                break;
            }
        }
    }
    if(i == max_msg_len + 1 && rres != PARSE_MSG_HDR_SUCCESS){
        return PARSE_MSG_HDR_FAIL;
    }
    return rres;
}

PUBLIC
parse_result_t parse_message(client_t *client){
    if(client->msg_pending == NULL){
        /**
         * probe must return PARSE_MSG_HDR_FAIL PARSE_MSG_HDR_NEED_MORE_DATA
         * or PARSE_MSG_HDR_SUCCESS
         */
        int pres = probe_message_header (client);

        assert(pres != PARSE_MSG_HDR_INVALID_GUIDE &&
                pres != PARSE_MSG_HDR_CRC_FAIL &&
                pres != PARSE_MSG_HDR_BODYLEN_INVALID);

        if(pres != PARSE_MSG_HDR_SUCCESS){
            return pres;
        }
    }
    //header success, continue parse
    //now pending msg has value
    int cpres = copy_packet_with_recovery(
                client->pkt_head,
                client->msg_pending);
    if(cpres == PARSE_MSG_BODY_CRC_FAIL ||
            cpres == PARSE_MSG_BODY_VERIFIER_INVALID){
        recycle_msg(client->msg_pending);
        client->msg_pending = NULL;
        if(drop_one_byte (client)){
            return PARSE_MSG_TRY_AGAIN;
        }else{
            return PARSE_MSG_BODY_NEED_MORE_DATA;
        }
    }else{
        return cpres;//ready to deliver, need more data.
    }
}


PRIVATE
void message_ready(client_t *client){
    queue_msg_r(client, client->msg_pending);
    client->msg_pending = NULL;
}

PUBLIC
int validate_msg_timeinterval(client_t *client){
    struct timeval tv;
    if(gettimeofday(&tv, NULL) >= 0){
        int64_t tms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
        if(tms - client->last_msg_time > MSG_INTERVAL_MILLIS){
            client->last_msg_time = tms;//ms
            return 0;
        }else{
            printf("warning , attack !!\n");
            return -1;
        }
    }else{
        perror("gettimeofday(&ready, NULL); fail");
        return -1;
    }
}

PUBLIC
void handle_undelivered_msg(client_t *client){
    //here we simplely discard
    //discard pending.
    if(client->msg_pending != NULL){
        recycle_msg(client->msg_pending);
    }
    //discard msgQ_ready, inface we should continue deliver.
    msg_t * msgr = client->msgQ_ready;
    while(msgr != NULL){
        msg_t *to_free = msgr;
        msgr = msgr->next;
        recycle_msg(to_free);
    }
    client->msgQ_ready = NULL;

    // discard msgQ_to_write, infact we should store and deliver in future.
    msg_t * msgw = client->msgQ_to_write;
    while(msgw != NULL){
        msg_t *to_free = msgw;
        msgw = msgw->next;
        recycle_msg(to_free);
    }
    client->msgQ_to_write = NULL;
}

PUBLIC
msg_t *make_string_message(char *msg, int msg_len){
    if(msg == NULL || msg_len < 0) {
        return NULL;
    }
    msg_header_t hdr;
    hdr.body_len = msg_len + sizeof(hdr.data_vrfy);
    hdr.start_code = START_CODE;
    //hdr.body_cksm = 0;
    //hdr.header_cksm = 0;
    hdr.data_vrfy = rand();
    int full_len = sizeof(msg_header_t) + hdr.body_len;
    msg_t *msgbit = new_message(full_len);
    int pos = 0;
    memcpy(msgbit->buffer + pos, &hdr, sizeof(msg_header_t));
    pos += sizeof(msg_header_t);
    memcpy(msgbit->buffer + pos, msg, msg_len);
    pos += msg_len;
    memcpy(msgbit->buffer + pos, (char*)&(hdr.data_vrfy),
           sizeof(hdr.data_vrfy));
    msgbit->msg_len = full_len;
    //fill header checksum
    msg_header_t *okhdr = (msg_header_t*)msgbit->buffer;
    okhdr->body_cksm = mk_body_checksum(msgbit);
    okhdr->header_cksm = mk_header_checksum(okhdr);
    return msgbit;
}


PUBLIC
int make_fixlen_message(msg_t *msg, int msg_len){
    if(msg == NULL || msg_len < 0) {
        return -1;
    }
    if(msg->buffer == NULL){
        return -1;
    }
    msg_header_t hdr;
    hdr.body_len = msg_len + sizeof(hdr.data_vrfy);;
    hdr.start_code = START_CODE;
    //hdr.body_cksm = 0;
    //hdr.header_cksm = 0;
    hdr.data_vrfy = rand();
    int full_len = sizeof(msg_header_t) + hdr.body_len;
    int pos = 0;

    memcpy(msg->buffer + pos, &hdr, sizeof(msg_header_t));

    pos += sizeof(msg_header_t);
    pos += msg_len;

    memcpy(msg->buffer + pos, (char*)&(hdr.data_vrfy), sizeof(hdr.data_vrfy));
    msg->msg_len = full_len;
    msg_header_t *okhdr = (msg_header_t*)msg->buffer;
    okhdr->body_cksm = mk_body_checksum(msg);
    okhdr->header_cksm = mk_header_checksum(okhdr);
    return 0;
}

PUBLIC
void dump_msg(msg_t *msg){
    msg_header_t *hdr = (msg_header_t*)msg->buffer;
    printf("----------------------------------------\n");
    printf("guide code should be %x\n", START_CODE);

    printf("guide code:%x\n", hdr->start_code);
    printf("data crc:%d\n", hdr->body_cksm);
    printf("header crc:%d\n", hdr->header_cksm);
    printf("body len:%d\n", hdr->body_len);
    printf("body verifier:%d\n", hdr->data_vrfy);
    printf("msg len:%d\n", msg->msg_len);
}

PUBLIC void queue_msg_r(client_t *client, msg_t *msg){
    assert(client != NULL && msg != NULL);
    msg_t *tail = client->msgQ_ready_tail;
    if(tail != NULL){
        assert(client->msgQ_ready != NULL);
        tail->next = msg;
        msg->prev = tail;
        client->msgQ_ready_tail = msg;
        //msg->next = NULL;
    }else{
        assert(client->msgQ_ready == NULL);
        client->msgQ_ready_tail = msg;
        client->msgQ_ready = msg;
        msg->prev = NULL;
        //msg->next = NULL;
    }
    msg->next = NULL;

}

PUBLIC
msg_t *pull_head_msg_r(client_t *client){
    assert(client != NULL);
    msg_t *head = client->msgQ_ready;
    if(head != NULL){
        client->msgQ_ready = head->next;
        if(client->msgQ_ready != NULL){
            client->msgQ_ready->prev = NULL;
        }else{
            client->msgQ_ready_tail = NULL;
        }
        head->next = NULL;
        head->prev = NULL;
    }else{
        assert(client->msgQ_ready_tail == NULL);
    }
    return head;
}

PUBLIC msg_t *pull_tail_msg_r(client_t *client){
    if(client == NULL){
        return NULL;
    }
    msg_t *tail = client->msgQ_ready_tail;
    if(tail != NULL){
        client->msgQ_ready_tail = tail->prev;
        if(client->msgQ_ready_tail != NULL){
            client->msgQ_ready_tail->next = NULL;
        }else{
            client->msgQ_ready = NULL;
        }
        tail->prev = NULL;
        tail->next = NULL;
    }else{
        assert(client->msgQ_ready == NULL);
    }
    return tail;
}












