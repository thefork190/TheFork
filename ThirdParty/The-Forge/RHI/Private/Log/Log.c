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

#include "Log.h"

#include "Config.h"

#include <stdarg.h>

#ifdef ENABLE_LOGGING
#include "IFileSystem.h"
#include "ILog.h"
#include "IThread.h"
#include "ITime.h"

#include "IMemory.h"

#define LOG_CALLBACK_MAX_ID FS_MAX_PATH
#define LOG_MAX_BUFFER      1024

typedef struct LogCallback
{
    char          mID[LOG_CALLBACK_MAX_ID];
    void*         mUserData;
    LogCallbackFn mCallback;
    LogCloseFn    mClose;
    LogFlushFn    mFlush;
    uint32_t      mLevel;
} LogCallback;

static void initLogCallback(LogCallback* pLogCallback, const char* id, void* pUserData, LogCallbackFn callback, LogCloseFn close,
                            LogFlushFn flush, uint32_t level)
{
    strncpy(pLogCallback->mID, id, sizeof pLogCallback->mID - 1);
    pLogCallback->mID[sizeof pLogCallback->mID - 1] = 0;

    pLogCallback->mUserData = pUserData;
    pLogCallback->mCallback = callback;
    pLogCallback->mClose = close;
    pLogCallback->mFlush = flush;
    pLogCallback->mLevel = level;
}

typedef struct Log
{
    LogCallback* pCallbacks;
    size_t       mCallbacksSize;
    Mutex        mLogMutex;
    uint32_t     mLogLevel;
    uint32_t     mIndentation;
} Log;

static bool gIsLoggerInitialized = false;
static Log  gLogger;

static THREAD_LOCAL char gLogBuffer[LOG_MAX_BUFFER + 2];
static bool              gConsoleLogging = true;

#define LOG_PREAMBLE_SIZE  (56 + MAX_THREAD_NAME_LENGTH + FILENAME_NAME_LENGTH_LOG)
#define LOG_LEVEL_SIZE     6
#define LOG_MESSAGE_OFFSET (LOG_PREAMBLE_SIZE + LOG_LEVEL_SIZE)

static void     addInitialLogFile(const char* appName);
static bool     isLogCallback(const char* id);
static uint32_t writeLogPreamble(char* buffer, uint32_t buffer_size, const char* file, int line);

bool fsMergeDirAndFileName(const char* dir, const char* path, char separator, size_t dstSize, char* dst);

// Returns the part of the path after the last / or \ (if any).
static const char* getFilename(const char* path)
{
    for (const char* ptr = path; *ptr != '\0'; ++ptr)
    {
        if (*ptr == '/' || *ptr == '\\')
        {
            path = ptr + 1;
        }
    }
    return path;
}

// Default callback
static void defaultCallback(void* user_data, const char* message)
{
    FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);

    fsWriteToStream(fh, message, strlen(message));
    fsFlushStream(fh);
}

// Close callback
static void defaultClose(void* user_data)
{
    FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);
    fsCloseStream(fh);
    tf_free(fh);
}

// Flush callback
static void defaultFlush(void* user_data)
{
    FileStream* fh = (FileStream*)user_data;
    ASSERT(fh);

    fsFlushStream(fh);
}

void initLog(const char* appName, LogLevel level /* = eALL */)
{
    if (!gIsLoggerInitialized)
    {
        gLogger.pCallbacks = NULL;
        gLogger.mCallbacksSize = 0;
        initMutex(&gLogger.mLogMutex);
        gLogger.mLogLevel = level;
        gLogger.mIndentation = 0;

        setMainThread();
        setCurrentThreadName("MainThread");

        if (appName)
            addInitialLogFile(appName);

        gIsLoggerInitialized = true;
    }
}

void exitLog(void)
{
    LOGF(eINFO, "Shutting down log system.");

    for (LogCallback* pCallback = gLogger.pCallbacks; pCallback != gLogger.pCallbacks + gLogger.mCallbacksSize; ++pCallback)
    {
        if (pCallback->mClose)
            pCallback->mClose(pCallback->mUserData);
    }

    destroyMutex(&gLogger.mLogMutex);
    tf_free(gLogger.pCallbacks);
    gIsLoggerInitialized = false;
}

