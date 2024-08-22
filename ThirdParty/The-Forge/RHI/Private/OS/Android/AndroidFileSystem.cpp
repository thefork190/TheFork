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

#include <android/asset_manager.h>
#include <dirent.h>
#include <errno.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include "IFileSystem.h"
#include "ILog.h"
#include "IOperatingSystem.h"

#include "IMemory.h"

extern "C"
{
    bool fsIsBundledResourceDir(ResourceDirectory resourceDir);
    bool fsMergeDirAndFileName(const char* dir, const char* path, char separator, size_t dstSize, char* dst);
}

static ANativeActivity* pNativeActivity = NULL;
static AAssetManager*   pAssetManager = NULL;

static bool        gInitialized = false;
static const char* gResourceMounts[RM_COUNT];
const char*        ioAndroidGetResourceMount(ResourceMount mount) { return gResourceMounts[mount]; }

struct AndroidFileStream
{
    AAsset* asset;
};

#define ASD(name, fs) struct AndroidFileStream* name = (struct AndroidFileStream*)(fs)->mUser.data

static size_t ioAssetStreamRead(FileStream* fs, void* dst, size_t size)
{
    ASD(stream, fs);
    return AAsset_read(stream->asset, dst, size);
}

static bool ioAssetStreamSeek(FileStream* fs, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    int origin = SEEK_SET;
    switch (baseOffset)
    {
    case SBO_START_OF_FILE:
        origin = SEEK_SET;
        break;
    case SBO_CURRENT_POSITION:
        origin = SEEK_CUR;
        break;
    case SBO_END_OF_FILE:
        origin = SEEK_END;
        break;
    }
    ASD(stream, fs);
    return AAsset_seek64(stream->asset, seekOffset, origin) != -1;
}

static ssize_t ioAssetStreamGetPosition(FileStream* fs)
{
    ASD(stream, fs);
    return (ssize_t)AAsset_seek64(stream->asset, 0, SEEK_CUR);
}

static ssize_t ioAssetStreamGetSize(FileStream* fs)
{
    ASD(stream, fs);
    return AAsset_getLength64(stream->asset);
}

static bool ioAssetStreamIsAtEnd(FileStream* fs)
{
    ASD(stream, fs);
    return AAsset_getRemainingLength64(stream->asset) == 0;
}

static bool ioAssetStreamClose(FileStream* fs)
{
    ASD(stream, fs);
    AAsset_close(stream->asset);
    return true;
}

extern IFileSystem gUnixSystemFileIO;

bool ioAssetStreamOpen(IFileSystem* io, ResourceDirectory rd, const char* fileName, FileMode mode, FileStream* fs)
{
    // Cant write to system files
    if (RD_SYSTEM == rd && (mode & FM_WRITE))
    {
        LOGF(LogLevel::eERROR, "Trying to write to system file with FileMode '%d'", mode);
        return false;
    }

    if (!fsIsBundledResourceDir(rd))
        return fsIoOpenStreamFromPath(&gUnixSystemFileIO, rd, fileName, mode, fs);

    char filePath[FS_MAX_PATH];
    fsMergeDirAndFileName(fsGetResourceDirectory(rd), fileName, '/', sizeof filePath, filePath);

    if ((mode & FM_WRITE) != 0)
    {
        LOGF(LogLevel::eERROR, "Cannot open %s with mode %i: the Android bundle is read-only.", filePath, mode);
        return false;
    }

    AAsset* file = AAssetManager_open(pAssetManager, filePath, AASSET_MODE_BUFFER);
    if (!file)
    {
        LOGF(LogLevel::eERROR, "Failed to open '%s' with mode %i.", filePath, mode);
        return false;
    }

    ASD(stream, fs);

    stream->asset = file;

    fs->mMode = mode;
    fs->pIO = io;
    fs->mMount = fsGetResourceDirectoryMount(rd);

    if ((mode & FM_READ) && (mode & FM_APPEND) && !(mode & FM_WRITE))
    {
        if (!io->Seek(fs, SBO_END_OF_FILE, 0))
        {
            ioAssetStreamClose(fs);
            return false;
        }
    }
    return true;
}

static IFileSystem gBundledFileIO = {
    ioAssetStreamOpen,    ioAssetStreamClose,        ioAssetStreamRead,    NULL,
    ioAssetStreamSeek,    ioAssetStreamGetPosition,  ioAssetStreamGetSize, NULL,
    ioAssetStreamIsAtEnd, ioAndroidGetResourceMount,
};

IFileSystem* pSystemFileIO = &gBundledFileIO;

bool initFileSystem(FileSystemInitDesc* pDesc)
{
    if (gInitialized)
    {
        LOGF(LogLevel::eWARNING, "FileSystem already initialized.");
        return true;
    }
    ASSERT(pDesc);

    for (uint32_t i = 0; i < RM_COUNT; ++i)
        gResourceMounts[i] = "";

    pNativeActivity = (ANativeActivity*)pDesc->pPlatformData;
    ASSERT(pNativeActivity);

    pAssetManager = pNativeActivity->assetManager;
    gResourceMounts[RM_CONTENT] = "\0";
    gResourceMounts[RM_DEBUG] = pNativeActivity->externalDataPath;
    gResourceMounts[RM_DOCUMENTS] = pNativeActivity->internalDataPath;
    gResourceMounts[RM_SAVE_0] = pNativeActivity->externalDataPath;
    gResourceMounts[RM_SYSTEM] = "/proc/";

    // Override Resource mounts
    for (uint32_t i = 0; i < RM_COUNT; ++i)
    {
        if (pDesc->pResourceMounts[i])
            gResourceMounts[i] = pDesc->pResourceMounts[i];
    }

    gInitialized = true;
    return true;
}

void exitFileSystem() { gInitialized = false; }
