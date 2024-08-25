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

#include "Nothings/stb_ds.h"
#include "bstrlib/bstrlib.h"
#include "tinyimageformat/tinyimageformat_apis.h"
#include "tinyimageformat/tinyimageformat_base.h"
#include "tinyimageformat/tinyimageformat_bits.h"
#include "tinyimageformat/tinyimageformat_query.h"

#include "Graphics/GraphicsConfig.h"

#define TINYKTX_IMPLEMENTATION
#include "tinyktx/tinyktx.h"

#define TINYDDS_IMPLEMENTATION
#include "tinydds/tinydds.h"

#include "IGraphics.h"
#include "IFileSystem.h"
#include "ILog.h"
#include "IThread.h"
#include "IResourceLoader.h"

#include "Math/ShaderUtilities.h" // Packing functions

#include "TextureContainers.h"

#include "IMemory.h"

#ifdef NX64
#include "murmurhash3/MurmurHash3_32.h"
#endif

// If facing strange gfx issues, corruption, GPU hangs, enable this for verbose logging of resource loading
#define RESOURCE_LOADER_VERBOSE 0
#if RESOURCE_LOADER_VERBOSE
#define LOADER_LOGF(...) LOGF(eINFO, __VA_ARGS__)
#else
#define LOADER_LOGF(...)
#endif

extern "C"
{
    void fsGetParentPath(const char* path, char* output);
    void fsGetPathExtension(const char* path, char* output);
    bool fsMergeDirAndFileName(const char* dir, const char* path, char separator, size_t dstSize, char* dst);
}

#define MIP_REDUCE(s, mip) (max(1u, (uint32_t)((s) >> (mip))))

#define SAFE_FREE(x)  \
    if ((x) != NULL)  \
    {                 \
        tf_free((x)); \
        (x) = NULL;   \
    }

#define MAX_FRAMES 3U

struct SubresourceDataDesc
{
    uint64_t mSrcOffset;
    uint32_t mMipLevel;
    uint32_t mArrayLayer;
#if defined(METAL) || defined(VULKAN)
    uint32_t mRowPitch;
    uint32_t mSlicePitch;
#endif
};

enum
{
    MAPPED_RANGE_FLAG_UNMAP_BUFFER = (1 << 0),
    MAPPED_RANGE_FLAG_TEMP_BUFFER = (1 << 1),
};

DECLARE_RENDERER_FUNCTION(void, getBufferSizeAlign, Renderer* pRenderer, const BufferDesc* pDesc, ResourceSizeAlign* pOut);
DECLARE_RENDERER_FUNCTION(void, getTextureSizeAlign, Renderer* pRenderer, const TextureDesc* pDesc, ResourceSizeAlign* pOut);
DECLARE_RENDERER_FUNCTION(void, addBuffer, Renderer* pRenderer, const BufferDesc* pDesc, Buffer** pp_buffer)
DECLARE_RENDERER_FUNCTION(void, removeBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, mapBuffer, Renderer* pRenderer, Buffer* pBuffer, ReadRange* pRange)
DECLARE_RENDERER_FUNCTION(void, unmapBuffer, Renderer* pRenderer, Buffer* pBuffer)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateBuffer, Cmd* pCmd, Buffer* pBuffer, uint64_t dstOffset, Buffer* pSrcBuffer, uint64_t srcOffset,
                          uint64_t size)
DECLARE_RENDERER_FUNCTION(void, cmdUpdateSubresource, Cmd* pCmd, Texture* pTexture, Buffer* pSrcBuffer,
                          const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, cmdCopySubresource, Cmd* pCmd, Buffer* pDstBuffer, Texture* pTexture,
                          const struct SubresourceDataDesc* pSubresourceDesc)
DECLARE_RENDERER_FUNCTION(void, addTexture, Renderer* pRenderer, const TextureDesc* pDesc, Texture** ppTexture)
DECLARE_RENDERER_FUNCTION(void, removeTexture, Renderer* pRenderer, Texture* pTexture)

extern PlatformParameters gPlatformParameters;

struct ShaderByteCodeBuffer
{
    // Make sure we don't stack overflow
#if defined(NX64)
    static FORGE_CONSTEXPR const uint32_t kStackSize = THREAD_STACK_SIZE_NX / 2;
#elif defined(ORBIS)
    static FORGE_CONSTEXPR const uint32_t kStackSize = THREAD_STACK_SIZE_ORBIS / 2;
#else
    static FORGE_CONSTEXPR const uint32_t kStackSize = 128u * TF_KB;
#endif

    // Stack memory, no need to deallocate it. Used first, if a shader is too big we allocate heap memory
    void*    pStackMemory;
    uint32_t mStackUsed;
};

const char* getShaderPlatformName();

struct FSLDerivative
{
    uint64_t mHash, mOffset, mSize;
};

struct FSLMetadata
{
    uint32_t mUseMultiView;
    uint32_t mICBCompatible;
    uint32_t mNumThreadsPerGroup[4];
    uint32_t mOutputRenderTargetTypesMask;
};

struct FSLHeader
{
    char        mMagic[4];
    uint32_t    mDerivativeCount;
    FSLMetadata mMetadata;
};

bool gl_compileShader(Renderer* pRenderer, ShaderStage stage, const char* fileName, uint32_t codeSize, const char* code,
                      BinaryShaderStageDesc* pOut, const char* pEntryPoint);

/************************************************************************/
/************************************************************************/

#if !(defined(XBOX) || defined(ORBIS) || defined(PROSPERO))
#define GFX_DRIVER_MANAGED_VIDEO_MEMORY
#endif

// Xbox, Orbis, Prospero, iOS have unified memory
// so we dont need a command buffer to upload linear data
// A simple memcpy suffices since the GPU memory is marked as CPU write combine
#if !defined(GFX_DRIVER_MANAGED_VIDEO_MEMORY) || defined(NX64)
static bool gUma = true;
#elif defined(ANDROID)
#if defined(VULKAN)
static bool gUma = true;
#else
static bool gUma = false;
#endif
#else
static bool gUma = false;
#endif

bool isUma() { return gUma; }

#if defined(DIRECT3D12)
#define STRICT_QUEUE_TYPE_BARRIERS
#endif

// Can only issue certain resource state barriers on particular queue type
static inline FORGE_CONSTEXPR bool StrictQueueTypeBarriers()
{
#if defined(STRICT_QUEUE_TYPE_BARRIERS)
    if (RENDERER_API_D3D12 == gPlatformParameters.mSelectedRendererApi)
    {
        return true;
    }
#endif
    return false;
}

// Need to issue barriers when doing texture copy operations
static inline bool IssueTextureCopyBarriers()
{
#if defined(DIRECT3D12)
    if (RENDERER_API_D3D12 == gPlatformParameters.mSelectedRendererApi)
    {
        return true;
    }
#endif
#if defined(VULKAN)
    if (RENDERER_API_VULKAN == gPlatformParameters.mSelectedRendererApi)
    {
        return true;
    }
#endif
    return false;
}

// Need to issue barriers when doing buffer copy operations
static inline FORGE_CONSTEXPR bool IssueBufferCopyBarriers() //-V524
{
#if defined(DIRECT3D12)
    if (RENDERER_API_D3D12 == gPlatformParameters.mSelectedRendererApi)
    {
        return true;
    }
#endif
    return false;
}

// All Vulkan resources are created in undefined state. Need to transition to desired layout manually unlike DX12 ResourceStartState
static inline bool IssueExplicitInitialStateBarrier()
{
#if defined(VULKAN)
    if (RENDERER_API_VULKAN == gPlatformParameters.mSelectedRendererApi)
    {
        return true;
    }
#endif
    return false;
}

ResourceLoaderDesc          gDefaultResourceLoaderDesc = { 8ull * TF_MB, 2, false };
/************************************************************************/
// Surface Utils
/************************************************************************/
static inline ResourceState ResourceStartState(bool uav)
{
    if (uav)
        return RESOURCE_STATE_UNORDERED_ACCESS;
    else
        return RESOURCE_STATE_SHADER_RESOURCE;
}

static inline ResourceState ResourceStartState(const BufferDesc* pDesc)
{
    // Host visible (Upload Heap)
    if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_ONLY || pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_CPU_TO_GPU)
    {
        return RESOURCE_STATE_GENERIC_READ;
    }
    // Device Local (Default Heap)
    else if (pDesc->mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
    {
        DescriptorType usage = (DescriptorType)pDesc->mDescriptors;
        ResourceState  ret = RESOURCE_STATE_UNDEFINED;

        // Try to limit number of states used overall to avoid sync complexities
        if (usage & DESCRIPTOR_TYPE_RW_BUFFER)
        {
            ret = RESOURCE_STATE_UNORDERED_ACCESS;
        }
        else
        {
            if (usage & (DESCRIPTOR_TYPE_VERTEX_BUFFER | DESCRIPTOR_TYPE_UNIFORM_BUFFER))
            {
                ret |= RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            }
            if (usage & DESCRIPTOR_TYPE_INDEX_BUFFER)
            {
                ret |= RESOURCE_STATE_INDEX_BUFFER;
            }
            if (usage & DESCRIPTOR_TYPE_BUFFER)
            {
                ret |= RESOURCE_STATE_SHADER_RESOURCE;
            }
        }

        return ret;
    }
    // Host Cached (Readback Heap)
    else
    {
        return RESOURCE_STATE_COPY_DEST;
    }
}

/************************************************************************/
// Internal Structures
/************************************************************************/
typedef void (*PreMipStepFn)(FileStream* pStream, uint32_t mip);

typedef struct BufferLoadDescInternal
{
    Buffer*       pBuffer;
    const void*   pData;
    uint64_t      mDataSize;
    Buffer*       pSrcBuffer;
    uint64_t      mSrcOffset;
    ResourceState mStartState;
    bool          mForceReset;
} BufferLoadDescInternal;

struct TextureLoadDescInternal
{
    Texture** ppTexture;
    union
    {
        struct
        {
            const char*          pFileName;
            Sampler*             pYcbcrSampler;
            TextureCreationFlags mFlags;
            TextureContainerType mContainer;
            uint32_t             mNodeIndex;
        };
        struct
        {
            ResourceState mStartState;
        };
    };
    bool mForceReset;
};

typedef struct TextureUpdateDescInternal
{
    Texture*          pTexture;
    FileStream        mStream;
    Cmd*              pCmd;
    MappedMemoryRange mRange;
    uint32_t          mBaseMipLevel;
    uint32_t          mMipLevels;
    uint32_t          mBaseArrayLayer;
    uint32_t          mLayerCount;
    PreMipStepFn      pPreMipFunc;
    ResourceState     mCurrentState;
    bool              mMipsAfterSlice;
} TextureUpdateDescInternal;

typedef struct CopyResourceSet
{
    Fence*     pFence = NULL;
    Semaphore* pSemaphore = NULL;
    Cmd*       pCmd = NULL;
    CmdPool*   pCmdPool = NULL;
    Buffer*    mBuffer = NULL;
    uint64_t   mAllocatedSpace = 0;

    /// Buffers created in case we ran out of space in the original staging buffer
    /// Will be cleaned up after the fence for this set is complete
    /// stb_ds array of Buffer*
    Buffer** mTempBuffers = NULL;

#if defined(STRICT_QUEUE_TYPE_BARRIERS)
    Cmd*     pPostCopyBarrierCmd = NULL;
    CmdPool* pPostCopyBarrierCmdPool = NULL;
    Fence*   pPostCopyBarrierFence = NULL;
    bool     mPostCopyBarrierRecording = false;
#endif
} CopyResourceSet;

// Synchronization?
typedef struct CopyEngineDesc
{
    uint64_t    mSize;
    const char* pQueueName;
    QueueType   mQueueType;
    uint32_t    mNodeIndex;
    uint32_t    mBufferCount;
} CopyEngineDesc;

typedef struct CopyEngine
{
    Queue*           pQueue;
    CopyResourceSet* resourceSets;
    uint64_t         bufferSize;
    Semaphore*       pLastSubmittedSemaphore;

    /// For reading back GPU generated textures, we need to ensure writes have completed before performing the copy.
    /// stb_ds array of Semaphore*
    Semaphore** mWaitSemaphores;

    typedef void (*FlushFunction)(CopyEngine*);
    FlushFunction pFnFlush;

    uint32_t bufferCount;
    uint32_t activeSet;
    /// Node index in linked GPU mode, Renderer index in unlinked mode
    uint32_t nodeIndex;

    bool isRecording;
    bool flushOnOverflow;
} CopyEngine;

typedef enum UpdateRequestType
{
    UPDATE_REQUEST_TEXTURE_BARRIER,
    UPDATE_REQUEST_LOAD_BUFFER,
    UPDATE_REQUEST_LOAD_TEXTURE,
    UPDATE_REQUEST_LOAD_GEOMETRY,
    UPDATE_REQUEST_COPY_TEXTURE,
    UPDATE_REQUEST_INVALID,
} UpdateRequestType;

typedef enum UploadFunctionResult
{
    UPLOAD_FUNCTION_RESULT_COMPLETED,
    UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL,
    UPLOAD_FUNCTION_RESULT_INVALID_REQUEST
} UploadFunctionResult;

struct UpdateRequest
{
    UpdateRequest(const BufferLoadDescInternal& buffer): mType(UPDATE_REQUEST_LOAD_BUFFER), bufLoadDesc(buffer) {}
    UpdateRequest(const TextureLoadDescInternal& texture): mType(UPDATE_REQUEST_LOAD_TEXTURE), texLoadDesc(texture) {}
    UpdateRequest(const GeometryLoadDesc& geom): mType(UPDATE_REQUEST_LOAD_GEOMETRY), geomLoadDesc(geom) {}
    UpdateRequest(const TextureBarrier& barrier): mType(UPDATE_REQUEST_TEXTURE_BARRIER), textureBarrier(barrier) {}
    UpdateRequest(const TextureCopyDesc& texture): mType(UPDATE_REQUEST_COPY_TEXTURE), texCopyDesc(texture) {}

    UpdateRequestType mType = UPDATE_REQUEST_INVALID;
    uint64_t          mWaitIndex = 0;
    union
    {
        BufferLoadDescInternal  bufLoadDesc;
        TextureLoadDescInternal texLoadDesc;
        GeometryLoadDesc        geomLoadDesc;
        TextureBarrier          textureBarrier;
        TextureCopyDesc         texCopyDesc;
    };
};

struct ResourceLoader
{
    Renderer* ppRenderers[MAX_MULTIPLE_GPUS];
    uint32_t  mGpuCount;

    ResourceLoaderDesc mDesc;

    volatile int mRun;
    ThreadHandle mThread;

    Mutex             mQueueMutex;
    ConditionVariable mQueueCond;
    Mutex             mTokenMutex;
    ConditionVariable mTokenCond;
    // array of stb_ds arrays
    UpdateRequest*    mRequestQueue[MAX_MULTIPLE_GPUS];

    tfrg_atomic64_t mTokenCompleted;
    tfrg_atomic64_t mTokenSubmitted;
    tfrg_atomic64_t mTokenCounter;

    Mutex mSemaphoreMutex;

    SyncToken mCurrentTokenState[MAX_FRAMES];
    SyncToken mMaxToken;

    CopyEngine pCopyEngines[MAX_MULTIPLE_GPUS];
    CopyEngine pUploadEngines[MAX_MULTIPLE_GPUS];
    Mutex      mUploadEngineMutex;
};

static ResourceLoader* pResourceLoader = NULL;

static uint32_t util_get_texture_row_alignment(Renderer* pRenderer)
{
    return max(1u, pRenderer->pGpu->mSettings.mUploadBufferTextureRowAlignment);
}

static uint32_t util_get_texture_subresource_alignment(Renderer* pRenderer, TinyImageFormat fmt = TinyImageFormat_UNDEFINED)
{
    uint32_t blockSize = max(1u, TinyImageFormat_BitSizeOfBlock(fmt) >> 3);
    uint32_t alignment = round_up(pRenderer->pGpu->mSettings.mUploadBufferTextureAlignment, blockSize);
    return round_up(alignment, util_get_texture_row_alignment(pRenderer));
}

static void* alignMemory(void* ptr, uint64_t alignment)
{
    uintptr_t offset = alignment - ((uintptr_t)ptr % alignment);
    if (offset != 0)
        ptr = (char*)ptr + offset;
    return ptr;
}

