/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "Config.h"

#include <mach/clock.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <time.h>

#include "ILog.h"
#include "IThread.h"
#include "IOperatingSystem.h"

#include "Threading/UnixThreadID.h"

#include "IMemory.h"

void callOnce(CallOnceGuard* pGuard, CallOnceFn pFn) { pthread_once(pGuard, pFn); }

bool initMutex(Mutex* pMutex)
{
    pMutex->mSpinCount = MUTEX_DEFAULT_SPIN_COUNT;
    pMutex->pHandle = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
    pthread_mutexattr_t attr;
    int                 status = pthread_mutexattr_init(&attr);
    status |= pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    status |= pthread_mutex_init(&pMutex->pHandle, &attr);
    status |= pthread_mutexattr_destroy(&attr);
    return status == 0;
}

void destroyMutex(Mutex* pMutex) { pthread_mutex_destroy(&pMutex->pHandle); }

void acquireMutex(Mutex* pMutex)
{
    uint32_t count = 0;

    while (count < pMutex->mSpinCount && pthread_mutex_trylock(&pMutex->pHandle) != 0)
        ++count;

    if (count == pMutex->mSpinCount)
    {
        int r = pthread_mutex_lock(&pMutex->pHandle);
        UNREF_PARAM(r);
        ASSERT(r == 0 && "Mutex::Acquire failed to take the lock");
    }
}

bool tryAcquireMutex(Mutex* pMutex) { return pthread_mutex_trylock(&pMutex->pHandle) == 0; }

void releaseMutex(Mutex* pMutex) { pthread_mutex_unlock(&pMutex->pHandle); }

bool initConditionVariable(ConditionVariable* pCv)
{
    pCv->pHandle = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    int res = pthread_cond_init(&pCv->pHandle, NULL);
    ASSERT(res == 0);
    return res == 0;
}

void destroyConditionVariable(ConditionVariable* pCv) { pthread_cond_destroy(&pCv->pHandle); }

void waitConditionVariable(ConditionVariable* pCv, Mutex* mutex, uint32_t ms)
{
    pthread_mutex_t* mutexHandle = (pthread_mutex_t*)&mutex->pHandle;

    if (ms == TIMEOUT_INFINITE)
    {
        pthread_cond_wait(&pCv->pHandle, mutexHandle);
    }
    else
    {
        struct timespec time;
        time.tv_sec = ms / 1000;                    // milliseconds to seconds
        time.tv_nsec = (ms % 1000) * NSEC_PER_MSEC; // remainder to nanoseconds

        pthread_cond_timedwait_relative_np(&pCv->pHandle, mutexHandle, &time);
    }
}

void wakeOneConditionVariable(ConditionVariable* pCv) { pthread_cond_signal(&pCv->pHandle); }

void wakeAllConditionVariable(ConditionVariable* pCv) { pthread_cond_broadcast(&pCv->pHandle); }

static ThreadID mainThreadID;

/*  void Thread::SetPriority(int priority)
{
      sched_param param;
      param.sched_priority = priority;
      pthread_setschedparam(pHandle, SCHED_OTHER, &param);
}*/

void setMainThread(void) { mainThreadID = getCurrentThreadID(); }

ThreadID getCurrentThreadID(void) { return getCurrentPthreadID(); }

void getCurrentThreadName(char* buffer, int buffer_size) { pthread_getname_np(pthread_self(), buffer, buffer_size); }

void setCurrentThreadName(const char* name) { pthread_setname_np(name); }

bool isMainThread(void) { return getCurrentThreadID() == mainThreadID; }

void threadSleep(unsigned mSec) { usleep(mSec * 1000); }

// threading class (Static functions)
unsigned int getNumCPUCores(void)
{
    size_t       len;
    unsigned int ncpu;
    len = sizeof(ncpu);
    sysctlbyname("hw.ncpu", &ncpu, &len, NULL, 0);
    return ncpu;
}

void* ThreadFunctionStatic(void* data)
{
    ThreadDesc item = *((ThreadDesc*)(data));
    tf_free(data);

    if (item.mThreadName[0] != 0)
        setCurrentThreadName(item.mThreadName);

    // TODO: implement affinity mask, if Apple at some point allows to set it.

    item.pFunc(item.pData);
    return 0;
}

bool initThread(ThreadDesc* pData, ThreadHandle* pHandle)
{
    // Copy the contents of ThreadDesc because if the variable is in the stack we might access corrupted data.
    ThreadDesc* pDataCopy = (ThreadDesc*)tf_malloc(sizeof(ThreadDesc));
    *pDataCopy = *pData;

    int res = pthread_create(pHandle, NULL, ThreadFunctionStatic, pDataCopy);
    if (res)
        tf_free(pDataCopy);
    return res == 0;
}

void joinThread(ThreadHandle handle) { pthread_join(handle, NULL); }

void detachThread(ThreadHandle handle) { pthread_detach(handle); }
