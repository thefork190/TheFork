#pragma once

// Imgui integration using The Forge RHI
// The vanilla TF integration of "IUI.h" was used as reference
// The layout of the code here pretty much follow the imgui backends


#include "imgui.h"      // IMGUI_IMPL_API

IMGUI_IMPL_API bool     ImGui_TheForge_Init();
IMGUI_IMPL_API void     ImGui_TheForge_Shutdown();
IMGUI_IMPL_API void     ImGui_TheForge_NewFrame();
IMGUI_IMPL_API void     ImGui_TheForge_RenderDrawData(ImDrawData* draw_data);
