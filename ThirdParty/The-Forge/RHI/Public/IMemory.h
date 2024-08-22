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

//--------------------------------------------------------------------------------------------
// NOTE: Make sure this is the last include in a .cpp file!
//       Never include this file from a header!!  If you must use the mem manager from a
//       header (which should be in rare cases, and usually only in core Forge source),
//       define "IMEMORY_FROM_HEADER" before including it.
//--------------------------------------------------------------------------------------------

#ifndef IMEMORY_H
#define IMEMORY_H

#include "Config.h"

#ifdef __cplusplus
#include <new>
#include <utility> // std::forward only
#else
#include <stdbool.h>
#include <stdint.h>
#endif

#define TF_KB (1024)
#define TF_MB (1024 * TF_KB)
#define TF_GB (1024 * TF_MB)

#ifdef ENABLE_MEMORY_TRACKING
typedef struct MemoryStatistics
{
    uint32_t totalReportedMemory;
    uint32_t totalActualMemory;
    uint32_t peakReportedMemory;
    uint32_t peakActualMemory;
    uint32_t accumulatedReportedMemory;
    uint32_t accumulatedActualMemory;
    uint32_t accumulatedAllocUnitCount;
    uint32_t totalAllocUnitCount;
    uint32_t peakAllocUnitCount;
} MemoryStatistics;
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    // appName is used to create dump file, pass NULL to avoid it
    FORGE_API bool initMemAlloc(const char* appName);
    FORGE_API void exitMemAlloc(void);

#ifdef ENABLE_MEMORY_TRACKING
    FORGE_API MemoryStatistics memGetStatistics(void);
#endif

    FORGE_API void* tf_malloc_internal(size_t size, const char* f, int l, const char* sf);
    FORGE_API void* tf_memalign_internal(size_t align, size_t size, const char* f, int l, const char* sf);
    FORGE_API void* tf_calloc_internal(size_t count, size_t size, const char* f, int l, const char* sf);
    FORGE_API void* tf_calloc_memalign_internal(size_t count, size_t align, size_t size, const char* f, int l, const char* sf);
    FORGE_API void* tf_realloc_internal(void* ptr, size_t size, const char* f, int l, const char* sf);
    FORGE_API void  tf_free_internal(void* ptr, const char* f, int l, const char* sf);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
template<typename T, typename... Args>
static T* tf_placement_new(void* ptr, Args&&... args)
{
    return new (ptr) T(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
static T* tf_new_internal(const char* f, int l, const char* sf, Args&&... args)
{
    T* ptr = (T*)tf_memalign_internal(alignof(T), sizeof(T), f, l, sf);
    return tf_placement_new<T>(ptr, std::forward<Args>(args)...);
}

template<typename T>
static void tf_delete_internal(T* ptr, const char* f, int l, const char* sf)
{
    if (ptr)
    {
        ptr->~T();
        tf_free_internal(ptr, f, l, sf);
    }
}
#endif

#ifndef tf_malloc
#define tf_malloc(size) tf_malloc_internal(size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef tf_memalign
#define tf_memalign(align, size) tf_memalign_internal(align, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef tf_calloc
#define tf_calloc(count, size) tf_calloc_internal(count, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef tf_calloc_memalign
#define tf_calloc_memalign(count, align, size) tf_calloc_memalign_internal(count, align, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef tf_realloc
#define tf_realloc(ptr, size) tf_realloc_internal(ptr, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef tf_free
#define tf_free(ptr) tf_free_internal(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif

#ifdef __cplusplus
#ifndef tf_new
#define tf_new(ObjectType, ...) tf_new_internal<ObjectType>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef tf_delete
#define tf_delete(ptr) tf_delete_internal(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif
#endif

#endif

#ifndef IMEMORY_FROM_HEADER
#ifndef malloc
#define malloc(size) static_assert(false, "Please use tf_malloc");
#endif
#ifndef calloc
#define calloc(count, size) static_assert(false, "Please use tf_calloc");
#endif
#ifndef memalign
#define memalign(align, size) static_assert(false, "Please use tf_memalign");
#endif
#ifndef realloc
#define realloc(ptr, size) static_assert(false, "Please use tf_realloc");
#endif
#ifndef free
#define free(ptr) static_assert(false, "Please use tf_free");
#endif

#ifdef __cplusplus
#ifndef new
#define new static_assert(false, "Please use tf_placement_new");
#endif
#ifndef delete
#define delete static_assert(false, "Please use tf_free with explicit destructor call");
#endif
#endif

#endif

#ifdef IMEMORY_FROM_HEADER
#undef IMEMORY_FROM_HEADER
#endif
