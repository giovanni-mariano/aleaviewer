#include "app.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <map>
#include <set>

// ---------------------------------------------------------------------------
// Tab 1: Universes & Cells (hierarchical tree)
// ---------------------------------------------------------------------------

static char cell_filter[128] = {};

static void tab_universes(AppState& app) {
    // Search filter
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##cellfilter", "Filter cells...", cell_filter, sizeof(cell_filter));
    ImGui::Spacing();

    ImGui::BeginChild("UniverseTree", ImVec2(0, 0), ImGuiChildFlags_None);

    size_t nu = alea_universe_count(app.sys);
    for (size_t ui = 0; ui < nu; ui++) {
        int uid = 0;
        size_t ucell_count = 0;
        alea_bbox_t ubbox;
        alea_universe_get(app.sys, ui, &uid, &ucell_count, &ubbox);

        char ulabel[64];
        snprintf(ulabel, sizeof(ulabel), "Universe %d (%zu cells)", uid, ucell_count);

        // If filter is active, check if any cell in universe matches
        bool has_match = (cell_filter[0] == '\0');
        if (!has_match && ucell_count > 0) {
            // Get cells in this universe
            std::vector<int> indices((int)ucell_count);
            int got = alea_cells_in_universe(app.sys, uid, indices.data(), ucell_count);
            for (int ci = 0; ci < got && !has_match; ci++) {
                alea_cell_info_t info;
                if (alea_cell_get_info(app.sys, (size_t)indices[ci], &info) == 0) {
                    char tmp[128];
                    snprintf(tmp, sizeof(tmp), "Cell %d (M%d)", info.cell_id, info.material_id);
                    if (strstr(tmp, cell_filter)) has_match = true;
                }
            }
        }
        if (!has_match) continue;

        ImGui::PushID((int)ui);
        bool open = ImGui::TreeNodeEx(ulabel, (nu == 1) ? ImGuiTreeNodeFlags_DefaultOpen : 0);

        if (open) {
            // Get cells in this universe
            std::vector<int> indices(ucell_count > 0 ? ucell_count : 1);
            int got = alea_cells_in_universe(app.sys, uid, indices.data(), ucell_count);

            for (int ci = 0; ci < got; ci++) {
                int cell_idx = indices[ci];
                alea_cell_info_t info;
                if (alea_cell_get_info(app.sys, (size_t)cell_idx, &info) != 0)
                    continue;

                // Apply filter
                if (cell_filter[0] != '\0') {
                    char tmp[128];
                    snprintf(tmp, sizeof(tmp), "Cell %d (M%d)", info.cell_id, info.material_id);
                    if (!strstr(tmp, cell_filter)) continue;
                }

                ImGui::PushID(cell_idx);
                bool selected = (cell_idx == app.selected_cell_index);

                // Color swatch
                uint8_t col[3];
                color_for_id(info.cell_id, col);
                ImVec4 cv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, 1.0f);
                ImGui::ColorButton("##c", cv, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
                ImGui::SameLine();

                char label[128];
                if (info.fill_universe >= 0) {
                    snprintf(label, sizeof(label), "Cell %d (M%d) fill=%d",
                             info.cell_id, info.material_id, info.fill_universe);
                } else {
                    snprintf(label, sizeof(label), "Cell %d (M%d)",
                             info.cell_id, info.material_id);
                }

                if (ImGui::Selectable(label, selected)) {
                    app.selected_cell_index = cell_idx;
                    app.selected_surface_index = -1;
                }

                ImGui::PopID();
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Tab 2: Surfaces
// ---------------------------------------------------------------------------

static const char* surface_type_names[] = {
    "All", "Plane", "Sphere", "Cylinder", "Cone", "Box", "Quadric", "Torus", "Macrobody"
};

static int surf_type_filter = 0;

static bool matches_type_filter(alea_primitive_type_t ptype, int filter) {
    if (filter == 0) return true;
    switch (filter) {
        case 1: return ptype == ALEA_PRIMITIVE_PLANE;
        case 2: return ptype == ALEA_PRIMITIVE_SPHERE || ptype == ALEA_PRIMITIVE_SPH;
        case 3: return ptype == ALEA_PRIMITIVE_CYLINDER_X || ptype == ALEA_PRIMITIVE_CYLINDER_Y ||
                       ptype == ALEA_PRIMITIVE_CYLINDER_Z;
        case 4: return ptype == ALEA_PRIMITIVE_CONE_X || ptype == ALEA_PRIMITIVE_CONE_Y ||
                       ptype == ALEA_PRIMITIVE_CONE_Z;
        case 5: return ptype == ALEA_PRIMITIVE_RPP;
        case 6: return ptype == ALEA_PRIMITIVE_QUADRIC;
        case 7: return ptype == ALEA_PRIMITIVE_TORUS_X || ptype == ALEA_PRIMITIVE_TORUS_Y ||
                       ptype == ALEA_PRIMITIVE_TORUS_Z;
        case 8: return ptype == ALEA_PRIMITIVE_RCC || ptype == ALEA_PRIMITIVE_BOX ||
                       ptype == ALEA_PRIMITIVE_TRC || ptype == ALEA_PRIMITIVE_ELL ||
                       ptype == ALEA_PRIMITIVE_REC || ptype == ALEA_PRIMITIVE_WED ||
                       ptype == ALEA_PRIMITIVE_RHP || ptype == ALEA_PRIMITIVE_ARB;
        default: return true;
    }
}

static const char* short_type_name(alea_primitive_type_t t) {
    switch (t) {
        case ALEA_PRIMITIVE_PLANE:       return "P";
        case ALEA_PRIMITIVE_SPHERE:      return "S";
        case ALEA_PRIMITIVE_CYLINDER_X:  return "CX";
        case ALEA_PRIMITIVE_CYLINDER_Y:  return "CY";
        case ALEA_PRIMITIVE_CYLINDER_Z:  return "CZ";
        case ALEA_PRIMITIVE_CONE_X:      return "KX";
        case ALEA_PRIMITIVE_CONE_Y:      return "KY";
        case ALEA_PRIMITIVE_CONE_Z:      return "KZ";
        case ALEA_PRIMITIVE_RPP:         return "RPP";
        case ALEA_PRIMITIVE_QUADRIC:     return "GQ";
        case ALEA_PRIMITIVE_TORUS_X:     return "TX";
        case ALEA_PRIMITIVE_TORUS_Y:     return "TY";
        case ALEA_PRIMITIVE_TORUS_Z:     return "TZ";
        case ALEA_PRIMITIVE_RCC:         return "RCC";
        case ALEA_PRIMITIVE_BOX: return "BOX";
        case ALEA_PRIMITIVE_SPH:         return "SPH";
        case ALEA_PRIMITIVE_TRC:         return "TRC";
        case ALEA_PRIMITIVE_ELL:         return "ELL";
        case ALEA_PRIMITIVE_REC:         return "REC";
        case ALEA_PRIMITIVE_WED:         return "WED";
        case ALEA_PRIMITIVE_RHP:         return "RHP";
        case ALEA_PRIMITIVE_ARB:         return "ARB";
        default:                        return "?";
    }
}

static void tab_surfaces(AppState& app) {
    ImGui::SetNextItemWidth(120);
    ImGui::Combo("Type", &surf_type_filter, surface_type_names, 9);
    ImGui::Spacing();

    ImGui::BeginChild("SurfList", ImVec2(0, 0), ImGuiChildFlags_None);

    int ns = (int)alea_surface_count(app.sys);

    if (surf_type_filter == 0) {
        // No filter: use clipper for performance
        ImGuiListClipper clipper;
        clipper.Begin(ns);
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                int sid = 0;
                alea_primitive_type_t ptype = (alea_primitive_type_t)0;
                alea_surface_get(app.sys, (size_t)i, &sid, &ptype, nullptr, nullptr, nullptr);

                ImGui::PushID(i);
                char label[64];
                snprintf(label, sizeof(label), "S%d: %s", sid, short_type_name(ptype));
                if (ImGui::Selectable(label, i == app.selected_surface_index)) {
                    app.selected_surface_index = i;
                    app.selected_cell_index = -1;
                }
                ImGui::PopID();
            }
        }
    } else {
        // Filtered: iterate without clipper (can't skip items inside clipper)
        for (int i = 0; i < ns; i++) {
            int sid = 0;
            alea_primitive_type_t ptype = (alea_primitive_type_t)0;
            alea_surface_get(app.sys, (size_t)i, &sid, &ptype, nullptr, nullptr, nullptr);

            if (!matches_type_filter(ptype, surf_type_filter))
                continue;

            ImGui::PushID(i);
            char label[64];
            snprintf(label, sizeof(label), "S%d: %s", sid, short_type_name(ptype));
            if (ImGui::Selectable(label, i == app.selected_surface_index)) {
                app.selected_surface_index = i;
                app.selected_cell_index = -1;
            }
            ImGui::PopID();
        }
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Tab 3: Materials
// ---------------------------------------------------------------------------

struct MaterialInfo {
    int material_id;
    int cell_count;
    double min_density;
    double max_density;
    std::vector<int> cell_indices;
};

static void tab_materials(AppState& app) {
    // Collect material info (cached per frame is fine since ImGui is immediate)
    std::map<int, MaterialInfo> mat_map;
    int nc = (int)alea_cell_count(app.sys);
    for (int i = 0; i < nc; i++) {
        alea_cell_info_t info;
        if (alea_cell_get_info(app.sys, (size_t)i, &info) != 0) continue;
        int mid = info.material_id;
        auto& m = mat_map[mid];
        if (m.cell_count == 0) {
            m.material_id = mid;
            m.min_density = info.density;
            m.max_density = info.density;
        } else {
            if (info.density < m.min_density) m.min_density = info.density;
            if (info.density > m.max_density) m.max_density = info.density;
        }
        m.cell_count++;
        m.cell_indices.push_back(i);
    }

    // Show All / Hide All buttons
    if (ImGui::Button("Show All")) {
        app.hidden_materials.clear();
        app.needs_recolor = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Hide All")) {
        for (auto& [mid, minfo] : mat_map)
            app.hidden_materials.insert(mid);
        app.needs_recolor = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Invert")) {
        std::set<int> inverted;
        for (auto& [mid, minfo] : mat_map) {
            if (!app.hidden_materials.count(mid))
                inverted.insert(mid);
        }
        app.hidden_materials = std::move(inverted);
        app.needs_recolor = true;
    }
    ImGui::Spacing();

    ImGui::BeginChild("MatList", ImVec2(0, 0), ImGuiChildFlags_None);

    for (auto& [mid, minfo] : mat_map) {
        ImGui::PushID(mid);

        // Visibility toggle circle
        bool visible = (app.hidden_materials.count(mid) == 0);
        ImVec2 cpos = ImGui::GetCursorScreenPos();
        float radius = 5.0f;
        ImVec2 center(cpos.x + radius + 2, cpos.y + ImGui::GetFrameHeight() * 0.5f);
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Invisible button for click target
        ImGui::InvisibleButton("##vis", ImVec2(radius * 2 + 4, ImGui::GetFrameHeight()));
        if (ImGui::IsItemClicked()) {
            if (visible)
                app.hidden_materials.insert(mid);
            else
                app.hidden_materials.erase(mid);
            app.needs_recolor = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(visible ? "Hide material" : "Show material");

        if (visible) {
            dl->AddCircleFilled(center, radius, IM_COL32(46, 150, 173, 255));  // teal
        } else {
            dl->AddCircle(center, radius, IM_COL32(80, 84, 94, 255), 0, 1.5f);  // dim gray ring
        }
        ImGui::SameLine();

        // Color swatch using material id
        uint8_t col[3];
        color_for_id(mid, col);
        ImVec4 cv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, 1.0f);
        ImGui::ColorButton("##m", cv, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
        ImGui::SameLine();

        char label[128];
        if (mid == 0) {
            snprintf(label, sizeof(label), "Void  %d cells", minfo.cell_count);
        } else if (minfo.min_density == minfo.max_density) {
            snprintf(label, sizeof(label), "M%d  %d cells  d=%.4g g/cc",
                     mid, minfo.cell_count, minfo.min_density);
        } else {
            snprintf(label, sizeof(label), "M%d  %d cells  d=[%.4g..%.4g] g/cc",
                     mid, minfo.cell_count, minfo.min_density, minfo.max_density);
        }

        bool open = ImGui::TreeNode(label);
        if (open) {
            for (int ci : minfo.cell_indices) {
                alea_cell_info_t info;
                if (alea_cell_get_info(app.sys, (size_t)ci, &info) != 0) continue;
                ImGui::PushID(ci);
                bool selected = (ci == app.selected_cell_index);
                char clabel[64];
                snprintf(clabel, sizeof(clabel), "Cell %d  u=%d", info.cell_id, info.universe_id);
                if (ImGui::Selectable(clabel, selected)) {
                    app.selected_cell_index = ci;
                    app.selected_surface_index = -1;
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// Tree panel (tabbed browser)
// ---------------------------------------------------------------------------

void panel_tree(AppState& app) {
    ImGui::Begin("Browser");

    if (!app.sys) {
        ImGui::TextDisabled("No geometry loaded.");
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("BrowserTabs")) {
        if (ImGui::BeginTabItem("Cells")) {
            tab_universes(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Surfaces")) {
            tab_surfaces(app);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Materials")) {
            tab_materials(app);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
