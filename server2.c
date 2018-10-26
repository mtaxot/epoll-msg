#include "server2.h"
#include "common.h"
#include "threads.h"
#include "connection.h"
static void handle_accept(int epollfd, int listenfd){
    int newfd;
    struct sockaddr_in newaddr;
    bzero(&newaddr, sizeof(newaddr));
    socklen_t addrlen = sizeof(newaddr);
    /*
     * accept multi times to solve client connection reset by peer.
     * although we could optimize server sysctl backlog, still could
     * not solve massive concurrent connection.
     */
    int i;
    for(i = 0; i < ACCEPT_MAX_CONNS_ONCE; i++){
        newfd = _atom_accept_(listenfd,(struct sockaddr*)&newaddr, &addrlen);
        printf("%d accept return %d\n", i, newfd);
        if(newfd < 0){
            //perror("accept error:\n\n");
            break;
        }else{
            //printf("new client connected: %s:%d\n\n",
            //        inet_ntoa(newaddr.sin_addr), newaddr.sin_port);
            setnonblock(newfd);
            client_t *client = create_new_client(newfd);
            if(client != NULL){
                add_event(client, epollfd, EPOLLIN);
            }else{
                printf("no memory to handle new connection\n\n");
                _atom_close_(newfd);
            }
        }
    }

}

static void do_read(int epollfd, client_t *client){
    int fd = client->fd;
    packet_state_t state;
    //just copy data to packet and do not check msg.
    for(;;){
        state = write_to_packet(client);
        if(state == PKT_STATE_FULL){
            continue;//continue means allocate new packet and read again.
        }else if(state == PKT_STATE_TRY_AGAIN){
            break;// socket buffer drained.
        }else if(state == PKT_STATE_ALLOC_FAIL){
            printf("no memory to handle packet\n\n");
            delete_event(client, epollfd, EPOLLIN);//stop reading.
            //prepare error message write close.
            client->status = CLIENT_CLOSED;
            return;
        }else if(state == PKT_STATE_CLIENT_CLOSE){
            client->status = CLIENT_CLOSED;
            break;
            //you must handle if there are unfinished read write job
            //write unfinished, keep that in a good place.
            //read unfinished , just discard.
            //PKT_STATE_CLIENT_ERR is the same.
        }else if(state == PKT_STATE_CLIENT_ERR){
            //read < 0 but not EAGAIN.
            printf("read client message error, force close %d\n\n", fd);
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            return;
        }else if(state == PKT_STATE_DISORDER){
            printf("packet state of client disorder\n\n");
            assert(0 == 1);//recommand
        }else{
            //unknown state
            printf("unknown packet state\n\n");
            assert(0 == 1);//recommand
        }
    }

    for(;;){
        // message.c parse_message_strict
        parse_result_t result = parse_message(client);
        if(result == PARSE_MSG_READY_TO_DELIVER){
            //do deliver
            client->msg_nums++;
            msg_t *head = pull_head_msg_r(client);
            recycle_msg(head);
            continue;
        }else if(result == PARSE_MSG_BODY_NEED_MORE_DATA){
            //printf("msg body not enough, feed more data please.. buffer size %d\n",
            //       get_pkt_avail(client->pkt_head));
            break;//packets data are all read.
        }else if(result == PARSE_MSG_COPY_NO_MEMORY){
            //we have no mem to copy
            //notify client server fail to deliver.
            printf("unable to handle message, wirte msg to nofity client before close.\n");
            delete_event(client, epollfd, EPOLLIN);//stop reading.
            client->status = CLIENT_CLOSED;
            //write to socket ,notify client stop.
            break;
        }else if(result == PARSE_MSG_BODY_VERIFIER_INVALID){
            printf("validate message body verifier invalid.\n");
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            break;
        }else if(result == PARSE_MSG_HDR_NEED_MORE_DATA){
            //printf("msg header not found, feed more data,"
            //        " available buffer size %d\n", get_pkt_avail(client->pkt_head));
            break;//packet all read, see handle PARSE_MSG_BODY_NEED_MORE_DATA
        }else if(result == PARSE_MSG_HDR_INVALID_GUIDE){
            printf("validate message header guide code fail.\n");
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            break;
        }else if(result == PARSE_MSG_HDR_CRC_FAIL){
            printf("validate message header crc fail.\n");
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            break;
        }else if(result == PARSE_MSG_HDR_BODYLEN_INVALID){
            printf("invalid message body length.\n");
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            break;
        }else if(result == PARSE_MSG_HDR_NO_PKT){
            printf("no packet to parse\n");
            break;//no packet,see handle PARSE_MSG_BODY_NEED_MORE_DATA
        }else if(result == PARSE_MSG_HDR_FAIL){
            printf("no message header.\n");
            delete_event(client, epollfd, EPOLLIN | EPOLLOUT);
            client->status = CLIENT_CLOSED;
            break;
        }else if (result == PARSE_MSG_TRY_AGAIN){
            continue;
        }else{
            printf("unknown parse state\n");
            exit(-1);
        }
    }
}


