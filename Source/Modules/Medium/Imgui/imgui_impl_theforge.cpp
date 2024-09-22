#include <vector>

#include <tinyimageformat/tinyimageformat_query.h>
#include <IGraphics.h>
#include <IFont.h>
#include <IResourceLoader.h>
#include <ILog.h>

#include "imgui_impl_theforge.h"

#define MAX_FRAMES 3u
#define FONT_TEXTURE_INDEX 0u

struct ImGui_ImplTheForge_Data
{
    uint32_t mMaxDynamicUIUpdatesPerBatch = 32u;
    uint32_t mFrameCount = 2u;

    uint32_t mMaxVerts = 64u * 1024u;
    uint32_t mMaxInds = 128u * 1024u;

    Renderer* pRenderer = nullptr;
    PipelineCache* pCache = nullptr;
    uint32_t  mFrameIdx = 0;

    uintptr_t pDefaultFallbackFont = 0;

    uint32_t mDynamicTexturesCount = 0;
    Shader* pShaderTextured[SAMPLE_COUNT_COUNT] = { nullptr };
    RootSignature* pRootSignatureTextured = nullptr;
    RootSignature* pRootSignatureTexturedMs = nullptr;
    DescriptorSet* pDescriptorSetUniforms = nullptr;
    DescriptorSet* pDescriptorSetTexture = nullptr;
    Pipeline* pPipelineTextured[SAMPLE_COUNT_COUNT] = { nullptr };
    Buffer* pVertexBuffer = nullptr;
    Buffer* pIndexBuffer = nullptr;
    Buffer* pUniformBuffer[MAX_FRAMES] = { nullptr };
    /// Default states
    Sampler* pDefaultSampler = nullptr;
    VertexLayout mVertexLayoutTextured = {};

    /// Caching fonts and textures
    struct UIFontResource
    {
        Texture*  pFontTex = nullptr;
        uint32_t  mFontId = 0;
        float     mFontSize = 0.f;
        uintptr_t pFont = 0;
    };
    std::vector<UIFontResource> mCachedFonts;

    Texture* pFontTex = nullptr;
};


// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplTheForge_Data* ImGui_ImplTheForge_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplTheForge_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static uint32_t ImGui_ImplTheForge_AddImguiFont(
    ImGui_ImplTheForge_Data* pBD,
    void* pFontBuffer, 
    uint32_t fontBufferSize, 
    void* pFontGlyphRanges, 
    uint32_t fontID, 
    float fontSize)
{
    ImGuiIO& io = ImGui::GetIO();

    // Build and load the texture atlas into a texture
    int32_t        width, height, bytesPerPixel;
    unsigned char* pixels = nullptr;

    uintptr_t font;
    uintptr_t* pFont = &font;

    io.Fonts->ClearInputData();
    if (pFontBuffer == nullptr)
    {
        *pFont = (uintptr_t)io.Fonts->AddFontDefault();
    }
    else
    {
        ImFontConfig config = {};
        config.FontDataOwnedByAtlas = false;
        ImFont* font = io.Fonts->AddFontFromMemoryTTF(pFontBuffer, fontBufferSize, fontSize, &config, (const ImWchar*)pFontGlyphRanges);
        if (font != nullptr)
        {
            io.FontDefault = font;
            *pFont = (uintptr_t)font;
        }
        else
        {
            ImFontConfig cfg = ImFontConfig();
            *pFont = (uintptr_t)io.Fonts->AddFontDefault(&cfg);
        }
    }

    io.Fonts->Build();
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

    // At this point you've got the texture data and you need to upload that your your graphic system:
    // After we have created the texture, store its pointer/identifier (_in whichever format your engine uses_) in 'io.Fonts->TexID'.
    // This will be passed back to your via the renderer. Basically ImTextureID == void*. Read FAQ below for details about ImTextureID.
    Texture* pTexture = NULL;
    SyncToken       token = {};
    TextureLoadDesc loadDesc = {};
    TextureDesc     textureDesc = {};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = height;
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = width;
    textureDesc.pName = "ImGui Font Texture";
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &pTexture;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    TextureUpdateDesc updateDesc = { pTexture, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    for (uint32_t r = 0; r < subresource.mRowCount; ++r)
    {
        memcpy(subresource.pMappedData + r * subresource.mDstRowStride, pixels + r * subresource.mSrcRowStride, subresource.mSrcRowStride);
    }
    endUpdateResource(&updateDesc);

    ImGui_ImplTheForge_Data::UIFontResource newCachedFont = { pTexture, fontID, fontSize, *pFont };
    pBD->mCachedFonts.push_back(newCachedFont);

    size_t fontTextureIndex = pBD->mCachedFonts.size() - 1;
    io.Fonts->TexID = (ImTextureID)fontTextureIndex;

    return (uint32_t)fontTextureIndex;
}

bool ImGui_TheForge_Init(ImGui_ImplTheForge_InitDesc const& initDesc)
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();

    if (io.BackendRendererUserData != nullptr)
    {
        IM_ASSERT(false && "Already initialized a renderer backend!");
        return false;
    }

    // Setup backend capabilities flags
    ImGui_ImplTheForge_Data* pBD = IM_NEW(ImGui_ImplTheForge_Data)();
    io.BackendRendererUserData = (void*)pBD;
    io.BackendRendererName = "imgui_impl_theforge";
    //TODO: multi-viewports
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    if (!initDesc.pRenderer)
    {
        IM_ASSERT(false && "TF renderer is invalid.");
        return false;
    }

    pBD->pRenderer = initDesc.pRenderer;
    pBD->pCache = initDesc.pCache;
    pBD->mMaxDynamicUIUpdatesPerBatch = initDesc.mMaxDynamicUIUpdatesPerBatch;
    pBD->mFrameCount = initDesc.mFrameCount;
    ASSERT(pBD->mFrameCount <= MAX_FRAMES);

    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(pBD->pRenderer, &samplerDesc, &pBD->pDefaultSampler);

    uint64_t const VERTEX_BUFFER_SIZE = initDesc.mMaxVerts * sizeof(ImDrawVert);
    uint64_t const INDEX_BUFFER_SIZE = initDesc.mMaxInds * sizeof(ImDrawIdx);
    pBD->mMaxVerts = initDesc.mMaxVerts;
    pBD->mMaxInds = initDesc.mMaxInds;

    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = VERTEX_BUFFER_SIZE * pBD->mFrameCount;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.pName = "UI Vertex Buffer";
    vbDesc.ppBuffer = &pBD->pVertexBuffer;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mSize = INDEX_BUFFER_SIZE * pBD->mFrameCount;
    vbDesc.mDesc.pName = "UI Index Buffer";
    ibDesc.ppBuffer = &pBD->pIndexBuffer;
    addResource(&ibDesc, nullptr);

    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(float) * 16;
    vbDesc.mDesc.pName = "UI Uniform Buffer";
    for (uint32_t i = 0; i < pBD->mFrameCount; ++i)
    {
        ubDesc.ppBuffer = &pBD->pUniformBuffer[i];
        addResource(&ubDesc, nullptr);
    }

    VertexLayout* vertexLayout = &pBD->mVertexLayoutTextured;
    vertexLayout->mBindingCount = 1;
    vertexLayout->mAttribCount = 3;
    vertexLayout->mAttribs[0].mSemantic = SEMANTIC_POSITION;
    vertexLayout->mAttribs[0].mFormat = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->mAttribs[0].mBinding = 0;
    vertexLayout->mAttribs[0].mLocation = 0;
    vertexLayout->mAttribs[0].mOffset = 0;
    vertexLayout->mAttribs[1].mSemantic = SEMANTIC_TEXCOORD0;
    vertexLayout->mAttribs[1].mFormat = TinyImageFormat_R32G32_SFLOAT;
    vertexLayout->mAttribs[1].mBinding = 0;
    vertexLayout->mAttribs[1].mLocation = 1;
    vertexLayout->mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(pBD->mVertexLayoutTextured.mAttribs[0].mFormat) / 8;
    vertexLayout->mAttribs[2].mSemantic = SEMANTIC_COLOR;
    vertexLayout->mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    vertexLayout->mAttribs[2].mBinding = 0;
    vertexLayout->mAttribs[2].mLocation = 2;
    vertexLayout->mAttribs[2].mOffset =
    vertexLayout->mAttribs[1].mOffset + TinyImageFormat_BitSizeOfBlock(pBD->mVertexLayoutTextured.mAttribs[1].mFormat) / 8;

    const char* imguiFrag[SAMPLE_COUNT_COUNT] = {
                "imgui_SAMPLE_COUNT_1.frag", "imgui_SAMPLE_COUNT_2.frag",  "imgui_SAMPLE_COUNT_4.frag",
                "imgui_SAMPLE_COUNT_8.frag", "imgui_SAMPLE_COUNT_16.frag",
    };
    ShaderLoadDesc texturedShaderDesc = {};
    texturedShaderDesc.mStages[0] = { "imgui.vert" };
    for (uint32_t s = 0; s < TF_ARRAY_COUNT(imguiFrag); ++s)
    {
        texturedShaderDesc.mStages[1] = { imguiFrag[s] };
        addShader(pBD->pRenderer, &texturedShaderDesc, &pBD->pShaderTextured[s]);
    }

    const char* pStaticSamplerNames[] = { "uSampler" };
    RootSignatureDesc textureRootDesc = { pBD->pShaderTextured, 1 };
    textureRootDesc.mStaticSamplerCount = 1;
    textureRootDesc.ppStaticSamplerNames = pStaticSamplerNames;
    textureRootDesc.ppStaticSamplers = &pBD->pDefaultSampler;
    addRootSignature(pBD->pRenderer, &textureRootDesc, &pBD->pRootSignatureTextured);

    DescriptorSetDesc setDesc = { pBD->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_PER_BATCH,
                                          1 + (pBD->mMaxDynamicUIUpdatesPerBatch * pBD->mFrameCount) };
    addDescriptorSet(pBD->pRenderer, &setDesc, &pBD->pDescriptorSetTexture);
    setDesc = { pBD->pRootSignatureTextured, DESCRIPTOR_UPDATE_FREQ_NONE, pBD->mFrameCount };
    addDescriptorSet(pBD->pRenderer, &setDesc, &pBD->pDescriptorSetUniforms);

    for (uint32_t i = 0; i < pBD->mFrameCount; ++i)
    {
        DescriptorData params[1] = {};
        params[0].pName = "uniformBlockVS";
        params[0].ppBuffers = &pBD->pUniformBuffer[i];
        updateDescriptorSet(pBD->pRenderer, i, pBD->pDescriptorSetUniforms, 1, params);
    }

    BlendStateDesc blendStateDesc = {};
    blendStateDesc.mSrcFactors[0] = BC_SRC_ALPHA;
    blendStateDesc.mDstFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    blendStateDesc.mSrcAlphaFactors[0] = BC_SRC_ALPHA;
    blendStateDesc.mDstAlphaFactors[0] = BC_ONE_MINUS_SRC_ALPHA;
    blendStateDesc.mColorWriteMasks[0] = COLOR_MASK_ALL;
    blendStateDesc.mRenderTargetMask = BLEND_STATE_TARGET_ALL;
    blendStateDesc.mIndependentBlend = false;

    DepthStateDesc depthStateDesc = {};
    depthStateDesc.mDepthTest = false;
    depthStateDesc.mDepthWrite = false;

    RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.mCullMode = CULL_MODE_NONE;
    rasterizerStateDesc.mScissor = true;

    PipelineDesc desc = {};
    desc.pCache = pBD->pCache;
    desc.mType = PIPELINE_TYPE_GRAPHICS;
    GraphicsPipelineDesc& pipelineDesc = desc.mGraphicsDesc;
    pipelineDesc.mDepthStencilFormat = TinyImageFormat_UNDEFINED;
    pipelineDesc.mRenderTargetCount = 1;
    pipelineDesc.mSampleCount = SAMPLE_COUNT_1;
    pipelineDesc.pBlendState = &blendStateDesc;
    pipelineDesc.mSampleQuality = 0;
    pipelineDesc.pColorFormats = (TinyImageFormat*)&initDesc.mColorFormat;
    pipelineDesc.pDepthState = &depthStateDesc;
    pipelineDesc.pRasterizerState = &rasterizerStateDesc;
    pipelineDesc.pRootSignature = pBD->pRootSignatureTextured;
    pipelineDesc.pVertexLayout = &pBD->mVertexLayoutTextured;
    pipelineDesc.mPrimitiveTopo = PRIMITIVE_TOPO_TRI_LIST;
    pipelineDesc.mVRFoveatedRendering = true;
    for (uint32_t s = 0; s < TF_ARRAY_COUNT(pBD->pShaderTextured); ++s)
    {
        pipelineDesc.pShaderProgram = pBD->pShaderTextured[s];
        addPipeline(pBD->pRenderer, &desc, &pBD->pPipelineTextured[s]);
    }

    return true;
}

