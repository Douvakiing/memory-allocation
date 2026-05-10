#include "gui_app.h"

#include "memory_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;

static void DrawMemoryLayout(const MemoryManager& manager) {
    if (!manager.IsInitialized()) {
        ImGui::TextUnformatted("Initialize memory to view the layout.");
        return;
    }

    struct Block {
        string label;
        int start = 0;
        int size = 0;
        ImU32 color = 0;
    };

    vector<Block> blocks;
    for (const auto& seg : manager.AllocatedSegments()) {
        blocks.push_back(
            {seg.processName + ":" + seg.segmentName, seg.start, seg.size, IM_COL32(80, 180, 255, 255)});
    }
    for (const auto& hole : manager.Holes()) {
        blocks.push_back({"Hole", hole.start, hole.size, IM_COL32(120, 220, 120, 255)});
    }

    sort(blocks.begin(), blocks.end(), [](const Block& a, const Block& b) { return a.start < b.start; });

    vector<Block> displayBlocks;
    int cursor = 0;
    for (const auto& block : blocks) {
        if (cursor < block.start) {
            displayBlocks.push_back({"Reserved", cursor, block.start - cursor, IM_COL32(170, 170, 170, 255)});
        }
        displayBlocks.push_back(block);
        cursor = block.start + block.size;
    }
    if (cursor < manager.TotalMemory()) {
        displayBlocks.push_back({"Reserved", cursor, manager.TotalMemory() - cursor, IM_COL32(170, 170, 170, 255)});
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float barHeight = 46.0f;
    drawList->AddRect(origin, ImVec2(origin.x + width, origin.y + barHeight), IM_COL32(255, 255, 255, 255), 0.0f, 0, 2.0f);

    const float total = static_cast<float>(manager.TotalMemory());
    for (const auto& block : displayBlocks) {
        const float x1 = origin.x + (static_cast<float>(block.start) / total) * width;
        const float x2 = origin.x + (static_cast<float>(block.start + block.size) / total) * width;
        drawList->AddRectFilled(ImVec2(x1, origin.y), ImVec2(x2, origin.y + barHeight), block.color);
        drawList->AddRect(ImVec2(x1, origin.y), ImVec2(x2, origin.y + barHeight), IM_COL32(30, 30, 30, 255));

        if ((x2 - x1) > 55.0f) {
            drawList->AddText(ImVec2(x1 + 4.0f, origin.y + 3.0f), IM_COL32(0, 0, 0, 255), block.label.c_str());
        }

        char boundaryLabel[32];
        snprintf(boundaryLabel, sizeof(boundaryLabel), "%d", block.start);
        drawList->AddText(ImVec2(x1 - 2.0f, origin.y + barHeight + 3.0f), IM_COL32(200, 200, 200, 255), boundaryLabel);
    }

    char endLabel[32];
    snprintf(endLabel, sizeof(endLabel), "%d", manager.TotalMemory());
    drawList->AddText(
        ImVec2(origin.x + width - 26.0f, origin.y + barHeight + 3.0f),
        IM_COL32(200, 200, 200, 255),
        endLabel);

    ImGui::Dummy(ImVec2(width, barHeight + 24.0f));
}

int RunMemoryAllocationGuiApp() {
    if (!glfwInit()) {
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1400, 900, "Memory Allocation - Segmentation", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    MemoryManager memoryManager;

    int totalMemoryInput = 1024;
    int holeStartInput = 0;
    int holeSizeInput = 128;
    vector<Hole> setupHoles;

    char processNameBuffer[64] = "P1";
    vector<SegmentInput> pendingSegments = {{"Code", 50}, {"Data", 200}, {"Stack", 100}};
    AllocationMethod selectedMethod = AllocationMethod::FirstFit;
    string statusMessage = "Set total memory and holes, then initialize.";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        const ImGuiWindowFlags rootWindowFlags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        ImGui::Begin("MainRootWindow", nullptr, rootWindowFlags);
        ImGui::TextUnformatted("Memory Allocation (First Fit / Best Fit)");
        ImGui::Separator();

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float memoryMapHeight = 150.0f;
        const float bottomPanelsHeight = max(170.0f, availableHeight * 0.26f);
        const float topPanelsHeight = max(260.0f, availableHeight - memoryMapHeight - bottomPanelsHeight - 12.0f);

        if (ImGui::BeginTable("TopPanels", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("HardwareSettingsPanel", ImVec2(0, topPanelsHeight), true);

            const float leftColumnAvailY = ImGui::GetContentRegionAvail().y;
            const float memorySettingsHeight = 95.0f;
            const float initializeSectionHeight = 54.0f;
            const float holesSectionHeight = max(120.0f, leftColumnAvailY - memorySettingsHeight - initializeSectionHeight - 10.0f);

            ImGui::BeginChild("MemorySettingsSection", ImVec2(0, memorySettingsHeight), true);
            ImGui::TextUnformatted("MEMORY SETTINGS");
            ImGui::InputInt("Total Memory Size", &totalMemoryInput);
            if (totalMemoryInput < 1) {
                totalMemoryInput = 1;
            }
            int methodChoice = static_cast<int>(selectedMethod);
            const char* methods[] = {"First Fit", "Best Fit"};
            if (ImGui::Combo("Allocation Algorithm", &methodChoice, methods, IM_ARRAYSIZE(methods))) {
                selectedMethod = static_cast<AllocationMethod>(methodChoice);
            }
            ImGui::EndChild();

            ImGui::BeginChild("HolesSection", ImVec2(0, holesSectionHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::TextUnformatted("INITIAL HOLES");
            ImGui::TextUnformatted("Start");
            ImGui::SetNextItemWidth(100.0f);
            ImGui::InputInt("##HoleStartAddress", &holeStartInput);
            ImGui::SameLine();
            ImGui::TextUnformatted("Size");
            ImGui::SetNextItemWidth(100.0f);
            ImGui::InputInt("##HoleSize", &holeSizeInput);
            if (holeSizeInput < 1) {
                holeSizeInput = 1;
            }
            if (ImGui::Button("Add Hole")) {
                setupHoles.push_back({holeStartInput, holeSizeInput});
                holeStartInput += holeSizeInput;
                statusMessage = "Hole added to setup list.";
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Hole List")) {
                setupHoles.clear();
                holeStartInput = 0;
                holeSizeInput = 128;
                statusMessage = "Cleared all setup holes.";
            }

            int removeHoleIndex = -1;
            if (ImGui::BeginTable("SetupHoleList", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.0f);
                ImGui::TableSetupColumn("Start");
                ImGui::TableSetupColumn("Size");
                ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableHeadersRow();
                for (int i = 0; i < static_cast<int>(setupHoles.size()); ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", setupHoles[i].start);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", setupHoles[i].size);
                    ImGui::TableSetColumnIndex(3);
                    ImGui::PushID(i + 10000);
                    if (ImGui::Button("Remove")) {
                        removeHoleIndex = i;
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            if (removeHoleIndex >= 0) {
                setupHoles.erase(setupHoles.begin() + removeHoleIndex);
                statusMessage = "Hole removed from setup list.";
            }
            ImGui::EndChild();

            ImGui::BeginChild("InitializeSection", ImVec2(0, initializeSectionHeight), true);
            if (ImGui::Button("Initialize Memory", ImVec2(-1.0f, 0.0f))) {
                memoryManager.Initialize(totalMemoryInput, setupHoles, statusMessage);
            }
            ImGui::EndChild();

            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("ProcessSetupPanel", ImVec2(0, topPanelsHeight), true);
            ImGui::TextUnformatted("PROCESS SETUP");
            ImGui::InputText("Process Name", processNameBuffer, IM_ARRAYSIZE(processNameBuffer));
            ImGui::Text("Segment Num: %d", static_cast<int>(pendingSegments.size()));

            if (ImGui::Button("Add Segment")) {
                pendingSegments.push_back({"Segment", 1});
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Segments")) {
                pendingSegments = {{"Code", 50}, {"Data", 200}, {"Stack", 100}};
                statusMessage = "Reset segment editor.";
            }

            int removeSegmentIndex = -1;
            for (int i = 0; i < static_cast<int>(pendingSegments.size()); ++i) {
                ImGui::PushID(i);
                char segmentNameBuffer[64];
                snprintf(segmentNameBuffer, sizeof(segmentNameBuffer), "%s", pendingSegments[i].name.c_str());
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputText("Segment Name", segmentNameBuffer, IM_ARRAYSIZE(segmentNameBuffer))) {
                    pendingSegments[i].name = segmentNameBuffer;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                ImGui::InputInt("Segment Size", &pendingSegments[i].size);
                if (pendingSegments[i].size < 1) {
                    pendingSegments[i].size = 1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    removeSegmentIndex = i;
                }
                ImGui::PopID();
            }
            if (removeSegmentIndex >= 0) {
                pendingSegments.erase(pendingSegments.begin() + removeSegmentIndex);
            }

            if (ImGui::Button("Allocate Process", ImVec2(-1.0f, 0.0f))) {
                memoryManager.AllocateProcess(processNameBuffer, pendingSegments, selectedMethod, statusMessage);
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }

        ImGui::BeginChild("MemoryMapPanel", ImVec2(0, memoryMapHeight), true);
        ImGui::TextUnformatted("MEMORY MAP");
        DrawMemoryLayout(memoryManager);
        ImGui::EndChild();

        if (ImGui::BeginTable("BottomPanels", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("ActiveProcessesPanel", ImVec2(0, bottomPanelsHeight), true);
            ImGui::TextUnformatted("ACTIVE PROCESSES");
            string processToRemove;
            for (const auto& entry : memoryManager.ProcessTables()) {
                ImGui::TextUnformatted(entry.first.c_str());
                ImGui::SameLine();
                ImGui::PushID(entry.first.c_str());
                if (ImGui::Button("Deallocate")) {
                    processToRemove = entry.first;
                }
                ImGui::PopID();
            }
            if (!processToRemove.empty()) {
                memoryManager.DeallocateProcess(processToRemove, statusMessage);
            }
            if (memoryManager.ProcessTables().empty()) {
                ImGui::TextUnformatted("No allocated processes yet.");
            }
            ImGui::Separator();
            ImGui::TextWrapped("Status: %s", statusMessage.c_str());
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("SegmentTablesPanel", ImVec2(0, bottomPanelsHeight), true);
            ImGui::TextUnformatted("SEGMENT TABLES");
            for (const auto& processEntry : memoryManager.ProcessTables()) {
                if (ImGui::CollapsingHeader(processEntry.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (ImGui::BeginTable(
                            (string("ProcessTable_") + processEntry.first).c_str(),
                            3,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                        ImGui::TableSetupColumn("Segment");
                        ImGui::TableSetupColumn("Limit (Size)");
                        ImGui::TableSetupColumn("Base Address");
                        ImGui::TableHeadersRow();
                        for (const auto& seg : processEntry.second) {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::TextUnformatted(seg.segmentName.c_str());
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text("%d", seg.size);
                            ImGui::TableSetColumnIndex(2);
                            ImGui::Text("%d", seg.start);
                        }
                        ImGui::EndTable();
                    }
                }
            }
            ImGui::EndChild();
            ImGui::EndTable();
        }

        ImGui::End();

        ImGui::Render();
        int displayW = 0;
        int displayH = 0;
        glfwGetFramebufferSize(window, &displayW, &displayH);
        glViewport(0, 0, displayW, displayH);
        glClearColor(0.09f, 0.10f, 0.12f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
