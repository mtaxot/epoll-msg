#include <pthread.h>
#include "connection.h"
#include "packet.h"

pthread_mutex_t sock_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t epoll_lock = PTHREAD_MUTEX_INITIALIZER;
int access_usrs = 0;

PUBLIC
client_t *create_new_client(int fd){
    int aloc_size = sizeof(client_t) + sizeof(msg_parser_t);
    client_t *client = (client_t*)malloc(aloc_size);
    if(client != NULL){
        memset(client, 0, aloc_size);
        client->fd = fd;
        client->parser = (msg_parser_t*)((void*)client + sizeof(client_t));
        client->parser->do_parse = parse_message;//stric mode parse.还有可以实现容错模式
        client->parser->do_write_packet = write_to_packet;
    }
    return client;
}

PUBLIC
int _atom_accept_(int fd, struct sockaddr *addr,
                socklen_t *len){
    pthread_mutex_lock(&sock_lock);
    int newfd = accept(fd, addr, len);
    if(newfd > 0){
        access_usrs++;
    }
    pthread_mutex_unlock(&sock_lock);
    return newfd;
}

PUBLIC
int _atom_close_(int fd){
    pthread_mutex_lock(&sock_lock);
    int ret = close(fd);
    pthread_mutex_unlock(&sock_lock);
    printf("close fd %d return %d\n", fd, ret);
    return ret;
}

PUBLIC
void close_client(client_t *client){
    _atom_close_(client->fd);
    recycle_all_packet(client);
    handle_undelivered_msg(client);

    free(client);
    //printf("alloc msg = %d free msg = %d, alloc pkt %d free pkt %d!\n\n",
    //       malloc_msg, free_msg , malloc_pkt, free_pkt);
}



PUBLIC
void setnonblock(int sock){
    int opts = fcntl(sock, F_GETFL);
    if(opts < 0){
        perror("setnonblock F_GETFL fail");
        exit(-1);
    }
    opts |= O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts) < 0){
        perror("setnonblock F_SETFL fail");
        exit(-1);
    }
}
PUBLIC
void add_event(client_t *client, int epollfd, int event){
    struct epoll_event ev;
    ev.events = event;
    ev.data.ptr = client;
    pthread_mutex_lock(&epoll_lock);
    epoll_ctl(epollfd, EPOLL_CTL_ADD, client->fd, &ev);
    pthread_mutex_unlock(&epoll_lock);
}



/*
static void modify_event(client_t *client, int epollfd, int event){
    struct epoll_event ev;
    ev.events = event;
    ev.data.ptr = client;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, client->fd, &ev);
}
*/

PUBLIC
void delete_event(client_t *client, int epollfd, int event){

    struct epoll_event ev;
    ev.events = event;
    ev.data.ptr = client;
    pthread_mutex_lock(&epoll_lock);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, client->fd, &ev);
    pthread_mutex_unlock(&epoll_lock);
}

PUBLIC
int socket_bind(char *ip, int port){
    int fd = -1;
    struct sockaddr_in listen_addr;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&listen_addr, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &listen_addr.sin_addr);
    listen_addr.sin_port = htons(port);
    int res = bind(fd, (struct sockaddr*)&listen_addr, sizeof(listen_addr));
    if(res < 0){
        perror("bind error:\n\n");
        exit(-1);
    }
    return fd;
}
