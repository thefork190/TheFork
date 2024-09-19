#include "imgui_impl_theforge.h"

struct ImGui_ImplTheForge_Data
{

};


// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
static ImGui_ImplTheForge_Data* ImGui_ImplTheForge_GetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGui_ImplTheForge_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

bool ImGui_TheForge_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGui_ImplTheForge_Data* bd = IM_NEW(ImGui_ImplTheForge_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_theforge";
    //TODO: multi-viewports
    //io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

    return true;
}

void ImGui_TheForge_Shutdown()
{

}

void ImGui_TheForge_NewFrame()
{

}

void ImGui_TheForge_RenderDrawData(ImDrawData* draw_data)
{

}
