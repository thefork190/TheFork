#include <tinyimageformat/tinyimageformat_query.h>
#include <IGraphics.h>
#include <IFont.h>
#include <IResourceLoader.h>
#include <ILog.h>

#include "imgui_impl_theforge.h"

#define MAX_FRAMES 3u

struct ImGui_ImplTheForge_Data
{
    uint32_t mMaxDynamicUIUpdatesPerBatch = 20u;
    uint32_t mMaxUIFonts = 10u;
    uint32_t mFrameCount = 2u;

    Renderer* pRenderer = nullptr;
    PipelineCache* pCache = nullptr;
    uint32_t  mFrameIdx = 0;

    struct TextureNode
    {
        uint64_t key = ~0ull;
        Texture* value = nullptr;
    }*pTextureHashmap = nullptr;
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
};


// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplTheForge_Data* ImGui_ImplTheForge_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplTheForge_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
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
    ImGui_ImplTheForge_Data* bd = IM_NEW(ImGui_ImplTheForge_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_theforge";
    //TODO: multi-viewports
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    if (!initDesc.pRenderer)
    {
        IM_ASSERT(false && "TF renderer is invalid.");
        return false;
    }

    bd->pRenderer = initDesc.pRenderer;
    bd->pCache = initDesc.pCache;
    bd->mMaxDynamicUIUpdatesPerBatch = initDesc.mMaxDynamicUIUpdatesPerBatch;
    bd->mMaxUIFonts = initDesc.mMaxUIFonts + 1; // +1 to account for a default fallback font
    bd->mFrameCount = initDesc.mFrameCount;
    ASSERT(bd->mFrameCount <= MAX_FRAMES);

    SamplerDesc samplerDesc = { FILTER_LINEAR,
                                FILTER_LINEAR,
                                MIPMAP_MODE_NEAREST,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE,
                                ADDRESS_MODE_CLAMP_TO_EDGE };
    addSampler(bd->pRenderer, &samplerDesc, &bd->pDefaultSampler);

    uint64_t const VERTEX_BUFFER_SIZE = initDesc.mMaxVerts * sizeof(ImDrawVert);
    uint64_t const INDEX_BUFFER_SIZE = initDesc.mMaxInds * sizeof(ImDrawIdx);

    BufferLoadDesc vbDesc = {};
    vbDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.mDesc.mSize = VERTEX_BUFFER_SIZE * bd->mFrameCount;
    vbDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vbDesc.mDesc.pName = "UI Vertex Buffer";
    vbDesc.ppBuffer = &bd->pVertexBuffer;
    addResource(&vbDesc, nullptr);

    BufferLoadDesc ibDesc = vbDesc;
    ibDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.mDesc.mSize = INDEX_BUFFER_SIZE * bd->mFrameCount;
    vbDesc.mDesc.pName = "UI Index Buffer";
    ibDesc.ppBuffer = &bd->pIndexBuffer;
    addResource(&ibDesc, nullptr);

    BufferLoadDesc ubDesc = {};
    ubDesc.mDesc.mDescriptors = DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubDesc.mDesc.mMemoryUsage = RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubDesc.mDesc.mFlags = BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    ubDesc.mDesc.mSize = sizeof(float) * 16;
    vbDesc.mDesc.pName = "UI Uniform Buffer";
    for (uint32_t i = 0; i < bd->mFrameCount; ++i)
    {
        ubDesc.ppBuffer = &bd->pUniformBuffer[i];
        addResource(&ubDesc, nullptr);
    }

    VertexLayout* vertexLayout = &bd->mVertexLayoutTextured;
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
    vertexLayout->mAttribs[1].mOffset = TinyImageFormat_BitSizeOfBlock(bd->mVertexLayoutTextured.mAttribs[0].mFormat) / 8;
    vertexLayout->mAttribs[2].mSemantic = SEMANTIC_COLOR;
    vertexLayout->mAttribs[2].mFormat = TinyImageFormat_R8G8B8A8_UNORM;
    vertexLayout->mAttribs[2].mBinding = 0;
    vertexLayout->mAttribs[2].mLocation = 2;
    vertexLayout->mAttribs[2].mOffset =
    vertexLayout->mAttribs[1].mOffset + TinyImageFormat_BitSizeOfBlock(bd->mVertexLayoutTextured.mAttribs[1].mFormat) / 8;

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
}

void ImGui_TheForge_RenderDrawData(ImDrawData* draw_data)
{

}