#if !defined(PROSPERO)
static void* allocShaderByteCode(ShaderByteCodeBuffer* pShaderByteCodeBuffer, uint32_t alignment, uint32_t size, const char* filename)
{
    ASSERT(pShaderByteCodeBuffer && pShaderByteCodeBuffer->pStackMemory);
    ASSERT(alignment > 0);

    uint8_t* pBufferStart = (uint8_t*)pShaderByteCodeBuffer->pStackMemory + pShaderByteCodeBuffer->mStackUsed;
    uint8_t* pBufferAligned = (uint8_t*)alignMemory(pBufferStart, alignment);

    void* pOutMemory = NULL;
    if (pBufferAligned + size <= (uint8_t*)pShaderByteCodeBuffer->pStackMemory + pShaderByteCodeBuffer->kStackSize)
    {
        pShaderByteCodeBuffer->mStackUsed += (uint32_t)((pBufferAligned + size) - pBufferStart);
        pOutMemory = pBufferAligned;
    }
    else
    {
        LOGF(eINFO, "Loading shader bytecode in heap memory (%s - %u bytes) (Stack total size: %u, Free size: %u)", filename,
             (uint32_t)(size + alignment), pShaderByteCodeBuffer->kStackSize,
             pShaderByteCodeBuffer->kStackSize - pShaderByteCodeBuffer->mStackUsed);
        pOutMemory = tf_memalign(alignment, size);
    }

    ASSERT(((uintptr_t)pOutMemory % alignment) == 0);
    return pOutMemory;
}

static void freeShaderByteCode(ShaderByteCodeBuffer* pShaderByteCodeBuffer, BinaryShaderDesc* pBinaryShaderDesc)
{
    ASSERT(pShaderByteCodeBuffer && pBinaryShaderDesc);

    // Free bytecode if it's not allocated on the buffer
#define FREE_BYTECODE_IF_ON_HEAP(stage)                                                                     \
    if (pShaderByteCodeBuffer->pStackMemory > stage.pByteCode ||                                            \
        ((char*)pShaderByteCodeBuffer->pStackMemory + pShaderByteCodeBuffer->kStackSize) < stage.pByteCode) \
    tf_free(stage.pByteCode)

    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mVert);
    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mFrag);
    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mGeom);
    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mHull);
    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mDomain);
    FREE_BYTECODE_IF_ON_HEAP(pBinaryShaderDesc->mComp);

#undef FREE_BYTECODE_IF_ON_HEAP
}
#else
static void freeShaderByteCode(ShaderByteCodeBuffer*, BinaryShaderDesc*) {}
#endif

/************************************************************************/
// Internal Functions
/************************************************************************/

/// Return a new staging buffer
static MappedMemoryRange allocateUploadMemory(Renderer* pRenderer, uint64_t memoryRequirement, uint32_t alignment)
{
    Buffer* buffer;

    {
        // LOGF(LogLevel::eINFO, "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer
        // capacity of %llu", memoryRequirement, size);
        buffer = {};
        BufferDesc bufferDesc = {};
        bufferDesc.mSize = memoryRequirement;
        bufferDesc.mAlignment = alignment;
        bufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_ONLY;
        bufferDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        bufferDesc.mNodeIndex = pRenderer->mUnlinkedRendererIndex;
        bufferDesc.pName = "temporary staging buffer";
        addBuffer(pRenderer, &bufferDesc, &buffer);
    }
    return { (uint8_t*)buffer->pCpuMappedAddress, buffer, 0, memoryRequirement, MAPPED_RANGE_FLAG_TEMP_BUFFER };
}

static void setupCopyEngine(Renderer* pRenderer, CopyEngineDesc* pDesc, CopyEngine* pCopyEngine)
{
    QueueDesc desc = { pDesc->mQueueType, QUEUE_FLAG_NONE, QUEUE_PRIORITY_NORMAL, pDesc->mNodeIndex, pDesc->pQueueName };
    addQueue(pRenderer, &desc, &pCopyEngine->pQueue);

    const uint64_t maxBlockSize = 32;
    pDesc->mSize = max(pDesc->mSize, maxBlockSize);

    pCopyEngine->resourceSets = (CopyResourceSet*)tf_malloc(sizeof(CopyResourceSet) * pDesc->mBufferCount);
    for (uint32_t i = 0; i < pDesc->mBufferCount; ++i)
    {
        tf_placement_new<CopyResourceSet>(pCopyEngine->resourceSets + i);

        CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
        addFence(pRenderer, &resourceSet.pFence);

        CmdPoolDesc cmdPoolDesc = {};
        cmdPoolDesc.pQueue = pCopyEngine->pQueue;
        addCmdPool(pRenderer, &cmdPoolDesc, &resourceSet.pCmdPool);

        CmdDesc cmdDesc = {};
        cmdDesc.pPool = resourceSet.pCmdPool;
#ifdef ENABLE_GRAPHICS_DEBUG
        static char buffer[MAX_DEBUG_NAME_LENGTH];
        const char* engineName = pDesc->pQueueName ? pDesc->pQueueName : "Unnamed";
        snprintf(buffer, sizeof(buffer), "Node %u %s CopyEngine buffer %u Cmd", pDesc->mNodeIndex, engineName, i);
        cmdDesc.pName = buffer;
#endif // ENABLE_GRAPHICS_DEBUG
        addCmd(pRenderer, &cmdDesc, &resourceSet.pCmd);

        addSemaphore(pRenderer, &resourceSet.pSemaphore);

        resourceSet.mBuffer = allocateUploadMemory(pRenderer, pDesc->mSize, util_get_texture_subresource_alignment(pRenderer)).pBuffer;
    }

    pCopyEngine->bufferSize = pDesc->mSize;
    pCopyEngine->bufferCount = pDesc->mBufferCount;
    pCopyEngine->nodeIndex = pDesc->mNodeIndex;
    pCopyEngine->isRecording = false;
    pCopyEngine->pLastSubmittedSemaphore = NULL;
}

static void cleanupCopyEngine(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
    for (uint32_t i = 0; i < pCopyEngine->bufferCount; ++i)
    {
        CopyResourceSet& resourceSet = pCopyEngine->resourceSets[i];
        removeBuffer(pRenderer, resourceSet.mBuffer);

        removeSemaphore(pRenderer, resourceSet.pSemaphore);

        removeCmd(pRenderer, resourceSet.pCmd);
        removeCmdPool(pRenderer, resourceSet.pCmdPool);
        removeFence(pRenderer, resourceSet.pFence);

        for (ptrdiff_t j = 0; j < arrlen(resourceSet.mTempBuffers); ++j)
        {
            removeBuffer(pRenderer, resourceSet.mTempBuffers[j]);
        }
        arrfree(resourceSet.mTempBuffers);

#if defined(STRICT_QUEUE_TYPE_BARRIERS)
        if (StrictQueueTypeBarriers() && resourceSet.pPostCopyBarrierFence)
        {
            removeFence(pRenderer, resourceSet.pPostCopyBarrierFence);
            removeCmd(pRenderer, resourceSet.pPostCopyBarrierCmd);
            removeCmdPool(pRenderer, resourceSet.pPostCopyBarrierCmdPool);
        }
#endif
    }

    tf_free(pCopyEngine->resourceSets);
    arrfree(pCopyEngine->mWaitSemaphores);

    removeQueue(pRenderer, pCopyEngine->pQueue);
}

static void waitCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
    ASSERT(!pCopyEngine->isRecording);
    CopyResourceSet& resourceSet = pCopyEngine->resourceSets[pCopyEngine->activeSet];

    FenceStatus status;
    getFenceStatus(pRenderer, resourceSet.pFence, &status);
    if (FENCE_STATUS_INCOMPLETE == status)
    {
        waitForFences(pRenderer, 1, &resourceSet.pFence);
    }

#if defined(STRICT_QUEUE_TYPE_BARRIERS)
    if (StrictQueueTypeBarriers() && resourceSet.pPostCopyBarrierFence)
    {
        getFenceStatus(pRenderer, resourceSet.pPostCopyBarrierFence, &status);
        if (FENCE_STATUS_INCOMPLETE == status)
        {
            waitForFences(pRenderer, 1, &resourceSet.pPostCopyBarrierFence);
        }
    }
#endif
}

static void resetCopyEngineSet(Renderer* pRenderer, CopyEngine* pCopyEngine)
{
    ASSERT(!pCopyEngine->isRecording);
    pCopyEngine->resourceSets[pCopyEngine->activeSet].mAllocatedSpace = 0;
    pCopyEngine->isRecording = false;

    Buffer**  tempBuffers = pCopyEngine->resourceSets[pCopyEngine->activeSet].mTempBuffers;
    ptrdiff_t tempBufferCount = arrlen(tempBuffers);
    for (ptrdiff_t i = 0; i < tempBufferCount; ++i)
    {
        removeBuffer(pRenderer, tempBuffers[i]);
    }
    arrsetlen(tempBuffers, 0);
}

static Cmd* acquireCmd(CopyEngine* pCopyEngine)
{
    CopyResourceSet& resourceSet = pCopyEngine->resourceSets[pCopyEngine->activeSet];
    if (!pCopyEngine->isRecording)
    {
        waitCopyEngineSet(pResourceLoader->ppRenderers[pCopyEngine->nodeIndex], pCopyEngine);
        resetCopyEngineSet(pResourceLoader->ppRenderers[pCopyEngine->nodeIndex], pCopyEngine);
        resetCmdPool(pResourceLoader->ppRenderers[pCopyEngine->nodeIndex], resourceSet.pCmdPool);
        beginCmd(resourceSet.pCmd);
#if !defined(XBOX)
        cmdBeginDebugMarker(resourceSet.pCmd, 1.0f, 0.5f, 0.1f,
                            QUEUE_TYPE_TRANSFER == pCopyEngine->pQueue->mType ? "Copy Cmd" : "Upload Cmd");
#endif
        pCopyEngine->isRecording = true;
    }
    return resourceSet.pCmd;
}

static Cmd* acquirePostCopyBarrierCmd(CopyEngine* pCopyEngine)
{
    if (!StrictQueueTypeBarriers() || pCopyEngine->pQueue->mType != QUEUE_TYPE_TRANSFER)
    {
        return acquireCmd(pCopyEngine);
    }
#if defined(STRICT_QUEUE_TYPE_BARRIERS)
    CopyResourceSet& resourceSet = pCopyEngine->resourceSets[pCopyEngine->activeSet];
    if (!resourceSet.mPostCopyBarrierRecording)
    {
        resetCmdPool(pResourceLoader->ppRenderers[pCopyEngine->nodeIndex], resourceSet.pPostCopyBarrierCmdPool);
        beginCmd(resourceSet.pPostCopyBarrierCmd);
        resourceSet.mPostCopyBarrierRecording = true;
    }
    return resourceSet.pPostCopyBarrierCmd;
#else
    return acquireCmd(pCopyEngine);
#endif
}

static void streamerFlush(CopyEngine* pCopyEngine)
{
    if (pCopyEngine->isRecording)
    {
        CopyResourceSet& resourceSet = pCopyEngine->resourceSets[pCopyEngine->activeSet];
#if !defined(XBOX)
        cmdEndDebugMarker(resourceSet.pCmd);
#endif
        endCmd(resourceSet.pCmd);
        QueueSubmitDesc submitDesc = {};
        submitDesc.mCmdCount = 1;
        submitDesc.ppCmds = &resourceSet.pCmd;
        submitDesc.mSignalSemaphoreCount = 1;
        submitDesc.ppSignalSemaphores = &resourceSet.pSemaphore;
        submitDesc.pSignalFence = resourceSet.pFence;
        if (arrlen(pCopyEngine->mWaitSemaphores))
        {
            submitDesc.mWaitSemaphoreCount = (uint32_t)arrlen(pCopyEngine->mWaitSemaphores);
            submitDesc.ppWaitSemaphores = &pCopyEngine->mWaitSemaphores[0];
            arrsetlen(pCopyEngine->mWaitSemaphores, 0);
        }
        queueSubmit(pCopyEngine->pQueue, &submitDesc);

#if defined(STRICT_QUEUE_TYPE_BARRIERS)
        if (StrictQueueTypeBarriers() && resourceSet.mPostCopyBarrierRecording)
        {
            endCmd(resourceSet.pPostCopyBarrierCmd);
            submitDesc = {};
            submitDesc.mCmdCount = 1;
            submitDesc.ppCmds = &resourceSet.pPostCopyBarrierCmd;
            submitDesc.mWaitSemaphoreCount = 1;
            submitDesc.ppWaitSemaphores = &resourceSet.pSemaphore;
            submitDesc.pSignalFence = resourceSet.pPostCopyBarrierFence;
            queueSubmit(resourceSet.pPostCopyBarrierCmdPool->pQueue, &submitDesc);
            resourceSet.mPostCopyBarrierRecording = false;
        }
#endif

        pCopyEngine->isRecording = false;
    }
}

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static MappedMemoryRange allocateStagingMemory(CopyEngine* pCopyEngine, uint64_t memoryRequirement, uint32_t alignment, uint32_t nodeIndex)
{
    // #NOTE: Call to make sure we dont reset copy engine after staging memory was already allocated
    acquireCmd(pCopyEngine);

    CopyResourceSet* pResourceSet = &pCopyEngine->resourceSets[pCopyEngine->activeSet];
    uint64_t         size = (uint64_t)pResourceSet->mBuffer->mSize;
    alignment = max((uint32_t)RESOURCE_BUFFER_ALIGNMENT, alignment);
    memoryRequirement = round_up_64(memoryRequirement, alignment);
    if (memoryRequirement > size)
    {
        MappedMemoryRange range = allocateUploadMemory(pResourceLoader->ppRenderers[nodeIndex], memoryRequirement, alignment);
        LOADER_LOGF(
            LogLevel::eINFO,
            "Allocating temporary staging buffer. Required allocation size of %llu is larger than the staging buffer capacity of %llu",
            memoryRequirement, size);
        arrpush(pResourceSet->mTempBuffers, range.pBuffer);
        return range;
    }

    uint64_t offset = round_up_64(pCopyEngine->resourceSets[pCopyEngine->activeSet].mAllocatedSpace, alignment);
    bool     memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
    if (memoryAvailable && pResourceSet->mBuffer->pCpuMappedAddress)
    {
        Buffer* buffer = pResourceSet->mBuffer;
        ASSERT(buffer->pCpuMappedAddress);
        uint8_t* pDstData = (uint8_t*)buffer->pCpuMappedAddress + offset;
        pCopyEngine->resourceSets[pCopyEngine->activeSet].mAllocatedSpace = offset + memoryRequirement;
        return { pDstData, buffer, offset, memoryRequirement };
    }
    else
    {
        if (pCopyEngine->flushOnOverflow)
        {
            ASSERT(pCopyEngine->pFnFlush);
            pCopyEngine->pFnFlush(pCopyEngine);
            return allocateStagingMemory(pCopyEngine, memoryRequirement, alignment, nodeIndex);
        }

        return {};
    }
}

