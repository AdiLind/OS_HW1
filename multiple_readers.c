#include "rwlock.h"

#define NUM_READERS 10
#define READER 0

atomic_int inside_read;
int exited = 0;

struct args
{
    int type;
    rwlock *lock;
};

void *reader(void *arg)
{
    struct args *args = (struct args *)arg;
    rwlock_acquire_read(args->lock);
    atomic_fetch_add(&inside_read, 1);
    while(exited == 0)
    {
        int read_count = atomic_load(&inside_read);
        if(read_count > 1)
        {
            exited = 1;
            break; // finally!
        }
    }
    atomic_fetch_sub(&inside_read, 1);
    rwlock_release_read(args->lock);
    return NULL;
}

int main()
{
    rwlock rwlock;
    rwlock_init(&rwlock);

    pthread_t threads[NUM_READERS];
    struct args args[NUM_READERS];

    atomic_store(&inside_read, 0);

    int i = 0;
    for(; i < NUM_READERS; i++)
    {
        args[i].type = READER;
        args[i].lock = &rwlock;
        pthread_create(&threads[i], NULL, reader, (void*)&args[i]);
    }

    // too much time => only one read per time

    sleep(1);
    if(exited == 0)
    {
        fprintf(stderr, "task4.multiple_readers\n");
        for(i = 0; i < NUM_READERS; i++)
        {
            pthread_cancel(threads[i]);  // forcefully terminate
        }
        return 1;
    }
    for(i = 0; i < NUM_READERS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    return 0;
}