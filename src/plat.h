// external platform header from ncnn
// support condition variable wait timeout

#ifndef RIFE_NCNN_PLAT_H
#define RIFE_NCNN_PLAT_H

#include "platform.h"

// #ifdef _TST // for test
#if (defined _WIN32 && !(defined __MINGW32__))
class Mutex
{
public:
    Mutex() { InitializeSRWLock(&srwlock); }
    ~Mutex() {}
    void lock() { AcquireSRWLockExclusive(&srwlock); }
    void unlock() { ReleaseSRWLockExclusive(&srwlock); }
private:
    friend class ConditionVariable;
    // NOTE SRWLock is available from windows vista
    SRWLOCK srwlock;
};

class ConditionVariable
{
public:
    ConditionVariable() { InitializeConditionVariable(&condvar); }
    ~ConditionVariable() {}
    bool wait(Mutex& mutex, unsigned long timeout_ms = INFINITE) { return SleepConditionVariableSRW(&condvar, &mutex.srwlock, timeout_ms, 0); }
    void broadcast() { WakeAllConditionVariable(&condvar); }
    void signal() { WakeConditionVariable(&condvar); }
private:
    CONDITION_VARIABLE condvar;
};
#else // (defined _WIN32 && !(defined __MINGW32__))
class Mutex
{
public:
    Mutex() { pthread_mutex_init(&mutex, 0); }
    ~Mutex() { pthread_mutex_destroy(&mutex); }
    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }
private:
    friend class ConditionVariable;
    pthread_mutex_t mutex;
};

timespec* timespec_from_ms(timespec &ts, unsigned long timeout_ms)
{
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    return &ts;
}

class ConditionVariable
{
public:
    ConditionVariable() { pthread_cond_init(&cond, 0); }
    ~ConditionVariable() { pthread_cond_destroy(&cond); }
    bool wait(Mutex& mutex, unsigned long timeout_ms = INFINITE) { timespec ts; return pthread_cond_timedwait(&cond, &mutex.mutex, timespec_from_ms(ts, timeout_ms)); }
    void broadcast() { pthread_cond_broadcast(&cond); }
    void signal() { pthread_cond_signal(&cond); }
private:
    pthread_cond_t cond;
};

#endif // (defined _WIN32 && !(defined __MINGW32__))

#endif // RIFE_NCNN_PLAT_H