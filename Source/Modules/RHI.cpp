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
    }

    RHI::~RHI()
    {
        ASSERT(pRenderer);
        
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

        auto acquireNextImg = ecs.system<>("Acquire Next Img")
            .kind(flecs::OnStore)
            .each([](flecs::iter& it, size_t i)
                {
                   /* auto pRHI = it.world().get_mut<RHI::RHI>();
                    ASSERT(pRHI);*/
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