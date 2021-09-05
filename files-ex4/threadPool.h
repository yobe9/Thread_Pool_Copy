// Yoav Berger
#ifndef __THREAD_POOL__
#define __THREAD_POOL__

#include "osqueue.h"
#include <sys/types.h>
#include <stdbool.h>

typedef struct thread_pool
{
    //setting booleans to use as flags for the functions to set when to stop running
    bool isStopFuncRun;
    bool isStopInsert;
    bool isDestroyNonZero;

    //pointers to threads array, task's queue, mutex and cond
    pthread_t* arrayThreads;
    OSQueue* taskQueue;
    pthread_mutex_t lock;
    pthread_cond_t condition;

    //number of threads in the pool
    int numThreads;

}ThreadPool;

//struct for functions we need to run and their arguments
typedef struct funcTask
{
    void (*func)(void*);
    void *arguments;
} funcTask;

ThreadPool* tpCreate(int numOfThreads);

void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks);

int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param);

#endif
