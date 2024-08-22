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

#include <errno.h>

#include "bstrlib/bstrlib.h"

#include "IFileSystem.h"
#include "ILog.h"
#include "IThread.h"
#include "ITime.h"

#include "IMemory.h"

/************************************************************************/
// MARK: - Minor filename manipulation
/************************************************************************/

static inline bool isDirectorySeparator(char c) { return (c) == '/' || (c) == '\\'; }

static inline char separatorFilter(char c, char separator) { return isDirectorySeparator(c) ? separator : c; }

static inline bool isDriveLetter(const char* path) { return path[0] && path[1] == ':' && isDirectorySeparator(path[2]); }

FORGE_API size_t fsNormalizePathContinue(const char* nextPath, char separator, const char* beg, char* cur, const char* end);

FORGE_API bool fsIsNormalizedPath(const char* path, char separator)
{
    const char* cur = path;

    // skip drive letter
    if (isDriveLetter(cur))
        cur += 2;

    // Whenever we found any entry except ".."
    bool realEntryFound = 0;

    // -1  cur is not an entry start
    // 0   cur must be an entry start
    // >0  number of dots in the beginning of the entry
    int dotCounter = -1;

    if (cur[0] == '.')
    {
        if (cur[1] == 0)
            return true;

        dotCounter = 1;
        ++cur;
    }

    for (;; ++cur)
    {
        char c = *cur;
        bool isSep = isDirectorySeparator(c);
        if (isSep || !c)
        {
            switch (dotCounter)
            {
            case 0: // double separator "//" detected
                if (isSep)
                    goto RETURN_FALSE;
                break;
            case 1: // entry "." is detected
                goto RETURN_FALSE;
            case 2: // entry ".." is detected
                if (realEntryFound)
                    goto RETURN_FALSE;
                break;
            }

            if (!c)
                break;

            // wrong separator
            if (c != separator)
                goto RETURN_FALSE;

            dotCounter = 0;
        }
        else if (c == '.')
        {
            if (dotCounter >= 0)
            {
                ++dotCounter;
                if (dotCounter > 2)
                    realEntryFound = true;
            }
        }
        else
        {
            realEntryFound = true;
            dotCounter = -1;
        }
    }

    return true;

RETURN_FALSE:
    // Test if opinion of fsNormalizePath and fsIsNormalizedPath matches.
    // Test failure can cause infinite recursion here.
#if defined(FORGE_DEBUG)
    if (strlen(path) > FS_MAX_PATH - 1)
        return false;
    char buffer[FS_MAX_PATH] = { 0 };
    fsNormalizePathContinue(path, separator, buffer, buffer, (char*)UINTPTR_MAX);
    ASSERT(strcmp(buffer, path) != 0);
#endif
    return false;
}