void addLogFile(const char* filename, FileMode file_mode, LogLevel log_level)
{
    if (filename == NULL)
        return;

    FileStream fh;
    memset(&fh, 0, sizeof(FileStream));
    if (fsOpenStreamFromPath(RD_LOG, filename, file_mode, &fh)) // If the File Exists
    {
        FileStream* user = (FileStream*)tf_malloc(sizeof(FileStream));
        *user = fh;
        // AddCallback will try to acquire mutex
        char path[FS_MAX_PATH] = { 0 };
        fsMergeDirAndFileName(fsGetResourceDirectory(RD_LOG), filename, '/', sizeof path, path);
        addLogCallback(path, log_level, user, defaultCallback, defaultClose, defaultFlush);

        acquireMutex(&gLogger.mLogMutex);
        {
            // Header
            static const char header[] = "date       time     "
                                         "[thread name/id ]"
                                         "                   file:line  "
                                         "  v |\n";

            fsWriteToStream(&fh, header, sizeof(header) - 1);
            fsFlushStream(&fh);
        }
        releaseMutex(&gLogger.mLogMutex);

        writeLog(eINFO, __FILE__, __LINE__, "Opened log file %s", filename);
    }
    else
    {
        writeLog(eERROR, __FILE__, __LINE__, "Failed to create log file %s", filename); // will try to acquire mutex
    }
}

void addLogCallback(const char* id, uint32_t log_level, void* user_data, LogCallbackFn callback, LogCloseFn close, LogFlushFn flush)
{
    acquireMutex(&gLogger.mLogMutex);
    {
        if (!isLogCallback(id))
        {
            LogCallback logCallback;
            memset(&logCallback, 0, sizeof(LogCallback));
            initLogCallback(&logCallback, id, user_data, callback, close, flush, log_level);
            LogCallback* pNewArray = (LogCallback*)tf_realloc(gLogger.pCallbacks, sizeof(LogCallback) * (gLogger.mCallbacksSize + 1));
            ASSERT(pNewArray != NULL);
            gLogger.pCallbacks = pNewArray;
            gLogger.pCallbacks[gLogger.mCallbacksSize] = logCallback;
            gLogger.mCallbacksSize = gLogger.mCallbacksSize + 1;
        }
        else
            close(user_data);
    }
    releaseMutex(&gLogger.mLogMutex);
}

typedef char LogStr[LOG_LEVEL_SIZE + 1];

void writeLogVaList(uint32_t level, const char* filename, int line_number, const char* message, va_list args)
{
    typedef struct Prefix
    {
        uint32_t    first;
        const char* second;
    } Prefix;

    static Prefix logLevelPrefixes[] = { { eWARNING, "WARN| " }, { eINFO, "INFO| " }, { eDEBUG, " DBG| " }, { eERROR, " ERR| " } };

    uint32_t log_levels[LEVELS_LOG];
    uint32_t log_level_count = 0;

    // Check flags
    for (uint32_t i = 0; i < sizeof(logLevelPrefixes) / sizeof(logLevelPrefixes[0]); ++i)
    {
        Prefix* it = &logLevelPrefixes[i];
        if ((it->first & level) && (gLogger.mLogLevel & level))
        {
            log_levels[log_level_count] = i;
            ++log_level_count;
        }
    }

    uint32_t preable_end = writeLogPreamble(gLogBuffer, LOG_PREAMBLE_SIZE, filename, line_number);

    // Prepare indentation
    uint32_t indentation = gLogger.mIndentation * INDENTATION_SIZE_LOG;
    memset(gLogBuffer + preable_end, ' ', indentation);

    uint32_t offset = preable_end + LOG_LEVEL_SIZE + indentation;
    offset += vsnprintf(gLogBuffer + offset, LOG_MAX_BUFFER - offset, message, args);

    offset = (offset > LOG_MAX_BUFFER) ? LOG_MAX_BUFFER : offset;
    gLogBuffer[offset] = '\n';
    gLogBuffer[offset + 1] = 0;

    // Log for each flag
    for (uint32_t i = 0; i < log_level_count; ++i)
    {
        strncpy(gLogBuffer + preable_end, logLevelPrefixes[log_levels[i]].second, LOG_LEVEL_SIZE);

        if (gConsoleLogging)
        {
            _PrintUnicode(gLogBuffer, level & eERROR);
        }

        acquireMutex(&gLogger.mLogMutex);
        {
            for (LogCallback* pCallback = gLogger.pCallbacks; pCallback != gLogger.pCallbacks + gLogger.mCallbacksSize; ++pCallback)
            {
                if (pCallback->mLevel & logLevelPrefixes[log_levels[i]].first)
                    pCallback->mCallback(pCallback->mUserData, gLogBuffer);
            }
        }
        releaseMutex(&gLogger.mLogMutex);
    }
}