void do_rw_job(tmaster_t *master, struct epoll_event *events){
    int epollfd = master->epollfd;
    int listenfd = master->host->fd;
    client_t *client = (client_t*)(events[0].data.ptr);
    uint32_t event = events[0].events;
    if(event & EPOLLIN){
        if(client->fd == listenfd){
            handle_accept(epollfd, listenfd);//new client connected.
        }else if(client->fd >= 0){
            do_read(epollfd, client);
            if(client->status == CLIENT_CLOSED){
                printf("client %d close, recv %ld msgs %ld bytes\n\n",
                       client->fd, client->msg_nums, client->bytes);
                close_client(client);
            }
        }else{
            printf("invalid sock fd %d\n", client->fd);
        }
    }else if(event & EPOLLOUT){
        //do_write(epollfd, fd);
    }else if(event & EPOLLHUP){
        printf("EPOLL HUB !\n");
    }else if(event & EPOLLMSG){
        printf("EPOLL MSG !\n");
    }else if(event & EPOLLONESHOT){
        printf("EPOLL ONESHOT !\n");
    }else if(event & EPOLLPRI){
        printf("EPOLL PRI !\n");
    }else if(event & EPOLLRDHUP){
        printf("EPOLL RDHUB !\n");
    }else if(event & EPOLLERR){
        printf("EPOLL ERROR !\n");
    }else{
        printf("unknown event %u\n\n", event);
    }
}


tworker_t* create_workers(tmaster_t *master, int nworker){
    assert(nworker > 0 && nworker <= 4096);
    int size = nworker * sizeof(tworker_t);
    tworker_t *workers = (tworker_t *)malloc(size);
    if(workers == NULL){
        printf("fail to start workers\n");
        exit(-1);
    }
    memset(workers, 0, size);
    int i;
    for(i = 0; i < nworker; i++){
        pthread_mutex_init(&workers[i].lock, NULL);
        pthread_cond_init(&workers[i].cond, NULL);
        workers[i].var = 0;
        workers[i].master = master;
        workers[i].name = "worker";
        workers[i].do_single_job = do_rw_job;//register callback
        pthread_t td = new_thread_default(run_worker, &workers[i]);
        if(td < 0){
            printf("fail to start worker thread\n");
            exit(-1);
        }
        workers[i].thread = td;
    }
    return workers;
}

tmaster_t* create_master(int worker_num){
    tmaster_t *master = (tmaster_t *)malloc(sizeof(tmaster_t));
    if(master == NULL){
        printf("fail to start master\n");
        exit(-1);
    }
    memset(master, 0, sizeof(tmaster_t));
    pthread_mutex_init(&master->lock, NULL);
    pthread_cond_init(&master->cond, NULL);
    master->var = 0;
    master->workers = NULL;
    master->worker_count = worker_num;
    master->name = "master";
    pthread_t t0 = new_thread_default(run_master, master);
    if(t0 < 0){
        printf("fail to start master thread\n");
        exit(-1);
    }
    master->thread = t0;
    return master;
}

PRIVATE
int do_epoll_in_master(tmaster_t *master){
    //notify master thread.
    //100ms, wait for master to enter thread wait is enough
    usleep(100*1000);
    pthread_mutex_lock(&master->lock);
    master->var = master->worker_count;
    pthread_cond_signal(&master->cond);
    pthread_mutex_unlock(&master->lock);
    return 0;
}

int main(int argc, char **argv){
    int worker_num = 8;
    tmaster_t * master = create_master(worker_num);
    tworker_t *workers = create_workers(master, worker_num);
    master->workers = workers;

    do_epoll_in_master(master);
    printf("server start success !\n");

    thread_master_join(master);
    thread_wokers_join(workers, worker_num);


    printf("sub thread quit unexpectly.....\n");
    return -1;
}
