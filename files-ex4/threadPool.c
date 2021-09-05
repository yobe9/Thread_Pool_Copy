// Yoav Berger
#include <pthread.h>
#include "threadPool.h"
#include "osqueue.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

//function to free allocated memory of whole pool
void freePoolMem(ThreadPool* pool){
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->condition));
    free(pool->arrayThreads);

    //free all the tasks in the queue
    while (!osIsQueueEmpty(pool->taskQueue))
    {
        funcTask* ft = osDequeue(pool->taskQueue);
        free(ft);
    }
    osDestroyQueue(pool->taskQueue);

    free(pool);
}

//function to give to the threads to run on
void* threadFunc(void* threadsPool){
    ThreadPool* pool = (ThreadPool*)threadsPool;
    //running the functions from the queue and inserting the threads into waiting if needed
    while (true)
    {
        //locking the mutex
        int mut = pthread_mutex_lock(&(pool->lock));
        if (mut != 0)
        {
            freePoolMem(pool);
            perror("mutex error");
            exit(-1);
        }
        
        //checking if there are no tasks and destroy hasn't called, then put threads to wait
        //if there was destroy and there are still tasks, keep going with them and stop when queue empty
        while (osIsQueueEmpty(pool->taskQueue) && !pool->isStopFuncRun)
        {
            if (osIsQueueEmpty(pool->taskQueue) && pool->isDestroyNonZero)
            {
                pool->isStopFuncRun = true;
                break;
            }
            int wa = pthread_cond_wait(&(pool->condition), &(pool->lock));
            if (wa != 0)
            {
                freePoolMem(pool);
                perror("pthread wait error");
                exit(-1);
            }
        }

        //if the tasks queue is not empty, and destroy hasn't called, dequeue task and call it
        //also free the mutex and brodcast for other threads
        if (!pool->isStopFuncRun)
        {
            funcTask* task = (funcTask*)osDequeue(pool->taskQueue);
            int mut = pthread_mutex_unlock(&(pool->lock));
            if (mut != 0)
            {
                freePoolMem(pool);
                perror("mutex unlock error");
                exit(-1);
            }
            int brod = pthread_cond_broadcast(&(pool->condition));
            if (brod != 0)
            {
                freePoolMem(pool);
                perror("thread brodcast error");
                exit(-1);
            }
            if (task != NULL)
            {
                task->func(task->arguments);
            }
            free(task);
        }
        //in case destroy has been called, we want to exit the while loop
        else{
            break;
        }
    }

    //free the mutex after the while and brodcast to other threads
    int mute = pthread_mutex_unlock(&(pool->lock));
    if (mute != 0)
    {
        freePoolMem(pool);
        perror("mutex unlock error");
        exit(-1);
    }
    int brodc = pthread_cond_broadcast(&(pool->condition));
    if (brodc != 0)
    {
        freePoolMem(pool);
        perror("thread brodcast error");
        exit(-1);
    }

    return NULL;

}


ThreadPool* tpCreate(int numOfThreads){
    //allocating the pool and it's fields, and set them
    OSQueue* taskOfQueue = osCreateQueue();
    if (taskOfQueue == NULL)
    {
        perror("OSQueue malloc error");
        exit(-1);
    }
    
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (pool == NULL)
    {
        perror("thread pool malloc error");
        osDestroyQueue(taskOfQueue);
        exit(-1);
    }

    pool->numThreads = numOfThreads;
    pool->isStopInsert = false;
    pool->isDestroyNonZero = false;
    pool->isStopFuncRun = false;
    pool->taskQueue = taskOfQueue;

    pthread_mutex_init(&(pool->lock), NULL);
    pthread_cond_init(&(pool->condition), NULL);

    //calculating the size of the array of the threads and allocate them
    int  threadArraySize = sizeof(pthread_t) * numOfThreads;
    pool->arrayThreads = (pthread_t*)malloc(threadArraySize);
    if (pool->arrayThreads == NULL)
    {
        osDestroyQueue(taskOfQueue);
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->condition));
        free(pool);
        perror("thread array malloc error");
        exit(-1);

    }

    //fiiling the thread's array
    int i;
    int createThread;
    for (i = 0; i < numOfThreads; i++)
    {
        createThread = pthread_create(&pool->arrayThreads[i],NULL, threadFunc, (void*)pool);
        if (createThread != 0)
        {
            freePoolMem(pool);
            perror("thread creation error");
            exit(-1);
        }
        
    }

    pool->isStopFuncRun = false;
    pool->isStopInsert = false;
    pool->isDestroyNonZero = false;
    
    return pool;    
}

//insert task into the pool task's array
int tpInsertTask(ThreadPool* threadPool, void (*computeFunc) (void *), void* param){
    //check that the pool wasn't destroy
    if (!threadPool->isStopInsert)
    {
        //allocating a new task and set its fields
        funcTask* ft = (funcTask*)malloc(sizeof(funcTask));
        if (ft == NULL)
        {
            freePoolMem(threadPool);
            perror("task malloc error");
            exit(-1);
        }
        ft->arguments = param;
        ft->func = computeFunc;

        //locking the mutex so we can enter a task
        int mute = pthread_mutex_lock(&(threadPool->lock));
        if (mute != 0)
        {
            freePoolMem(threadPool);
            perror("mutex unlock error");
            exit(-1);
        }
        osEnqueue(threadPool->taskQueue, ft);

        //free the mutex and sending signal
        int mut = pthread_mutex_unlock(&(threadPool->lock));
        if (mut != 0)
        {
            freePoolMem(threadPool);
            perror("mutex unlock error");
            exit(-1);
        }
        int co = pthread_cond_signal(&(threadPool->condition));
        if (co != 0)
        {
            freePoolMem(threadPool);
            perror("signal error");
            exit(-1);
        }

        //in case everything was ok
        return 0;        
    }
    //in case destroy allready been called
    else{
        return -1;
    }
}

//destroy the thread's pool
void tpDestroy(ThreadPool* threadPool, int shouldWaitForTasks){
    //check if destroy was done allready and setts the flags to stop the running of the function
    if (threadPool->isStopInsert == false)
    {
        threadPool->isStopInsert = true;
        //in case zero was passed, set overall stop flag to stop
        if (shouldWaitForTasks == 0)
        {
            threadPool->isStopFuncRun = true;
        }
        
        //broadcasting and free mutex //********************************* maybe need to change order, may need to delete both
        int brodc = pthread_cond_broadcast(&(threadPool->condition));
        if (brodc != 0)
        {
            freePoolMem(threadPool);
            perror("thread brodcast error");
            exit(-1);
        }
        int mute = pthread_mutex_unlock(&(threadPool->lock));
        if (mute != 0)
        {
            freePoolMem(threadPool);
            perror("mutex unlock error");
            exit(-1);
        }

        //setting the flag to stop when there are no more tasks
        threadPool->isDestroyNonZero = true;

        //joining all the threads so the will be ended
        int i;
        for (i = 0; i < threadPool->numThreads; i++)
        {
            int jo = pthread_join(threadPool->arrayThreads[i], NULL);
            if (jo != 0)
            {
                freePoolMem(threadPool);
                perror("pthread join error");
                exit(-1);
            }
            
        }
        freePoolMem(threadPool);
    }
}





