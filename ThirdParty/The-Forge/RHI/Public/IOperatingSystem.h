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

#include "OS/CPUConfig.h"

#if defined(_WINDOWS) || defined(XBOX)
#include <stdlib.h>
#include <sys/stat.h>
#include <windows.h>
#endif

#if defined(__APPLE__)
#if !defined(TARGET_IOS)
#import <Carbon/Carbon.h>
#else
#include <stdint.h>
typedef uint64_t uint64;
#endif

// Support fixed set of OS versions in order to minimize maintenance efforts
// Two runtimes. Using IOS in the name as iOS versions are easier to remember
// Try to match the macOS version with the relevant features in iOS version
#define IOS14_API     API_AVAILABLE(macos(11.0), ios(14.1))
#define IOS14_RUNTIME @available(macOS 11.0, iOS 14.1, *)

#define IOS17_API     API_AVAILABLE(macos(14.0), ios(17.0))
#define IOS17_RUNTIME @available(macOS 14.0, iOS 17.0, *)

#elif defined(__ANDROID__)
#include <android/log.h>
#include <android_native_app_glue.h>

#elif defined(__linux__) && !defined(VK_USE_PLATFORM_GGP)
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#if defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_XCB_KHR)
#include <X11/Xutil.h>
// X11 defines primitive types which conflict with Forge libraries
#undef Bool
#endif

#elif defined(NX64)
#include "Switch/Common_3/OS/NX/NXTypes.h"

#elif defined(ORBIS)
#define THREAD_STACK_SIZE_ORBIS (64u * TF_KB)
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// For time related functions such as getting localtime
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define IMEMORY_FROM_HEADER
#include "IMemory.h"

#if !defined(_WINDOWS) && !defined(XBOX)
#include <unistd.h>
#define stricmp(a, b) strcasecmp(a, b)
#if !defined(ORBIS) && !defined(PROSPERO)
#define vsprintf_s vsnprintf
#endif
#endif

#if defined(XBOX)
#define stricmp(a, b) _stricmp(a, b)
#endif

typedef enum WindowHandleType
{
    WINDOW_HANDLE_TYPE_UNKNOWN,
    WINDOW_HANDLE_TYPE_WIN32,
    WINDOW_HANDLE_TYPE_XLIB,
    WINDOW_HANDLE_TYPE_XCB,
    WINDOW_HANDLE_TYPE_WAYLAND,
    WINDOW_HANDLE_TYPE_ANDROID,
    WINDOW_HANDLE_TYPE_VI_NN,
} WindowHandleType;

typedef struct WindowHandle
{
    WindowHandleType type;

#if defined(_WIN32)
    HWND window;
#elif defined(ANDROID)
    ANativeWindow* window;
    ANativeActivity* activity;
    AConfiguration* configuration;
#elif defined(__APPLE__) || defined(NX64) || defined(ORBIS) || defined(PROSPERO)
    void* window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t* connection;
    xcb_window_t             window;
    xcb_screen_t* screen;
    xcb_intern_atom_reply_t* atom_wm_delete_window;
#elif defined(VK_USE_PLATFORM_XLIB_KHR) || defined(VK_USE_PLATFORM_WAYLAND_KHR)
    union
    {
#if defined(VK_USE_PLATFORM_XLIB_KHR)
        struct
        {
            Display* display;
            Window   window;
            Atom     xlib_wm_delete_window;
            Colormap colormap;
        };
#endif
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
        struct
        {
            struct wl_display* wl_display;
            struct wl_surface* wl_surface;
        };
#endif
    };
#endif
} WindowHandle;

// Shell commands

#ifdef __cplusplus
extern "C"
{
#endif
    /// @param stdOutFile The file to which the output of the command should be written. May be NULL.
    FORGE_API int systemRun(const char* command, const char** arguments, size_t argumentCount, const char* stdOutFile);
#ifdef __cplusplus
}
#endif
