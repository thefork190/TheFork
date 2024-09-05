#include <vector>
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <IResourceLoader.h>
#include "ILog.h"
#include "Low/Engine.h"
#include "Low/RHI.h"
#include "Low/Window.h"
#include "HelloTriangle.h"

namespace HelloTriangle
{
    // Storing everything here, but in practice this wouldn't be the case.
    struct RenderPassData
    {
        Shader* pTriShader = nullptr;
        RootSignature* pRootSignature = nullptr;
        DescriptorSet* pDescriptorSetUniforms = nullptr;
        Pipeline* pPipeline = nullptr;
        VertexLayout vertexLayout = {};
        Buffer* pVertexBuffer = nullptr;
        Buffer* pIndexBuffer = nullptr;
        std::vector<Buffer*> uniformsBuffers;

        // Uniforms data
        struct UniformsData
        {
            glm::mat4 mvp = {};
            glm::vec4 color = {};
        } uniformsData;

        void Reset()
        {
            pTriShader = nullptr;
            pRootSignature = nullptr;
            pDescriptorSetUniforms = nullptr;
            pPipeline = nullptr;
            vertexLayout = {};
            pVertexBuffer = nullptr;
            pIndexBuffer = nullptr;
            uniformsBuffers.clear();
            uniformsData = {};
        }
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

    static void AddRootSignature(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        Shader* shaders[1];
        uint32_t shadersCount = 0;
        shaders[shadersCount++] = passDataInOut.pTriShader;

        RootSignatureDesc rootDesc = {};
        rootDesc.mShaderCount = shadersCount;
        rootDesc.ppShaders = shaders;
        addRootSignature(pRenderer, &rootDesc, &passDataInOut.pRootSignature);
    }

    static void RemoveRootSignature(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeRootSignature(pRenderer, passDataInOut.pRootSignature);
    }

    static void AddDescriptorSet(RHI::RHI const* pRHI, RenderPassData& passDataInOut)
    {
        DescriptorSetDesc desc = { passDataInOut.pRootSignature, DESCRIPTOR_UPDATE_FREQ_PER_FRAME, pRHI->dataBufferCount };
        addDescriptorSet(pRHI->pRenderer, &desc, &passDataInOut.pDescriptorSetUniforms);
    }

    static void RemoveDescriptorSet(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeDescriptorSet(pRenderer, passDataInOut.pDescriptorSetUniforms);
    }

    static void AddPipeline(
        RHI::RHI const* pRHI,
        Window::SDLWindow const* pWindow,
        RenderPassData& passDataInOut)
    {
        RasterizerStateDesc rasterizerStateDesc = {};
        rasterizerStateDesc.mCullMode = CULL_MODE_NONE;

        RasterizerStateDesc rasterizeStateDesc = {};
        rasterizeStateDesc.mCullMode = CULL_MODE_FRONT;

        DepthStateDesc depthStateDesc = {};

        PipelineDesc desc = {};
        desc.mType = PIPELINE_TYPE_GRAPHICS;
        GraphicsPipelineDesc& pipelineSettings = desc.mGraphicsDesc;
        pipelineSettings.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
        pipelineSettings.mRenderTargetCount = 1;
        pipelineSettings.pDepthState = &depthStateDesc;
        pipelineSettings.pColorFormats = &pWindow->pSwapChain->ppRenderTargets[0]->mFormat;
        pipelineSettings.mSampleCount = pWindow->pSwapChain->ppRenderTargets[0]->mSampleCount;
        pipelineSettings.mSampleQuality = pWindow->pSwapChain->ppRenderTargets[0]->mSampleQuality;
        pipelineSettings.pRootSignature = passDataInOut.pRootSignature;
        pipelineSettings.pShaderProgram = passDataInOut.pTriShader;
        pipelineSettings.pVertexLayout = &passDataInOut.vertexLayout;
        pipelineSettings.pRasterizerState = &rasterizeStateDesc;
        pipelineSettings.mVRFoveatedRendering = true;
        addPipeline(pRHI->pRenderer, &desc, &passDataInOut.pPipeline);
    }

    static void RemovePipeline(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removePipeline(pRenderer, passDataInOut.pPipeline);
    }