static UploadFunctionResult updateBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, const BufferUpdateDesc& bufUpdateDesc)
{
    UNREF_PARAM(pRenderer);
    Buffer* pBuffer = bufUpdateDesc.pBuffer;
    ASSERT(pCopyEngine->pQueue->mNodeIndex == pBuffer->mNodeIndex);
    ASSERT(RESOURCE_MEMORY_USAGE_GPU_ONLY == pBuffer->mMemoryUsage);

    Cmd* pCmd = acquireCmd(pCopyEngine);

    if (IssueBufferCopyBarriers() && bufUpdateDesc.mCurrentState != RESOURCE_STATE_COPY_DEST)
    {
        BufferBarrier barrier = { bufUpdateDesc.pBuffer, bufUpdateDesc.mCurrentState, RESOURCE_STATE_COPY_DEST };
        cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
    }

    MappedMemoryRange range = bufUpdateDesc.mInternal.mMappedRange;
    cmdUpdateBuffer(pCmd, pBuffer, bufUpdateDesc.mDstOffset, range.pBuffer, range.mOffset,
                    bufUpdateDesc.mSize ? bufUpdateDesc.mSize : range.mSize);

    if (IssueBufferCopyBarriers() && bufUpdateDesc.mCurrentState != RESOURCE_STATE_COPY_DEST)
    {
        BufferBarrier barrier = { bufUpdateDesc.pBuffer, RESOURCE_STATE_COPY_DEST, bufUpdateDesc.mCurrentState };
        cmdResourceBarrier(pCmd, 1, &barrier, 0, NULL, 0, NULL);
    }

    return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadBuffer(Renderer* pRenderer, CopyEngine* pCopyEngine, const UpdateRequest& updateRequest)
{
    const BufferLoadDescInternal& loadDesc = updateRequest.bufLoadDesc;
    BufferUpdateDesc              updateDesc = { loadDesc.pBuffer };
    updateDesc.mCurrentState = RESOURCE_STATE_COPY_DEST;
    MappedMemoryRange range = {};
    bool              mapped = false;
    if (loadDesc.pSrcBuffer)
    {
        range.mOffset = loadDesc.mSrcOffset;
        range.mSize = loadDesc.pBuffer->mSize;
        range.pBuffer = loadDesc.pSrcBuffer;

        if (!loadDesc.pSrcBuffer->pCpuMappedAddress)
        {
            mapBuffer(pRenderer, loadDesc.pSrcBuffer, NULL);
            mapped = true;
        }

        range.pData = (uint8_t*)loadDesc.pSrcBuffer->pCpuMappedAddress + loadDesc.mSrcOffset;
    }
    else
    {
        range = allocateStagingMemory(pCopyEngine, loadDesc.pBuffer->mSize, RESOURCE_BUFFER_ALIGNMENT, pCopyEngine->nodeIndex);
        if (!range.pData)
        {
            return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
        }
    }

    updateDesc.mInternal.mMappedRange = range;
    updateDesc.pMappedData = updateDesc.mInternal.mMappedRange.pData;
    if (loadDesc.mForceReset)
    {
        memset(updateDesc.pMappedData, 0, (size_t)loadDesc.pBuffer->mSize);
    }
    else
    {
        memcpy(updateDesc.pMappedData, loadDesc.pData, (size_t)loadDesc.mDataSize);
    }

    if (range.pData == loadDesc.pBuffer->pCpuMappedAddress)
    {
        if (mapped)
        {
            unmapBuffer(pRenderer, loadDesc.pSrcBuffer);
        }

        return UPLOAD_FUNCTION_RESULT_COMPLETED;
    }

    UploadFunctionResult res = updateBuffer(pRenderer, pCopyEngine, updateDesc);
    if (UPLOAD_FUNCTION_RESULT_COMPLETED == res)
    {
        if (IssueBufferCopyBarriers() && loadDesc.mStartState != RESOURCE_STATE_COPY_DEST)
        {
            BufferBarrier barrier = { loadDesc.pBuffer, RESOURCE_STATE_COPY_DEST, loadDesc.mStartState };
            Cmd*          cmd = acquirePostCopyBarrierCmd(pCopyEngine);
            cmdResourceBarrier(cmd, 1, &barrier, 0, NULL, 0, NULL);
        }
    }

    return res;
}

static UploadFunctionResult updateTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, const TextureUpdateDescInternal& texUpdateDesc)
{
    // When this call comes from updateResource, staging buffer data is already filled
    // All that is left to do is record and execute the Copy commands
    bool                  dataAlreadyFilled = texUpdateDesc.mRange.pBuffer ? true : false;
    Texture*              texture = texUpdateDesc.pTexture;
    const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
    FileStream            stream = texUpdateDesc.mStream;

    ASSERT(pCopyEngine->pQueue->mNodeIndex == texUpdateDesc.pTexture->mNodeIndex);

    const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
    const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
    const uint64_t requiredSize = util_get_surface_size(fmt, texture->mWidth, texture->mHeight, texture->mDepth, rowAlignment,
                                                        sliceAlignment, texUpdateDesc.mBaseMipLevel, texUpdateDesc.mMipLevels,
                                                        texUpdateDesc.mBaseArrayLayer, texUpdateDesc.mLayerCount);

    MappedMemoryRange upload =
        dataAlreadyFilled ? texUpdateDesc.mRange : allocateStagingMemory(pCopyEngine, requiredSize, sliceAlignment, texture->mNodeIndex);
    uint64_t offset = 0;

    Cmd* cmd = texUpdateDesc.pCmd ? texUpdateDesc.pCmd : acquireCmd(pCopyEngine);
    if (IssueTextureCopyBarriers() && texUpdateDesc.mCurrentState != RESOURCE_STATE_COPY_DEST)
    {
        TextureBarrier barrier = { texture, texUpdateDesc.mCurrentState, RESOURCE_STATE_COPY_DEST };
        cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
    }

    // #TODO: Investigate - fsRead crashes if we pass the upload buffer mapped address. Allocating temporary buffer as a workaround. Does NX
    // support loading from disk to GPU shared memory?
#ifdef NX64
    void* nxTempBuffer = NULL;
    if (!dataAlreadyFilled)
    {
        size_t remainingBytes = fsGetStreamFileSize(&stream) - fsGetStreamSeekPosition(&stream);
        nxTempBuffer = tf_malloc(remainingBytes);
        ssize_t bytesRead = fsReadFromStream(&stream, nxTempBuffer, remainingBytes);
        if (bytesRead != remainingBytes)
        {
            fsCloseStream(&stream);
            tf_free(nxTempBuffer);
            return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
        }

        fsCloseStream(&stream);
        fsOpenStreamFromMemory(nxTempBuffer, remainingBytes, FM_READ, true, &stream);
    }
#endif

    if (!upload.pData)
    {
        return UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL;
    }

    uint32_t firstStart = texUpdateDesc.mMipsAfterSlice ? texUpdateDesc.mBaseMipLevel : texUpdateDesc.mBaseArrayLayer;
    uint32_t firstEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels)
                                                      : (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount);
    uint32_t secondStart = texUpdateDesc.mMipsAfterSlice ? texUpdateDesc.mBaseArrayLayer : texUpdateDesc.mBaseMipLevel;
    uint32_t secondEnd = texUpdateDesc.mMipsAfterSlice ? (texUpdateDesc.mBaseArrayLayer + texUpdateDesc.mLayerCount)
                                                       : (texUpdateDesc.mBaseMipLevel + texUpdateDesc.mMipLevels);

    for (uint32_t p = 0; p < 1; ++p)
    {
        for (uint32_t j = firstStart; j < firstEnd; ++j)
        {
            if (texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMipFunc)
            {
                texUpdateDesc.pPreMipFunc(&stream, j);
            }

            for (uint32_t i = secondStart; i < secondEnd; ++i)
            {
                if (!texUpdateDesc.mMipsAfterSlice && texUpdateDesc.pPreMipFunc)
                {
                    texUpdateDesc.pPreMipFunc(&stream, i);
                }

                uint32_t mip = texUpdateDesc.mMipsAfterSlice ? j : i;
                uint32_t layer = texUpdateDesc.mMipsAfterSlice ? i : j;

                uint32_t w = MIP_REDUCE(texture->mWidth, mip);
                uint32_t h = MIP_REDUCE(texture->mHeight, mip);
                uint32_t d = MIP_REDUCE(texture->mDepth, mip);

                uint32_t numBytes = 0;
                uint32_t rowBytes = 0;
                uint32_t numRows = 0;

                bool ret = util_get_surface_info(w, h, fmt, &numBytes, &rowBytes, &numRows);
                if (!ret)
                {
                    return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
                }

                uint32_t subRowPitch = round_up(rowBytes, rowAlignment);
                uint32_t subSlicePitch = round_up(subRowPitch * numRows, sliceAlignment);
                uint32_t subNumRows = numRows;
                uint32_t subDepth = d;
                uint8_t* data = upload.pData + offset;

                if (!dataAlreadyFilled)
                {
                    for (uint32_t z = 0; z < subDepth; ++z)
                    {
                        uint8_t* dstData = data + subSlicePitch * z;
                        for (uint32_t r = 0; r < subNumRows; ++r)
                        {
                            ssize_t bytesRead = fsReadFromStream(&stream, dstData + r * subRowPitch, rowBytes);
                            if (bytesRead != rowBytes)
                            {
                                return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
                            }
                        }
                    }
                }
                SubresourceDataDesc subresourceDesc = {};
                subresourceDesc.mArrayLayer = layer;
                subresourceDesc.mMipLevel = mip;
                subresourceDesc.mSrcOffset = upload.mOffset + offset;
#if defined(METAL) || defined(VULKAN)
                subresourceDesc.mRowPitch = subRowPitch;
                subresourceDesc.mSlicePitch = subSlicePitch;
#endif
                cmdUpdateSubresource(cmd, texture, upload.pBuffer, &subresourceDesc);
                offset += subDepth * subSlicePitch;
            }
        }
    }

    if (IssueTextureCopyBarriers() && texUpdateDesc.mCurrentState != RESOURCE_STATE_COPY_DEST)
    {
        TextureBarrier barrier = { texture, RESOURCE_STATE_COPY_DEST, texUpdateDesc.mCurrentState };
        cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
    }

    if (stream.pIO)
    {
        fsCloseStream(&stream);
    }

    return UPLOAD_FUNCTION_RESULT_COMPLETED;
}

static UploadFunctionResult loadTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, const UpdateRequest& pTextureUpdate)
{
    const TextureLoadDescInternal* pTextureDesc = &pTextureUpdate.texLoadDesc;

    if (pTextureDesc->mForceReset)
    {
        Texture* texture = *pTextureDesc->ppTexture;

        if (IssueExplicitInitialStateBarrier())
        {
            Cmd*           cmd = acquireCmd(pCopyEngine);
            TextureBarrier barrier = { texture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST };
            cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
        }

        TinyImageFormat   fmt = (TinyImageFormat)texture->mFormat;
        const uint32_t    sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
        const uint32_t    rowAlignment = util_get_texture_row_alignment(pRenderer);
        const uint64_t    requiredSize = util_get_surface_size(fmt, texture->mWidth, texture->mHeight, texture->mDepth, rowAlignment,
                                                            sliceAlignment, 0, texture->mMipLevels, 0, texture->mArraySizeMinusOne + 1u);
        MappedMemoryRange range = allocateStagingMemory(pCopyEngine, requiredSize, sliceAlignment, texture->mNodeIndex);
        memset(range.pData, 0, range.mSize);

        // Zero out all subresources
        TextureUpdateDescInternal updateDesc = {};
        updateDesc.mLayerCount = texture->mArraySizeMinusOne + 1u;
        updateDesc.mMipLevels = texture->mMipLevels;
        updateDesc.pTexture = texture;
        updateDesc.mRange = range;
        updateDesc.mCurrentState = RESOURCE_STATE_COPY_DEST;
        updateTexture(pRenderer, pCopyEngine, updateDesc);

        if (IssueTextureCopyBarriers() && pTextureDesc->mStartState != RESOURCE_STATE_COPY_DEST)
        {
            TextureBarrier barrier = { texture, RESOURCE_STATE_COPY_DEST, pTextureDesc->mStartState };
            Cmd*           cmd = acquirePostCopyBarrierCmd(pCopyEngine);
            cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
        }

        return UPLOAD_FUNCTION_RESULT_COMPLETED;
    }

    ASSERT((((pTextureDesc->mFlags & TEXTURE_CREATION_FLAG_SRGB) == 0) || (pTextureDesc->pFileName != NULL)) &&
           "Only textures loaded from file can have TEXTURE_CREATION_FLAG_SRGB. "
           "Please change format of the provided texture if you need srgb format.");

    if (pTextureDesc->pFileName)
    {
        FileStream stream = {};
        bool       success = false;

        TextureUpdateDescInternal updateDesc = {};
        TextureContainerType      container = pTextureDesc->mContainer;

        if (TEXTURE_CONTAINER_DEFAULT == container)
        {
#if defined(TARGET_IOS) || defined(__ANDROID__) || defined(NX64)
            container = TEXTURE_CONTAINER_KTX;
#elif defined(_WINDOWS) || defined(XBOX) || defined(__APPLE__) || defined(__linux__)
            container = TEXTURE_CONTAINER_DDS;
#elif defined(ORBIS) || defined(PROSPERO)
            container = TEXTURE_CONTAINER_GNF;
#endif
        }

        TextureDesc textureDesc = {};
        textureDesc.pName = pTextureDesc->pFileName;
        textureDesc.mFlags |= pTextureDesc->mFlags;

        // Validate that we have found the file format now
        ASSERT(container != TEXTURE_CONTAINER_DEFAULT); //-V547
        if (TEXTURE_CONTAINER_DEFAULT == container)     //-V547
        {
            return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
        }

        switch (container)
        {
        case TEXTURE_CONTAINER_DDS:
        {
#if defined(XBOX)
            success = fsOpenStreamFromPath(RD_TEXTURES, pTextureDesc->pFileName, FM_READ, &stream);
            uint32_t res = 1;
            if (success)
            {
                extern uint32_t loadXDDSTexture(Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags,
                                                Texture** ppTexture);
                res = loadXDDSTexture(pRenderer, &stream, pTextureDesc->pFileName, pTextureDesc->mFlags, pTextureDesc->ppTexture);
                fsCloseStream(&stream);
            }

            if (!res)
            {
                return UPLOAD_FUNCTION_RESULT_COMPLETED;
            }

            LOGF(eINFO, "XDDS: Could not find XDDS texture %s. Trying to load Desktop version", pTextureDesc->pFileName);
#else
            success = fsOpenStreamFromPath(RD_TEXTURES, pTextureDesc->pFileName, FM_READ, &stream);
            if (success)
            {
                success = loadDDSTextureDesc(&stream, &textureDesc);
            }
#endif
            break;
        }
        case TEXTURE_CONTAINER_KTX:
        {
            success = fsOpenStreamFromPath(RD_TEXTURES, pTextureDesc->pFileName, FM_READ, &stream);
            if (success)
            {
                success = loadKTXTextureDesc(&stream, &textureDesc);
                updateDesc.mMipsAfterSlice = true;
                // KTX stores mip size before the mip data
                // This function gets called to skip the mip size so we read the mip data
                updateDesc.pPreMipFunc = [](FileStream* pStream, uint32_t)
                {
                    uint32_t mipSize = 0;
                    fsReadFromStream(pStream, &mipSize, sizeof(mipSize));
                };
            }
            break;
        }
        case TEXTURE_CONTAINER_GNF:
        {
#if defined(ORBIS) || defined(PROSPERO)
            success = fsOpenStreamFromPath(RD_TEXTURES, pTextureDesc->pFileName, FM_READ, &stream);
            uint32_t res = 1;
            if (success)
            {
                extern uint32_t loadGnfTexture(Renderer * pRenderer, FileStream * stream, const char* name, TextureCreationFlags flags,
                                               Texture** ppTexture);
                res = loadGnfTexture(pRenderer, &stream, pTextureDesc->pFileName, pTextureDesc->mFlags, pTextureDesc->ppTexture);
                fsCloseStream(&stream);
            }

            return res ? UPLOAD_FUNCTION_RESULT_INVALID_REQUEST : UPLOAD_FUNCTION_RESULT_COMPLETED;
#endif
        }
        default:
            break;
        }

        if (success)
        {
            textureDesc.mStartState = RESOURCE_STATE_COPY_DEST;
            textureDesc.mNodeIndex = pTextureDesc->mNodeIndex;

            if (pTextureDesc->mFlags & TEXTURE_CREATION_FLAG_SRGB)
            {
                TinyImageFormat srgbFormat = TinyImageFormat_ToSRGB(textureDesc.mFormat);
                if (srgbFormat != TinyImageFormat_UNDEFINED)
                    textureDesc.mFormat = srgbFormat;
                else
                {
                    LOGF(eWARNING,
                         "Trying to load '%s' image using SRGB profile. "
                         "But image has '%s' format, which doesn't have SRGB counterpart.",
                         pTextureDesc->pFileName, TinyImageFormat_Name(textureDesc.mFormat));
                }
            }

#if defined(VULKAN)
            if (pTextureDesc->pYcbcrSampler)
            {
                textureDesc.pSamplerYcbcrConversionInfo = &pTextureDesc->pYcbcrSampler->mVk.mSamplerYcbcrConversionInfo;
            }
#endif
            addTexture(pRenderer, &textureDesc, pTextureDesc->ppTexture);

            updateDesc.mStream = stream;
            updateDesc.pTexture = *pTextureDesc->ppTexture;
            updateDesc.mBaseMipLevel = 0;
            updateDesc.mMipLevels = textureDesc.mMipLevels;
            updateDesc.mBaseArrayLayer = 0;
            updateDesc.mLayerCount = textureDesc.mArraySize;
            updateDesc.mCurrentState = RESOURCE_STATE_COPY_DEST;

            if (IssueExplicitInitialStateBarrier())
            {
                TextureBarrier barrier = { updateDesc.pTexture, RESOURCE_STATE_UNDEFINED, RESOURCE_STATE_COPY_DEST };
                Cmd*           cmd = acquireCmd(pCopyEngine);
                cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
            }

            UploadFunctionResult res = updateTexture(pRenderer, pCopyEngine, updateDesc);

            if (IssueTextureCopyBarriers() && UPLOAD_FUNCTION_RESULT_COMPLETED == res)
            {
                TextureBarrier barrier = { *pTextureDesc->ppTexture, RESOURCE_STATE_COPY_DEST, RESOURCE_STATE_SHADER_RESOURCE };
                Cmd*           cmd = acquirePostCopyBarrierCmd(pCopyEngine);
                cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);
            }

            return res;
        }
    }

    LOGF(eERROR, "Failed to open texture file %s", pTextureDesc->pFileName ? pTextureDesc->pFileName : "<NULL>");
    ASSERT(false);
    return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
}