void writeLog(uint32_t level, const char* filename, int line_number, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    writeLogVaList(level, filename, line_number, message, args);
    va_end(args);
}

void writeRawLog(uint32_t level, bool error, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    vsnprintf(gLogBuffer, LOG_MAX_BUFFER, message, args);
    va_end(args);

    if (gConsoleLogging)
    {
        _PrintUnicode(gLogBuffer, error);
    }

    acquireMutex(&gLogger.mLogMutex);
    {
        for (LogCallback* pCallback = gLogger.pCallbacks; pCallback != gLogger.pCallbacks + gLogger.mCallbacksSize; ++pCallback)
        {
            if (pCallback->mLevel & level)
                pCallback->mCallback(pCallback->mUserData, gLogBuffer);
        }
    }
    releaseMutex(&gLogger.mLogMutex);
}

void _FailedAssert(const char* file, int line, const char* statement, const char* msgFmt, ...)
{
    char usrMsgBuf[LOG_MAX_BUFFER];
    if (msgFmt && msgFmt[0])
    {
        va_list args;
        va_start(args, msgFmt);
        vsnprintf(usrMsgBuf, sizeof(usrMsgBuf), msgFmt, args);
        va_end(args);
    }
    else
    {
        usrMsgBuf[0] = '\0';
    }

    if (!gIsLoggerInitialized)
    {
        if (usrMsgBuf[0])
            _OutputDebugString("Assert failed: (%s)\n\nFile: %s\nLine: %d\nMessage: %s\n\n", statement, file, line, usrMsgBuf);
        else
            _OutputDebugString("Assert failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
    }
    else
    {
        if (usrMsgBuf[0])
            writeLog(eERROR, file, line, "Assert failed: %s\nAssert message: %s", statement, usrMsgBuf);
        else
            writeLog(eERROR, file, line, "Assert failed: %s", statement);
    }

    _FailedAssertImpl(file, line, statement, usrMsgBuf[0] ? usrMsgBuf : NULL);
}

static void addInitialLogFile(const char* appName)
{
    // Add new file with executable name

    const char*  extension = ".log";
    const size_t extensionLength = strlen(extension);

    char exeFileName[FS_MAX_PATH] = { 0 };
    strcpy(exeFileName, appName);

    // Minimum length check
    if (exeFileName[0] == 0 || exeFileName[1] == 0)
    {
        strncpy(exeFileName, "Log", 4);
    }
    strncat(exeFileName, extension, extensionLength + 1);

    addLogFile(exeFileName, FM_WRITE_ALLOW_READ, eALL);
}

static uint32_t writeLogPreamble(char* buffer, uint32_t buffer_size, const char* file, int line)
{
    uint32_t pos = 0;
    // Date and time
    if (pos < buffer_size)
    {
        time_t    t = time(NULL);
        struct tm time_info;
#if defined(_WINDOWS) || defined(XBOX)
        localtime_s(&time_info, &t);
#elif defined(ORBIS) || defined(PROSPERO)
        localtime_s(&t, &time_info);
#elif defined(NX64)
        t = getTimeSinceStart();
        localtime_r(&t, &time_info);
#else
        localtime_r(&t, &time_info);
#endif
        pos += snprintf(buffer + pos, buffer_size - pos, "%04d-%02d-%02d %02d:%02d:%02d ", 1900 + time_info.tm_year, 1 + time_info.tm_mon,
                        time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
    }

    if (pos < buffer_size)
    {
        char thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
        getCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);
        pos += snprintf(buffer + pos, buffer_size - pos, "[%-15s]", thread_name[0] == 0 ? "NoName" : thread_name);
    }

    // File and line
    if (pos < buffer_size)
    {
        file = getFilename(file);
        pos += snprintf(buffer + pos, buffer_size - pos, " %23.*s:%-5i ", FILENAME_NAME_LENGTH_LOG, file, line);
    }

    return pos;
}

static bool isLogCallback(const char* id)
{
    if (gLogger.mCallbacksSize)
    {
        for (const LogCallback* pCallback = gLogger.pCallbacks; pCallback != gLogger.pCallbacks + gLogger.mCallbacksSize; ++pCallback)
        {
            if (strcmp(pCallback->mID, id) == 0)
                return true;
        }
    }

    return false;
}

#else
void initLog(const char* appName, LogLevel level) {}
void exitLog(void) {}

void addLogFile(const char* filename, FileMode file_mode, LogLevel log_level) {}
void addLogCallback(const char* id, uint32_t log_level, void* user_data, LogCallbackFn callback, LogCloseFn close, LogFlushFn flush) {}

void writeLog(uint32_t level, const char* filename, int line_number, const char* message, va_list args) {}
void writeLog(uint32_t level, const char* filename, int line_number, const char* message, ...) {}
void writeRawLog(uint32_t level, bool error, const char* message, ...) {}

void _FailedAssert(const char* file, int line, const char* statement, const char* msgFmt, ...) {}
#endif

// minimum length is 7 + precision
static size_t doubleToShortStr(double d, int precision, char* str)
{
    *str = 0;

    int size = snprintf(NULL, 0, "%.*f", precision, d);
    snprintf(str, (unsigned)size + 1, "%.*f", precision, d);

    // remove trailing zeroes
    int end = size;
    for (int i = size - 1; i >= 0; i--)
    {
        if (str[i] == '0')
        {
            if (end == i + 1)
                end = i;
            continue;
        }

        if (str[i] != '.')
            continue;

        if (end == i + 1)
            end = i;
        str[end] = 0;
        break;
    }

    return end > 0 ? (size_t)end : 0;
}

struct HumanReadableValue humanReadableSSize(ssize_t size)
{
    const char* strs[] = {
        "B\0", "KB", "MB", "GB", "TB",
    };

    double value = (double)size;

    uint64_t i = 0;

    while ((value < 0 ? -value : value) >= 1024 && i < sizeof(strs) / sizeof(*strs) - 1)
    {
        value /= 1024;
        ++i;
    }

    struct HumanReadableValue hrs;
    memcpy(hrs.str + doubleToShortStr(value, 1, hrs.str), strs[i], 3);
    return hrs;
}

struct HumanReadableValue humanReadableTimeD(double value)
{
    const char* strs[] = {
        "ns", "ms", "s\0", "m\0", "h\0", "d\0",
    };

    uint64_t i = 0;

    double capacity = 1000;
    while ((value < 0 ? -value : value) >= capacity && i < sizeof(strs) / sizeof(*strs) - 1)
    {
        value /= capacity;
        ++i;

        switch (i)
        {
        case 0:
        case 1:
        case 2:
            capacity = 1000;
            break;
        case 3:
            capacity = 60;
            break;
        case 4:
            capacity = 24;
            break;
        }
    }

    const int maxPrecision = 7;

    int    precision = 1;
    double v = value;
    while (v < 100 && precision < maxPrecision)
    {
        v *= 100;
        ++precision;
    }

    struct HumanReadableValue hrs;
    memcpy(hrs.str + doubleToShortStr(value, precision, hrs.str), strs[i], 3);
    return hrs;
}