FORGE_API size_t fsNormalizePathContinue(const char* nextPath, char separator, const char* beg, char* cur, const char* end)
{
    ASSERT(nextPath);
    ASSERT(separator);
    ASSERT(beg <= cur && cur <= end && beg < end);
    if (end <= beg)
        return 0;

    const bool notEmptyAtStart = cur > beg;

    // noback points to after last separator of "../../../" sequence

    // e.g.:
    // /a/../../b/../c/
    //  ^noback

    // /../a/../b/
    //     ^ noback

    // set noback to beg (dropping const)
    char* noback = cur - (cur - beg);

    if (cur > beg)
    {
        if (isDriveLetter(noback))
            noback += 2;

        if (separatorFilter(*noback, separator) == separator)
            ++noback;

        for (; noback <= cur - 3;)
        {
            char c = noback[0];
            char nc = noback[1];
            char nnc = noback[2];

            if (c != '.' || nc != '.' || nnc != separator)
                break;

            noback += 3;
        }
    }
    else
    {
        if (noback == cur && separatorFilter(*nextPath, separator) == separator)
            ++noback;
    }

    for (const char* src = nextPath; *src; ++src)
    {
        char c = separatorFilter(*src, separator);

        if (c == separator)
        {
            if (                                              // test for "a//b" case
                (cur != beg && isDirectorySeparator(cur[-1])) //
                ||                                            // test for "a/..//b" case
                (cur == beg && src != nextPath))
            {
                // Detailed explanaition

                // "a/..///b" path resolves to "//b".
                // "(cur == dst && src != path)" fixes this to "b"

                // "a/b/..///c" resolves to "a///c"
                // (isDirectorySeparator(cur[-1])) fixes this to "a/c".

                // Phew!
                continue;
            }

            *(cur++) = separator;
            if (cur == end)
                break;
            continue;
        }

        const bool entryStart = cur == beg || cur[-1] == separator;

        if (!entryStart || c != '.')
        {
            *(cur++) = c;
            if (cur == end)
                break;
            continue;
        }

        // c='.' nc='.' nnc='/'

        const char nc = separatorFilter(src[1], separator);
        if (!nc)
            break;

        if (nc == separator)
        {
            // resolve "./"
            ++src;
            continue;
        }

        const char nnc = separatorFilter(src[2], separator);

        // backlink is ".." entry
        const bool backlink = nc == '.' && (nnc == separator || !nnc);

        // Are we having parent dir to resolve backlink?
        bool isNoback = cur == noback;

        if (backlink && isNoback)
            noback += 3; // strlen("../");

        if (!backlink || isNoback)
        {
            // skip unresolvable "../" or whatever characters here
            *(cur++) = c;
            if (cur == end)
                break;
            *(cur++) = nc;
            if (cur == end)
                break;
            if (nnc)
            {
                *(cur++) = nnc;
                if (cur == end)
                    break;
            }
        }
        else
        {
            // resolve ".." (remove "parentdir/..")
            for (cur -= 2; cur >= beg && *cur != separator; --cur)
                ;
            ++cur;
        }

        // In the for(;;) loop we are skipping 1 char
        // We are passing ".." or "../" here.
        // We need to skip only 2 chars if '/' not presented
        src += 1 + (nnc != 0);
    }

    size_t size = (size_t)(cur - beg);

    ASSERT(cur <= end);

    // failure
    if (cur == end)
    {
        cur[-1] = 0;
        return (size_t)(end - beg);
    }

    // If input wasn't empty strings, write "."
    if (!size && (notEmptyAtStart || *nextPath))
    {
        *cur = '.';
        ++cur;
    }

    // success
    *cur = 0;
    ASSERT(fsIsNormalizedPath(beg, separator));
    return size;
}

FORGE_API bool fsMergeDirAndFileName(const char* prePath, const char* postPath, char separator, size_t outputSize, char* output)
{
    *output = 0;

    size_t outputLength = fsNormalizePathContinue(prePath, separator, output, output, output + outputSize);

    if (                                         // put separator between paths, if conditions are met:
        outputLength &&                          // if path isn't empty
        output[outputLength - 1] != separator && // if separator is missing
        !isDirectorySeparator(*postPath))        // if separator is missing in postpath
    {
        output[outputLength++] = separator;
        output[outputLength] = 0;
    }

    outputLength = fsNormalizePathContinue(postPath, separator, output, output + outputLength, output + outputSize);

    ASSERT(outputLength <= outputSize);

    bool success = outputLength < outputSize;
    if (!success)
    {
        LOGF(eERROR, "Failed to append path: path exceeds path limit of %llu.", (unsigned long long)outputSize);
        LOGF(eERROR, "Base path is '%s'", prePath);
        LOGF(eERROR, "Appending path is '%s'", postPath);
        LOGF(eERROR, "Only this part that fits: '%s'", output);
    }

    // Delete any trailing directory separator.
    if (outputLength && output[outputLength - 1] == separator)
        output[outputLength - 1] = 0;
    return success;
}

// output size is FS_MAX_PATH
FORGE_API void fsAppendPathExtension(const char* basePath, const char* extension, char* output)
{
    size_t       extensionLength = strlen(extension);
    const size_t baseLength = strlen(basePath);

    // + 1 due to a possible added directory slash.
    const size_t maxPathLength = baseLength + extensionLength + 1;

    if (!VERIFYMSG(maxPathLength < FS_MAX_PATH, "Extension path length '%zu' greater than FS_MAX_PATH", maxPathLength))
    {
        return;
    }

    memcpy(output, basePath, baseLength);
    output[baseLength] = 0;

    if (extensionLength == 0)
    {
        return;
    }

    // Extension validation
    for (size_t i = 0; i < extensionLength; i += 1)
    {
        LOGF_IF(eERROR, isDirectorySeparator(extension[i]), "Extension '%s' contains directory specifiers", extension);
        ASSERT(!isDirectorySeparator(extension[i]));
    }
    LOGF_IF(eERROR, extension[extensionLength - 1] == '.', "Extension '%s' ends with a '.' character", extension);

    if (extension[0] == '.')
    {
        extension += 1;
        extensionLength -= 1;
    }

    output[baseLength] = '.';
    memcpy(output + baseLength + 1, extension, extensionLength);
    output[baseLength + 1 + extensionLength] = 0;
}