static void fillGeometryUpdateDesc(Renderer* pRenderer, CopyEngine* pCopyEngine, GeometryLoadDesc* pDesc, Geometry* geom,
                                   uint32_t* indexStride, BufferUpdateDesc vertexUpdateDesc[MAX_VERTEX_BINDINGS],
                                   BufferUpdateDesc indexUpdateDesc[1])
{
    UNREF_PARAM(pCopyEngine);
    bool     structuredBuffers = (pDesc->mFlags & GEOMETRY_LOAD_FLAG_STRUCTURED_BUFFERS) > 0;
    uint32_t indexBufferSize = *indexStride * geom->mIndexCount;

    if (pDesc->pGeometryBuffer)
    {
        if (pDesc->pGeometryBufferLayoutDesc)
        {
            *indexStride = pDesc->pGeometryBufferLayoutDesc->mIndexType == INDEX_TYPE_UINT16 ? sizeof(uint16_t) : sizeof(uint32_t);
            indexBufferSize = *indexStride * geom->mIndexCount;
        }

        addGeometryBufferPart(&pDesc->pGeometryBuffer->mIndex, indexBufferSize, *indexStride, &geom->mIndexBufferChunk);

        indexUpdateDesc->pBuffer = pDesc->pGeometryBuffer->mIndex.pBuffer;
        indexUpdateDesc->mDstOffset = geom->mIndexBufferChunk.mOffset;
    }
    else
    {
        BufferDesc loadDesc = {};
        loadDesc.mDescriptors =
            DESCRIPTOR_TYPE_INDEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
                                                              : (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
        loadDesc.mFlags |= (pDesc->mFlags & GEOMETRY_LOAD_FLAG_RAYTRACING_INPUT)
                               ? (BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS | BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT)
                               : BUFFER_CREATION_FLAG_NONE;
        loadDesc.mSize = indexBufferSize;
        loadDesc.mElementCount = (uint32_t)(loadDesc.mSize / (structuredBuffers ? *indexStride : sizeof(uint32_t)));
        loadDesc.mStructStride = *indexStride;
        loadDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        loadDesc.mStartState = gUma ? gIndexBufferState : RESOURCE_STATE_COPY_DEST;
        addBuffer(pRenderer, &loadDesc, &geom->pIndexBuffer);
        indexUpdateDesc->pBuffer = geom->pIndexBuffer;
        indexUpdateDesc->mDstOffset = 0;
    }

    indexUpdateDesc->mSize = geom->mIndexCount * *indexStride;

    // We need to check for pCpuMappedAddress because when we allocate a custom ResourceHeap with GPU_ONLY memory we don't get any CPU
    // mapped address and we need staging memory
    if (gUma && indexUpdateDesc->pBuffer->pCpuMappedAddress)
    {
        indexUpdateDesc->mInternal.mMappedRange = { (uint8_t*)indexUpdateDesc->pBuffer->pCpuMappedAddress + indexUpdateDesc->mDstOffset };
    }
    else
    {
        indexUpdateDesc->mInternal.mMappedRange.pData = (uint8_t*)tf_calloc_memalign(1, RESOURCE_BUFFER_ALIGNMENT, indexUpdateDesc->mSize);
    }
    indexUpdateDesc->pMappedData = indexUpdateDesc->mInternal.mMappedRange.pData;

    // Vertex buffers
    uint32_t bufferCounter = 0;
    for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
    {
        if (!geom->mVertexStrides[i])
            continue;

        uint32_t size = geom->mVertexStrides[i] * geom->mVertexCount;

        if (pDesc->pGeometryBuffer)
        {
            addGeometryBufferPart(&pDesc->pGeometryBuffer->mVertex[i], size, geom->mVertexStrides[i], &geom->mVertexBufferChunks[i]);
            vertexUpdateDesc[i].pBuffer = pDesc->pGeometryBuffer->mVertex[i].pBuffer;
            vertexUpdateDesc[i].mDstOffset = geom->mVertexBufferChunks[i].mOffset;
        }
        else
        {
            BufferDesc vertexBufferDesc = {};
            vertexBufferDesc.mDescriptors =
                DESCRIPTOR_TYPE_VERTEX_BUFFER | (structuredBuffers ? (DESCRIPTOR_TYPE_BUFFER | DESCRIPTOR_TYPE_RW_BUFFER)
                                                                   : (DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW));
            vertexBufferDesc.mFlags |=
                (pDesc->mFlags & GEOMETRY_LOAD_FLAG_RAYTRACING_INPUT)
                    ? (BUFFER_CREATION_FLAG_SHADER_DEVICE_ADDRESS | BUFFER_CREATION_FLAG_ACCELERATION_STRUCTURE_BUILD_INPUT)
                    : BUFFER_CREATION_FLAG_NONE;
            vertexBufferDesc.mSize = size;
            vertexBufferDesc.mElementCount =
                (uint32_t)(vertexBufferDesc.mSize / (structuredBuffers ? geom->mVertexStrides[i] : sizeof(uint32_t)));
            vertexBufferDesc.mStructStride = geom->mVertexStrides[i];
            vertexBufferDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
            vertexBufferDesc.mStartState = gUma ? gVertexBufferState : RESOURCE_STATE_COPY_DEST;
            vertexBufferDesc.pName = "VertexBuffer";
            addBuffer(pRenderer, &vertexBufferDesc, &geom->pVertexBuffers[bufferCounter]);

            vertexUpdateDesc[i].pBuffer = geom->pVertexBuffers[bufferCounter];
            vertexUpdateDesc[i].mDstOffset = 0;
        }

        vertexUpdateDesc[i].mSize = size;

        // We need to check for pCpuMappedAddress because when we allocate a custom ResourceHeap with GPU_ONLY memory we don't get any CPU
        // mapped address and we need staging memory
        if (gUma && vertexUpdateDesc[i].pBuffer->pCpuMappedAddress)
        {
            vertexUpdateDesc[i].mInternal.mMappedRange = { (uint8_t*)vertexUpdateDesc[i].pBuffer->pCpuMappedAddress +
                                                           vertexUpdateDesc[i].mDstOffset };
        }
        else
        {
            vertexUpdateDesc[i].mInternal.mMappedRange.pData =
                (uint8_t*)tf_calloc_memalign(1, RESOURCE_BUFFER_ALIGNMENT, vertexUpdateDesc[i].mSize);
        }
        vertexUpdateDesc[i].pMappedData = vertexUpdateDesc[i].mInternal.mMappedRange.pData;
        ++bufferCounter;
    }

    geom->mVertexBufferCount = bufferCounter;
}

static UploadFunctionResult loadGeometry(
    Renderer* pRenderer, 
    CopyEngine* pCopyEngine, 
    UpdateRequest& pGeometryLoad)
{
    GeometryLoadDesc* pDesc = &pGeometryLoad.geomLoadDesc;

    // Upload mesh
    UploadFunctionResult uploadResult = UPLOAD_FUNCTION_RESULT_COMPLETED;
    BufferBarrier        barriers[MAX_VERTEX_BINDINGS + 1] = {};
    uint32_t             barrierCount = 0;

    if (!gUma || (pDesc->mIndexUpdateDesc.pMappedData && !pDesc->mIndexUpdateDesc.pBuffer->pCpuMappedAddress))
    {
        pDesc->mIndexUpdateDesc.mCurrentState = gUma ? pDesc->mIndexUpdateDesc.mCurrentState : RESOURCE_STATE_COPY_DEST;
        pDesc->mIndexUpdateDesc.mInternal.mMappedRange =
            allocateStagingMemory(pCopyEngine, pDesc->mIndexUpdateDesc.mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
        ASSERT(pDesc->mIndexUpdateDesc.pMappedData);
        memcpy(pDesc->mIndexUpdateDesc.mInternal.mMappedRange.pData, pDesc->mIndexUpdateDesc.pMappedData, pDesc->mIndexUpdateDesc.mSize);
        tf_free(pDesc->mIndexUpdateDesc.pMappedData);
        if (pDesc->mIndexUpdateDesc.mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER)
        {
            setBufferName(pRenderer, pDesc->mIndexUpdateDesc.mInternal.mMappedRange.pBuffer, pDesc->pFileName);
        }
        pDesc->mIndexUpdateDesc.pMappedData = pDesc->mIndexUpdateDesc.mInternal.mMappedRange.pData;
        uploadResult = updateBuffer(pRenderer, pCopyEngine, pDesc->mIndexUpdateDesc);
    }

    barriers[barrierCount++] = { pDesc->mIndexUpdateDesc.pBuffer, RESOURCE_STATE_COPY_DEST, gIndexBufferState };

    for (uint32_t i = 0; i < MAX_VERTEX_BINDINGS; ++i)
    {
        if (pDesc->mVertexUpdateDesc[i].pBuffer)
        {
            if (!gUma || (pDesc->mVertexUpdateDesc[i].pMappedData && !pDesc->mVertexUpdateDesc[i].pBuffer->pCpuMappedAddress))
            {
                pDesc->mVertexUpdateDesc[i].mCurrentState = gUma ? pDesc->mVertexUpdateDesc[i].mCurrentState : RESOURCE_STATE_COPY_DEST;
                pDesc->mVertexUpdateDesc[i].mInternal.mMappedRange =
                    allocateStagingMemory(pCopyEngine, pDesc->mVertexUpdateDesc[i].mSize, RESOURCE_BUFFER_ALIGNMENT, pDesc->mNodeIndex);
                ASSERT(pDesc->mVertexUpdateDesc[i].pMappedData);
                memcpy(pDesc->mVertexUpdateDesc[i].mInternal.mMappedRange.pData, pDesc->mVertexUpdateDesc[i].pMappedData, pDesc->mVertexUpdateDesc[i].mSize);
                tf_free(pDesc->mVertexUpdateDesc[i].pMappedData);
                if (pDesc->mVertexUpdateDesc[i].mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_TEMP_BUFFER)
                {
                    setBufferName(pRenderer, pDesc->mVertexUpdateDesc[i].mInternal.mMappedRange.pBuffer, pDesc->pFileName);
                }
                pDesc->mVertexUpdateDesc[i].pMappedData = pDesc->mVertexUpdateDesc[i].mInternal.mMappedRange.pData;
                uploadResult = updateBuffer(pRenderer, pCopyEngine, pDesc->mVertexUpdateDesc[i]);
            }
            barriers[barrierCount++] = { pDesc->mVertexUpdateDesc[i].pBuffer, RESOURCE_STATE_COPY_DEST, gVertexBufferState };
        }
    }

    if (!gUma && IssueBufferCopyBarriers())
    {
        Cmd* cmd = acquirePostCopyBarrierCmd(pCopyEngine);
        cmdResourceBarrier(cmd, barrierCount, barriers, 0, NULL, 0, NULL);
    }

    return uploadResult;
}

static UploadFunctionResult copyTexture(Renderer* pRenderer, CopyEngine* pCopyEngine, TextureCopyDesc& pTextureCopy)
{
    UNREF_PARAM(pRenderer);
    Texture*              texture = pTextureCopy.pTexture;
    const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;

    Cmd* cmd = acquireCmd(pCopyEngine);

    if (pTextureCopy.pWaitSemaphore)
        arrpush(pCopyEngine->mWaitSemaphores, pTextureCopy.pWaitSemaphore);

    TextureBarrier barrier = { texture, pTextureCopy.mTextureState, RESOURCE_STATE_COPY_SOURCE };
    barrier.mAcquire = 1;
    barrier.mQueueType = pTextureCopy.mQueueType;
    cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);

    uint32_t numBytes = 0;
    uint32_t rowBytes = 0;
    uint32_t numRows = 0;

    bool ret = util_get_surface_info(texture->mWidth, texture->mHeight, fmt, &numBytes, &rowBytes, &numRows);
    if (!ret)
    {
        return UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;
    }

    SubresourceDataDesc subresourceDesc = {};
    subresourceDesc.mArrayLayer = pTextureCopy.mTextureArrayLayer;
    subresourceDesc.mMipLevel = pTextureCopy.mTextureMipLevel;
    subresourceDesc.mSrcOffset = pTextureCopy.mBufferOffset;
#if defined(METAL) || defined(VULKAN)
    const uint32_t sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
    const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
    uint32_t       subRowPitch = round_up(rowBytes, rowAlignment);
    uint32_t       subSlicePitch = round_up(subRowPitch * numRows, sliceAlignment);
    subresourceDesc.mRowPitch = subRowPitch;
    subresourceDesc.mSlicePitch = subSlicePitch;
#endif
    cmdCopySubresource(cmd, pTextureCopy.pBuffer, pTextureCopy.pTexture, &subresourceDesc);

    barrier = { texture, RESOURCE_STATE_COPY_SOURCE, pTextureCopy.mTextureState };
    barrier.mRelease = 1;
    barrier.mQueueType = pTextureCopy.mQueueType;
    cmdResourceBarrier(cmd, 0, NULL, 1, &barrier, 0, NULL);

    return UPLOAD_FUNCTION_RESULT_COMPLETED;
}
/************************************************************************/
// Internal Resource Loader Implementation
/************************************************************************/
static bool areTasksAvailable(ResourceLoader* pLoader)
{
    for (size_t i = 0; i < MAX_MULTIPLE_GPUS; ++i)
    {
        if (arrlen(pLoader->mRequestQueue[i]))
        {
            return true;
        }
    }

    return false;
}

static void streamerThreadFunc(void* pThreadData)
{
    ResourceLoader* pLoader = (ResourceLoader*)pThreadData;
    ASSERT(pLoader);

    while (pLoader->mRun)
    {
        acquireMutex(&pLoader->mQueueMutex);

        // Check for pending tokens
        // Safe to use mTokenCounter as we are inside critical section
        bool allTokensSignaled = (pLoader->mTokenCompleted == tfrg_atomic64_load_relaxed(&pLoader->mTokenCounter));

        while (!areTasksAvailable(pLoader) && allTokensSignaled && pLoader->mRun)
        {
            // No waiting if not running dedicated resource loader thread.
            if (pLoader->mDesc.mSingleThreaded)
            {
                releaseMutex(&pLoader->mQueueMutex);
                return;
            }
            // Sleep until someone adds an update request to the queue
            waitConditionVariable(&pLoader->mQueueCond, &pLoader->mQueueMutex, TIMEOUT_INFINITE);
        }

        releaseMutex(&pLoader->mQueueMutex);

        for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
        {
            CopyEngine* copyEngine = &pLoader->pCopyEngines[nodeIndex];
            waitCopyEngineSet(pLoader->ppRenderers[nodeIndex], copyEngine);
            resetCopyEngineSet(pLoader->ppRenderers[nodeIndex], copyEngine);
            copyEngine->activeSet = (copyEngine->activeSet + 1) % pLoader->mDesc.mBufferCount;
        }

        // Signal pending tokens from previous frames
        acquireMutex(&pLoader->mTokenMutex);
        tfrg_atomic64_store_release(&pLoader->mTokenCompleted, pLoader->mCurrentTokenState[pLoader->pCopyEngines[0].activeSet]);
        releaseMutex(&pLoader->mTokenMutex);
        wakeAllConditionVariable(&pLoader->mTokenCond);

        uint64_t completionMask = 0;

        for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
        {
            acquireMutex(&pLoader->mQueueMutex);

            UpdateRequest** pRequestQueue = &pLoader->mRequestQueue[nodeIndex];
            CopyEngine*     pCopyEngine = &pLoader->pCopyEngines[nodeIndex];

            if (!arrlen(*pRequestQueue))
            {
                releaseMutex(&pLoader->mQueueMutex);
                continue;
            }

            UpdateRequest* activeQueue = *pRequestQueue;
            *pRequestQueue = NULL;
            releaseMutex(&pLoader->mQueueMutex);

            Renderer* pRenderer = pLoader->ppRenderers[nodeIndex];
            SyncToken maxNodeToken = {};

            ASSERT(arrlen(activeQueue));

            for (ptrdiff_t j = 0; j < arrlen(activeQueue); ++j)
            {
                UpdateRequest updateState = activeQueue[j];
                // #NOTE: acquireCmd also resets copy engine on first use
                Cmd*          cmd = acquireCmd(pCopyEngine);

                UploadFunctionResult result = UPLOAD_FUNCTION_RESULT_COMPLETED;
                switch (updateState.mType)
                {
                case UPDATE_REQUEST_TEXTURE_BARRIER:
                    cmdResourceBarrier(cmd, 0, NULL, 1, &updateState.textureBarrier, 0, NULL);
                    result = UPLOAD_FUNCTION_RESULT_COMPLETED;
                    break;
                case UPDATE_REQUEST_LOAD_BUFFER:
                    result = loadBuffer(pRenderer, pCopyEngine, updateState);
                    break;
                case UPDATE_REQUEST_LOAD_TEXTURE:
                    result = loadTexture(pRenderer, pCopyEngine, updateState);
                    break;
                case UPDATE_REQUEST_LOAD_GEOMETRY:
                    result = loadGeometry(pRenderer, pCopyEngine, updateState);
                    break;
                case UPDATE_REQUEST_COPY_TEXTURE:
                    result = copyTexture(pRenderer, pCopyEngine, updateState.texCopyDesc);
                    break;
                case UPDATE_REQUEST_INVALID:
                    break;
                }

                bool completed = result == UPLOAD_FUNCTION_RESULT_COMPLETED || result == UPLOAD_FUNCTION_RESULT_INVALID_REQUEST;

                completionMask |= (uint64_t)completed << nodeIndex;

                if (updateState.mWaitIndex && completed)
                {
                    ASSERT(maxNodeToken < updateState.mWaitIndex);
                    maxNodeToken = updateState.mWaitIndex;
                }

                ASSERT(result != UPLOAD_FUNCTION_RESULT_STAGING_BUFFER_FULL);
            }

            arrfree(activeQueue);
            pLoader->mMaxToken = max(pLoader->mMaxToken, maxNodeToken);
        }

        if (completionMask != 0)
        {
            for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
            {
                if (completionMask & ((uint64_t)1 << nodeIndex))
                {
                    CopyEngine* copyEngine = &pLoader->pCopyEngines[nodeIndex];
                    streamerFlush(copyEngine);
                    acquireMutex(&pLoader->mSemaphoreMutex);
                    copyEngine->pLastSubmittedSemaphore = copyEngine->resourceSets[copyEngine->activeSet].pSemaphore;
                    releaseMutex(&pLoader->mSemaphoreMutex);
                }
            }
        }

        SyncToken nextToken = max(pLoader->mMaxToken, getLastTokenCompleted());
        pLoader->mCurrentTokenState[pLoader->pCopyEngines[0].activeSet] = nextToken;

        // Signal submitted tokens
        acquireMutex(&pLoader->mTokenMutex);
        tfrg_atomic64_store_release(&pLoader->mTokenSubmitted, pLoader->mCurrentTokenState[pLoader->pCopyEngines[0].activeSet]);
        releaseMutex(&pLoader->mTokenMutex);
        wakeAllConditionVariable(&pLoader->mTokenCond);

        if (pResourceLoader->mDesc.mSingleThreaded)
        {
            return;
        }
    }

    for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
    {
        streamerFlush(&pLoader->pCopyEngines[nodeIndex]);
        waitQueueIdle(pLoader->pCopyEngines[nodeIndex].pQueue);
        cleanupCopyEngine(pLoader->ppRenderers[nodeIndex], &pLoader->pCopyEngines[nodeIndex]);
    }
}

static void CopyEngineFlush(CopyEngine* pCopyEngine)
{
    streamerFlush(pCopyEngine);
    acquireMutex(&pResourceLoader->mSemaphoreMutex);
    pCopyEngine->pLastSubmittedSemaphore = pCopyEngine->resourceSets[pCopyEngine->activeSet].pSemaphore;
    releaseMutex(&pResourceLoader->mSemaphoreMutex);

    SyncToken nextToken = max(pResourceLoader->mMaxToken, getLastTokenCompleted());
    pResourceLoader->mCurrentTokenState[pResourceLoader->pCopyEngines[0].activeSet] = nextToken;

    // Signal submitted tokens
    acquireMutex(&pResourceLoader->mTokenMutex);
    tfrg_atomic64_store_release(&pResourceLoader->mTokenSubmitted,
                                pResourceLoader->mCurrentTokenState[pResourceLoader->pCopyEngines[0].activeSet]);
    releaseMutex(&pResourceLoader->mTokenMutex);
    wakeAllConditionVariable(&pResourceLoader->mTokenCond);

    pCopyEngine->activeSet = (pCopyEngine->activeSet + 1) % pResourceLoader->mDesc.mBufferCount;
    acquireCmd(pCopyEngine);
}

static void initResourceLoader(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc, ResourceLoader** ppLoader)
{
    ASSERT(rendererCount > 0);
    ASSERT(rendererCount <= MAX_MULTIPLE_GPUS);

    if (!pDesc)
        pDesc = &gDefaultResourceLoaderDesc;

    ResourceLoader* pLoader = tf_new(ResourceLoader);

    uint32_t gpuCount = rendererCount;
    if (ppRenderers[0]->mGpuMode != GPU_MODE_UNLINKED)
    {
        ASSERT(rendererCount == 1);
        gpuCount = ppRenderers[0]->mLinkedNodeCount;
    }

    pLoader->mGpuCount = gpuCount;

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        ASSERT(rendererCount == 1 || ppRenderers[i]->mGpuMode == GPU_MODE_UNLINKED);
        // Replicate single renderer in linked mode, for uniform handling of linked and unlinked multi gpu.
        pLoader->ppRenderers[i] = (rendererCount > 1) ? ppRenderers[i] : ppRenderers[0];
    }

    pLoader->mRun = true; //-V601
    pLoader->mDesc = *pDesc;

    initMutex(&pLoader->mQueueMutex);
    initMutex(&pLoader->mTokenMutex);
    initConditionVariable(&pLoader->mQueueCond);
    initConditionVariable(&pLoader->mTokenCond);
    initMutex(&pLoader->mSemaphoreMutex);
    initMutex(&pLoader->mUploadEngineMutex);

    pLoader->mTokenCounter = 0;
    pLoader->mTokenCompleted = 0;
    pLoader->mTokenSubmitted = 0;

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        CopyEngineDesc desc = {};
        desc.mBufferCount = pLoader->mDesc.mBufferCount;
        desc.mNodeIndex = i;
        desc.mQueueType = QUEUE_TYPE_GRAPHICS;
        desc.mSize = pLoader->mDesc.mBufferSize;
        desc.pQueueName = "UPLOAD";
        setupCopyEngine(pLoader->ppRenderers[i], &desc, &pLoader->pUploadEngines[i]);

        desc = {};
        desc.mBufferCount = pLoader->mDesc.mBufferCount;
        desc.mNodeIndex = i;
        desc.mQueueType = QUEUE_TYPE_TRANSFER;
        desc.mSize = pLoader->mDesc.mBufferSize;
        desc.pQueueName = "COPY";
        setupCopyEngine(pLoader->ppRenderers[i], &desc, &pLoader->pCopyEngines[i]);

        CopyEngine* copyEngine = &pLoader->pCopyEngines[i];
        copyEngine->flushOnOverflow = true;
        copyEngine->pFnFlush = CopyEngineFlush;

#if defined(STRICT_QUEUE_TYPE_BARRIERS)
        if (StrictQueueTypeBarriers())
        {
            for (uint32_t b = 0; b < pDesc->mBufferCount; ++b)
            {
                CopyResourceSet& resourceSet = pLoader->pCopyEngines[i].resourceSets[b];
                CmdPoolDesc      poolDesc = {};
                poolDesc.pQueue = pLoader->pUploadEngines[i].pQueue;
                addCmdPool(pLoader->ppRenderers[i], &poolDesc, &resourceSet.pPostCopyBarrierCmdPool);
                CmdDesc cmdDesc = {};
                cmdDesc.pPool = resourceSet.pPostCopyBarrierCmdPool;
#ifdef ENABLE_GRAPHICS_DEBUG
                static char buffer[MAX_DEBUG_NAME_LENGTH];
                snprintf(buffer, sizeof(buffer), "Node %u Strict Queue buffer %u Cmd", i, b);
                cmdDesc.pName = buffer;
#endif // ENABLE_GRAPHICS_DEBUG
                addCmd(pLoader->ppRenderers[i], &cmdDesc, &resourceSet.pPostCopyBarrierCmd);
                addFence(pLoader->ppRenderers[i], &resourceSet.pPostCopyBarrierFence);
            }
        }
#endif
    }

    ThreadDesc threadDesc = {};
    threadDesc.pFunc = streamerThreadFunc;
    threadDesc.pData = pLoader;
    strncpy(threadDesc.mThreadName, "ResourceLoaderTask", sizeof(threadDesc.mThreadName));

