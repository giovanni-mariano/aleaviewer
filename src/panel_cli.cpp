#include "app.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>

// ---------------------------------------------------------------------------
// Input callback for history navigation
// ---------------------------------------------------------------------------

static int input_callback(ImGuiInputTextCallbackData* data) {
    AppState* app = (AppState*)data->UserData;

    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
        int hist_size = (int)app->cmd_history.size();
        if (hist_size == 0) return 0;

        if (data->EventKey == ImGuiKey_UpArrow) {
            if (app->history_pos < 0)
                app->history_pos = hist_size - 1;
            else if (app->history_pos > 0)
                app->history_pos--;
        } else if (data->EventKey == ImGuiKey_DownArrow) {
            if (app->history_pos >= 0) {
                app->history_pos++;
                if (app->history_pos >= hist_size)
                    app->history_pos = -1;
            }
        }

        if (app->history_pos >= 0 && app->history_pos < hist_size) {
            const std::string& h = app->cmd_history[app->history_pos];
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, h.c_str());
        } else {
            data->DeleteChars(0, data->BufTextLen);
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Log tab
// ---------------------------------------------------------------------------

static void tab_log(AppState& app) {
    // Log area (scrollable)
    float input_height = ImGui::GetFrameHeightWithSpacing() * 2.0f;
    ImGui::BeginChild("LogRegion", ImVec2(0, -input_height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    for (auto& line : app.log_lines) {
        if (line.size() > 7 && line.substr(0, 7) == "[ERROR]") {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.400f, 0.380f, 1.0f));  // #FF6661
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        } else if (line.size() > 1 && line[0] == '>') {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.506f, 0.745f, 0.906f, 1.0f));  // #81BEE7
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.741f, 0.753f, 0.784f, 1.0f));  // #BDC0C8
            ImGui::TextUnformatted(line.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (app.scroll_to_bottom) {
        ImGui::SetScrollHereY(1.0f);
        app.scroll_to_bottom = false;
    }

    ImGui::EndChild();

    // Input line
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.180f, 0.588f, 0.678f, 1.0f));
    ImGui::Text("\xC2\xBB");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                      ImGuiInputTextFlags_CallbackHistory |
                                      ImGuiInputTextFlags_CallbackAlways;

    static bool reclaim_focus = false;
    if (ImGui::InputText("##Input", app.input_buf, sizeof(app.input_buf),
                         input_flags, input_callback, &app)) {
        char* s = app.input_buf;
        while (*s == ' ') s++;

        if (*s) {
            app.log_lines.push_back(std::string("> ") + s);
            app.cmd_history.push_back(s);
            app.history_pos = -1;
            commands_execute(app, s);
            app.scroll_to_bottom = true;
        }
        app.input_buf[0] = '\0';
        reclaim_focus = true;
    }

    // Give focus on first frame or after pressing Enter
    static bool first = true;
    if (reclaim_focus || first) {
        ImGui::SetKeyboardFocusHere(-1);
        reclaim_focus = false;
        first = false;
    }
}

// ---------------------------------------------------------------------------
// Query Results tab
// ---------------------------------------------------------------------------

static void tab_query_results(AppState& app) {
    if (app.query_results.empty()) {
        ImGui::TextDisabled("No query results. Right-click the slice viewport to query a point.");
        return;
    }

    if (ImGui::Button("Clear")) {
        app.query_results.clear();
        return;
    }

    ImGui::BeginChild("QueryResults", ImVec2(0, 0), ImGuiChildFlags_None);

    for (int qi = (int)app.query_results.size() - 1; qi >= 0; qi--) {
        auto& qr = app.query_results[qi];

        ImGui::PushID(qi);
        char header[128];
        snprintf(header, sizeof(header), "Query #%d: (%.4f, %.4f, %.4f) - %d hits",
                 qi + 1, qr.x, qr.y, qr.z, (int)qr.hits.size());

        if (ImGui::TreeNodeEx(header, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::BeginTable("##qhits", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("Depth");
                ImGui::TableSetupColumn("Cell");
                ImGui::TableSetupColumn("Material");
                ImGui::TableSetupColumn("Universe");
                ImGui::TableSetupColumn("Local Coords");
                ImGui::TableHeadersRow();

                for (auto& hit : qr.hits) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", hit.depth);
                    ImGui::TableNextColumn();

                    // Clickable cell ID
                    char cellbuf[32];
                    snprintf(cellbuf, sizeof(cellbuf), "%d", hit.cell_id);
                    if (ImGui::Selectable(cellbuf, false, ImGuiSelectableFlags_SpanAllColumns)) {
                        if (app.sys) {
                            int idx = alea_cell_find(app.sys, hit.cell_id);
                            if (idx >= 0) {
                                app.selected_cell_index = idx;
                                app.selected_surface_index = -1;
                            }
                        }
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", hit.material_id);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", hit.universe_id);
                    ImGui::TableNextColumn();
                    ImGui::Text("(%.4f, %.4f, %.4f)", hit.local_x, hit.local_y, hit.local_z);
                }

                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Console panel (tabbed)
// ---------------------------------------------------------------------------

void panel_cli(AppState& app) {
    ImGui::Begin("Console");

    if (ImGui::BeginTabBar("ConsoleTabs")) {
        if (ImGui::BeginTabItem("Log")) {
            app.bottom_tab = 0;
            tab_log(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Query Results")) {
            app.bottom_tab = 1;
            tab_query_results(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