void ImGui_TheForge_Shutdown()
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();
    ASSERT(pBD != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasViewports;

    for (uint32_t s = 0; s < TF_ARRAY_COUNT(pBD->pShaderTextured); ++s)
    {
        removePipeline(pBD->pRenderer, pBD->pPipelineTextured[s]);
    }

    for (uint32_t s = 0; s < TF_ARRAY_COUNT(pBD->pShaderTextured); ++s)
    {
        removeShader(pBD->pRenderer, pBD->pShaderTextured[s]);
    }
    removeDescriptorSet(pBD->pRenderer, pBD->pDescriptorSetTexture);
    removeDescriptorSet(pBD->pRenderer, pBD->pDescriptorSetUniforms);
    removeRootSignature(pBD->pRenderer, pBD->pRootSignatureTextured);
    
    removeSampler(pBD->pRenderer, pBD->pDefaultSampler);

    removeResource(pBD->pVertexBuffer);
    removeResource(pBD->pIndexBuffer);
    for (uint32_t i = 0; i < pBD->mFrameCount; ++i)
    {
        if (pBD->pUniformBuffer[i])
        {
            removeResource(pBD->pUniformBuffer[i]);
            pBD->pUniformBuffer[i] = NULL;
        }
    }

    for (ptrdiff_t i = 0; i < pBD->mCachedFonts.size(); ++i)
        removeResource(pBD->mCachedFonts[i].pFontTex);

    if (pBD->pFontTex)
        removeResource(pBD->pFontTex);

    IM_DELETE(pBD);
}