#if defined(NX64)
    threadDesc.setAffinityMask = true;
    threadDesc.affinityMask[0] = 1;
#endif

#if defined(ANDROID) && defined(USE_MULTIPLE_RENDER_APIS)
    gUma = gPlatformParameters.mSelectedRendererApi == RENDERER_API_VULKAN;
#endif

    // Create dedicated resource loader thread.
    if (!pLoader->mDesc.mSingleThreaded)
    {
        initThread(&threadDesc, &pLoader->mThread);
    }

    *ppLoader = pLoader;
}

static void exitResourceLoader(ResourceLoader* pLoader)
{
    pLoader->mRun = false; //-V601

    if (pLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pLoader);
    }
    else
    {
        wakeOneConditionVariable(&pLoader->mQueueCond);
        joinThread(pLoader->mThread);
    }

    for (uint32_t nodeIndex = 0; nodeIndex < pLoader->mGpuCount; ++nodeIndex)
    {
        waitQueueIdle(pLoader->pUploadEngines[nodeIndex].pQueue);

        Renderer* renderer = pLoader->ppRenderers[nodeIndex];
        cleanupCopyEngine(renderer, &pLoader->pUploadEngines[nodeIndex]);
    }

    destroyConditionVariable(&pLoader->mQueueCond);
    destroyConditionVariable(&pLoader->mTokenCond);
    destroyMutex(&pLoader->mQueueMutex);
    destroyMutex(&pLoader->mTokenMutex);
    destroyMutex(&pLoader->mSemaphoreMutex);
    destroyMutex(&pLoader->mUploadEngineMutex);

    tf_delete(pLoader);
}

static void queueBufferLoad(ResourceLoader* pLoader, BufferLoadDescInternal* pBufferLoad, SyncToken* token)
{
    uint32_t nodeIndex = pBufferLoad->pBuffer->mNodeIndex;
    acquireMutex(&pLoader->mQueueMutex);

    SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

    arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pBufferLoad));
    UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
    if (pLastRequest)
        pLastRequest->mWaitIndex = t;

    releaseMutex(&pLoader->mQueueMutex);
    wakeOneConditionVariable(&pLoader->mQueueCond);
    if (token)
        *token = max(t, *token);

    if (pResourceLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pResourceLoader);
    }
}

static void queueTextureLoad(ResourceLoader* pLoader, TextureLoadDescInternal* pTextureLoad, SyncToken* token)
{
    uint32_t nodeIndex = pTextureLoad->mNodeIndex;
    acquireMutex(&pLoader->mQueueMutex);

    SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

    arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pTextureLoad));
    UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
    if (pLastRequest)
        pLastRequest->mWaitIndex = t;

    releaseMutex(&pLoader->mQueueMutex);
    wakeOneConditionVariable(&pLoader->mQueueCond);
    if (token)
        *token = max(t, *token);

    if (pResourceLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pResourceLoader);
    }
}

static void queueGeometryLoad(ResourceLoader* pLoader, GeometryLoadDesc* pGeometryLoad, SyncToken* token)
{
    uint32_t nodeIndex = pGeometryLoad->mNodeIndex;
    acquireMutex(&pLoader->mQueueMutex);

    SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

    arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pGeometryLoad));
    UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
    if (pLastRequest)
        pLastRequest->mWaitIndex = t;

    releaseMutex(&pLoader->mQueueMutex);
    wakeOneConditionVariable(&pLoader->mQueueCond);
    if (token)
        *token = max(t, *token);

    if (pResourceLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pResourceLoader);
    }
}

static void queueTextureBarrier(ResourceLoader* pLoader, Texture* pTexture, ResourceState state, SyncToken* token)
{
    uint32_t nodeIndex = pTexture->mNodeIndex;
    acquireMutex(&pLoader->mQueueMutex);

    SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

    arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(TextureBarrier{ pTexture, RESOURCE_STATE_UNDEFINED, state }));
    UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
    if (pLastRequest)
        pLastRequest->mWaitIndex = t;

    releaseMutex(&pLoader->mQueueMutex);
    wakeOneConditionVariable(&pLoader->mQueueCond);
    if (token)
        *token = max(t, *token);

    if (pResourceLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pResourceLoader);
    }
}

static void queueTextureCopy(ResourceLoader* pLoader, TextureCopyDesc* pTextureCopy, SyncToken* token)
{
    ASSERT(pTextureCopy->pTexture->mNodeIndex == pTextureCopy->pBuffer->mNodeIndex);
    uint32_t nodeIndex = pTextureCopy->pTexture->mNodeIndex;
    acquireMutex(&pLoader->mQueueMutex);

    SyncToken t = tfrg_atomic64_add_relaxed(&pLoader->mTokenCounter, 1) + 1;

    arrpush(pLoader->mRequestQueue[nodeIndex], UpdateRequest(*pTextureCopy));
    UpdateRequest* pLastRequest = arrback(pLoader->mRequestQueue[nodeIndex]);
    if (pLastRequest)
        pLastRequest->mWaitIndex = t;

    releaseMutex(&pLoader->mQueueMutex);
    wakeOneConditionVariable(&pLoader->mQueueCond);
    if (token)
        *token = max(t, *token);

    if (pResourceLoader->mDesc.mSingleThreaded)
    {
        streamerThreadFunc(pResourceLoader);
    }
}

static void waitForToken(ResourceLoader* pLoader, const SyncToken* token)
{
    if (pLoader->mDesc.mSingleThreaded)
    {
        return;
    }
    acquireMutex(&pLoader->mTokenMutex);
    while (!isTokenCompleted(token))
    {
        waitConditionVariable(&pLoader->mTokenCond, &pLoader->mTokenMutex, TIMEOUT_INFINITE);
    }
    releaseMutex(&pLoader->mTokenMutex);
}

static void waitForTokenSubmitted(ResourceLoader* pLoader, const SyncToken* token)
{
    if (pLoader->mDesc.mSingleThreaded)
    {
        return;
    }
    acquireMutex(&pLoader->mTokenMutex);
    while (!isTokenSubmitted(token))
    {
        waitConditionVariable(&pLoader->mTokenCond, &pLoader->mTokenMutex, TIMEOUT_INFINITE);
    }
    releaseMutex(&pLoader->mTokenMutex);
}

/************************************************************************/
// Resource Loader Interface Implementation
/************************************************************************/
void initResourceLoaderInterface(Renderer* pRenderer, ResourceLoaderDesc* pDesc)
{
    initResourceLoader(&pRenderer, 1, pDesc, &pResourceLoader);
}

void exitResourceLoaderInterface(Renderer* pRenderer)
{
    UNREF_PARAM(pRenderer);

    exitResourceLoader(pResourceLoader);
}

