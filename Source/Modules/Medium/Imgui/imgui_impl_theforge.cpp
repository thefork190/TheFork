#include <vector>

#include <tinyimageformat/tinyimageformat_query.h>
#include <IGraphics.h>
#include <IFont.h>
#include <IResourceLoader.h>
#include <ILog.h>

#include "imgui_impl_theforge.h"

#define MAX_FRAMES 3u
#define FALLBACK_FONT_TEXTURE_INDEX 0u

struct ImGui_ImplTheForge_Data
{
    uint32_t mMaxDynamicUIUpdatesPerBatch = 20u;
    uint32_t mMaxUIFonts = 10u;
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

    uintptr_t* pFont = nullptr;

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
            *pFont = (uintptr_t)io.Fonts->AddFontDefault();
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
    pBD->mMaxUIFonts = initDesc.mMaxUIFonts + 1; // +1 to account for a default fallback font
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

    // Cache default font
    uint32_t fallbackFontTexId = ImGui_ImplTheForge_AddImguiFont(pBD, nullptr, 0, nullptr, UINT_MAX, 0.f);
    ASSERT(fallbackFontTexId == FALLBACK_FONT_TEXTURE_INDEX);

    return true;
}

void ImGui_TheForge_Shutdown()
{
    ImGui_ImplTheForge_Data* bd = ImGui_ImplTheForge_GetBackendData();
    ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_RendererHasViewports;
    IM_DELETE(bd);
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


void ImGui_TheForge_RenderDrawData(ImDrawData* draw_data)
{

}

ImFont* ImGui_TheForge_GetOrAddFont(uint32_t const fontId, float const size)
{
    ImGui_ImplTheForge_Data* pBD = ImGui_ImplTheForge_GetBackendData();

    uintptr_t ret = 0;

    if (pBD)
    {
        // Functions not accessible via normal interface header
        extern void* fntGetRawFontData(uint32_t fontID);
        extern uint32_t fntGetRawFontDataSize(uint32_t fontID);

        bool useDefaultFallbackFont = false;

        // Use Requested Forge Font
        void* pFontBuffer = fntGetRawFontData(fontId);
        uint32_t fontBufferSize = fntGetRawFontDataSize(fontId);
        if (pFontBuffer)
        {
            // See if that specific font id and size is already in use, if so just reuse it
            size_t cachedFontIndex = -1;
            for (size_t i = 0; i < pBD->mCachedFonts.size(); ++i)
            {
                if (pBD->mCachedFonts[i].mFontId == fontId &&
                    pBD->mCachedFonts[i].mFontSize == size)
                {
                    cachedFontIndex = i;

                    ret = pBD->mCachedFonts[i].pFont;

                    break;
                }
            }

            if (cachedFontIndex == -1) // didn't find that font in the cache
            {
                // Ensure we don't pass max amount of fonts
                if (pBD->mCachedFonts.size() < pBD->mMaxUIFonts)
                {
                    uint32_t fallbackFontTexId =
                        ImGui_ImplTheForge_AddImguiFont(pBD, pFontBuffer, fontBufferSize, nullptr, fontId, size);
                    ASSERT(fallbackFontTexId != FALLBACK_FONT_TEXTURE_INDEX);
                }
                else
                {
                    LOGF(eWARNING, "ImGui_TheForge_GetOrAddFont() has reached fonts capacity.  Consider increasing 'mMaxUIFonts' when initializing the "
                        "user interface.");
                    useDefaultFallbackFont = true;
                }
            }
        }
        else
        {
            LOGF(eWARNING, "ImGui_TheForge_GetOrAddFont() was provided an unknown font id (%u).  Will fallback to default UI font.", fontId);
            useDefaultFallbackFont = true;
        }

        if (useDefaultFallbackFont)
        {
            ret = pBD->pDefaultFallbackFont;
        }
    }

    return (ImFont*)ret;
}