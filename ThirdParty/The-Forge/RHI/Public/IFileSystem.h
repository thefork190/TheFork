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

#pragma once
#include "Config.h"

#include "IOperatingSystem.h"

// IOS Simulator paths can get a bit longer then 256 bytes
#ifdef TARGET_IOS_SIMULATOR
#define FS_MAX_PATH 320
#else
#define FS_MAX_PATH 512
#endif

struct bstring;

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum ResourceMount
    {
        /// Installed game directory / bundle resource directory
        RM_CONTENT = 0,
        /// For storing debug data such as log files. To be used only during development
        RM_DEBUG,
        /// Documents directory
        RM_DOCUMENTS,
#if defined(ANDROID)
        // System level files (/proc/ or equivalent if available)
        RM_SYSTEM,
#endif
        /// Save game data mount 0
        RM_SAVE_0,

        /// Empty mount for absolute paths
        RM_EMPTY,

        RM_COUNT,
    } ResourceMount;

    typedef enum ResourceDirectory
    {
        /// The main application's shader binaries directory
        RD_SHADER_BINARIES = 0,

        RD_PIPELINE_CACHE,
        /// The main application's texture source directory (TODO processed texture folder)
        RD_TEXTURES,
        RD_COMPILED_MATERIALS,
        RD_MESHES,
        RD_FONTS,
        RD_ANIMATIONS,
        RD_AUDIO,
        RD_GPU_CONFIG,
        RD_LOG,
        RD_SCRIPTS,
        RD_SCREENSHOTS,
        RD_DEBUG,
#if defined(ANDROID)
        // #TODO: Add for others if necessary
        RD_SYSTEM,
#endif
        RD_OTHER_FILES,

        // Libraries can have their own directories.
        // Up to 100 libraries are supported.
        ____rd_lib_counter_begin = RD_OTHER_FILES + 1,

        // Add libraries here
        RD_MIDDLEWARE_0 = ____rd_lib_counter_begin,
        RD_MIDDLEWARE_1,
        RD_MIDDLEWARE_2,
        RD_MIDDLEWARE_3,
        RD_MIDDLEWARE_4,
        RD_MIDDLEWARE_5,
        RD_MIDDLEWARE_6,
        RD_MIDDLEWARE_7,
        RD_MIDDLEWARE_8,
        RD_MIDDLEWARE_9,
        RD_MIDDLEWARE_10,
        RD_MIDDLEWARE_11,
        RD_MIDDLEWARE_12,
        RD_MIDDLEWARE_13,
        RD_MIDDLEWARE_14,
        RD_MIDDLEWARE_15,

        ____rd_lib_counter_end = ____rd_lib_counter_begin + 99 * 2,
        RD_COUNT
    } ResourceDirectory;

    typedef enum SeekBaseOffset
    {
        SBO_START_OF_FILE = 0,
        SBO_CURRENT_POSITION,
        SBO_END_OF_FILE,
    } SeekBaseOffset;

    typedef enum FileMode
    {
        // Get read access for file. Error if file not exist.
        FM_READ = 1 << 0,

        // Get write access for file. File is created if not exist.
        FM_WRITE = 1 << 1,

        // Set initial seek position to the end of file.
        FM_APPEND = 1 << 2,

        // Read access for other processes.
        // Note: flag is required for Windows&Xbox.
        //       On other platforms read access is always available.
        FM_ALLOW_READ = 1 << 4,

        // RW mode
        FM_READ_WRITE = FM_READ | FM_WRITE,

        // W mode and set position to the end
        FM_WRITE_APPEND = FM_WRITE | FM_APPEND,

        // R mode and set position to the end
        FM_READ_APPEND = FM_READ | FM_APPEND,

        // RW mode and set position to the end
        FM_READ_WRITE_APPEND = FM_READ | FM_APPEND,

        // -- mode and -- and also read access for other processes.
        FM_WRITE_ALLOW_READ = FM_WRITE | FM_ALLOW_READ,
        FM_READ_WRITE_ALLOW_READ = FM_READ_WRITE | FM_ALLOW_READ,
        FM_WRITE_APPEND_ALLOW_READ = FM_WRITE_APPEND | FM_ALLOW_READ,
        FM_READ_WRITE_APPEND_ALLOW_READ = FM_READ_WRITE_APPEND | FM_ALLOW_READ,
    } FileMode;

    typedef struct IFileSystem IFileSystem;

    struct FileStreamUserData
    {
        uintptr_t data[6];
    };

    /// After stream is opened, only FileStream::pIO must be used for this stream.
    /// Example:
    ///   io->Open(&stream); // stream is opened
    ///   io->Read(&stream, ...); // bug, potentially uses wrong io on wrong stream.
    ///   stream.pIO->Read(&stream, ...); // correct
    /// The reason for this is that IFileSystem::Open can open stream using another
    /// IFileSystem handle.
    ///
    /// It is best to use IFileSystem IO shortcuts "fsReadFromStream(&stream,...)"
    typedef struct FileStream
    {
        IFileSystem*              pIO;
        FileMode                  mMode;
        ResourceMount             mMount;
        struct FileStreamUserData mUser; // access to this field is IO exclusive
    } FileStream;

    typedef struct FileSystemInitDesc
    {
        const char* pAppName;
        void*       pPlatformData;
        const char* pResourceMounts[RM_COUNT];
    } FileSystemInitDesc;

    struct IFileSystem
    {
        bool (*Open)(IFileSystem* pIO, const ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

        /// Closes and invalidates the file stream.
        bool (*Close)(FileStream* pFile);

        /// Returns the number of bytes read.
        size_t (*Read)(FileStream* pFile, void* outputBuffer, size_t bufferSizeInBytes);

        /// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
        /// Returns the number of bytes written.
        size_t (*Write)(FileStream* pFile, const void* sourceBuffer, size_t byteCount);

        /// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
        bool (*Seek)(FileStream* pFile, SeekBaseOffset baseOffset, ssize_t seekOffset);

        /// Gets the current seek position in the file.
        ssize_t (*GetSeekPosition)(FileStream* pFile);

        /// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
        ssize_t (*GetFileSize)(FileStream* pFile);

        /// Flushes all writes to the file stream to the underlying subsystem.
        bool (*Flush)(FileStream* pFile);

        /// Returns whether the current seek position is at the end of the file stream.
        bool (*IsAtEnd)(FileStream* pFile);

        const char* (*GetResourceMount)(ResourceMount mount);

        // Acquire unique file identifier.
        // Only Archive FS supports it currently.
        bool (*GetFileUid)(IFileSystem* pIO, ResourceDirectory rd, const char* name, uint64_t* outUid);

        // Open file using unique identifier. Use GetFileUid to get uid.
        bool (*OpenByUid)(IFileSystem* pIO, uint64_t uid, FileMode fm, FileStream* pOut);

        // Creates virtual address space of file.
        // When memory mapping is done, file can be accessed just like an array.
        // This is more efficient than using "FILE" stream.
        // Not all platforms are supported.
        // Use fsStreamWrapMemoryMap for strong cross-platform compatibility.
        // This function does read-only memory map.
        bool (*MemoryMap)(FileStream* fs, size_t* outSize, void const** outData);

        void* pUser;
    };

    /// Default file system using C File IO or Bundled File IO (Android) based on the ResourceDirectory
    FORGE_API extern IFileSystem* pSystemFileIO;
    /************************************************************************/
    // MARK: - Initialization
    /************************************************************************/
    /// Initializes the FileSystem API
    FORGE_API bool                initFileSystem(FileSystemInitDesc* pDesc);

    /// Frees resources associated with the FileSystem API
    FORGE_API void exitFileSystem(void);

    /************************************************************************/
    // MARK: - File IO
    /************************************************************************/

    /// Opens the file at `filePath` using the mode `mode`, returning a new FileStream that can be used
    /// to read from or modify the file. May return NULL if the file could not be opened.
    FORGE_API bool fsOpenStreamFromPath(ResourceDirectory resourceDir, const char* fileName, FileMode mode, FileStream* pOut);

    /// Opens a memory buffer as a FileStream, returning a stream that must be closed with `fsCloseStream`.
    /// Use 'fsStreamMemoryMap' to do the opposite.
    FORGE_API bool fsOpenStreamFromMemory(const void* buffer, size_t bufferSize, FileMode mode, bool owner, FileStream* pOut);

    FORGE_API bool fsFindStream(FileStream* fs, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition);
    FORGE_API bool fsFindReverseStream(FileStream* fs, const void* pFind, size_t findSize, ssize_t maxSeek, ssize_t* pPosition);

    /// Checks if stream is a standard system stream
    FORGE_API bool fsIsSystemFileStream(FileStream* fs);
    /// Checks if stream is a memory stream
    FORGE_API bool fsIsMemoryStream(FileStream* fs);

    /// symbolsCount can be SIZE_MAX, then reads until the end of file
    /// appends '\0' to the end of string
    FORGE_API size_t fsReadBstringFromStream(FileStream* stream, struct bstring* pStr, size_t symbolsCount);

    /// Wraps stream into new memory stream using fsStreamMemoryMap
    /// returns true: old stream is wrapped by new one with new IO.
    /// returns false: stream is unaffected.
    /// In both cases stream stays in valid state.
    /// fsCloseStream(FileStream*) takes care of cleaning wrapped stream.
    /// So checking return value is optional.
    FORGE_API bool fsStreamWrapMemoryMap(FileStream* fs);

    /************************************************************************/
    // MARK: - IFileSystem IO shortcuts
    /************************************************************************/

    static inline bool fsIoOpenStreamFromPath(IFileSystem* pIO, const ResourceDirectory rd, const char* fileName, FileMode mode,
                                              FileStream* pOut)
    {
        return pIO->Open(pIO, rd, fileName, mode, pOut);
    }

    /// Closes and invalidates the file stream.
    static inline bool fsCloseStream(FileStream* fs)
    {
        if (!fs->pIO)
            return true;
        bool success = fs->pIO->Close(fs);
        memset(fs, 0, sizeof *fs);
        return success;
    }

    /// Returns the number of bytes read.
    static inline size_t fsReadFromStream(FileStream* fs, void* pOutputBuffer, size_t bufferSizeInBytes)
    {
        return fs->pIO->Read(fs, pOutputBuffer, bufferSizeInBytes);
    }

    /// Reads at most `bufferSizeInBytes` bytes from sourceBuffer and writes them into the file.
    /// Returns the number of bytes written.
    static inline size_t fsWriteToStream(FileStream* fs, const void* pSourceBuffer, size_t byteCount)
    {
        if (!fs->pIO->Write)
            return 0;
        return fs->pIO->Write(fs, pSourceBuffer, byteCount);
    }

    /// Seeks to the specified position in the file, using `baseOffset` as the reference offset.
    static inline bool fsSeekStream(FileStream* fs, SeekBaseOffset baseOffset, ssize_t seekOffset)
    {
        return fs->pIO->Seek(fs, baseOffset, seekOffset);
    }

    /// Gets the current seek position in the file.
    static inline ssize_t fsGetStreamSeekPosition(FileStream* fs) { return fs->pIO->GetSeekPosition(fs); }

    /// Gets the current size of the file. Returns -1 if the size is unknown or unavailable.
    static inline ssize_t fsGetStreamFileSize(FileStream* fs) { return fs->pIO->GetFileSize(fs); }

    /// Flushes all writes to the file stream to the underlying subsystem.
    static inline bool fsFlushStream(FileStream* fs)
    {
        if (!fs->pIO->Flush)
            return false;
        return fs->pIO->Flush(fs);
    }

    /// Returns whether the current seek position is at the end of the file stream.
    static inline bool fsStreamAtEnd(FileStream* fs) { return fs->pIO->IsAtEnd(fs); }

    static inline const char* fsIoGetResourceMount(IFileSystem* pIO, ResourceMount mount)
    {
        if (!pIO->GetResourceMount)
            return "";
        return pIO->GetResourceMount(mount);
    }

    static inline bool fsIoGetFileUid(IFileSystem* pIO, ResourceDirectory rd, const char* fileName, uint64_t* outUid)
    {
        if (!pIO->GetFileUid)
            return false;
        return pIO->GetFileUid(pIO, rd, fileName, outUid);
    }

    static inline bool fsIoOpenByUid(IFileSystem* pIO, uint64_t index, FileMode mode, FileStream* pOutStream)
    {
        if (!pIO->OpenByUid)
            return false;
        return pIO->OpenByUid(pIO, index, mode, pOutStream);
    }

    static inline bool fsStreamMemoryMap(FileStream* fs, size_t* outSize, void const** outData)
    {
        if (!fs->pIO->MemoryMap)
            return false;
        return fs->pIO->MemoryMap(fs, outSize, outData);
    }

    /************************************************************************/
    // MARK: - Directory queries
    /************************************************************************/
    /// Returns location set for resource directory in fsSetPathForResourceDir.
    FORGE_API const char*   fsGetResourceDirectory(ResourceDirectory resourceDir);
    /// Returns Resource Mount point for resource directory
    FORGE_API ResourceMount fsGetResourceDirectoryMount(ResourceDirectory resourceDir);

    /// Sets the relative path for `resourceDir` from `mount` to `bundledFolder`.
    /// The `resourceDir` will making use of the given IFileSystem `pIO` file functions.
    /// When `mount` is set to `RM_CONTENT` for a `resourceDir`, this directory is marked as a bundled resource folder.
    /// Bundled resource folders should only be used for Read operations.
    /// NOTE: A `resourceDir` can only be set once.
    FORGE_API void fsSetPathForResourceDir(IFileSystem* pIO, ResourceMount mount, ResourceDirectory resourceDir, const char* bundledFolder);

    /************************************************************************/
    // MARK: - File Queries
    /************************************************************************/

    /// Gets the time of last modification for the file at `fileName`, within 'resourceDir'.
    FORGE_API time_t fsGetLastModifiedTime(ResourceDirectory resourceDir, const char* fileName);

    /************************************************************************/
    // MARK: - Platform-dependent function definitions
    /************************************************************************/

    bool fsCreateResourceDirectory(ResourceDirectory resourceDir);

#ifdef __cplusplus
} // extern "C"
#endif
