#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "tas_semaphore.h"

#define THREADS 10
#define TIMES 5000

struct args
{
    semaphore *sem;
    int inside;
};

struct args2
{
    semaphore *sem;
    int inside;
};

void *thread_function(void *arg)
{
    struct args *args = (struct args*)arg;
    for(int i = 0; i < TIMES; i++)
    {
        semaphore_signal(args->sem);
    }
    for(int i = 0; i < TIMES; i++)
    {
        semaphore_wait(args->sem);
    }
    return NULL;
}

void *thread_function2(void *arg)
{
    struct args2 *args = (struct args2*)arg;
    args->inside = 0;
    semaphore_wait(args->sem); // should cause deadlock
    args->inside = 1;
    return NULL;
}

int main(void)
{
    semaphore sem;
    semaphore_init(&sem, 1);

    pthread_t threads[THREADS];
    struct args args[THREADS];

    for(int i = 0; i < THREADS; i++)
    {
        args[i].sem = &sem;
        args[i].inside = 0;
        pthread_create(&threads[i], NULL, thread_function, (void*)&args[i]);
    }

    for(int i = 0; i < THREADS; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // now, verify that another thread waits on semaphore
    struct args2 args2;
    args2.sem = &sem;
    args2.inside = 0;
    pthread_t thread;
    pthread_create(&thread, NULL, thread_function2, (void*)&args2);
    sleep(1);
    pthread_cancel(thread);
    if(args2.inside != 0)
    {
        printf("Value is %d\n", args2.inside);
        fprintf(stderr, "semaphore_no_signal_wait_atomicity");
    }
    return 0;
}