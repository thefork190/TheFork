#include <IResourceLoader.h>
#include "Low/RHI.h"
#include "HelloTriangle.h"
#include "ILog.h"

namespace HelloTriangle
{
    // Storing everything here, but in practice this wouldn't be the case.
    struct RenderPassData
    {
        Shader* pTriShader = nullptr;
    };
    
    static void AddShaders(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        ShaderLoadDesc basicShader = {};
        basicShader.mStages[0].pFileName = "HelloTriangle.vert";
        basicShader.mStages[1].pFileName = "HelloTriangle.frag";
        addShader(pRenderer, &basicShader, &passDataInOut.pTriShader);
    }

    static void RemoveShaders(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeShader(pRenderer, passDataInOut.pTriShader);
    }

    // 

    module::module(flecs::world& ecs)
    {
        ecs.import<RHI::module>();
        ecs.module<module>();

        ecs.component<RenderPassData>();
        
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;
        ASSERTMSG(pRHI, "RHI singleton doesn't exist.");

        RenderPassData renderPassData = {};
        AddShaders(pRHI->pRenderer, renderPassData);
        ecs.set<RenderPassData>(renderPassData);
    }

    void module::OnExit(flecs::world& ecs)
    {
        if (ecs.has<RHI::RHI>() && ecs.has<RenderPassData>())
        {
            Renderer* pRenderer = ecs.get<RHI::RHI>()->pRenderer;
            RenderPassData* pRenderPassData = ecs.get_mut<RenderPassData>();
            RemoveShaders(pRenderer, *pRenderPassData);
        }
    }
}