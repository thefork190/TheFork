#include <ILog.h>

#include "Engine.h"
#include "Window.h"
#include "RHI.h"

namespace RHI
{
    RHI::RHI()
    {
        RendererDesc rendDesc;
        memset(&rendDesc, 0, sizeof(rendDesc));
        initRenderer(APP_NAME, &rendDesc, &pRenderer);

        ASSERT(pRenderer);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        addQueue(pRenderer, &queueDesc, &pGfxQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGfxQueue;
        cmdRingDesc.mPoolCount = dataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gfxCmdRing);

        addSemaphore(pRenderer, &pImgAcqSemaphore);
    }

    RHI::~RHI()
    {
        ASSERT(pRenderer);
        
        removeSemaphore(pRenderer, pImgAcqSemaphore);
        pImgAcqSemaphore = nullptr;

        removeGpuCmdRing(pRenderer, &gfxCmdRing);
        
        removeQueue(pRenderer, pGfxQueue);
        pGfxQueue = nullptr;

        exitRenderer(pRenderer);
        pRenderer = nullptr;
    }

    module::module(flecs::world& ecs) 
    {
        ecs.import<Engine::module>();
        ecs.import<Window::module>();

        ecs.module<module>();
    }
}