// output size is FS_MAX_PATH
FORGE_API void fsGetPathExtension(const char* path, char* output)
{
    size_t pathLength = strlen(path);
    ASSERT(pathLength != 0);
    const char* dotLocation = strrchr(path, '.');
    if (dotLocation == NULL)
    {
        return;
    }
    dotLocation += 1;
    const size_t extensionLength = strlen(dotLocation);

    // Make sure it is not "../"
    if (extensionLength == 0 || isDirectorySeparator(dotLocation[0]))
    {
        return;
    }

    strncpy(output, dotLocation, FS_MAX_PATH - 1);
    output[FS_MAX_PATH - 1] = 0;
}

// output size is FS_MAX_PATH
FORGE_API void fsReplacePathExtension(const char* path, const char* newExtension, char* output)
{
    size_t       newExtensionLength = strlen(newExtension);
    const size_t baseLength = strlen(path);

    // + 1 due to a possible added directory slash.
    const size_t maxPathLength = baseLength + newExtensionLength + 1;

    ASSERT(baseLength != 0);
    if (!VERIFYMSG(maxPathLength < FS_MAX_PATH, "New extension path length '%zu' greater than FS_MAX_PATH", maxPathLength))
    {
        return;
    }

    strncpy(output, path, FS_MAX_PATH - 1);
    output[FS_MAX_PATH - 1] = 0;

    size_t newPathLength = baseLength;

    if (newExtensionLength == 0)
    {
        return;
    }

    // Extension validation
    for (size_t i = 0; i < newExtensionLength; i += 1)
    {
        LOGF_IF(eERROR, isDirectorySeparator(newExtension[i]), "Extension '%s' contains directory specifiers", newExtension);
        ASSERT(!isDirectorySeparator(newExtension[i]));
    }
    LOGF_IF(eERROR, newExtension[newExtensionLength - 1] == '.', "Extension '%s' ends with a '.' character", newExtension);

    if (newExtension[0] == '.')
    {
        newExtension += 1; // Skip over the first '.'.
        newExtensionLength -= 1;
    }

    char currentExtension[FS_MAX_PATH] = { 0 };
    fsGetPathExtension(path, currentExtension);
    newPathLength -= strlen(currentExtension);
    if (output[newPathLength - 1] != '.')
    {
        output[newPathLength] = '.';
        newPathLength += 1;
    }

    strncpy(output + newPathLength, newExtension, newExtensionLength);
    output[newPathLength + newExtensionLength] = '\0';
}

// output size is FS_MAX_PATH
FORGE_API void fsGetParentPath(const char* path, char* output)
{
    size_t pathLength = strlen(path);
    if (!pathLength)
    {
        // We do this just before exit instead of at the beggining of the function to support calls
        // like fsGetParentPath(path, path), were the output path points to the same memory as the input path
        output[0] = 0;
        return;
    }

    const char* dirSeperatorLoc = NULL;
    for (const char* cur = path + pathLength - 1; cur >= path; --cur)
    {
        if (!isDirectorySeparator(*cur))
            continue;
        dirSeperatorLoc = cur;
        break;
    }

    if (!dirSeperatorLoc)
    {
        // We do this just before exit instead of at the beggining of the function to support calls
        // like fsGetParentPath(path, path), were the output path points to the same memory as the input path
        output[0] = 0;
        return;
    }

    size_t outputLength = (size_t)(dirSeperatorLoc - path);

    size_t reslen = outputLength > FS_MAX_PATH - 1 ? FS_MAX_PATH - 1 : outputLength;
    memcpy(output, path, reslen);
    output[reslen] = 0;
}

