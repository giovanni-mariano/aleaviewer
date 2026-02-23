#include "app.h"
#include "imgui.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// Primitive type name
// ---------------------------------------------------------------------------

static const char* prim_type_name(alea_primitive_type_t type) {
    switch (type) {
        case ALEA_PRIMITIVE_PLANE:       return "PX/PY/PZ/P";
        case ALEA_PRIMITIVE_SPHERE:      return "SO/S/SX/SY/SZ";
        case ALEA_PRIMITIVE_CYLINDER_X:  return "CX/C/X";
        case ALEA_PRIMITIVE_CYLINDER_Y:  return "CY/C/Y";
        case ALEA_PRIMITIVE_CYLINDER_Z:  return "CZ/C/Z";
        case ALEA_PRIMITIVE_CONE_X:      return "KX";
        case ALEA_PRIMITIVE_CONE_Y:      return "KY";
        case ALEA_PRIMITIVE_CONE_Z:      return "KZ";
        case ALEA_PRIMITIVE_RPP:         return "RPP";
        case ALEA_PRIMITIVE_QUADRIC:     return "GQ/SQ";
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
        default:                        return "???";
    }
}

static const char* op_name(alea_operation_t op) {
    switch (op) {
        case ALEA_OP_UNION:        return "UNION";
        case ALEA_OP_INTERSECTION: return "INTER";
        case ALEA_OP_DIFFERENCE:   return "DIFF";
        case ALEA_OP_COMPLEMENT:   return "COMPL";
        default:                  return "???";
    }
}

// ---------------------------------------------------------------------------
// Recursive CSG node rendering
// ---------------------------------------------------------------------------

static void render_csg_node(AppState& app, alea_node_id_t node, int depth) {
    if (depth > 50) return;

    alea_operation_t op = alea_node_operation(app.sys, node);

    if (op == ALEA_OP_PRIMITIVE) {
        int sense = alea_node_sense(app.sys, node);
        int surf_id = alea_node_surface_id(app.sys, node);
        alea_primitive_type_t ptype = alea_node_primitive_type(app.sys, node);

        char label[128];
        snprintf(label, sizeof(label), "%cS%d (%s)",
                 sense > 0 ? '+' : '-', surf_id, prim_type_name(ptype));

        bool highlight = (surf_id == app.hovered_surface_id);
        if (highlight) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.306f, 0.788f, 0.831f, 1.0f));

        ImGui::TreeNodeEx((void*)(intptr_t)node,
                         ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen,
                         "%s", label);

        if (ImGui::IsItemHovered()) {
            app.hovered_surface_id = surf_id;
        }

        if (highlight) ImGui::PopStyleColor();
        return;
    }

    bool open = ImGui::TreeNodeEx((void*)(intptr_t)node, 0, "%s", op_name(op));
    if (open) {
        alea_node_id_t left = alea_node_left(app.sys, node);
        if (left != (alea_node_id_t)-1)
            render_csg_node(app, left, depth+1);

        if (op != ALEA_OP_COMPLEMENT) {
            alea_node_id_t right = alea_node_right(app.sys, node);
            if (right != (alea_node_id_t)-1)
                render_csg_node(app, right, depth+1);
        }
        ImGui::TreePop();
    }
}

// ---------------------------------------------------------------------------
// Cell properties
// ---------------------------------------------------------------------------

