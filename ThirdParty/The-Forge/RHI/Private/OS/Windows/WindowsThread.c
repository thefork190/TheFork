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

#if defined(_WINDOWS) || defined(XBOX)

#include "ILog.h"
#include "IThread.h"
#include "IOperatingSystem.h"

#include "IMemory.h"

#include <process.h> // _beginthreadex

/*
 * Function pointer can't be casted to void*,
 * so we wrap it with a struct
 */
typedef struct CallOnceFnWrapper
{
    CallOnceFn fn;
} CallOnceFnWrapper;

static BOOL callOnceImpl(PINIT_ONCE initOnce, PVOID pWrapper, PVOID* ppContext)
{
    UNREF_PARAM(initOnce);
    UNREF_PARAM(ppContext);
    CallOnceFn fn = ((CallOnceFnWrapper*)pWrapper)->fn;
    if (fn)
        fn();

    return TRUE;
}

void callOnce(CallOnceGuard* pGuard, CallOnceFn fn)
{
    CallOnceFnWrapper wrapper = { fn };
    InitOnceExecuteOnce(pGuard, callOnceImpl, &wrapper, NULL);
}

bool initMutex(Mutex* mutex)
{
    return InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)&mutex->mHandle, (DWORD)MUTEX_DEFAULT_SPIN_COUNT);
}

void destroyMutex(Mutex* mutex)
{
    CRITICAL_SECTION* cs = (CRITICAL_SECTION*)&mutex->mHandle;
    DeleteCriticalSection(cs);
    memset(&mutex->mHandle, 0, sizeof(mutex->mHandle));
}

void acquireMutex(Mutex* mutex) { EnterCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

bool tryAcquireMutex(Mutex* mutex) { return TryEnterCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

void releaseMutex(Mutex* mutex) { LeaveCriticalSection((CRITICAL_SECTION*)&mutex->mHandle); }

bool initConditionVariable(ConditionVariable* cv)
{
    cv->pHandle = (CONDITION_VARIABLE*)tf_calloc(1, sizeof(CONDITION_VARIABLE));
    InitializeConditionVariable((PCONDITION_VARIABLE)cv->pHandle);
    return true;
}

void destroyConditionVariable(ConditionVariable* cv) { tf_free(cv->pHandle); }

void waitConditionVariable(ConditionVariable* cv, Mutex* pMutex, uint32_t ms)
{
    SleepConditionVariableCS((PCONDITION_VARIABLE)cv->pHandle, (PCRITICAL_SECTION)&pMutex->mHandle, ms);
}

void wakeOneConditionVariable(ConditionVariable* cv) { WakeConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

void wakeAllConditionVariable(ConditionVariable* cv) { WakeAllConditionVariable((PCONDITION_VARIABLE)cv->pHandle); }

static ThreadID mainThreadID = 0;

void setMainThread() { mainThreadID = getCurrentThreadID(); }

ThreadID getCurrentThreadID() { return GetCurrentThreadId(); /* Windows built-in function*/ }

char* thread_name()
{
    __declspec(thread) static char name[MAX_THREAD_NAME_LENGTH + 1];
    return name;
}

void getCurrentThreadName(char* buffer, int size)
{
    const char* name = thread_name();
    if (name[0])
        snprintf(buffer, (size_t)size, "%s", name);
    else
        buffer[0] = 0;
}

void setCurrentThreadName(const char* name) { strcpy_s(thread_name(), MAX_THREAD_NAME_LENGTH + 1, name); }

bool isMainThread() { return getCurrentThreadID() == mainThreadID; }

typedef int(__cdecl* SETTHREADDESCFUNC)(HANDLE, PCWSTR);

unsigned WINAPI ThreadFunctionStatic(void* data)
{
    ThreadDesc item = *((ThreadDesc*)(data));
    tf_free(data);

    if (item.mThreadName[0] != 0)
    {
        // Local TheForge thread name, used for logging
        setCurrentThreadName(item.mThreadName);

#ifdef _WINDOWS
        HINSTANCE hinstLib = GetModuleHandle(TEXT("KernelBase.dll"));
        if (hinstLib != NULL)
        {
            SETTHREADDESCFUNC ProcAdd = (SETTHREADDESCFUNC)GetProcAddress(hinstLib, "SetThreadDescription");
            if (ProcAdd != NULL)
            {
                WCHAR windowsThreadName[sizeof(item.mThreadName)] = { 0 };
                mbstowcs(windowsThreadName, item.mThreadName, strlen(item.mThreadName) + 1);
                HRESULT res = ProcAdd(GetCurrentThread(), windowsThreadName);
                ASSERT(!FAILED(res));
            }
        }
#endif
    }

    if (item.setAffinityMask)
    {
        uint32_t nCores = getNumCPUCores();

        for (uint32_t groupId = 0; groupId < nCores / 64; ++groupId)
        {
            GROUP_AFFINITY groupAffinity = { 0 };

            groupAffinity.Mask = item.affinityMask[groupId];
            groupAffinity.Group = (WORD)groupId;

            BOOL res = SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, NULL);
            if (res != 0)
            {
                LOGF(eERROR, "Failed to set affinity for thread %s for CPU group %u: 0x%x", item.mThreadName, groupId, res);
            }
        }
    }

    item.pFunc(item.pData);
    return 0;
}

void threadSleep(unsigned mSec) { Sleep(mSec); }

bool initThread(ThreadDesc* pDesc, ThreadHandle* pHandle)
{
    ASSERT(pHandle);
    *pHandle = NULL;

    // Copy the contents of ThreadDesc because if the variable is in the stack we might access corrupted data.
    ThreadDesc* pDescCopy = (ThreadDesc*)tf_malloc(sizeof(ThreadDesc));
    *pDescCopy = *pDesc;

    *pHandle = (ThreadHandle)_beginthreadex(0, 0, ThreadFunctionStatic, pDescCopy, 0, 0);
    return *pHandle != NULL;
}

void joinThread(ThreadHandle handle)
{
    if (!handle)
        return;
    WaitForSingleObject((HANDLE)handle, INFINITE);
    CloseHandle((HANDLE)handle);
}

void detachThread(ThreadHandle handle)
{
    if (!handle)
        return;
    CloseHandle((HANDLE)handle);
}

unsigned int getNumCPUCores(void)
{
    struct _SYSTEM_INFO systemInfo;
    GetSystemInfo(&systemInfo);
    return systemInfo.dwNumberOfProcessors;
}
#endif
