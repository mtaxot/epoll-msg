#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "common.h"
#include "message.h"
#include "packet.h"
#include "threads.h"

int srand_num = 0;
void *run_test(void *var){
    //printf("run......\n");
    int                 sockfd;
    int res;
    srand(srand_num++);
    struct sockaddr_in  servaddr;
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET,HOST_IP,&servaddr.sin_addr);
    res = connect(sockfd,(struct sockaddr*)&servaddr,sizeof(servaddr));
    if(res < 0){
        perror("connect server fail !\n");
        close(sockfd);
        return 0;
    }
    int i;
    long int num = 0;
    long int bytes = 0;
    char buffer[BODY_AND_VERIFIER_LEN];
    memset(buffer, 0, sizeof(buffer));
    int max_msg_len = sizeof(buffer) - 4;
    msg_t *msg = make_string_message(buffer, max_msg_len);

    int rand_msg_len = ((unsigned int) rand()) % max_msg_len + 1;
    //printf("msg len = %d  %d\n", msg->msg_len, BODY_AND_VERIFIER_LEN);
    if(msg == NULL){
        close(sockfd);
        return 0;
    }

    int totals = 1000;

    for(i = 0; i < totals; i++){

        //dump_msg(p);
        usleep(1 *1000);
        res = make_fixlen_message(msg, rand_msg_len);
        if(res < 0){
            printf("make fix len msg fail!\n");
            continue;
        }
        //printf("msg len = %d\n", msg->msg_len);
        //unsigned int guide = START_CODE;
        //res = write(sockfd, msg->buffer, sizeof(msg_header_t));
        //res = write(sockfd, (char*)&guide, sizeof(guide));
        res = write(sockfd, "tes", strlen("test"));
        res = write(sockfd, msg->buffer, msg->msg_len);
        res = write(sockfd, msg->buffer, msg->msg_len - 4);
        if(res < 0){
            printf("pid = %d, write res < 0:\n\n", getpid());
            perror("");
            break;
        }else if(res == 0){
            printf("pid = %d, write res == 0:\n\n",getpid()); break;
        }else{
            num++;
            bytes += msg->msg_len;
            //printf("%d of %ld msg\n", (i+1), num);
        }
        //printf("%d wirte bytes = %d bytes\n", i, res);
    }
    recycle_msg(msg);
    printf("pid %d send %ld msgs, bytes %ld\n", getpid(), num, bytes);

    close(sockfd);
    return 0;

}

pthread_t *create_workers(int nworker){
    pthread_t *ts = (pthread_t*)malloc(nworker *sizeof(pthread_t));
    if(ts == NULL){
        return NULL;
    }
    int i;
    for(i = 0; i < nworker; i++){
        pthread_t td = new_thread_default(run_test, NULL);
        if(td < 0){
            printf("fail to start worker thread\n");
            exit(-1);
        }
        ts[i] = td;
    }
    return ts;
}



int main(int argc,char *argv[])
{
    signal(SIGPIPE, SIG_IGN);
    int threads = 8;
    pthread_t *wks = create_workers(threads);
    if(wks == NULL){
        printf("fail to create test thread\n");
        return -1;
    }
    int n;
    for(n = 0; n < threads; n++){
        pthread_join(wks[n], NULL);
    }
    return -1;
}

void on_signal(int code){
    printf("signal : %d happened\n", code);
}
