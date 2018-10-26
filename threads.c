#include "connection.h"
#include "threads.h"

extern int access_usrs;
PUBLIC
pthread_t new_thread_default(void *(runnable)(void*), void *arg){
    if(runnable == NULL){
        return -1;
    }
    pthread_t thread_id;
    int ret = pthread_create(&thread_id, NULL,(void*)runnable, arg);
    if(ret != 0){
        return -1;
    }
    return thread_id;
}

PRIVATE int bind_local(){
    int listenfd = socket_bind(HOST_IP, PORT);

    listen(listenfd,MAX_FD);
    setnonblock(listenfd);
    return listenfd;
}

PRIVATE int init_epoll(){
    int epollfd = epoll_create(MAX_FD);
    if(epollfd < 0){
        printf("create epoll fail\n\n");
        exit(-1);
    }
    return epollfd;
}

PRIVATE void master_listening(tmaster_t *master){
    int listenfd = bind_local();
    master->epollfd = init_epoll();
    client_t *host = create_new_client(listenfd);
    if(host == NULL){
        printf("no memory to allocate a client_t for listening fd\n\n");
        exit(-1);
    }
    master->host = host;
    add_event(host, master->epollfd, EPOLLIN);

}


PUBLIC
void *run_master(void *var){
    signal(SIGPIPE, SIG_IGN);
    tmaster_t *master = (tmaster_t*)var;
    master_listening(master);
    event_job_t ejob;
    struct epoll_event events[MAX_EVENTS];
    int alloced_jobs = 0;
    int real_alloc_jobs = 0;
    int left_job;
    int i;
    printf("run master = %ld\n", master->thread);
    while(1){
        pthread_mutex_lock(&master->lock);
        while(master->var < master->worker_count){
            pthread_cond_wait(&master->cond, &master->lock);
        }
        pthread_mutex_unlock(&master->lock);
        /**
         * here is the magic.
         * at the first beginning, master is thread waiting, when main
         * thread wake up master, main thread set master->var to be
         * master->worker_count
        */

        //printf("alloc msg = %d free msg = %d, alloc pkt %d free pkt %d!\n\n",
        //       malloc_msg, free_msg , malloc_pkt, free_pkt);
        //printf("all user = %d\n", access_usrs);
        ejob.events = events;
        ejob.num = epoll_wait(master->epollfd, events, MAX_EVENTS, -1);
        if(ejob.num < 0){
            perror("epoll_wait return < 0\n");
            usleep(10*1000);
            continue;
        }

        int maxload = ejob.num / master->worker_count;
        if(maxload == 0){
            maxload = 1;
        }else{
            if(ejob.num % master->worker_count != 0){
                maxload += 1;
            }
        }
        left_job = ejob.num;
        for(i = 0; i < master->worker_count; i++){
            //dispatch jobs.
            alloced_jobs += maxload;
            if(alloced_jobs <= ejob.num){
                real_alloc_jobs = maxload;
                left_job -= real_alloc_jobs;
            }else{
                real_alloc_jobs = left_job;
                left_job = 0;
            }
            if(real_alloc_jobs != 0){
                master->var--;
                //after dispatch finish, master will wait.
                //printf("### %s ## dispatch %d jobs for %s.......\n",
                //       master->name, real_alloc_jobs, master->workers[i].name);
            }
            pthread_mutex_lock(&master->workers[i].lock);
            master->workers[i].var = real_alloc_jobs;//alloc job num.
            master->workers[i].priv = ejob.events + i * maxload;
            pthread_cond_signal(&master->workers[i].cond);
            pthread_mutex_unlock(&master->workers[i].lock);
        }

    }

    printf("master %ld quit\n", master->thread);
    close_client(master->host);//close master listening fd.
    return NULL;
}

PUBLIC
void *run_worker(void *var){
    tworker_t *worker = (tworker_t*)var;
    assert(worker != NULL);
    printf("run worker = %ld\n", worker->thread);
    while(1){
        pthread_mutex_lock(&worker->lock);
        while(worker->var == 0){
            //no data.
            pthread_cond_wait(&worker->cond, &worker->lock);
        }
        struct epoll_event *events = (struct epoll_event*)worker->priv;
        //client_t *client = (client_t *)events->data.ptr;
        int i;
        int total = worker->var;
        for(i = 0; i < total; i++){
            worker->do_single_job(worker->master, events + i);
            //printf("worker-%ld----handle %d\n", worker->thread, client->fd);
            worker->var--;
        }
        pthread_mutex_lock(&worker->master->lock);
        worker->master->var++;
        pthread_cond_signal(&worker->master->cond);
        pthread_mutex_unlock(&worker->master->lock);
        pthread_mutex_unlock(&worker->lock);
    }
    printf("worker %ld quit\n", worker->thread);
    return NULL;
}

PUBLIC void thread_master_join(tmaster_t *master){
    pthread_join(master->thread, NULL);
}

PUBLIC void thread_wokers_join(tworker_t *workers, int worker_num){
    int i;
    for(i = 0; i < worker_num; i++){
        pthread_join(workers[i].thread, NULL);
    }
}