FORGE_API void fsGetPathFileName(const char* path, char* output)
{
    const size_t pathLength = strlen(path);
    ASSERT(pathLength != 0);

    char parentPath[FS_MAX_PATH] = { 0 };
    fsGetParentPath(path, parentPath);
    size_t parentPathLength = strlen(parentPath);

    if (parentPathLength < pathLength && (isDirectorySeparator(path[parentPathLength])))
    {
        parentPathLength += 1;
    }

    char extension[FS_MAX_PATH] = { 0 };
    fsGetPathExtension(path, extension);

    // Include dot in the length
    const size_t extensionLength = extension[0] != 0 ? strlen(extension) + 1 : 0;

    const size_t outputLength = pathLength - parentPathLength - extensionLength;
    strncpy(output, path + parentPathLength, outputLength);
    output[outputLength] = '\0';
}

/************************************************************************/
// MARK: - Filesystem
/************************************************************************/

#define MEMORY_STREAM_GROW_SIZE 4096
#define STREAM_COPY_BUFFER_SIZE 4096
#define STREAM_FIND_BUFFER_SIZE 1024

typedef struct ResourceDirectoryInfo
{
    IFileSystem*  pIO;
    ResourceMount mMount;
    char          mPath[FS_MAX_PATH];
    bool          mBundled;
} ResourceDirectoryInfo;

static ResourceDirectoryInfo gResourceDirectories[RD_COUNT] = { { 0 } };

// required by NXFileSystem.cpp
FORGE_API bool fsIsBundledResourceDir(ResourceDirectory resourceDir) { return gResourceDirectories[resourceDir].mBundled; }

/************************************************************************/
// Memory Stream Functions
/************************************************************************/

struct MemoryStream
{
    uint8_t*    pBuffer;
    uintptr_t   mCursor;
    uintptr_t   mCapacity;
    intptr_t    mSize;
    uintptr_t   mIsOwner;
    FileStream* wrappedStream;
};

#define MEMSD(name, fs) struct MemoryStream* name = (struct MemoryStream*)(fs)->mUser.data

static inline size_t MemoryStreamAvailableSize(struct MemoryStream* stream, size_t requestedSize)
{
    ssize_t sizeLeft = (ssize_t)stream->mSize - (ssize_t)stream->mCursor;
    if (sizeLeft < 0)
        sizeLeft = 0;
    return (ssize_t)requestedSize > sizeLeft ? (size_t)sizeLeft : requestedSize;
}

static bool ioMemoryStreamClose(FileStream* fs)
{
    MEMSD(stream, fs);

    if (stream->mIsOwner)
    {
        tf_free(stream->pBuffer);
    }

    if (stream->wrappedStream)
    {
        fsCloseStream(stream->wrappedStream);
        tf_free(stream->wrappedStream);
    }

    return true;
}

static size_t ioMemoryStreamRead(FileStream* fs, void* dst, size_t size)
{
    if (!(fs->mMode & FM_READ))
    {
        LOGF(eWARNING, "Attempting to read from stream that doesn't have FM_READ flag.");
        return 0;
    }

    MEMSD(stream, fs);

    if ((intptr_t)stream->mCursor >= stream->mSize)
    {
        return 0;
    }

    size_t bytesToRead = MemoryStreamAvailableSize(stream, size);
    memcpy(dst, stream->pBuffer + stream->mCursor, bytesToRead);
    stream->mCursor += bytesToRead;
    return bytesToRead;
}

static size_t ioMemoryStreamWrite(FileStream* fs, const void* src, size_t size)
{
    if (!(fs->mMode & FM_WRITE))
    {
        LOGF(eWARNING, "Attempting to write to stream that doesn't have FM_WRITE flag.");
        return 0;
    }

    MEMSD(stream, fs);

    if (stream->mCursor > (size_t)stream->mSize)
    {
        LOGF(eWARNING, "Creating discontinuity in initialized memory in memory stream.");
    }

    size_t availableCapacity = 0;
    if (stream->mCapacity >= stream->mCursor)
        availableCapacity = stream->mCapacity - stream->mCursor;

    if (size > availableCapacity)
    {
        size_t newCapacity = stream->mCursor + size;

        newCapacity =
            MEMORY_STREAM_GROW_SIZE * (newCapacity / MEMORY_STREAM_GROW_SIZE + (newCapacity % MEMORY_STREAM_GROW_SIZE == 0 ? 0 : 1));

        void* newBuffer = tf_realloc(stream->pBuffer, newCapacity);
        if (!newBuffer)
        {
            LOGF(eERROR,
                 "Failed to reallocate memory stream buffer with new capacity "
                 "%llu.",
                 (unsigned long long)newCapacity);
            return 0;
        }

        stream->pBuffer = (uint8_t*)newBuffer;
        stream->mCapacity = newCapacity;
    }

    memcpy(stream->pBuffer + stream->mCursor, src, size);
    stream->mCursor += size;

    stream->mSize = stream->mSize > (ssize_t)stream->mCursor ? stream->mSize : (ssize_t)stream->mCursor;
    return size;
}

