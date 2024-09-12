#include <vector>
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <IResourceLoader.h>
#include "ILog.h"
#include "Low/Engine.h"
#include "Low/Inputs.h"
#include "Low/RHI.h"
#include "Low/Window.h"
#include "FlappyClone.h"

namespace FlappyClone
{
    // Game related constants (TODO: drive these via debug UI)
    float const             OBSTACLE_WIDTH = 0.2f;              // Width of an obstacle
    float const             DIST_BETWEEN_OBSTACLES = 0.5f;      // Dist to next obstacle
    unsigned int const      TOTAL_OBSTACLES = 20u;              // Should have enough to cover ultra wide resolution
   
    // COMPONENT /////////////
    // Rendering resources.
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
#define MAX_QUADS 64 // this needs to match the same define in DrawQuad.h.fsl
        struct UniformsData
        {
            glm::mat4 proj = {};
            glm::mat4 mv[MAX_QUADS] = {};
            glm::vec4 color[MAX_QUADS] = {};
        } uniformsData;
        size_t curUniformIndex = 0;

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

    struct Position
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.1f;
    };

    struct Scale
    {
        float x = 1.f;
        float y = 1.f;
    };

    struct Color
    {
        float r = 1.f;
        float g = 1.f;
        float b = 1.f;
        float a = 1.f;
    };

    struct Obstacle {};
    //////////////////////////
    
    static void AddShaders(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        ShaderLoadDesc basicShader = {};
        basicShader.mStages[0].pFileName = "DrawQuad.vert";
        basicShader.mStages[1].pFileName = "DrawQuad.frag";
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
        pipelineSettings.pRasterizerState = &rasterizerStateDesc;
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
        ecs.component<Scale>();
        ecs.component<Position>();
        ecs.component<Color>();
        
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
            ubDesc.mDesc.pName = "FlappyClone_UniformBuffer";
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

        std::vector<glm::vec3> triPositions(4);
        triPositions[0] = { -0.5f, -0.5f , 0.f };
        triPositions[1] = { 0.5f, -0.5f , 0.f };
        triPositions[2] = { 0.5f, 0.5f , 0.f };
        triPositions[3] = { -0.5f, 0.5f , 0.f };

        BufferLoadDesc vbDesc = {};
        vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.mDesc.mSize = triPositions.size() * 12;
        vbDesc.pData = triPositions.data();
        vbDesc.ppBuffer = &renderPassData.pVertexBuffer;
        addResource(&vbDesc, nullptr);

        std::vector<uint16_t> triIndices(8); 
        triIndices[0] = 0;
        triIndices[1] = 1;
        triIndices[2] = 2;
        triIndices[3] = 2;
        triIndices[4] = 3;
        triIndices[5] = 0;

        BufferLoadDesc ibDesc = {};
        ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        ibDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        ibDesc.mDesc.mSize = triIndices.size() * sizeof(uint16_t);
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

        // Create obstacle entities
        // Pipe entities
        // A pipe entity will have 2 children: a top and bottom obstacle.  In flappy bird, a bottom and top pipe are always on the same Y axis.
        float const start_x_offset = 0; // TODO: start from outside of view based on aspect ratio
        for (unsigned int i = 0; i < TOTAL_OBSTACLES; ++i)
        {
            auto obstacle = ecs.entity((std::string("Obstacle ") + std::to_string(i)).c_str());
            obstacle.add<Obstacle>();

            for (unsigned int j = 0; j < 2; ++j)
            {
                auto child = ecs.entity((std::string("Obstacle ") + std::to_string(i) + (j == 0 ? "  TOP" : "  BOTTOM")).c_str());

                child.set<Color>({ 0.f, 0.0, 1.f, 1.f });
                child.set<Scale>({ OBSTACLE_WIDTH, OBSTACLE_WIDTH });
                Position pos = {};
                pos.x = start_x_offset + OBSTACLE_WIDTH / 2.f + (i * DIST_BETWEEN_OBSTACLES);
                pos.y = (j == 0) ? 1.f - OBSTACLE_WIDTH / 2.f : OBSTACLE_WIDTH / 2.f;
                pos.z = 0.1f;
                child.set<Position>(pos);
                child.add(flecs::ChildOf, obstacle);
            }
        }

        // SYSTEMS (note that the order of declaration is important for systems within the same phase)
        ecs.system<Position, Scale, Color>("FlappyClone::UpdateObstacles")
            .kind(flecs::OnUpdate)
            .with<Obstacle>().up(flecs::ChildOf)
            .each([](flecs::iter& it, size_t i, Position& position, Scale& scale, Color const& color)
                {
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;
                    
                    // Rendering data
                    if (pRPD)
                    {
                        RenderPassData::UniformsData& updatedData = pRPD->uniformsData;

                        glm::mat4 modelMat(1.f);
                        modelMat = glm::translate(modelMat, glm::vec3(position.x, position.y, position.z));
                        modelMat = glm::scale(modelMat, glm::vec3(scale.x, scale.y, 1.f));
                        updatedData.mv[pRPD->curUniformIndex] = modelMat;
                        updatedData.color[pRPD->curUniformIndex] = glm::vec4(color.r, color.g, color.b, color.a);

                        pRPD->curUniformIndex++;
                    }
                }
            );
                
        ecs.system<Engine::Canvas, Window::SDLWindow>("FlappyClone::UpdateUniforms")
            .kind(flecs::PreStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas const& canvas, Window::SDLWindow const& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;
                    Inputs::RawKeboardStates const* pKeyboard = it.world().has<Inputs::RawKeboardStates>() ? it.world().get<Inputs::RawKeboardStates>() : nullptr;
                    Engine::Context* pEngineContext = it.world().has<Engine::Context>() ? it.world().get_mut<Engine::Context>() : nullptr;

                    // Rendering update
                    if (pRHI && pRPD)
                    {
                        float const aspect = canvas.width / static_cast<float>(canvas.height);
                        pRPD->uniformsData.proj = glm::orthoLH_ZO(0.f, aspect, 0.f, 1.f, 0.1f, 1.f);
                        
                        // Update uniform buffers
                        BufferUpdateDesc updateDesc = { pRPD->uniformsBuffers[pRHI->frameIndex] };
                        beginUpdateResource(&updateDesc);
                        memcpy(updateDesc.pMappedData, &pRPD->uniformsData, sizeof(RenderPassData::UniformsData));
                        endUpdateResource(&updateDesc);

                        // Reset uniform index for next frame
                        pRPD->curUniformIndex = 0;
                    }

                    // Exit if ESC is pressed
                    if (pKeyboard && pEngineContext)
                    {
                        if (pKeyboard->WasPressed(SDLK_ESCAPE))
                        {
                            LOGF(eDEBUG, "ESC pressed, requesting to exit the app.");
                            pEngineContext->RequestExit();
                        }
                    }
                }
            );

        ecs.system<Engine::Canvas, Window::SDLWindow>("FlappyClone::Draw")
            .kind(flecs::PreStore)
            .each([](flecs::iter& it, size_t i, Engine::Canvas& canvas, Window::SDLWindow& sdlWin)
                {
                    ASSERTMSG(i == 0, "Drawing to more than one window not implemented.");

                    RHI::RHI const* pRHI = it.world().has<RHI::RHI>() ? it.world().get<RHI::RHI>() : nullptr;
                    RenderPassData* pRPD = it.world().has<RenderPassData>() ? it.world().get_mut<RenderPassData>() : nullptr;

                    if (pRHI && pRPD && sdlWin.pCurRT)
                    {
                        Cmd* pCmd = pRHI->curCmdRingElem.pCmds[0];
                        ASSERT(pCmd);

                        cmdBeginDebugMarker(pCmd, 1, 0, 1, "FlappyClone::DrawObstacles");

                        BindRenderTargetsDesc bindRenderTargets = {};
                        bindRenderTargets.mRenderTargetCount = 1;
                        bindRenderTargets.mRenderTargets[0] = { sdlWin.pCurRT, LOAD_ACTION_LOAD };
                        cmdBindRenderTargets(pCmd, &bindRenderTargets);
                        cmdSetViewport(pCmd, 0.0f, 0.0f, static_cast<float>(canvas.width), static_cast<float>(canvas.height), 0.0f, 1.0f);
                        cmdSetScissor(pCmd, 0, 0, canvas.width, canvas.height);

                        cmdBindPipeline(pCmd, pRPD->pPipeline);
                        cmdBindDescriptorSet(pCmd, pRHI->frameIndex, pRPD->pDescriptorSetUniforms);
                        cmdBindVertexBuffer(pCmd, 1, &pRPD->pVertexBuffer, &pRPD->vertexLayout.mBindings[0].mStride, nullptr);
                        cmdBindIndexBuffer(pCmd, pRPD->pIndexBuffer, INDEX_TYPE_UINT16, 0);

                        unsigned int const totalObstacles = TOTAL_OBSTACLES * 2; // bottom and top "pipes"
                        cmdDrawIndexedInstanced(pCmd, 6, 0, totalObstacles, 0, 0);

                        cmdBindRenderTargets(pCmd, nullptr);

                        cmdEndDebugMarker(pCmd);
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