void ImGui_TheForge_NewFrame()
{
    ImGui_ImplTheForge_Data* bd = ImGui_ImplTheForge_GetBackendData();
    ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplTheForge_Init()?");
    bd->mDynamicTexturesCount = 0u;
}

static void cmdPrepareRenderingForUI(
    ImGui_ImplTheForge_Data* pBD,
    Cmd* pCmd, 
    const float2& displayPos, const float2& displaySize, 
    Pipeline* pPipeline,
    const uint64_t vOffset, const uint64_t iOffset)
{
    const float                  L = displayPos.x;
    const float                  R = displayPos.x + displaySize.x;
    const float                  T = displayPos.y;
    const float                  B = displayPos.y + displaySize.y;
    alignas(alignof(mat4)) float mvp[4][4] = {
        { 2.0f / (R - L), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (T - B), 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.5f, 0.0f },
        { (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f },
    };

    BufferUpdateDesc update = { pBD->pUniformBuffer[pBD->mFrameIdx] };
    beginUpdateResource(&update);
    memcpy(update.pMappedData, &mvp, sizeof(mvp));
    endUpdateResource(&update);

    const uint32_t vertexStride = sizeof(ImDrawVert);

    cmdSetViewport(pCmd, 0.0f, 0.0f, displaySize.x, displaySize.y, 0.0f, 1.0f);
    cmdSetScissor(pCmd, (uint32_t)displayPos.x, (uint32_t)displayPos.y, (uint32_t)displaySize.x, (uint32_t)displaySize.y);

    cmdBindPipeline(pCmd, pPipeline);
    cmdBindIndexBuffer(pCmd, pBD->pIndexBuffer, sizeof(ImDrawIdx) == sizeof(uint16_t) ? INDEX_TYPE_UINT16 : INDEX_TYPE_UINT32,
        iOffset);
    cmdBindVertexBuffer(pCmd, 1, &pBD->pVertexBuffer, &vertexStride, &vOffset);
    cmdBindDescriptorSet(pCmd, pBD->mFrameIdx, pBD->pDescriptorSetUniforms);
}

static void cmdDrawUICommand(ImGui_ImplTheForge_Data* pBD, Cmd* pCmd, const ImDrawCmd* pImDrawCmd, const float2& displayPos, const float2& displaySize,
    Pipeline** ppPipelineInOut, Pipeline** ppPrevPipelineInOut, uint32_t& globalVtxOffsetInOut,
    uint32_t& globalIdxOffsetInOut, uint32_t& prevSetIndexInOut, const int32_t vertexCount, const int32_t indexCount)
{
    float2 clipMin = { clamp(pImDrawCmd->ClipRect.x - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->ClipRect.y - displayPos.y, 0.0f, displaySize.y) };
    float2 clipMax = { clamp(pImDrawCmd->ClipRect.z - displayPos.x, 0.0f, displaySize.x),
                       clamp(pImDrawCmd->ClipRect.w - displayPos.y, 0.0f, displaySize.y) };
    if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
    {
        return;
    }
    if (!pImDrawCmd->ElemCount)
    {
        return;
    }

    uint2 offset = { (uint32_t)clipMin.x, (uint32_t)clipMin.y };
    uint2 ext = { (uint32_t)(clipMax.x - clipMin.x), (uint32_t)(clipMax.y - clipMin.y) };
    cmdSetScissor(pCmd, offset.x, offset.y, ext.x, ext.y);

    ptrdiff_t id = (ptrdiff_t)pImDrawCmd->TextureId;
    uint32_t  setIndex = (uint32_t)id;
    if (id != FONT_TEXTURE_INDEX) // it's not a font, it's an external texture
    {
        if (pBD->mDynamicTexturesCount >= pBD->mMaxDynamicUIUpdatesPerBatch)
        {
            LOGF(eWARNING,
                "Too many dynamic UIs.  Consider increasing 'mMaxDynamicUIUpdatesPerBatch' when initializing the user interface.");
            return;
        }

        Texture* tex = (Texture*)pImDrawCmd->TextureId;
        setIndex = 1 + ((ptrdiff_t)pBD->mFrameIdx * pBD->mMaxDynamicUIUpdatesPerBatch +
            pBD->mDynamicTexturesCount++);


        DescriptorData params[1] = {};
        params[0].pName = "uTex";
        params[0].ppTextures = &tex;
        updateDescriptorSet(pBD->pRenderer, setIndex, pBD->pDescriptorSetTexture, 1, params);

        uint32_t pipelineIndex = (uint32_t)log2(params[0].ppTextures[0]->mSampleCount);
        *ppPipelineInOut = pBD->pPipelineTextured[pipelineIndex];
    }
    else
    {
        *ppPipelineInOut = pBD->pPipelineTextured[0];
    }

    if (*ppPrevPipelineInOut != *ppPipelineInOut)
    {
        cmdBindPipeline(pCmd, *ppPipelineInOut);
        *ppPrevPipelineInOut = *ppPipelineInOut;
    }

    if (setIndex != prevSetIndexInOut)
    {
        cmdBindDescriptorSet(pCmd, setIndex, pBD->pDescriptorSetTexture);
        prevSetIndexInOut = setIndex;
    }

    cmdDrawIndexed(pCmd, pImDrawCmd->ElemCount, pImDrawCmd->IdxOffset + globalIdxOffsetInOut,
                   pImDrawCmd->VtxOffset + globalVtxOffsetInOut);
    globalIdxOffsetInOut += indexCount;
    globalVtxOffsetInOut += vertexCount;
}

void ImGui_TheForge_RenderDrawData(ImDrawData* pImDrawData, Cmd* pCmd)
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();
    ASSERT(pBD != nullptr && "Context or backend not initialized! Did you call ImGui_ImplTheForge_Init()?");

    float2 displayPos(pImDrawData->DisplayPos.x, pImDrawData->DisplayPos.y);
    float2 displaySize(pImDrawData->DisplaySize.x, pImDrawData->DisplaySize.y);

    uint64_t vSize = pImDrawData->TotalVtxCount * sizeof(ImDrawVert);
    uint64_t iSize = pImDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    const uint64_t vertexBufferSize = pBD->mMaxVerts * sizeof(ImDrawVert);
    const uint64_t indexBufferSize = pBD->mMaxInds * sizeof(ImDrawIdx);

    vSize = min<uint64_t>(vSize, vertexBufferSize);
    iSize = min<uint64_t>(iSize, indexBufferSize);

    uint64_t vOffset = pBD->mFrameIdx * vertexBufferSize;
    uint64_t iOffset = pBD->mFrameIdx * indexBufferSize;

    if (pImDrawData->TotalVtxCount > pBD->mMaxVerts || pImDrawData->TotalIdxCount > pBD->mMaxInds)
    {
        LOGF(eWARNING, "UI exceeds amount of verts/inds.  Consider updating mMaxVerts/mMaxInds when calling ImGui_ImplTheForge_Init().");
        LOGF(eWARNING, "Num verts: %d (max %d) | Num inds: %d (max %d)", pImDrawData->TotalVtxCount, pBD->mMaxVerts,
            pImDrawData->TotalIdxCount, pBD->mMaxInds);
        pImDrawData->TotalVtxCount =
            pImDrawData->TotalVtxCount > pImDrawData->TotalVtxCount ? pImDrawData->TotalVtxCount : pImDrawData->TotalVtxCount;
        pImDrawData->TotalIdxCount = pImDrawData->TotalIdxCount > pBD->mMaxInds ? pBD->mMaxInds : pImDrawData->TotalIdxCount;
    }

    uint64_t vtxDst = vOffset;
    uint64_t idxDst = iOffset;

    for (int32_t i = 0; i < pImDrawData->CmdListsCount; i++)
    {
        const ImDrawList* pCmdList = pImDrawData->CmdLists[i];
        const uint64_t    vtxSize = pCmdList->VtxBuffer.size() * sizeof(ImDrawVert);
        const uint64_t idxSize = pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx);
        BufferUpdateDesc update = { pBD->pVertexBuffer, vtxDst, vtxSize };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pCmdList->VtxBuffer.Data, pCmdList->VtxBuffer.size() * sizeof(ImDrawVert));
        endUpdateResource(&update);

        update = { pBD->pIndexBuffer, idxDst, idxSize };
        beginUpdateResource(&update);
        memcpy(update.pMappedData, pCmdList->IdxBuffer.Data, pCmdList->IdxBuffer.size() * sizeof(ImDrawIdx));
        endUpdateResource(&update);

        // Round up in case the buffer alignment is not a multiple of vertex/index size
        vtxDst += round_up_64(vtxSize, sizeof(ImDrawVert));
        idxDst += round_up_64(idxSize, sizeof(ImDrawIdx));
    }

    Pipeline* pPipeline = pBD->pPipelineTextured[0];
    Pipeline* pPreviousPipeline = pPipeline;
    uint32_t  prevSetIndex = UINT32_MAX;

    cmdPrepareRenderingForUI(pBD, pCmd, displayPos, displaySize, pPipeline, vOffset, iOffset);

    // Render command lists
    uint32_t globalVtxOffset = 0;
    uint32_t globalIdxOffset = 0;

    {
        for (int n = 0; n < pImDrawData->CmdListsCount; n++)
        {
            const ImDrawList* pCmdList = pImDrawData->CmdLists[n];

            for (int c = 0; c < pCmdList->CmdBuffer.size(); c++)
            {
                const ImDrawCmd* pImDrawCmd = &pCmdList->CmdBuffer[c];

                if (pImDrawCmd->UserCallback)
                {
                    // User callback (registered via ImDrawList::AddCallback)
                    pImDrawCmd->UserCallback(pCmdList, pImDrawCmd);
                    continue;
                }

                int32_t vertexCount, indexCount;
                if (c == pCmdList->CmdBuffer.size() - 1)
                {
                    vertexCount = pCmdList->VtxBuffer.size();
                    indexCount = pCmdList->IdxBuffer.size();
                }
                else
                {
                    vertexCount = 0;
                    indexCount = 0;
                }
                cmdDrawUICommand(pBD, pCmd, pImDrawCmd, displayPos, displaySize, &pPipeline, &pPreviousPipeline, globalVtxOffset,
                                 globalIdxOffset, prevSetIndex, vertexCount, indexCount);
            }
        }
    }

    pBD->mFrameIdx = (pBD->mFrameIdx + 1) % pBD->mFrameCount;
}

