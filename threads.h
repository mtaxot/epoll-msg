#ifndef THREADS_H_
#define THREADS_H_

#include <pthread.h>
#include <sys/epoll.h>
#include "common.h"

typedef struct _tworker_t tworker_t;
typedef struct _tmaster_t tmaster_t;
typedef struct _event_job_t event_job_t;

/* how many epoll event that will dispatch to slave to process.*/
typedef struct _event_job_t{
    struct epoll_event *events;
    int num;
} event_job_t;

typedef struct _tworker_t{
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    volatile int var;//cond var
    tmaster_t *master;
    void (*do_single_job)(tmaster_t*, struct epoll_event*);
    char *name;
    void *priv;
    pthread_t thread;
} tworker_t;


typedef struct _tmaster_t{
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    volatile int var;
    tworker_t *workers;
    int worker_count;
    char *name;
    client_t *host;
    int epollfd;
    pthread_t thread;
} tmaster_t;

/**
 * master thread routine run()
 */
PUBLIC void *run_master(void *var);


/**
 * worker thread rounine run()
 */
PUBLIC void *run_worker(void *var);


/**
 * create thread with defaut attr.
 */
PUBLIC pthread_t new_thread_default(void *(runnable)(void*), void *arg);

PUBLIC void thread_master_join(tmaster_t *master);

PUBLIC void thread_wokers_join(tworker_t *workers, int worker_num);

#endif /* THREADS_H_ */
