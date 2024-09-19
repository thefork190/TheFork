#pragma once

// Imgui integration using The Forge RHI
// The vanilla TF integration of "IUI.h" was used as reference
// The layout of the code here pretty much follow the imgui backends


#include "imgui.h"      // IMGUI_IMPL_API

struct Renderer;
struct PipelineCache;

struct ImGui_ImplTheForge_InitDesc
{
	Renderer* pRenderer = nullptr;
	PipelineCache* pCache = nullptr;

	uint32_t mMaxDynamicUIUpdatesPerBatch = 32u;
	uint32_t mMaxUIFonts = 32u;
	uint32_t mFrameCount = 2u;

	uint32_t mMaxVerts = 64u * 1024u;
	uint32_t mMaxInds = 128u * 1024u;
};

IMGUI_IMPL_API bool     ImGui_TheForge_Init(ImGui_ImplTheForge_InitDesc const& initDesc);
IMGUI_IMPL_API void     ImGui_TheForge_Shutdown();
IMGUI_IMPL_API void     ImGui_TheForge_NewFrame();
IMGUI_IMPL_API void     ImGui_TheForge_RenderDrawData(ImDrawData* drawData);


IMGUI_IMPL_API ImFont*  ImGui_TheForge_GetOrAddFont(uint32_t const fontId, float const size);