void initResourceLoaderInterface(Renderer** ppRenderers, uint32_t rendererCount, ResourceLoaderDesc* pDesc)
{
    initResourceLoader(ppRenderers, rendererCount, pDesc, &pResourceLoader);
}

void exitResourceLoaderInterface(Renderer** pRenderers, uint32_t rendererCount)
{
    UNREF_PARAM(pRenderers);
    UNREF_PARAM(rendererCount);
    exitResourceLoader(pResourceLoader);
}

void getResourceSizeAlign(const BufferLoadDesc* pDesc, ResourceSizeAlign* pOut)
{
    getBufferSizeAlign(pResourceLoader->ppRenderers[pDesc->mDesc.mNodeIndex], &pDesc->mDesc, pOut);
}

void getResourceSizeAlign(const TextureLoadDesc* pDesc, ResourceSizeAlign* pOut)
{
    getTextureSizeAlign(pResourceLoader->ppRenderers[pDesc->mNodeIndex], pDesc->pDesc, pOut);
}

void addResource(BufferLoadDesc* pBufferDesc, SyncToken* token)
{
    if (token)
    {
        *token = max<uint64_t>(0, *token);
    }

    if (pBufferDesc->pData && !token)
    {
        LOADER_LOGF(eWARNING,
                    "addResource : BufferLoadDesc(%s)::pData is non NULL but token is NULL. It is undefined behavior if pData is freed "
                    "before the buffer load has completed on the ResourceLoader thread. Use waitForAllResourceLoads before freeing pData "
                    "when explicit token was not passed",
                    pBufferDesc->mDesc.pName ? pBufferDesc->mDesc.pName : "Unnamed");
    }

    ResourceState startState = pBufferDesc->mDesc.mStartState;
    if (RESOURCE_MEMORY_USAGE_GPU_ONLY == pBufferDesc->mDesc.mMemoryUsage && !pBufferDesc->mDesc.mStartState)
    {
        startState = ResourceStartState(&pBufferDesc->mDesc);
        LOADER_LOGF(eWARNING, "Buffer start state not provided. Determined the start state as (%u) based on the provided BufferDesc",
                    (uint32_t)pBufferDesc->mDesc.mStartState);
    }

    Renderer*  pRenderer = pResourceLoader->ppRenderers[pBufferDesc->mDesc.mNodeIndex];
    const bool update = pBufferDesc->pData || pBufferDesc->mForceReset;
    const bool gpuUpdate = pBufferDesc->mDesc.mMemoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY && update && !gUma;

    if (gpuUpdate)
    {
        pBufferDesc->mDesc.mStartState = RESOURCE_STATE_COPY_DEST;
    }
    else
    {
        pBufferDesc->mDesc.mStartState = startState;
    }

    const uint64_t bufferSize = pBufferDesc->mDesc.mSize;
    addBuffer(pRenderer, &pBufferDesc->mDesc, pBufferDesc->ppBuffer);

    if (update)
    {
        BufferLoadDescInternal loadDesc = {};
        loadDesc.mForceReset = pBufferDesc->mForceReset;
        loadDesc.mStartState = startState;
        loadDesc.pBuffer = *pBufferDesc->ppBuffer;
        loadDesc.pData = pBufferDesc->pData;
        loadDesc.mDataSize = bufferSize;
        if (gpuUpdate && RESOURCE_MEMORY_USAGE_GPU_ONLY == loadDesc.pBuffer->mMemoryUsage)
        {
            loadDesc.pSrcBuffer = pBufferDesc->pSrcBuffer;
            loadDesc.mSrcOffset = pBufferDesc->mSrcOffset;
        }
        else
        {
            ASSERT(!pBufferDesc->pSrcBuffer);
            loadDesc.pSrcBuffer = loadDesc.pBuffer;
            loadDesc.mSrcOffset = 0;
        }
        queueBufferLoad(pResourceLoader, &loadDesc, token);
    }
}

void addResource(TextureLoadDesc* pTextureDesc, SyncToken* token)
{
    ASSERT(pTextureDesc->ppTexture);

    if (token)
    {
        *token = max<uint64_t>(0, *token);
    }

    if (!pTextureDesc->pFileName && pTextureDesc->pDesc)
    {
        ASSERT(pTextureDesc->pDesc->mStartState);

        TextureDesc textureDesc = *pTextureDesc->pDesc;
#if defined(GFX_DRIVER_MANAGED_VIDEO_MEMORY)
        if (pTextureDesc->mForceReset)
        {
            // If we are going to mem zero using staging buffer set start state to copy dest to avoid one barrier in the beginning
            textureDesc.mStartState = RESOURCE_STATE_COPY_DEST;
        }
#endif
        // If texture is supposed to be filled later (UAV / Update later / ...) proceed with the mStartState provided by the user in the
        // texture description
        addTexture(pResourceLoader->ppRenderers[pTextureDesc->mNodeIndex], &textureDesc, pTextureDesc->ppTexture);

        if (pTextureDesc->mForceReset)
        {
#if !defined(GFX_DRIVER_MANAGED_VIDEO_MEMORY)
            Texture* texture = *pTextureDesc->ppTexture;
#if defined(ORBIS)
            void*    ptr = texture->mStruct.mSrvDescriptor.getBaseAddress();
            uint64_t size = texture->mStruct.mSrvDescriptor.getSizeAlign().m_size;
#elif defined(PROSPERO)
            void*    ptr = texture->mStruct.mSrv.getDataAddress();
            uint64_t size = PROSPERO_RENDERER_NAMESPACE::getSize(&texture->mStruct.mSrv).m_size;
#elif defined(XBOX)
            void*               ptr = (void*)texture->mDx.pResource->GetGPUVirtualAddress();
            D3D12_RESOURCE_DESC desc = texture->mDx.pResource->GetDesc();
            uint64_t            size = 0;
            pResourceLoader->ppRenderers[pTextureDesc->mNodeIndex]->mDx.pDevice->GetCopyableFootprints(
                &desc, 0, desc.MipLevels * desc.DepthOrArraySize, 0, NULL, NULL, NULL, &size);
#else
#error : Not implemented
#endif
            memset(ptr, 0, size);
#else
            TextureLoadDescInternal loadDesc = {};
            loadDesc.ppTexture = pTextureDesc->ppTexture;
            loadDesc.mForceReset = true;
            loadDesc.mStartState = pTextureDesc->pDesc->mStartState;
            queueTextureLoad(pResourceLoader, &loadDesc, token);
#endif
            return;
        }

        if (IssueExplicitInitialStateBarrier())
        {
            ResourceState startState = pTextureDesc->pDesc->mStartState;
            // Check whether this is required (user specified a state other than undefined / common)
            if (startState == RESOURCE_STATE_UNDEFINED || startState == RESOURCE_STATE_COMMON) //-V560
            {
                startState = ResourceStartState(pTextureDesc->pDesc->mDescriptors & DESCRIPTOR_TYPE_RW_TEXTURE);
            }
            queueTextureBarrier(pResourceLoader, *pTextureDesc->ppTexture, startState, token);
        }
    }
    else
    {
        TextureLoadDescInternal loadDesc = {};
        loadDesc.ppTexture = pTextureDesc->ppTexture;
        loadDesc.mContainer = pTextureDesc->mContainer;
        loadDesc.mFlags = pTextureDesc->mCreationFlag;
        loadDesc.mNodeIndex = pTextureDesc->mNodeIndex;
        loadDesc.pFileName = pTextureDesc->pFileName;
        loadDesc.pYcbcrSampler = pTextureDesc->pYcbcrSampler;
        queueTextureLoad(pResourceLoader, &loadDesc, token);
    }
}

void addResource(GeometryLoadDesc* pDesc, SyncToken* token)
{
    ASSERT(pDesc->pVertexLayout);
    ASSERT(pDesc->ppGeometry);

    GeometryLoadDesc updateDesc = *pDesc;
    updateDesc.pFileName = pDesc->pFileName;

    uint32_t extraSize = sizeof(VertexLayout);

    VertexLayout* pCopyVertexLayout = (VertexLayout*)tf_malloc(extraSize);
    memcpy(pCopyVertexLayout, pDesc->pVertexLayout, sizeof(VertexLayout));
    updateDesc.pVertexLayout = pCopyVertexLayout;

    queueGeometryLoad(pResourceLoader, &updateDesc, token);
}

void removeResource(Buffer* pBuffer) { removeBuffer(pResourceLoader->ppRenderers[pBuffer->mNodeIndex], pBuffer); }

void removeResource(Texture* pTexture) { removeTexture(pResourceLoader->ppRenderers[pTexture->mNodeIndex], pTexture); }

void removeResource(Geometry* pGeom)
{
    if (!pGeom)
        return;

    if (pGeom->pGeometryBuffer)
    {
        removeGeometryBufferPart(&pGeom->pGeometryBuffer->mIndex, &pGeom->mIndexBufferChunk);

        for (uint32_t i = 0; i < pGeom->mVertexBufferCount; ++i)
        {
            removeGeometryBufferPart(&pGeom->pGeometryBuffer->mVertex[i], &pGeom->mVertexBufferChunks[i]);
        }
    }
    else
    {
        removeResource(pGeom->pIndexBuffer);

        for (uint32_t i = 0; i < pGeom->mVertexBufferCount; ++i)
        {
            removeResource(pGeom->pVertexBuffers[i]);
        }
    }

    tf_free(pGeom);
}

void removeResource(GeometryData* pGeom)
{
    removeGeometryShadowData(pGeom);
    tf_free(pGeom);
}

void removeGeometryShadowData(GeometryData* pGeom)
{
    if (pGeom->pShadow)
    {
        tf_free(pGeom->pShadow);
        pGeom->pShadow = nullptr;
    }
}

// Interface to add/remove BufferChunkAllocators is currently private but we could expose it in the IResourceLoader interface if needed
typedef struct BufferChunkAllocatorDesc
{
    Buffer* pBuffer;
} BufferChunkAllocatorDesc;

static void addBufferChunkAllocator(BufferChunkAllocatorDesc* pDesc, BufferChunkAllocator* pOut)
{
    ASSERT(pDesc);
    ASSERT(pOut);

    pOut->pBuffer = pDesc->pBuffer;
    pOut->mSize = (uint32_t)pDesc->pBuffer->mSize;

    BufferChunk firstUnusedChunk{ 0, (uint32_t)pDesc->pBuffer->mSize };
    arrpush(pOut->mUnusedChunks, firstUnusedChunk);
}

static void removeBufferChunkAllocator(BufferChunkAllocator* pBuffer)
{
    ASSERT(pBuffer);
    ASSERT(pBuffer->mUsedChunkCount == 0 && "Expecting all parts to be released at this point");

    if (pBuffer->pBuffer)
    {
        ASSERT(arrlen(pBuffer->mUnusedChunks) == 1 && "Expecting just one chunk since the buffer is completely empty");

        // We are checking that the unnused chunk offset is 0 because we currently assume that a BufferChunkAllocator covers the entire
        // buffer, but we could change this to allow to have several BufferChunkAllocators over the same buffer, each working on a fixed
        // memory range of the buffer.
        //
        // For example: In buffer below we could do the following splits
        // Buffer: [------------------------------------------------------]
        // Splits: [------------------A-------------------|--------B------]
        //
        // One BufferChunkAllocator would cover memory range A and would be use by the App to store big chunks of data while other
        // BufferChunkAllocator would cover memory range B and be used to fit smaller ammounts of data.
        //
        // Note: If we want this behavior we would need extend BufferChunkAllocatorDesc to provide mOffset and mSize that the
        // BufferChunkAllocator would cover,
        //       if mSize is 0 we would use the size of the buffer.
        //       We would also need to consider if we want to expose the add/removeBufferChunkAllocator interface to the user and let him
        //       allocate the BufferChunkAllocator or we want to include this splitting logic in addGeometryBuffer.
        ASSERT(pBuffer->mUnusedChunks && (pBuffer->mUnusedChunks[0].mOffset == 0) && (pBuffer->mUnusedChunks[0].mSize == pBuffer->mSize) &&
               "Expecting just one chunk since the buffer is completely empty");

        arrfree(pBuffer->mUnusedChunks);
    }
}

void addGeometryBuffer(GeometryBufferLoadDesc* pDesc)
{
    DescriptorType flags = DESCRIPTOR_TYPE_BUFFER_RAW | DESCRIPTOR_TYPE_RW_BUFFER_RAW;

    GeometryBuffer* pBuffer = (GeometryBuffer*)tf_calloc(1, sizeof *pBuffer);

    if (!VERIFYMSG(pBuffer, "Couldn't allocate GeometryBuffer"))
    {
        return;
    }

    *pDesc->pOutGeometryBuffer = pBuffer;

    BufferLoadDesc loadDesc = {};

    Buffer* pIndexBuffer = NULL;
    Buffer* pVertexBuffer = NULL;

    loadDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
    loadDesc.mDesc.mSize = pDesc->mIndicesSize;
    loadDesc.ppBuffer = &pIndexBuffer;
    loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER | flags;
    loadDesc.mDesc.mStructStride = sizeof(uint32_t);
    loadDesc.mDesc.mElementCount = (uint32_t)(loadDesc.mDesc.mSize / loadDesc.mDesc.mStructStride);
    loadDesc.mDesc.mStartState = gUma ? gIndexBufferState : pDesc->mStartState;
    loadDesc.mDesc.pName = pDesc->pNameIndexBuffer ? pDesc->pNameIndexBuffer : "GeometryBuffer Indices (unnamed)";
    loadDesc.mDesc.pPlacement = pDesc->pIndicesPlacement;
    addResource(&loadDesc, nullptr);

    BufferChunkAllocatorDesc allocDesc = { pIndexBuffer };
    addBufferChunkAllocator(&allocDesc, &pBuffer->mIndex);

    for (size_t i = 0; i < TF_ARRAY_COUNT(pDesc->mVerticesSizes); ++i)
    {
        if (!pDesc->mVerticesSizes[i])
            continue;

        loadDesc.mDesc.mSize = pDesc->mVerticesSizes[i];
        loadDesc.ppBuffer = &pVertexBuffer;
        loadDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER | flags;
        loadDesc.mDesc.mStructStride = sizeof(uint32_t);
        loadDesc.mDesc.mElementCount = (uint32_t)(loadDesc.mDesc.mSize / loadDesc.mDesc.mStructStride);
        loadDesc.mDesc.mStartState = gUma ? gVertexBufferState : pDesc->mStartState;
        loadDesc.mDesc.pName = pDesc->pNamesVertexBuffers[i] ? pDesc->pNamesVertexBuffers[i] : "GeometryBuffer Vertices (unnamed)";
        loadDesc.mDesc.pPlacement = pDesc->pVerticesPlacements[i];
        addResource(&loadDesc, nullptr);

        allocDesc = { pVertexBuffer };
        addBufferChunkAllocator(&allocDesc, &pBuffer->mVertex[i]);
    }
}

void removeGeometryBuffer(GeometryBuffer* pGeomBuffer)
{
    if (!pGeomBuffer)
        return;

    removeBufferChunkAllocator(&pGeomBuffer->mIndex);
    if (pGeomBuffer->mIndex.pBuffer)
        removeResource(pGeomBuffer->mIndex.pBuffer);

    for (size_t i = 0; i < TF_ARRAY_COUNT(pGeomBuffer->mVertex); ++i)
    {
        removeBufferChunkAllocator(&pGeomBuffer->mVertex[i]);
        if (pGeomBuffer->mVertex[i].pBuffer)
            removeResource(pGeomBuffer->mVertex[i].pBuffer);
    }

    tf_free(pGeomBuffer);
}

