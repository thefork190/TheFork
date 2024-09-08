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

        initResourceLoaderInterface(pRenderer);

        QueueDesc queueDesc = {};
        queueDesc.mType = QUEUE_TYPE_GRAPHICS;
        addQueue(pRenderer, &queueDesc, &pGfxQueue);

        GpuCmdRingDesc cmdRingDesc = {};
        cmdRingDesc.pQueue = pGfxQueue;
        cmdRingDesc.mPoolCount = dataBufferCount;
        cmdRingDesc.mCmdPerPoolCount = 1;
        cmdRingDesc.mAddSyncPrimitives = true;
        addGpuCmdRing(pRenderer, &cmdRingDesc, &gfxCmdRing);
    }

    RHI::~RHI()
    {
        ASSERT(pRenderer);
        
        removeGpuCmdRing(pRenderer, &gfxCmdRing);
        
        removeQueue(pRenderer, pGfxQueue);
        pGfxQueue = nullptr;

        exitResourceLoaderInterface(pRenderer);

        exitRenderer(pRenderer);
        pRenderer = nullptr;
    }



    module::module(flecs::world& ecs)
    {
        ecs.import<Engine::module>();
        ecs.import<Window::module>();

        ecs.module<module>();

        auto beginFrame = ecs.system("Begin Frame")
            .kind(flecs::PostLoad)
            .run([](flecs::iter& it)
                {
                    auto pRHI = it.world().get_mut<RHI>();

                    // Stall if CPU is running "gDataBufferCount" frames ahead of GPU
                    pRHI->curCmdRingElem = getNextGpuCmdRingElement(&pRHI->gfxCmdRing, true, 1);
                    FenceStatus fenceStatus;
                    getFenceStatus(pRHI->pRenderer, pRHI->curCmdRingElem.pFence, &fenceStatus);
                    if (fenceStatus == FENCE_STATUS_INCOMPLETE)
                        waitForFences(pRHI->pRenderer, 1, &pRHI->curCmdRingElem.pFence);

                    // Reset cmd pool for this frame
                    resetCmdPool(pRHI->pRenderer, pRHI->curCmdRingElem.pCmdPool);

                    // Begin the cmd
                    Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                    beginCmd(pCmd);
                }
            );
    }

    bool CreateRHI(flecs::world& ecs)
    {
        // Ensure the singleton doesn't exist yet
        if (ecs.get<RHI>())
            return true;
        
        // Create the RHI
        ecs.add<RHI>();

        return ecs.get<RHI>()->pRenderer != nullptr;
    }
}