static bool ioMemoryStreamSeek(FileStream* fs, SeekBaseOffset baseOffset, ssize_t seekOffset)
{
    MEMSD(stream, fs);

    switch (baseOffset)
    {
    case SBO_START_OF_FILE:
    {
        if (seekOffset < 0 || seekOffset > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)seekOffset;
    }
    break;
    case SBO_CURRENT_POSITION:
    {
        ssize_t newPosition = (ssize_t)stream->mCursor + seekOffset;
        if (newPosition < 0 || newPosition > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)newPosition;
    }
    break;
    case SBO_END_OF_FILE:
    {
        ssize_t newPosition = (ssize_t)stream->mSize + seekOffset;
        if (newPosition < 0 || newPosition > stream->mSize)
        {
            return false;
        }
        stream->mCursor = (size_t)newPosition;
    }
    break;
    }
    return true;
}

static ssize_t ioMemoryStreamGetPosition(FileStream* fs)
{
    MEMSD(stream, fs);
    return (ssize_t)stream->mCursor;
}

static ssize_t ioMemoryStreamGetSize(FileStream* fs)
{
    MEMSD(stream, fs);
    return stream->mSize;
}

static bool ioMemoryStreamFlush(FileStream* fs)
{
    (void)fs;
    // No-op.
    return true;
}

static bool ioMemoryStreamIsAtEnd(FileStream* fs)
{
    MEMSD(stream, fs);
    return (ssize_t)stream->mCursor == stream->mSize;
}

static bool ioMemoryStreamMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
{
    if (fs->mMode & FM_WRITE)
        return false;

    MEMSD(stream, fs);
    *outSize = stream->mCapacity;
    *outData = stream->pBuffer;
    return true;
}

static IFileSystem gMemoryFileIO = {
    NULL,
    ioMemoryStreamClose,
    ioMemoryStreamRead,
    ioMemoryStreamWrite,
    ioMemoryStreamSeek,
    ioMemoryStreamGetPosition,
    ioMemoryStreamGetSize,
    ioMemoryStreamFlush,
    ioMemoryStreamIsAtEnd,
    NULL,
    NULL,
    NULL,
    ioMemoryStreamMemoryMap,
    NULL,
};

/************************************************************************/
// File IO
/************************************************************************/

bool fsIsMemoryStream(FileStream* pStream) { return pStream->pIO == &gMemoryFileIO; }

bool fsIsSystemFileStream(FileStream* pStream) { return pStream->pIO == pSystemFileIO; }

bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* fs)
{
    memset(fs, 0, sizeof *fs);

    size_t size = buffer ? bufferSize : 0;
    size_t capacity = bufferSize;
    // Move cursor to the end for appending buffer
    size_t cursor = (mode & FM_APPEND) ? size : 0;

    // For write streams we have to own the memory as we might need to resize it
    if ((mode & FM_WRITE) && (!owner || !buffer))
    {
        // make capacity multiple of MEMORY_STREAM_GROW_SIZE
        capacity = MEMORY_STREAM_GROW_SIZE * (capacity / MEMORY_STREAM_GROW_SIZE + (capacity % MEMORY_STREAM_GROW_SIZE == 0 ? 0 : 1));
        void* newBuffer = tf_malloc(capacity);
        ASSERT(newBuffer);
        if (buffer)
            memcpy(newBuffer, buffer, size);

        buffer = newBuffer;
        owner = true;
    }

    fs->pIO = &gMemoryFileIO;
    fs->mMode = mode;

    MEMSD(stream, fs);

    stream->pBuffer = (uint8_t*)buffer;
    stream->mCursor = cursor;
    stream->mCapacity = capacity;
    stream->mIsOwner = owner;
    stream->mSize = (ssize_t)size;
    return true;
}

/// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
/// to read from or modify the file. May return NULL if the file could not be opened.
bool fsOpenStreamFromPath(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut)
{
    IFileSystem* io = gResourceDirectories[resourceDir].pIO;
    if (!io)
    {
        LOGF(eERROR,
             "Trying to get an unset resource directory '%d' to open stream for '%s', make sure the resourceDirectory is set on start of "
             "the application",
             resourceDir, fileName ? fileName : "<NULL>");
        return false;
    }

    return fsIoOpenStreamFromPath(io, resourceDir, fileName, mode, pOut);
}

size_t fsReadBstringFromStream(FileStream* stream, bstring* pStr, size_t symbolsCount)
{
    ASSERT(bisvalid(pStr));

    // Read until the end of the file
    if (symbolsCount == SIZE_MAX)
    {
        bassignliteral(pStr, "");
        int readBytes = 0;
        // read one page at a time
        do
        {
            balloc(pStr, pStr->slen + 512);
            readBytes = (int)fsReadFromStream(stream, pStr->data + pStr->slen, 512);
            ASSERT(INT_MAX - pStr->slen > readBytes && "Integer overflow");
            pStr->slen += readBytes;
        } while (readBytes == 512);
        balloc(pStr, pStr->slen + 1);
        pStr->data[pStr->slen] = '\0';
        return (size_t)pStr->slen;
    }

    ASSERT(symbolsCount < (size_t)INT_MAX);

    bassignliteral(pStr, "");
    balloc(pStr, (int)symbolsCount + 1);
    size_t readBytes = fsReadFromStream(stream, pStr->data, symbolsCount);
    pStr->data[readBytes] = '\0';
    pStr->slen = (int)readBytes;
    return readBytes;
}

bool fsFindStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition)
{
    ASSERT(pStream && pFind && pPosition);
    ASSERT(findSize < STREAM_FIND_BUFFER_SIZE);
    ASSERT(maxSeek >= 0);
    if (findSize > (size_t)maxSeek)
        return false;
    if (findSize == 0)
        return true;

    const uint8_t* pattern = (const uint8_t*)pFind;
    // Fill longest proper prefix which is also suffix(lps) array
    uint32_t       lps[STREAM_FIND_BUFFER_SIZE];

    lps[0] = 0;
    for (uint32_t i = 1, prefixLength = 0; i < findSize; ++i)
    {
        while (prefixLength > 0 && pattern[i] != pattern[prefixLength])
        {
            prefixLength = lps[prefixLength - 1];
        }
        if (pattern[i] == pattern[prefixLength])
            ++prefixLength;
        lps[i] = prefixLength;
    }

    size_t patternPos = 0;
    for (; maxSeek != 0; --maxSeek)
    {
        uint8_t byte;
        if (fsReadFromStream(pStream, &byte, sizeof(byte)) != sizeof(byte))
            return false;

        while (true)
        {
            if (byte == pattern[patternPos])
            {
                ++patternPos;
                if (patternPos == findSize)
                {
                    bool result = fsSeekStream(pStream, SBO_CURRENT_POSITION, -(ssize_t)findSize);
                    UNREF_PARAM(result);
                    ASSERT(result);
                    *pPosition = fsGetStreamSeekPosition(pStream);
                    return true;
                }
                break;
            }

            if (patternPos == 0)
                break;

            patternPos = lps[patternPos - 1];
        }
    }
    return false;
}