ImFont* ImGui_TheForge_GetOrAddFont(uint32_t const fontId, float const size)
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();

    uintptr_t ret = 0;

    //if (pBD)
    //{
    //    // Functions not accessible via normal interface header
    //    extern void* fntGetRawFontData(uint32_t fontID);
    //    extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

    //    bool useDefaultFallbackFont = false;

    //    // Use Requested Forge Font
    //    void* pFontBuffer = fntGetRawFontData(fontId);
    //    uint32_t fontBufferSize = fntGetRawFontDataSize(fontId);
    //    if (pFontBuffer)
    //    {
    //        // See if that specific font id and size is already in use, if so just reuse it
    //        size_t cachedFontIndex = -1;
    //        for (size_t i = 0; i < pBD->mCachedFonts.size(); ++i)
    //        {
    //            if (pBD->mCachedFonts[i].mFontId == fontId &&
    //                pBD->mCachedFonts[i].mFontSize == size)
    //            {
    //                cachedFontIndex = i;

    //                ret = pBD->mCachedFonts[i].pFont;

    //                break;
    //            }
    //        }

    //        if (cachedFontIndex == -1) // didn't find that font in the cache
    //        {
    //            uint32_t newFontTexId =
    //                ImGui_ImplTheForge_AddImguiFont(pBD, pFontBuffer, fontBufferSize, nullptr, fontId, size);
    //            ASSERT(newFontTexId != FALLBACK_FONT_TEXTURE_INDEX);
    //            ASSERT(pBD->mCachedFonts[newFontTexId].pFontTex);

    //            DescriptorData params[1] = {};
    //            params[0].pName = "uTex";
    //            params[0].ppTextures = &pBD->mCachedFonts[newFontTexId].pFontTex;
    //            updateDescriptorSet(pBD->pRenderer, newFontTexId, pBD->pDescriptorSetTexture, 1, params);

    //            ret = newFontTexId;
    //        }
    //    }
    //    else
    //    {
    //        LOGF(eWARNING, "ImGui_TheForge_GetOrAddFont() was provided an unknown font id (%u).  Will fallback to default UI font.", fontId);
    //        ret = pBD->pDefaultFallbackFont;
    //    }
    //}

    return (ImFont*)ret;
}

