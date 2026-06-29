#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include "ECThread.h"

#include <pthread.h>
#include <queue>
#include <semaphore.h>

class ThreadTask
{
public:
    ThreadTask() {}
    virtual ~ThreadTask() {}

    virtual void run() = 0;
};

class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    int createThreadPool(int threadCount);
    int waitTask();
    int postTask(ThreadTask* task);

    static void* mainThread(void* argc);

private:
    static std::queue<ThreadTask*> m_taskQueue;
    static pthread_mutex_t m_queueLock;

    sem_t m_signalSem;
};

#endif
