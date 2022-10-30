#include "HmlImgui.h"


std::unique_ptr<HmlImgui> HmlImgui::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlResourceManager> hmlResourceManager) noexcept {
    auto hmlImgui = std::make_unique<HmlImgui>();
    hmlImgui->hmlWindow = hmlWindow;
    hmlImgui->hmlResourceManager = hmlResourceManager;

    hmlImgui->vertexBuffer = std::make_shared<std::unique_ptr<HmlBuffer>>(std::unique_ptr<HmlBuffer>());
    hmlImgui->indexBuffer  = std::make_shared<std::unique_ptr<HmlBuffer>>(std::unique_ptr<HmlBuffer>());

    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = 1.0f;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);

    // Color scheme
    // ImGuiStyle& style = ImGui::GetStyle();
    // style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    // style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    // style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    // style.Colors[ImGuiCol_Header] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    // style.Colors[ImGuiCol_CheckMark] = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
    // Dimensions
    const auto [width, height] = hmlWindow->getFramebufferSize();
    io.DisplaySize = ImVec2(width, height);
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    ImGui_ImplGlfw_InitForVulkan(hmlWindow->window, true);

    return hmlImgui;
};


HmlImgui::~HmlImgui() {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlImgui...\n";
#endif

    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}


void HmlImgui::updateForDt(float dt) noexcept {
    ImGui::GetIO().DeltaTime = dt;
}


void HmlImgui::beginFrame() noexcept {
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}


void HmlImgui::finilize() noexcept {
    // ================ Construct render data ================
    ImGui::Render();
    // ================ Update vertex and index buffers ================
    // Note: Alignment is done inside buffer creation
    ImDrawData* imDrawData = ImGui::GetDrawData();
    VkDeviceSize newVertexBufferSizeBytes = imDrawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize newIndexBufferSizeBytes  = imDrawData->TotalIdxCount * sizeof(ImDrawIdx);

    if ((newVertexBufferSizeBytes == 0) || (newIndexBufferSizeBytes == 0)) return;

    // Collect vertex and index data bytes in a contiguous array
    std::vector<ImDrawVert> vertexData(imDrawData->TotalVtxCount);
    std::vector<ImDrawIdx>   indexData(imDrawData->TotalIdxCount);
    ImDrawVert* vtxDst = (ImDrawVert*)vertexData.data();
    ImDrawIdx*  idxDst = (ImDrawIdx*)  indexData.data();
    for (int i = 0; i < imDrawData->CmdListsCount; i++) {
        const ImDrawList* cmd_list = imDrawData->CmdLists[i];
        memcpy(vtxDst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmd_list->VtxBuffer.Size;
        idxDst += cmd_list->IdxBuffer.Size;
    }

    // Vertex buffer
    if (true) {
        auto newVertexBuffer = hmlResourceManager->createVertexBufferWithData(vertexData.data(), newVertexBufferSizeBytes);
        // NOTE we write "(*a)." instead of "a->" explicitly in order to highlihght
        // that we call swap on the inner object
        (*vertexBuffer).swap(newVertexBuffer);
    }

    // Index buffer
    if (true) {
        auto newIndexBuffer = hmlResourceManager->createIndexBufferWithData(indexData.data(), newIndexBufferSizeBytes);
        // NOTE see comment above about vertexBuffer
        (*indexBuffer).swap(newIndexBuffer);
    }
}