bool fsFindReverseStream(FileStream* pStream, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition)
{
    ASSERT(pStream && pFind && pPosition);
    ASSERT(findSize < STREAM_FIND_BUFFER_SIZE);
    ASSERT(maxSeek >= 0);
    if (findSize > (size_t)maxSeek)
        return false;
    if (findSize == 0)
        return true;

    const uint8_t* pattern = (const uint8_t*)pFind;
    // Fill longest proper prefix which is also suffix(lps) array
    uint32_t       lps[STREAM_FIND_BUFFER_SIZE];

    lps[findSize - 1] = 0;

    // Doing reverse pass

    for (uint32_t i = (uint32_t)findSize - 1, prefixLength = 0; i-- > 0;)
    {
        uint32_t prefixPos = (uint32_t)findSize - 1 - prefixLength;
        while (prefixLength > 0 && pattern[i] != pattern[prefixPos])
        {
            prefixLength = lps[prefixPos + 1];
            prefixPos = (uint32_t)findSize - 1 - prefixLength;
        }
        if (pattern[i] == pattern[prefixPos])
            ++prefixLength;
        lps[i] = prefixLength;
    }

    size_t patternPos = findSize - 1;
    for (; maxSeek != 0; --maxSeek)
    {
        uint8_t byte;
        if (!fsSeekStream(pStream, SBO_CURRENT_POSITION, -1))
            return false;

        size_t readBytes = fsReadFromStream(pStream, &byte, sizeof(byte));
        ASSERT(readBytes == 1);
        UNREF_PARAM(readBytes);
        fsSeekStream(pStream, SBO_CURRENT_POSITION, -1);

        while (true)
        {
            if (byte == pattern[patternPos])
            {
                if (patternPos-- == 0)
                {
                    *pPosition = fsGetStreamSeekPosition(pStream);
                    return true;
                }
                break;
            }
            else if (patternPos == findSize - 1)
                break;
            else
                patternPos = findSize - 1 - lps[patternPos + 1];
        }
    }
    return false;
}

FORGE_API bool fsStreamWrapMemoryMap(FileStream* fs)
{
    if (fsIsMemoryStream(fs))
        return true;

    size_t      size;
    const void* mem;
    if (!fsStreamMemoryMap(fs, &size, &mem))
        return false;

    FileStream wrapFs;
    if (!fsOpenStreamFromMemory(mem, size, FM_READ, false, &wrapFs))
    {
        // NOTE: fsOpenStreamFromMemory never returns false
        LOGF(eERROR, "Failed to open stream from memory");
        return false;
    }

    MEMSD(stream, &wrapFs);
    stream->mCursor = (size_t)fsGetStreamSeekPosition(fs);

    FileStream* wrappedFs = tf_malloc(sizeof *wrappedFs);
    if (!wrappedFs)
    {
        fsCloseStream(&wrapFs);
        return false;
    }

    memcpy(wrappedFs, fs, sizeof *wrappedFs);
    stream->wrappedStream = wrappedFs;

    *fs = wrapFs;
    return true;
}

/************************************************************************/
// Platform independent directory queries
/************************************************************************/

const char* fsGetResourceDirectory(ResourceDirectory resourceDir)
{
    const ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

    if (!dir->pIO)
    {
        LOGF_IF(eERROR, !dir->mPath[0],
                "Trying to get an unset resource directory '%d', make sure the resourceDirectory is set on start of the application",
                resourceDir);
        ASSERT(dir->mPath[0] != 0);
    }
    return dir->mPath;
}

ResourceMount fsGetResourceDirectoryMount(ResourceDirectory resourceDir) { return gResourceDirectories[resourceDir].mMount; }

void fsSetPathForResourceDir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder)
{
    ASSERT(pIO);
    ResourceDirectoryInfo* dir = &gResourceDirectories[resourceDir];

    if (dir->mPath[0] != 0)
    {
        LOGF(eWARNING, "Resource directory {%d} already set on:'%s'", resourceDir, dir->mPath);
        return;
    }

#if !defined(FORGE_DEBUG) && !defined(ENABLE_LOGGING)
    // Ignore RM_DEBUG on shipping builds, it's only supposed to be used in testing
    if (mount == RM_DEBUG)
    {
        LOGF(eWARNING, "RM_DEBUG is not available on shipping builds");
        return;
    }
#endif

    dir->mMount = mount;

    if (RM_CONTENT == mount)
    {
        dir->mBundled = true;
    }

    char resourcePath[FS_MAX_PATH] = { 0 };
    fsMergeDirAndFileName(pIO->GetResourceMount ? pIO->GetResourceMount(mount) : "", bundledFolder, '/', sizeof resourcePath, resourcePath);
    strncpy(dir->mPath, resourcePath, FS_MAX_PATH);
    dir->pIO = pIO;

    if (!dir->mBundled && dir->mPath[0] != 0)
    {
        if (!fsCreateResourceDirectory(resourceDir))
        {
            LOGF(eERROR, "Could not create direcotry '%s' in filesystem", resourcePath);
        }
    }
}

/************************************************************************/
/************************************************************************/