    module::module(flecs::world& ecs)
    {
        ecs.import<RHI::module>();
        ecs.import<Window::module>();
        ecs.import<Engine::module>();

        ecs.module<module>();

        ecs.component<RenderPassData>();
        
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;
        ASSERTMSG(pRHI, "RHI singleton doesn't exist.");

        RenderPassData renderPassData = {};
        
        AddShaders(pRHI->pRenderer, renderPassData);
        AddRootSignature(pRHI->pRenderer, renderPassData);
        AddDescriptorSet(pRHI, renderPassData);

        renderPassData.uniformsBuffers.resize(pRHI->dataBufferCount);
        BufferLoadDesc ubDesc = {};
        ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        ubDesc.pData = nullptr;
        for (uint32_t i = 0; i < pRHI->dataBufferCount; ++i)
        {
            ubDesc.mDesc.pName = "HelloTriangle_UniformBuffer";
            ubDesc.mDesc.mSize = sizeof(RenderPassData::UniformsData);
            ubDesc.ppBuffer = &renderPassData.uniformsBuffers[i];
            addResource(&ubDesc, nullptr);
                        
            DescriptorData uParams[1] = {};
            uParams[0].pName = "UniformBlock";
            uParams[0].ppBuffers = &renderPassData.uniformsBuffers[i];
            updateDescriptorSet(pRHI->pRenderer, i, renderPassData.pDescriptorSetUniforms, 1, uParams);
        }

        renderPassData.vertexLayout.mBindingCount = 1;
        renderPassData.vertexLayout.mBindings[0].mStride = 12; // xyz pos
        renderPassData.vertexLayout.mAttribCount = 1;
        renderPassData.vertexLayout.mAttribs[0].mSemantic = SEMANTIC_POSITION;
        renderPassData.vertexLayout.mAttribs[0].mFormat = TinyImageFormat_R32G32B32_SFLOAT;
        renderPassData.vertexLayout.mAttribs[0].mBinding = 0;
        renderPassData.vertexLayout.mAttribs[0].mLocation = 0;
        renderPassData.vertexLayout.mAttribs[0].mOffset = 0;

        std::vector<glm::vec3> triPositions(3);
        triPositions[0] = { -0.5f, -0.5f , 0.f };
        triPositions[1] = { 0.5f, -0.5f , 0.f };
        triPositions[2] = { 0.5f, 0.5f , 0.f };

        BufferLoadDesc vbDesc = {};
        vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.mDesc.mSize = 3 * 12;
        vbDesc.pData = triPositions.data();
        vbDesc.ppBuffer = &renderPassData.pVertexBuffer;
        addResource(&vbDesc, nullptr);

        std::vector<uint32_t> triIndices(3);
        triIndices[0] = 0;
        triIndices[1] = 1;
        triIndices[2] = 2;

        BufferLoadDesc ibDesc = {};
        ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        ibDesc.mDesc.mSize = sizeof(uint32_t) * 3;
        ibDesc.pData = triIndices.data();
        ibDesc.ppBuffer = &renderPassData.pIndexBuffer;
        addResource(&ibDesc, nullptr);

        flecs::query<Window::SDLWindow> windowQuery = ecs.query_builder<Window::SDLWindow>().build();
        windowQuery.each([pRHI, &renderPassData](flecs::iter& it, size_t i, Window::SDLWindow& window)
            {
                ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");
                AddPipeline(pRHI, &window, renderPassData);
            });


        waitForAllResourceLoads();

        ecs.set<RenderPassData>(renderPassData);

        // System to update gpu constants
        ecs.system("HelloTriangle::Prepare")
            .kind(flecs::OnUpdate)
            .run([](flecs::iter& it)
                {
                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData const* pRPD = it.world().has<RenderPassData>() ? it.world().get<RenderPassData>() : nullptr;

                    if (pRHI && pRPD)
                    {
                        RenderPassData::UniformsData updatedData = {};
                        updatedData.mvp = glm::orthoLH(-1.f, 1.f, -1.f, 1.f, 0.1f, 1.f);
                        updatedData.color = glm::vec4(1.f, 1.f, 1.f, 1.f);

                        // Update uniform buffers
                        BufferUpdateDesc updateDesc = { pRPD->uniformsBuffers[pRHI->frameIndex] };
                        beginUpdateResource(&updateDesc);
                        memcpy(updateDesc.pMappedData, &updatedData, sizeof(RenderPassData::UniformsData));
                        endUpdateResource(&updateDesc);
                    }
                }
            );

        ecs.system<Engine::Canvas, Window::SDLWindow>("HelloTriangle::Draw")
            .kind(flecs::PreStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;

                    if (pRHI && pRPD)
                    {
                        Cmd* pCmd = sdlWin.curCmdRingElem.pCmds[0];
                        ASSERT(pCmd);

                        BindRenderTargetsDesc bindRenderTargets = {};
                        bindRenderTargets.mRenderTargetCount = 1;
                        bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_LOAD };
                        cmdBindRenderTargets(pCmd, &bindRenderTargets);
                        cmdSetViewport(pCmd, 0.0f, 0.0f, (float)canvas.width, (float)canvas.height, 0.0f, 1.0f);
                        cmdSetScissor(pCmd, 0, 0, canvas.width, canvas.height);

                        cmdBindPipeline(pCmd, pRPD->pPipeline);
                        cmdBindDescriptorSet(pCmd, pRHI->frameIndex, pRPD->pDescriptorSetUniforms);
                        cmdBindVertexBuffer(pCmd, 1, &pRPD->pVertexBuffer, &pRPD->vertexLayout.mBindings[0].mStride, nullptr);
                        cmdBindIndexBuffer(pCmd, pRPD->pIndexBuffer, INDEX_TYPE_UINT16, 0);
                        //cmdDrawIndexed(pCmd, 3, 0, 0);

                        cmdBindRenderTargets(pCmd, nullptr);
                    }
                }
            );
    }

    void module::OnExit(flecs::world& ecs)
    {
        if (ecs.has<RHI::RHI>() && ecs.has<RenderPassData>())
        {
            RHI::RHI const* pRHI = ecs.get<RHI::RHI>();

            waitQueueIdle(pRHI->pGfxQueue);

            Renderer* pRenderer = pRHI->pRenderer;
            RenderPassData* pRenderPassData = ecs.get_mut<RenderPassData>();
            RemoveDescriptorSet(pRenderer, *pRenderPassData);
            RemoveRootSignature(pRenderer, *pRenderPassData);
            RemoveShaders(pRenderer, *pRenderPassData);
            
            for (uint32_t i = 0; i < pRHI->dataBufferCount; ++i)
            {
                removeResource(pRenderPassData->uniformsBuffers[i]);
            }

            removeResource(pRenderPassData->pVertexBuffer);
            removeResource(pRenderPassData->pIndexBuffer);

            RemovePipeline(pRenderer, *pRenderPassData);

            pRenderPassData->Reset();
        }
    }
}