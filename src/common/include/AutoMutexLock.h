#ifndef _AUTOMUTEXLOCK_H
#define _AUTOMUTEXLOCK_H

#include <pthread.h>

class AutoMutexLock
{
public:
    explicit AutoMutexLock(pthread_mutex_t* lock) : m_lock(lock)
    {
        if (m_lock)
        {
            pthread_mutex_lock(m_lock);
        }
    }

    ~AutoMutexLock()
    {
        if (m_lock)
        {
            pthread_mutex_unlock(m_lock);
        }
    }

private:
    AutoMutexLock();
    AutoMutexLock(const AutoMutexLock&);
    AutoMutexLock& operator=(const AutoMutexLock&);

    pthread_mutex_t* m_lock;
};

#endif