bool ImGui_TheForge_FontIsLoaded(uint32_t const fontId, float const size)
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();

    if (!pBD)
        return false;

    size_t cachedFontIndex = -1;
    for (size_t i = 0; i < pBD->mCachedFonts.size(); ++i)
    {
        if (pBD->mCachedFonts[i].mFontId == fontId &&
            pBD->mCachedFonts[i].mFontSize == size)
        {
            cachedFontIndex = i;
            break;
        }
    }

    return cachedFontIndex != -1;
}

ImFont* ImGui_TheForge_GetFallbackFont()
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();

    if (!pBD)
        return nullptr;

    return (ImFont*)pBD->pDefaultFallbackFont;
}

void ImGui_TheForge_BuildFontAtlas(Queue* pGfxQueue)
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();

    if (!pBD)
        return;

    // TODO: instead of waiting for idle, create a new texture and evict the old once once all in-flight frames are done
    if (pGfxQueue)
        waitQueueIdle(pGfxQueue);

    if (pBD->pFontTex)
    {
        removeResource(pBD->pFontTex);
        pBD->pFontTex = nullptr;
    }

    ImGuiIO& io = ImGui::GetIO();

    io.Fonts->Build();

    int32_t        width, height, bytesPerPixel;
    unsigned char* pixels = nullptr;

    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytesPerPixel);

    SyncToken       token = {};
    TextureLoadDesc loadDesc = {};
    TextureDesc     textureDesc = {};
    textureDesc.mArraySize = 1;
    textureDesc.mDepth = 1;
    textureDesc.mDescriptors = DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    textureDesc.mHeight = height;
    textureDesc.mMipLevels = 1;
    textureDesc.mSampleCount = SAMPLE_COUNT_1;
    textureDesc.mStartState = RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    textureDesc.mWidth = width;
    textureDesc.pName = "ImGui Font Texture";
    loadDesc.pDesc = &textureDesc;
    loadDesc.ppTexture = &pBD->pFontTex;
    addResource(&loadDesc, &token);
    waitForToken(&token);

    TextureUpdateDesc updateDesc = { pBD->pFontTex, 0, 1, 0, 1, RESOURCE_STATE_PIXEL_SHADER_RESOURCE };
    beginUpdateResource(&updateDesc);
    TextureSubresourceUpdate subresource = updateDesc.getSubresourceUpdateDesc(0, 0);
    for (uint32_t r = 0; r < subresource.mRowCount; ++r)
    {
        memcpy(subresource.pMappedData + r * subresource.mDstRowStride, pixels + r * subresource.mSrcRowStride, subresource.mSrcRowStride);
    }
    endUpdateResource(&updateDesc);

    io.Fonts->TexID = (ImTextureID)FONT_TEXTURE_INDEX;

    DescriptorData params[1] = {};
    params[0].pName = "uTex";
    params[0].ppTextures = &pBD->pFontTex;
    updateDescriptorSet(pBD->pRenderer, FONT_TEXTURE_INDEX, pBD->pDescriptorSetTexture, 1, params);
}