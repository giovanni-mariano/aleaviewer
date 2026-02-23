#include "app.h"
#include "imgui.h"
#include <cstdio>

// Thin vertical divider drawn via draw list
static void status_divider() {
    ImGui::SameLine(0, 12);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetFrameHeight();
    dl->AddLine(ImVec2(p.x, p.y + 2), ImVec2(p.x, p.y + h - 2),
                IM_COL32(78, 201, 212, 80), 1.0f);
    ImGui::Dummy(ImVec2(1, h));
    ImGui::SameLine(0, 12);
}

void panel_status(AppState& app) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float bar_h = ImGui::GetFrameHeight() + 8.0f;  // padding (12,4) -> 4*2

    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x,
                                   viewport->WorkPos.y + viewport->WorkSize.y - bar_h));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, bar_h));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 4));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.110f, 0.310f, 0.380f, 1.0f));  // teal-tinted

    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Axis labels
        const char* u_label = "?";
        const char* v_label = "?";
        const char* n_label = "?";
        switch (app.axis) {
            case SLICE_Z: u_label = "X"; v_label = "Y"; n_label = "Z"; break;
            case SLICE_X: u_label = "Y"; v_label = "Z"; n_label = "X"; break;
            case SLICE_Y: u_label = "X"; v_label = "Z"; n_label = "Y"; break;
        }

        if (app.cursor_in_viewport) {
            if (app.arbitrary_slice) {
                ImGui::Text("U=%.3f  V=%.3f", app.cursor_u, app.cursor_v);
            } else {
                ImGui::Text("%s=%.3f  %s=%.3f  %s=%.3f",
                            u_label, app.cursor_u, v_label, app.cursor_v,
                            n_label, app.slice_value);
            }

            status_divider();

            if (app.cursor_cell > 0) {
                ImGui::Text("Cell %d  Mat %d", app.cursor_cell, app.cursor_mat);
            } else {
                ImGui::TextDisabled("No cell");
            }

            status_divider();

            ImGui::Text("%.1fx", 100.0 / app.view_half_ext);
        } else {
            if (app.arbitrary_slice) {
                ImGui::TextDisabled("Arbitrary plane");
                status_divider();
                ImGui::TextDisabled("%.1fx", 100.0 / app.view_half_ext);
            } else {
                const char* plane_label = "XY";
                if (app.axis == SLICE_X) plane_label = "YZ";
                else if (app.axis == SLICE_Y) plane_label = "XZ";
                ImGui::TextDisabled("%s  %s=%.3f", plane_label, n_label, app.slice_value);
                status_divider();
                ImGui::TextDisabled("%.1fx", 100.0 / app.view_half_ext);
            }
        }

        // Right-aligned info
        if (app.sys) {
            char info[128];
            snprintf(info, sizeof(info), "%s  |  Cells: %d  Surfaces: %d",
                     app.loaded_file.c_str(),
                     (int)alea_cell_count(app.sys),
                     (int)alea_surface_count(app.sys));
            float text_w = ImGui::CalcTextSize(info).x;
            ImGui::SameLine(ImGui::GetWindowWidth() - text_w - 14);
            ImGui::TextDisabled("%s", info);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}