void addGeometryBufferPart(BufferChunkAllocator* pBuffer, uint32_t size, uint32_t alignment, BufferChunk* pOut,
                           BufferChunk* pRequestedChunk)
{
    if (size == 0)
        return;
    if (size > pBuffer->mSize)
    {
        *pOut = {};
        ASSERT(false);
        return;
    }

    if (pRequestedChunk)
    {
        ASSERT(pRequestedChunk->mOffset + pRequestedChunk->mSize <= pBuffer->mSize);

        // Try to allocate the requested slot
        for (uint32_t i = 0; i < arrlenu(pBuffer->mUnusedChunks); ++i)
        {
            BufferChunk* chunk = &pBuffer->mUnusedChunks[i];

            const uint32_t chunkEnd = chunk->mOffset + chunk->mSize;
            const uint32_t requestedEnd = pRequestedChunk->mOffset + pRequestedChunk->mSize;
            if (chunk->mOffset <= pRequestedChunk->mOffset && chunkEnd >= requestedEnd)
            {
                ++pBuffer->mUsedChunkCount;

                *pOut = *pRequestedChunk;

                if (chunk->mOffset == pRequestedChunk->mOffset && chunkEnd == requestedEnd)
                {
                    // Exact chunk
                    arrdel(pBuffer->mUnusedChunks, i);
                    return;
                }

                // There's unnused memory before the requested chunk
                if (chunk->mOffset < pRequestedChunk->mOffset)
                {
                    BufferChunk prevChunk = { chunk->mOffset, pRequestedChunk->mOffset - chunk->mOffset };
                    arrins(pBuffer->mUnusedChunks, i, prevChunk);
                    i++;
                    chunk = &pBuffer->mUnusedChunks[i];
                }

                if (chunkEnd == requestedEnd)
                {
                    // We consummed the full chunk
                    arrdel(pBuffer->mUnusedChunks, i);
                }
                else
                {
                    // There's unnused memory after the requested chunk
                    chunk->mSize = chunkEnd - requestedEnd;
                    chunk->mOffset = pRequestedChunk->mOffset + pRequestedChunk->mSize;
                    ASSERT(chunk->mSize > 0);
                }

                return;
            }
        }

        ASSERT(false && "Failed to allocate the requested chunk");
        return;
    }

    // TODO binary search. (unused chunk array is sorted already)
    for (uint32_t i = 0; i < arrlenu(pBuffer->mUnusedChunks); ++i)
    {
        BufferChunk* chunk = &pBuffer->mUnusedChunks[i];

        if (chunk->mSize < size)
            continue;

        if (alignment > 0)
        {
            uint32_t padding = chunk->mOffset % alignment;
            if (padding > 0)
                padding = alignment - padding;

            if (chunk->mSize - padding < size)
                continue;

            if (padding > 0)
            {
                BufferChunk paddingChunk = {
                    chunk->mOffset,
                    padding,
                };

                arrins(pBuffer->mUnusedChunks, i, paddingChunk);
                i++;

                chunk = &pBuffer->mUnusedChunks[i];
                chunk->mOffset += padding;
                chunk->mSize -= padding;
            }
        }

        pOut->mOffset = chunk->mOffset;
        pOut->mSize = size;

        chunk->mOffset += size;
        chunk->mSize -= size;

        if (chunk->mSize == 0)
        {
            arrdel(pBuffer->mUnusedChunks, i);
        }

        ++pBuffer->mUsedChunkCount;
        return;
    }

    *pOut = {};
    ASSERT(false);
}

void removeGeometryBufferPart(BufferChunkAllocator* pBuffer, BufferChunk* pChunk)
{
    ASSERT(pChunk->mSize ? pBuffer != NULL : true);
    if (!pBuffer || pChunk->mSize == 0)
        return;

    ASSERT(pBuffer->mUsedChunkCount);

    --pBuffer->mUsedChunkCount;

    const uint64_t partEnd = pChunk->mOffset + pChunk->mSize;

    // TODO binary search. (unused chunk array is sorted already)
    uint64_t i = 0;
    for (; i < arrlenu(pBuffer->mUnusedChunks); ++i)
    {
        BufferChunk* chunk = &pBuffer->mUnusedChunks[i];
        if (partEnd < chunk->mOffset)
            break;

        auto chunkEnd = chunk->mOffset + chunk->mSize;
        if (chunkEnd == pChunk->mOffset) // if pChunk goes after chunk, merge both
        {
            chunk->mSize += pChunk->mSize;
            if (i + 1 < arrlenu(pBuffer->mUnusedChunks)) // If there's another empty chunk after pChunk, merge that one too
            {
                if (partEnd == pBuffer->mUnusedChunks[i + 1].mOffset)
                {
                    chunk->mSize += pBuffer->mUnusedChunks[i + 1].mSize;
                    arrdel(pBuffer->mUnusedChunks, i + 1);
                }
            }
            return;
        }

        if (partEnd == chunk->mOffset) // If pChunk goes after chunk, merge both
        {
            chunk->mOffset = pChunk->mOffset;
            chunk->mSize += pChunk->mSize;
            return;
        }
    }

    arrins(pBuffer->mUnusedChunks, i, *pChunk);
}

void beginUpdateResource(BufferUpdateDesc* pBufferUpdate)
{
    Buffer*   pBuffer = pBufferUpdate->pBuffer;
    Renderer* pRenderer = pResourceLoader->ppRenderers[pBuffer->mNodeIndex];
    ASSERT(pBuffer);

    uint64_t size = pBufferUpdate->mSize > 0 ? pBufferUpdate->mSize : (pBufferUpdate->pBuffer->mSize - pBufferUpdate->mDstOffset);
    ASSERT(pBufferUpdate->mDstOffset + size <= pBuffer->mSize);

    ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
    if (gUma || memoryUsage != RESOURCE_MEMORY_USAGE_GPU_ONLY)
    {
        ASSERTMSG(!pBufferUpdate->pSrcBuffer, "No point in staging buffer when we are directly writing into dst buffer. "
                                              "If this is not a GPU_ONLY buffer you can use use isUma() to handle this case, no need to "
                                              "create this staging buffer on the App side.");
        bool map = !pBuffer->pCpuMappedAddress;
        if (map)
        {
            mapBuffer(pRenderer, pBuffer, NULL);
        }

        pBufferUpdate->mInternal.mMappedRange = { (uint8_t*)pBuffer->pCpuMappedAddress + pBufferUpdate->mDstOffset, pBuffer };
        pBufferUpdate->pMappedData = pBufferUpdate->mInternal.mMappedRange.pData;
        pBufferUpdate->mInternal.mMappedRange.mFlags = map ? MAPPED_RANGE_FLAG_UNMAP_BUFFER : 0;
    }
    else
    {
        // Staging buffer provided by user
        if (pBufferUpdate->pSrcBuffer)
        {
            pBufferUpdate->mInternal.mMappedRange.pBuffer = pBufferUpdate->pSrcBuffer;
            pBufferUpdate->mInternal.mMappedRange.mOffset = pBufferUpdate->mSrcOffset;
            pBufferUpdate->mInternal.mMappedRange.mSize = size;
            pBufferUpdate->mInternal.mMappedRange.pData =
                (uint8_t*)pBufferUpdate->pSrcBuffer->pCpuMappedAddress + pBufferUpdate->mSrcOffset;
            pBufferUpdate->pMappedData = pBufferUpdate->mInternal.mMappedRange.pData;
            return;
        }

        MutexLock         lock(pResourceLoader->mUploadEngineMutex);
        const uint32_t    nodeIndex = pBufferUpdate->pBuffer->mNodeIndex;
        CopyEngine*       pCopyEngine = &pResourceLoader->pUploadEngines[nodeIndex];
        MappedMemoryRange range = allocateStagingMemory(pCopyEngine, size, RESOURCE_BUFFER_ALIGNMENT, nodeIndex);
        if (!range.pData)
        {
            range = allocateUploadMemory(pRenderer, size, RESOURCE_BUFFER_ALIGNMENT);
            arrpush(pCopyEngine->resourceSets[pCopyEngine->activeSet].mTempBuffers, range.pBuffer);
        }

        pBufferUpdate->pMappedData = range.pData;
        pBufferUpdate->mInternal.mMappedRange = range;
    }
}

void endUpdateResource(BufferUpdateDesc* pBufferUpdate)
{
    const uint32_t nodeIndex = pBufferUpdate->pBuffer->mNodeIndex;
    if (pBufferUpdate->mInternal.mMappedRange.mFlags & MAPPED_RANGE_FLAG_UNMAP_BUFFER)
    {
        unmapBuffer(pResourceLoader->ppRenderers[nodeIndex], pBufferUpdate->pBuffer);
    }

    ResourceMemoryUsage memoryUsage = (ResourceMemoryUsage)pBufferUpdate->pBuffer->mMemoryUsage;
    if (!gUma && memoryUsage == RESOURCE_MEMORY_USAGE_GPU_ONLY)
    {
        MutexLock   lock(pResourceLoader->mUploadEngineMutex);
        CopyEngine* pCopyEngine = &pResourceLoader->pUploadEngines[nodeIndex];
        updateBuffer(pResourceLoader->ppRenderers[nodeIndex], pCopyEngine, *pBufferUpdate);
    }

    // Restore the state to before the beginUpdateResource call.
    pBufferUpdate->pMappedData = NULL;
    pBufferUpdate->mInternal = {};
}

TextureSubresourceUpdate TextureUpdateDesc::getSubresourceUpdateDesc(uint32_t mip, uint32_t layer)
{
    TextureSubresourceUpdate ret = {};
    Texture*                 texture = pTexture;
    const TinyImageFormat    fmt = (TinyImageFormat)texture->mFormat;
    Renderer*                pRenderer = pResourceLoader->ppRenderers[texture->mNodeIndex];
    const uint32_t           sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);

    bool success = util_get_surface_info(MIP_REDUCE(texture->mWidth, mip), MIP_REDUCE(texture->mHeight, mip), fmt, &ret.mSrcSliceStride,
                                         &ret.mSrcRowStride, &ret.mRowCount);
    ASSERT(success);
    UNREF_PARAM(success);

    ret.mDstRowStride = round_up(ret.mSrcRowStride, util_get_texture_row_alignment(pRenderer));
    ret.mDstSliceStride = round_up(ret.mDstRowStride * ret.mRowCount, sliceAlignment);
    ret.pMappedData = mInternal.mMappedRange.pData + (mInternal.mDstSliceStride * (layer - mBaseArrayLayer));
    // Calculate the offset for the mip in this array layer
    for (uint32_t i = mBaseMipLevel; i < mip; ++i)
    {
        uint32_t srcSliceStride = 0;
        uint32_t srcRowStride = 0;
        uint32_t rowCount = 0;
        success = util_get_surface_info(MIP_REDUCE(texture->mWidth, i), MIP_REDUCE(texture->mHeight, i), fmt, &srcSliceStride,
                                        &srcRowStride, &rowCount);
        ASSERT(success);
        uint32_t d = MIP_REDUCE(texture->mDepth, i);

        uint32_t dstRowStride = round_up(srcRowStride, util_get_texture_row_alignment(pRenderer));
        uint32_t dstSliceStride = round_up(dstRowStride * rowCount, sliceAlignment);
        ret.pMappedData += (dstSliceStride * d);
    }

    return ret;
}

void beginUpdateResource(TextureUpdateDesc* pTextureUpdate)
{
    const Texture*        texture = pTextureUpdate->pTexture;
    const TinyImageFormat fmt = (TinyImageFormat)texture->mFormat;
    Renderer*             pRenderer = pResourceLoader->ppRenderers[texture->mNodeIndex];
    const uint32_t        sliceAlignment = util_get_texture_subresource_alignment(pRenderer, fmt);
    pTextureUpdate->mMipLevels = max(1u, pTextureUpdate->mMipLevels);
    pTextureUpdate->mLayerCount = max(1u, pTextureUpdate->mLayerCount);

    const uint32_t rowAlignment = util_get_texture_row_alignment(pRenderer);
    const uint64_t requiredSize = util_get_surface_size(fmt, texture->mWidth, texture->mHeight, texture->mDepth, rowAlignment,
                                                        sliceAlignment, pTextureUpdate->mBaseMipLevel, pTextureUpdate->mMipLevels,
                                                        pTextureUpdate->mBaseArrayLayer, pTextureUpdate->mLayerCount);

    // We need to use a staging buffer.
    MutexLock         lock(pResourceLoader->mUploadEngineMutex);
    const uint32_t    nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
    CopyEngine*       pCopyEngine = &pResourceLoader->pUploadEngines[nodeIndex];
    MappedMemoryRange range = allocateStagingMemory(pCopyEngine, requiredSize, sliceAlignment, texture->mNodeIndex);
    if (!range.pData)
    {
        range = allocateUploadMemory(pRenderer, requiredSize, sliceAlignment);
        arrpush(pCopyEngine->resourceSets[pCopyEngine->activeSet].mTempBuffers, range.pBuffer);
    }

    pTextureUpdate->mInternal = {};
    pTextureUpdate->mInternal.mMappedRange = range;

    // Pre-calculate stride for the mip chain. Will be used in getSubresourceUpdateDesc
    for (uint32_t mip = pTextureUpdate->mBaseMipLevel; mip < pTextureUpdate->mMipLevels; ++mip)
    {
        uint32_t srcSliceStride = 0;
        uint32_t srcRowStride = 0;
        uint32_t rowCount = 0;
        bool     success = util_get_surface_info(MIP_REDUCE(texture->mWidth, mip), MIP_REDUCE(texture->mHeight, mip), fmt, &srcSliceStride,
                                             &srcRowStride, &rowCount);
        ASSERT(success);
        uint32_t d = MIP_REDUCE(texture->mDepth, mip);

        uint32_t dstRowStride = round_up(srcRowStride, util_get_texture_row_alignment(pRenderer));
        uint32_t dstSliceStride = round_up(dstRowStride * rowCount, sliceAlignment);
        pTextureUpdate->mInternal.mDstSliceStride += (dstSliceStride * d);
    }
}

void endUpdateResource(TextureUpdateDesc* pTextureUpdate)
{
    TextureUpdateDescInternal desc = {};
    desc.pTexture = pTextureUpdate->pTexture;
    desc.mRange = pTextureUpdate->mInternal.mMappedRange;
    desc.pCmd = pTextureUpdate->pCmd;
    desc.mBaseMipLevel = pTextureUpdate->mBaseMipLevel;
    desc.mMipLevels = pTextureUpdate->mMipLevels;
    desc.mBaseArrayLayer = pTextureUpdate->mBaseArrayLayer;
    desc.mLayerCount = pTextureUpdate->mLayerCount;
    desc.mCurrentState = pTextureUpdate->mCurrentState;
    MutexLock      lock(pResourceLoader->mUploadEngineMutex);
    const uint32_t nodeIndex = pTextureUpdate->pTexture->mNodeIndex;
    CopyEngine*    pCopyEngine = &pResourceLoader->pUploadEngines[nodeIndex];
    updateTexture(pResourceLoader->ppRenderers[nodeIndex], pCopyEngine, desc);

    // Restore the state to before the beginUpdateResource call.
    pTextureUpdate->mInternal = {};
}

void copyResource(TextureCopyDesc* pTextureDesc, SyncToken* token) { queueTextureCopy(pResourceLoader, pTextureDesc, token); }

void flushResourceUpdates(FlushResourceUpdateDesc* pDesc)
{
    MutexLock lock(pResourceLoader->mUploadEngineMutex);

    static FlushResourceUpdateDesc dummyDesc = {};
    FlushResourceUpdateDesc&       desc = pDesc ? *pDesc : dummyDesc;
    const uint32_t                 nodeIndex = desc.mNodeIndex;
    CopyEngine*                    pCopyEngine = &pResourceLoader->pUploadEngines[nodeIndex];
    const uint32_t                 activeSet = pCopyEngine->activeSet;

    desc.pOutFence = pCopyEngine->resourceSets[activeSet].pFence;
    desc.pOutSubmittedSemaphore = pCopyEngine->resourceSets[activeSet].pSemaphore;

    if (!pCopyEngine->isRecording)
    {
        return;
    }
    for (uint32_t i = 0; i < desc.mWaitSemaphoreCount; ++i)
    {
        arrpush(pCopyEngine->mWaitSemaphores, desc.ppWaitSemaphores[i]);
    }
    streamerFlush(pCopyEngine);
    pCopyEngine->activeSet = (activeSet + 1) % pCopyEngine->bufferCount;
}

SyncToken getLastTokenCompleted() { return tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted); }

bool isTokenCompleted(const SyncToken* token) { return *token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted); }

void waitForToken(const SyncToken* token) { waitForToken(pResourceLoader, token); }

SyncToken getLastTokenSubmitted() { return tfrg_atomic64_load_acquire(&pResourceLoader->mTokenSubmitted); }

bool isTokenSubmitted(const SyncToken* token) { return *token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenSubmitted); }

void waitForTokenSubmitted(const SyncToken* token) { waitForTokenSubmitted(pResourceLoader, token); }