static void render_cell_properties(AppState& app) {
    if (app.selected_cell_index < 0) {
        ImGui::TextDisabled("Click a cell in the viewport to inspect it.");
        return;
    }

    alea_cell_info_t info;
    if (alea_cell_get_info(app.sys, (size_t)app.selected_cell_index, &info) != 0) {
        ImGui::TextDisabled("Invalid selection.");
        return;
    }

    // Color swatch + cell header
    uint8_t col[3];
    color_for_id(info.cell_id, col);
    ImVec4 cv(col[0]/255.0f, col[1]/255.0f, col[2]/255.0f, 1.0f);
    ImGui::ColorButton("##cellcol", cv, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
    ImGui::SameLine();
    ImGui::Text("Cell %d", info.cell_id);

    // Properties table
    if (ImGui::BeginTable("##props", 2, ImGuiTableFlags_SizingFixedFit)) {
        float key_w = ImGui::CalcTextSize("Material  ").x;
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, key_w);
        ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("Material"); ImGui::TableNextColumn();
        ImGui::TextWrapped("%d", info.material_id);

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("Density"); ImGui::TableNextColumn();
        ImGui::TextWrapped("%.6g", info.density);

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("Universe"); ImGui::TableNextColumn();
        ImGui::TextWrapped("%d", info.universe_id);

        if (info.fill_universe >= 0) {
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::TextDisabled("Fill"); ImGui::TableNextColumn();
            ImGui::TextWrapped("%d", info.fill_universe);
        }

        if (info.lat_type > 0) {
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::TextDisabled("Lattice"); ImGui::TableNextColumn();
            ImGui::TextWrapped("type=%d  pitch=[%.3f, %.3f, %.3f]",
                        info.lat_type, info.lat_pitch[0], info.lat_pitch[1], info.lat_pitch[2]);
        }

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("BBox"); ImGui::TableNextColumn();
        ImGui::TextWrapped("[%.2f,%.2f] x [%.2f,%.2f] x [%.2f,%.2f]",
                    info.bbox.min_x, info.bbox.max_x,
                    info.bbox.min_y, info.bbox.max_y,
                    info.bbox.min_z, info.bbox.max_z);

        ImGui::EndTable();
    }

    // CSG tree for selected cell
    if (info.root != (alea_node_id_t)-1) {
        if (ImGui::TreeNodeEx("CSG Definition", ImGuiTreeNodeFlags_DefaultOpen)) {
            render_csg_node(app, info.root, 0);
            ImGui::TreePop();
        }
    }
}

// ---------------------------------------------------------------------------
// Surface properties
// ---------------------------------------------------------------------------

