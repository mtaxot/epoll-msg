#include <pthread.h>
#include "packet.h"
#include "common.h"
#include "connection.h"

PUBLIC
packet_t* new_packet(){
    pthread_mutex_lock(&statistic_lock);
    packet_t *p = (packet_t*)malloc(sizeof(packet_t));
    if(p != NULL){
        memset(p, 0, sizeof(packet_t));
        malloc_pkt++;
    }
    pthread_mutex_unlock(&statistic_lock);
    return p;
}

PUBLIC packet_t* append_new_packet(client_t *client){
    packet_t *p = new_packet();
    if(p != NULL){
        //append
        if(client->pkt_tail != NULL){
            client->pkt_tail->next = p;
            p->prev = client->pkt_tail;
            client->pkt_tail = p;
        }else{
            client->pkt_tail = p;
            client->pkt_head = p;
        }
        return p;
    }else{
        return NULL;
    }
}

PUBLIC void recycle_all_packet(client_t *client){

    packet_t *pkt = client->pkt_head;
    while(pkt != NULL){
        packet_t *to_free = pkt;
        pkt = pkt->next;
        recycle_packet(to_free);
    }
}

PUBLIC
void recycle_packet(packet_t *pkt){
    //printf ("recycle packet addr = %ld\n", (unsigned long)pkt);
    pthread_mutex_lock(&statistic_lock);
    if(pkt != NULL){
        free(pkt);
        ++free_pkt;
    }
    pthread_mutex_unlock(&statistic_lock);
}
PUBLIC
packet_t *remove_head_packet(packet_t *pkt){
    if(pkt == NULL){
        return NULL;
    }
    //most likely
    if(pkt->prev == NULL && pkt->next != NULL){
        pkt->next->prev = NULL;
    }
    return pkt->next;
}

PUBLIC int packet_full(packet_t *pkt){
    if(pkt->used_size == sizeof(pkt->buf)){
        return 1;
    }
    return 0;
}

PUBLIC
packet_state_t write_to_packet(client_t *client){
    int fd = client->fd;
    int nread;

    if(client->pkt_tail == NULL){
        if(client->pkt_head != NULL){
            printf("error: wrong state head packet exist, tail packet empty!\n\n");
            return PKT_STATE_DISORDER;
        }
        packet_t *pkt = append_new_packet(client);
        if(pkt == NULL){
            printf("no memory to handle new packet\n\n");
            return PKT_STATE_ALLOC_FAIL;
        }
    }

    for(;;){
        int free_size = get_pkt_free(client->pkt_tail);
        if(free_size == 0){
            //new packet.
            packet_t *pkt = append_new_packet(client);
            if(pkt  == NULL){
                return PKT_STATE_ALLOC_FAIL;
            }
            free_size = get_pkt_free(client->pkt_tail);
        }

        nread = read(fd, pkt_write_start_addr(client->pkt_tail), free_size);
        //printf("free size = %d, read size = %d\n", free_size, nread);
        if(nread < 0){
            if(errno != EAGAIN){
                return PKT_STATE_CLIENT_ERR;
            }else{
                return PKT_STATE_TRY_AGAIN;
            }
        }else if(nread == 0){

            return PKT_STATE_CLIENT_CLOSE;
        }else{
            client->pkt_tail->used_size += nread;
            client->bytes += nread;
            client->buf_avail_bytes += nread;
        }
    }
}