bool allResourceLoadsCompleted()
{
    SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
    return token <= tfrg_atomic64_load_acquire(&pResourceLoader->mTokenCompleted);
}

void waitForAllResourceLoads()
{
    SyncToken token = tfrg_atomic64_load_relaxed(&pResourceLoader->mTokenCounter);
    waitForToken(pResourceLoader, &token);
}

bool isResourceLoaderSingleThreaded()
{
    ASSERT(pResourceLoader);
    return pResourceLoader->mDesc.mSingleThreaded;
}

Semaphore* getLastSemaphoreSubmitted(uint32_t nodeIndex)
{
    acquireMutex(&pResourceLoader->mSemaphoreMutex);
    Semaphore* sem = pResourceLoader->pCopyEngines[nodeIndex].pLastSubmittedSemaphore;
    releaseMutex(&pResourceLoader->mSemaphoreMutex);
    return sem;
}

/************************************************************************/
// Shader loading
/************************************************************************/
static bool load_shader_stage_byte_code(Renderer* pRenderer, const char* name, ShaderStage stage, BinaryShaderStageDesc* pOut,
                                        ShaderByteCodeBuffer* pShaderByteCodeBuffer, FSLMetadata* pOutMetadata)
{
    UNREF_PARAM(pRenderer);
    UNREF_PARAM(stage);
    char binaryShaderPath[FS_MAX_PATH];

    {
        const char* rendererApi = getShaderPlatformName();

        const char* postfix = "";
#if defined(METAL)
        postfix = ".metal";
#endif

        int length = 0;
        if (rendererApi[0])
        {
            length = snprintf(binaryShaderPath, sizeof binaryShaderPath, "%s/%s%s", rendererApi, name, postfix);
        }
        else
        {
            length = snprintf(binaryShaderPath, sizeof binaryShaderPath, "%s%s", name, postfix);
        }

        if (length >= FS_MAX_PATH)
        {
            LOGF(eERROR, "Shader name is too long: '%s'", name);
            return false;
        }
    }

    FileStream binaryFileStream = {};

    // NOTE: On some platforms, we might not be allowed to write in the `RD_SHADER_BINARIES` directory.
    // If we want to load re-compiled binaries, then they must be cached elsewhere and queried here.

    void*      pCachedByteCode = NULL;
    uint32_t   cachedByteCodeSize = 0;
    const bool result = /*platformReloadClientGetShaderBinary(binaryShaderPath, &pCachedByteCode, &cachedByteCodeSize)
                            ? fsOpenStreamFromMemory(pCachedByteCode, cachedByteCodeSize, FM_READ, false, &binaryFileStream)
                            : */fsOpenStreamFromPath(RD_SHADER_BINARIES, binaryShaderPath, FM_READ, &binaryFileStream);

    ASSERT(result);
    if (!result)
        return result;

    ssize_t size = fsGetStreamFileSize(&binaryFileStream);
    ASSERT(size > 0);

    FSLHeader header = {};
    if (sizeof(FSLHeader) != fsReadFromStream(&binaryFileStream, (void*)&header, sizeof(FSLHeader)))
        ASSERT(false);

    if (strncmp("@FSL", header.mMagic, 4) != 0)
    {
        // Shader was not compiled using FSL script
        fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, 0);

#if defined(PROSPERO)
        extern void prospero_loadByteCode(Renderer*, FileStream*, ssize_t, BinaryShaderStageDesc*);
        prospero_loadByteCode(pRenderer, &binaryFileStream, size, pOut);
#else
        pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 256, (uint32_t)size, binaryShaderPath);
        pOut->mByteCodeSize = (uint32_t)size;
        fsReadFromStream(&binaryFileStream, (void*)pOut->pByteCode, size);
#endif
    }
    else
    {
        ASSERT(strncmp("@FSL", header.mMagic, 4) == 0);
        const size_t   derivativesSize = sizeof(FSLDerivative) * header.mDerivativeCount;
        FSLDerivative* pDerivatives = (FSLDerivative*)alloca(derivativesSize);
        if (derivativesSize != fsReadFromStream(&binaryFileStream, (void*)pDerivatives, derivativesSize))
            ASSERT(false);

        if (pOutMetadata)
            *pOutMetadata = header.mMetadata;

#if defined(PROSPERO)
        ASSERT(header.mDerivativeCount == 1);
        fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, pDerivatives[0].mOffset);

        extern void prospero_loadByteCode(Renderer*, FileStream*, ssize_t, BinaryShaderStageDesc*);
        prospero_loadByteCode(pRenderer, &binaryFileStream, pDerivatives[0].mSize, pOut);
#else
        uint64_t derivativeHash = 0;

#if defined(VULKAN)
        if (gPlatformParameters.mSelectedRendererApi == RENDERER_API_VULKAN)
        {
            // Needs to match with the way we set the derivatives in FSL scripts (vulkan.py, compilers.py)
            derivativeHash = (uint64_t)pRenderer->pGpu->mVk.mShaderSampledImageArrayDynamicIndexingSupported |
                             (uint64_t)pRenderer->pGpu->mVk.mDescriptorIndexingExtension << 1;
        }
#endif

        for (uint32_t i = 0; i < header.mDerivativeCount; ++i)
        {
            // If we only have one shader it means it's compatible with any GPU, otherwise we need to check the hash
            if (header.mDerivativeCount == 1 || derivativeHash == pDerivatives[i].mHash)
            {
                if (!fsSeekStream(&binaryFileStream, SBO_START_OF_FILE, pDerivatives[i].mOffset))
                {
                    LOGF(eERROR, "Failed to read file '%s'", binaryShaderPath);
                    break;
                }

                size = pDerivatives[i].mSize;

                pOut->pByteCode = allocShaderByteCode(pShaderByteCodeBuffer, 256, (uint32_t)size, binaryShaderPath);
                pOut->mByteCodeSize = (uint32_t)pDerivatives[i].mSize;
                if (fsReadFromStream(&binaryFileStream, (void*)pOut->pByteCode, size) != (size_t)size)
                {
                    LOGF(eERROR, "Failed to read file '%s'", binaryShaderPath);
                }
                break;
            }
        }

        ASSERT(pOut->pByteCode);
#endif
    }

    fsCloseStream(&binaryFileStream);
    return true;
}

const char* getShaderPlatformName()
{
    switch (gPlatformParameters.mSelectedRendererApi)
    {
#if defined(DIRECT3D12)
#if defined(SCARLETT)
    case RENDERER_API_D3D12:
        return "SCARLETT";
        break;
#elif defined(XBOX)
    case RENDERER_API_D3D12:
        return "XBOX";
        break;
#else
    case RENDERER_API_D3D12:
        return "DIRECT3D12";
        break;
#endif
#endif
#if defined(VULKAN)
#if defined(__ANDROID__)
    case RENDERER_API_VULKAN:
        return "ANDROID_VULKAN";
        break;
#elif defined(NX64)
    case RENDERER_API_VULKAN:
        return "SWITCH";
        break;
#else
    case RENDERER_API_VULKAN:
        return "VULKAN";
        break;
#endif
#endif
#if defined(METAL)
#if defined(TARGET_IOS)
    case RENDERER_API_METAL:
        return "IOS";
        break;
#else
    case RENDERER_API_METAL:
        return "MACOS";
        break;
#endif
#endif
#if defined(ORBIS)
    case RENDERER_API_ORBIS:
        return "ORBIS";
        break;
#endif
#if defined(PROSPERO)
    case RENDERER_API_PROSPERO:
        return "PROSPERO";
        break;
#endif
    default:
        break;
    }

    ASSERT(false && "Renderer API name not defined");
    return "";
}

static bool find_shader_stage(const char* extension, BinaryShaderDesc* pBinaryDesc, BinaryShaderStageDesc** pOutStage, ShaderStage* pStage)
{
    if (stricmp(extension, "vert") == 0)
    {
        *pOutStage = &pBinaryDesc->mVert;
        *pStage = SHADER_STAGE_VERT;
    }
    else if (stricmp(extension, "frag") == 0)
    {
        *pOutStage = &pBinaryDesc->mFrag;
        *pStage = SHADER_STAGE_FRAG;
    }
#ifndef METAL
    else if (stricmp(extension, "tesc") == 0)
    {
        *pOutStage = &pBinaryDesc->mHull;
        *pStage = SHADER_STAGE_HULL;
    }
    else if (stricmp(extension, "tese") == 0)
    {
        *pOutStage = &pBinaryDesc->mDomain;
        *pStage = SHADER_STAGE_DOMN;
    }
    else if (stricmp(extension, "geom") == 0)
    {
        *pOutStage = &pBinaryDesc->mGeom;
        *pStage = SHADER_STAGE_GEOM;
    }
#endif
    else if (stricmp(extension, "comp") == 0)
    {
        *pOutStage = &pBinaryDesc->mComp;
        *pStage = SHADER_STAGE_COMP;
    }
    else
    {
        return false;
    }

    return true;
}

void addShader(Renderer* pRenderer, const ShaderLoadDesc* pDesc, Shader** ppShader)
{
    BinaryShaderDesc binaryDesc = {};

    ShaderByteCodeBuffer shaderByteCodeBuffer = {};
#if !defined(PROSPERO)
    char bytecodeStack[ShaderByteCodeBuffer::kStackSize] = {};
    shaderByteCodeBuffer.pStackMemory = bytecodeStack;
#endif

#if defined(METAL)
    bool bIsICBCompatible = true;
#endif

    ShaderStage stages = SHADER_STAGE_NONE;
    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        if (pDesc->mStages[i].pFileName && pDesc->mStages[i].pFileName[0] != 0)
        {
            ShaderStage            stage;
            BinaryShaderStageDesc* pStage = NULL;
            char                   ext[FS_MAX_PATH] = { 0 };
            fsGetPathExtension(pDesc->mStages[i].pFileName, ext);
            if (find_shader_stage(ext, &binaryDesc, &pStage, &stage))
                stages |= stage;
        }
    }
    for (uint32_t i = 0; i < SHADER_STAGE_COUNT; ++i)
    {
        const char* fileName = pDesc->mStages[i].pFileName;
        if (!fileName || !*fileName)
            continue;

        ShaderStage            stage;
        BinaryShaderStageDesc* pStage = NULL;
        {
            char ext[FS_MAX_PATH];
            fsGetPathExtension(fileName, ext);
            if (!find_shader_stage(ext, &binaryDesc, &pStage, &stage))
                continue;
        }

        FSLMetadata metadata = {};
        if (!load_shader_stage_byte_code(pRenderer, fileName, stage, pStage, &shaderByteCodeBuffer, &metadata))
        {
            freeShaderByteCode(&shaderByteCodeBuffer, &binaryDesc);
            return;
        }

        binaryDesc.mStages |= stage;
        pStage->pName = fileName;

#if defined(METAL)
        bIsICBCompatible &= metadata.mICBCompatible;
#endif

#if defined(METAL)
        if (pDesc->mStages[i].pEntryPointName)
            pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;

        if (SHADER_STAGE_COMP == stage)
        {
            pStage->mNumThreadsPerGroup[0] = metadata.mNumThreadsPerGroup[0];
            pStage->mNumThreadsPerGroup[1] = metadata.mNumThreadsPerGroup[1];
            pStage->mNumThreadsPerGroup[2] = metadata.mNumThreadsPerGroup[2];
        }
        else if (SHADER_STAGE_FRAG == stage)
        {
            pStage->mOutputRenderTargetTypesMask = metadata.mOutputRenderTargetTypesMask;
        }

#elif !defined(ORBIS) && !defined(PROSPERO)
        if (pDesc->mStages[i].pEntryPointName)
            pStage->pEntryPoint = pDesc->mStages[i].pEntryPointName;
        else
            pStage->pEntryPoint = "main";
#endif
    }

#if defined(PROSPERO)
    binaryDesc.mOwnByteCode = true;
#endif

    binaryDesc.mConstantCount = pDesc->mConstantCount;
    binaryDesc.pConstants = pDesc->pConstants;

    addShaderBinary(pRenderer, &binaryDesc, ppShader);
    freeShaderByteCode(&shaderByteCodeBuffer, &binaryDesc);

    Shader* pShader = *ppShader;

#if defined(METAL)
    pShader->mICB = bIsICBCompatible;
#else
    if (SHADER_STAGE_COMP == binaryDesc.mStages)
    {
        pShader->mNumThreadsPerGroup[0] = pShader->pReflection->mStageReflections[0].mNumThreadsPerGroup[0];
        pShader->mNumThreadsPerGroup[1] = pShader->pReflection->mStageReflections[0].mNumThreadsPerGroup[1];
        pShader->mNumThreadsPerGroup[2] = pShader->pReflection->mStageReflections[0].mNumThreadsPerGroup[2];
    }
#endif

#if defined(METAL)
    if (ppShader)
    {
        (*ppShader)->mICB = bIsICBCompatible;
    }
#endif
}

/************************************************************************/
// Pipeline cache save, load
/************************************************************************/
void loadPipelineCache(Renderer* pRenderer, const PipelineCacheLoadDesc* pDesc, PipelineCache** ppPipelineCache)
{
#if defined(DIRECT3D12) || defined(VULKAN)

    char rendererApi[FS_MAX_PATH] = {};
#if defined(USE_MULTIPLE_RENDER_APIS)
    switch (gPlatformParameters.mSelectedRendererApi)
    {
#if defined(DIRECT3D12)
    case RENDERER_API_D3D12:
        strcat(rendererApi, "DIRECT3D12/");
        break;
#endif
#if defined(VULKAN)
    case RENDERER_API_VULKAN:
        strcat(rendererApi, "VULKAN/");
        break;
#endif
    default:
        break;
    }
#endif

    ASSERT(strlen(rendererApi) + strlen(pDesc->pFileName) < sizeof(rendererApi));
    strcat(rendererApi, pDesc->pFileName);

    FileStream stream = {};
    bool       success = fsOpenStreamFromPath(RD_PIPELINE_CACHE, rendererApi, FM_READ, &stream);
    ssize_t    dataSize = 0;
    void*      data = NULL;
    if (success)
    {
        dataSize = fsGetStreamFileSize(&stream);
        data = NULL;
        if (dataSize)
        {
            data = tf_malloc(dataSize);
            fsReadFromStream(&stream, data, dataSize);
        }

        fsCloseStream(&stream);
    }

    PipelineCacheDesc desc = {};
    desc.mFlags = pDesc->mFlags;
    desc.pData = data;
    desc.mSize = dataSize;
    addPipelineCache(pRenderer, &desc, ppPipelineCache);

    if (data)
    {
        tf_free(data);
    }
#endif
}

void savePipelineCache(Renderer* pRenderer, PipelineCache* pPipelineCache, PipelineCacheSaveDesc* pDesc)
{
#if defined(DIRECT3D12) || defined(VULKAN)

    char rendererApi[FS_MAX_PATH] = {};
#if defined(USE_MULTIPLE_RENDER_APIS)
    switch (gPlatformParameters.mSelectedRendererApi)
    {
#if defined(DIRECT3D12)
    case RENDERER_API_D3D12:
        strcat(rendererApi, "DIRECT3D12/");
        break;
#endif
#if defined(VULKAN)
    case RENDERER_API_VULKAN:
        strcat(rendererApi, "VULKAN/");
        break;
#endif
    default:
        break;
    }
#endif

    ASSERT(strlen(rendererApi) + strlen(pDesc->pFileName) < sizeof(rendererApi));
    strcat(rendererApi, pDesc->pFileName);

    FileStream stream = {};
    if (fsOpenStreamFromPath(RD_PIPELINE_CACHE, rendererApi, FM_WRITE, &stream))
    {
        size_t dataSize = 0;
        getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, NULL);
        if (dataSize)
        {
            void* data = tf_malloc(dataSize);
            getPipelineCacheData(pRenderer, pPipelineCache, &dataSize, data);
            fsWriteToStream(&stream, data, dataSize);
            tf_free(data);
        }

        fsCloseStream(&stream);
    }
#endif
}
/************************************************************************/
/************************************************************************/

void waitCopyQueueIdle()
{
    for (uint32_t nodeIndex = 0; nodeIndex < pResourceLoader->mGpuCount; ++nodeIndex)
    {
        waitQueueIdle(pResourceLoader->pCopyEngines[nodeIndex].pQueue);
    }
}
