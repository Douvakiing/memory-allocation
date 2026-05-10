#include "gui_app.h"

#include "memory_manager.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Layout tuning (names match UI sections). Edit values to resize panels.
// Top row: left = MEMORY SETTINGS + ADD HOLES, right = PROCESS SETUP.
static constexpr float kMemorySettingsProcessSetupRowHeightRatio = 0.5f;
static constexpr float kMemorySettingsProcessSetupRowMinHeight = 250.0f;
static constexpr float kMemorySettingsProcessSetupRowMaxHeight = 320.0f;

static constexpr float kMemorySettingsSectionHeight = 80.0f;
static constexpr float kAddHolesSectionMinHeight = 140.0f;
static constexpr float kMemorySettingsToAddHolesGap = 10.0f;

// MEMORY MAP (full-width bar below top row).
static constexpr float kMemoryMapSectionHeight = 100.0f;

// Bottom row: ACTIVE PROCESSES | SEGMENT TABLES.
static constexpr float kActiveProcessesSegmentTablesRowMinHeight = 140.0f;
static constexpr float kActiveProcessesSegmentTablesContentMinHeight = 120.0f;

// STATUS line at the very bottom.
static constexpr float kStatusSectionHeight = 40.0f;

// Vertical spacing between major blocks and inside bottom row.
static constexpr float kMainVerticalSpacing = 12.0f;
static constexpr float kBottomRowInnerPadding = 6.0f;

static ImU32 GenerateSequentialProcessColor(int processIndex) {
    const float rawHue = static_cast<float>(processIndex) * 0.61803398875f;
    const float hue = rawHue - static_cast<int>(rawHue);
    return ImGui::ColorConvertFloat4ToU32(ImVec4(ImColor::HSV(hue, 0.78f, 0.95f)));
}

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
    static unordered_map<string, ImU32> processColors;
    static int nextProcessColorIndex = 0;
    for (const auto& seg : manager.AllocatedSegments()) {
        auto [it, inserted] = processColors.emplace(seg.processName, 0);
        if (inserted) {
            it->second = GenerateSequentialProcessColor(nextProcessColorIndex++);
        }
        blocks.push_back(
            {seg.processName + "\n" + seg.segmentName, seg.start, seg.size, it->second});
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

        if ((x2 - x1) > 42.0f) {
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

    char processNameBuffer[64] = "P1";
    vector<SegmentInput> pendingSegments = {{"Code", 50}, {"Data", 200}, {"Stack", 100}};
    AllocationMethod selectedMethod = AllocationMethod::FirstFit;
    string statusMessage = "Set total memory size and initialize memory first.";

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
        const float memoryMapHeight = kMemoryMapSectionHeight;
        const float statusBarHeight = kStatusSectionHeight;
        const float topPanelsHeight = clamp(
            availableHeight * kMemorySettingsProcessSetupRowHeightRatio,
            kMemorySettingsProcessSetupRowMinHeight,
            kMemorySettingsProcessSetupRowMaxHeight);
        const float bottomPanelsHeight = max(
            kActiveProcessesSegmentTablesRowMinHeight,
            availableHeight - topPanelsHeight - memoryMapHeight - kMainVerticalSpacing);
        const float bottomPanelsContentHeight = max(
            kActiveProcessesSegmentTablesContentMinHeight,
            bottomPanelsHeight - statusBarHeight - kBottomRowInnerPadding);

        if (ImGui::BeginTable("TopPanels", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV)) {
            ImGui::TableNextColumn();
            ImGui::BeginChild("HardwareSettingsPanel", ImVec2(0, topPanelsHeight), true);

            const float leftColumnAvailY = ImGui::GetContentRegionAvail().y;
            const float memorySettingsHeight = kMemorySettingsSectionHeight;
            const float holesSectionHeight =
                max(kAddHolesSectionMinHeight, leftColumnAvailY - memorySettingsHeight - kMemorySettingsToAddHolesGap);

            ImGui::BeginChild("MemorySettingsSection", ImVec2(0, memorySettingsHeight), true);
            ImGui::TextUnformatted("MEMORY SETTINGS");
            ImGui::InputInt("Total Memory Size", &totalMemoryInput);
            if (totalMemoryInput < 1) {
                totalMemoryInput = 1;
            }
            if (ImGui::Button("Initialize Memory", ImVec2(-1.0f, 0.0f))) {
                memoryManager.Initialize(totalMemoryInput, statusMessage);
            }
            ImGui::EndChild();

            ImGui::BeginChild("HolesSection", ImVec2(0, holesSectionHeight), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::TextUnformatted("ADD HOLES");
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Start");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("##HoleStartAddress", &holeStartInput);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Size");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("##HoleSize", &holeSizeInput);
            if (holeSizeInput < 1) {
                holeSizeInput = 1;
            }
            if (ImGui::Button("Add Hole")) {
                if (memoryManager.AddHole(holeStartInput, holeSizeInput, statusMessage)) {
                    holeStartInput += holeSizeInput;
                }
            }

            if (ImGui::BeginTable("SetupHoleList", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24.0f);
                ImGui::TableSetupColumn("Start");
                ImGui::TableSetupColumn("Size");
                ImGui::TableHeadersRow();
                const auto& holes = memoryManager.Holes();
                for (int i = 0; i < static_cast<int>(holes.size()); ++i) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%d", i + 1);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%d", holes[i].start);
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%d", holes[i].size);
                }
                ImGui::EndTable();
            }
            if (!memoryManager.IsInitialized()) {
                ImGui::TextUnformatted("Initialize memory before adding holes.");
            } else if (memoryManager.Holes().empty()) {
                ImGui::TextUnformatted("No holes added yet.");
            }
            ImGui::EndChild();

            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("ProcessSetupPanel", ImVec2(0, topPanelsHeight), true);
            ImGui::TextUnformatted("PROCESS SETUP");
            int methodChoice = static_cast<int>(selectedMethod);
            const char* methods[] = {"First Fit", "Best Fit"};
            if (ImGui::Combo("Allocation Algorithm", &methodChoice, methods, IM_ARRAYSIZE(methods))) {
                selectedMethod = static_cast<AllocationMethod>(methodChoice);
            }
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
            ImGui::BeginChild("ActiveProcessesPanel", ImVec2(0, bottomPanelsContentHeight), true);
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
            ImGui::EndChild();

            ImGui::TableNextColumn();
            ImGui::BeginChild("SegmentTablesPanel", ImVec2(0, bottomPanelsContentHeight), true);
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
        ImGui::BeginChild("BottomStatusBar", ImVec2(0, statusBarHeight), true);
        ImGui::TextWrapped("Status: %s", statusMessage.c_str());
        ImGui::EndChild();

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
