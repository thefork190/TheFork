#include <vector>
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <IResourceLoader.h>
#include "ILog.h"
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
            glm::mat4 mvp;
            glm::vec4 color;
        } uniformsData;
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

    static void AddDescriptorSet(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        DescriptorSetDesc desc = { passDataInOut.pRootSignature, DESCRIPTOR_UPDATE_FREQ_NONE, 1 };
        addDescriptorSet(pRenderer, &desc, &passDataInOut.pDescriptorSetUniforms);
    }

    static void RemoveDescriptorSet(Renderer* const pRenderer, RenderPassData& passDataInOut)
    {
        removeDescriptorSet(pRenderer, passDataInOut.pDescriptorSetUniforms);
    }

    static void AddPipeline(
        Renderer* const pRenderer,
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
        //pipelineSettings.pVertexLayout = &gSphereVertexLayout;
        pipelineSettings.pRasterizerState = &rasterizeStateDesc;
        pipelineSettings.mVRFoveatedRendering = true;
        addPipeline(pRenderer, &desc, &passDataInOut.pPipeline);
    }

    module::module(flecs::world& ecs)
    {
        ecs.import<RHI::module>();
        ecs.module<module>();

        ecs.component<RenderPassData>();
        
        RHI::RHI const* pRHI = ecs.has<RHI::RHI>() ? ecs.get<RHI::RHI>() : nullptr;
        ASSERTMSG(pRHI, "RHI singleton doesn't exist.");

        RenderPassData renderPassData = {};
        
        AddShaders(pRHI->pRenderer, renderPassData);
        AddRootSignature(pRHI->pRenderer, renderPassData);
        AddDescriptorSet(pRHI->pRenderer, renderPassData);

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

        BufferLoadDesc sphereVbDesc = {};
        sphereVbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
        sphereVbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        sphereVbDesc.mDesc.mSize = 3 * 12;
        sphereVbDesc.pData = triPositions.data();
        sphereVbDesc.ppBuffer = &renderPassData.pVertexBuffer;
        addResource(&sphereVbDesc, nullptr);

        std::vector<uint32_t> triIndices(3);
        triIndices[0] = 0;
        triIndices[1] = 1;
        triIndices[2] = 2;

        BufferLoadDesc sphereIbDesc = {};
        sphereIbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
        sphereIbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_GPU_ONLY;
        sphereIbDesc.mDesc.mSize = sizeof(uint32_t) * 3;
        sphereIbDesc.pData = triIndices.data();
        sphereIbDesc.ppBuffer = &renderPassData.pIndexBuffer;
        addResource(&sphereIbDesc, nullptr);

        waitForAllResourceLoads();

        ecs.set<RenderPassData>(renderPassData);
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
        }
    }
}