static void render_surface_properties(AppState& app) {
    if (app.selected_surface_index < 0) {
        ImGui::TextDisabled("Click a surface in the browser to inspect it.");
        return;
    }

    int sid = 0;
    alea_primitive_type_t ptype = (alea_primitive_type_t)0;
    alea_node_id_t pos_node = 0;
    alea_boundary_type_t btype = ALEA_BOUNDARY_TRANSMISSIVE;
    alea_surface_get(app.sys, (size_t)app.selected_surface_index,
                         &sid, &ptype, &pos_node, nullptr, &btype);

    ImGui::Text("Surface %d", sid);

    if (ImGui::BeginTable("##sprops", 2, ImGuiTableFlags_SizingFixedFit)) {
        float key_w = ImGui::CalcTextSize("Material  ").x;
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, key_w);
        ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("Type"); ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", prim_type_name(ptype));

        ImGui::TableNextRow(); ImGui::TableNextColumn();
        ImGui::TextDisabled("Boundary"); ImGui::TableNextColumn();
        const char* bnames[] = {"Transmissive", "Reflective", "White", "Periodic", "Vacuum"};
        ImGui::TextWrapped("%s", bnames[btype < 5 ? btype : 0]);

        // Show primitive data
        alea_primitive_data_t pdata;
        if (alea_node_primitive_data(app.sys, pos_node, &pdata) == 0) {
            switch (ptype) {
                case ALEA_PRIMITIVE_PLANE:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Coeffs"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g %.6g %.6g %.6g",
                                pdata.plane.a, pdata.plane.b, pdata.plane.c, pdata.plane.d);
                    break;
                case ALEA_PRIMITIVE_SPHERE:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Center"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)",
                                pdata.sphere.center_x, pdata.sphere.center_y, pdata.sphere.center_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radius"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.sphere.radius);
                    break;
                case ALEA_PRIMITIVE_CYLINDER_Z:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Center"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g)", pdata.cyl_z.center_x, pdata.cyl_z.center_y);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radius"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cyl_z.radius);
                    break;
                case ALEA_PRIMITIVE_CYLINDER_X:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Center"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g)", pdata.cyl_x.center_y, pdata.cyl_x.center_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radius"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cyl_x.radius);
                    break;
                case ALEA_PRIMITIVE_CYLINDER_Y:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Center"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g)", pdata.cyl_y.center_x, pdata.cyl_y.center_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radius"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cyl_y.radius);
                    break;
                case ALEA_PRIMITIVE_CONE_X:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Apex"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)", pdata.cone_x.apex_x,
                                pdata.cone_x.apex_y, pdata.cone_x.apex_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("tan^2"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cone_x.tan_angle_sq);
                    break;
                case ALEA_PRIMITIVE_CONE_Y:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Apex"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)", pdata.cone_y.apex_x,
                                pdata.cone_y.apex_y, pdata.cone_y.apex_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("tan^2"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cone_y.tan_angle_sq);
                    break;
                case ALEA_PRIMITIVE_CONE_Z:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Apex"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)", pdata.cone_z.apex_x,
                                pdata.cone_z.apex_y, pdata.cone_z.apex_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("tan^2"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.cone_z.tan_angle_sq);
                    break;
                case ALEA_PRIMITIVE_RPP:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Extents"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("[%.4g,%.4g] x [%.4g,%.4g] x [%.4g,%.4g]",
                                pdata.box.min_x, pdata.box.max_x,
                                pdata.box.min_y, pdata.box.max_y,
                                pdata.box.min_z, pdata.box.max_z);
                    break;
                case ALEA_PRIMITIVE_QUADRIC:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Coeffs"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("A=%.6g B=%.6g C=%.6g",
                                pdata.quadric.coeffs[0], pdata.quadric.coeffs[1], pdata.quadric.coeffs[2]);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("D=%.6g E=%.6g F=%.6g",
                                pdata.quadric.coeffs[3], pdata.quadric.coeffs[4], pdata.quadric.coeffs[5]);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TextWrapped("G=%.6g H=%.6g I=%.6g J=%.6g",
                                pdata.quadric.coeffs[6], pdata.quadric.coeffs[7],
                                pdata.quadric.coeffs[8], pdata.quadric.coeffs[9]);
                    break;
                case ALEA_PRIMITIVE_TORUS_X:
                case ALEA_PRIMITIVE_TORUS_Y:
                case ALEA_PRIMITIVE_TORUS_Z:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Center"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)",
                                pdata.torus.center_x, pdata.torus.center_y, pdata.torus.center_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radii"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("R=%.6g r=%.6g B=%.6g",
                                pdata.torus.major_radius, pdata.torus.minor_radius, pdata.torus.axial_semiwidth_B);
                    break;
                case ALEA_PRIMITIVE_RCC:
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Base"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)",
                                pdata.rcc.base_x, pdata.rcc.base_y, pdata.rcc.base_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Height"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("(%.4g, %.4g, %.4g)",
                                pdata.rcc.height_x, pdata.rcc.height_y, pdata.rcc.height_z);
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    ImGui::TextDisabled("Radius"); ImGui::TableNextColumn();
                    ImGui::TextWrapped("%.6g", pdata.rcc.radius);
                    break;
                default:
                    break;
            }
        }

        // Count cells using this surface
        int nc = (int)alea_cell_count(app.sys);
        int usage_count = 0;
        for (int i = 0; i < nc && usage_count < 999; i++) {
            alea_cell_info_t ci;
            if (alea_cell_get_info(app.sys, (size_t)i, &ci) != 0) continue;
            if (ci.root == ALEA_NODE_ID_INVALID) continue;
            // Quick check: walk the tree (shallow) for this surface
            // For simplicity, just count - a full walk is expensive
            // So we'll just show "used" without a count for now
        }
        (void)usage_count;

        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
// Properties panel
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Properties panel
// ---------------------------------------------------------------------------

void panel_properties(AppState& app) {
    ImGui::Begin("Properties");

    if (!app.sys) {
        ImGui::TextDisabled("No geometry loaded.");
        ImGui::End();
        return;
    }

    // Reset hovered surface each frame
    app.hovered_surface_id = -1;

    // Show cell or surface properties depending on what's selected
    if (app.selected_surface_index >= 0 && app.selected_cell_index < 0) {
        render_surface_properties(app);
    } else {
        render_cell_properties(app);
        if (app.selected_surface_index >= 0) {
            ImGui::Separator();
            render_surface_properties(app);
        }
    }

    ImGui::End();
}
