#include "app.h"
#include "imgui.h"
#include <cmath>
#include <algorithm>

void panel_3d(AppState& app) {
    ImGui::Begin("3D View");

    RaycastState& rc = app.rc;

    // ---- Toolbar ----
    {
        float fov_f = (float)rc.cam_fov;
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("FOV", &fov_f, 20.0f, 120.0f, "%.0f")) {
            rc.cam_fov = fov_f;
            rc.needs_rerender = true;
        }
        ImGui::SameLine();

        const char* cmodes[] = { "Cell", "Material", "Universe", "Density" };
        int cm = (int)app.color_mode;
        ImGui::SetNextItemWidth(100);
        if (ImGui::Combo("##3DColor", &cm, cmodes, 4)) {
            app.color_mode = (ColorMode)cm;
            rc.needs_rerender = true;
            app.needs_rerender = true;  // also recolor 2D
        }
        ImGui::SameLine();

        if (ImGui::Button("Fit")) {
            if (app.sys) {
                int nc = (int)alea_cell_count(app.sys);
                if (nc > 0) {
                    alea_cell_info_t ci;
                    alea_cell_get_info(app.sys, 0, &ci);
                    double xmin = ci.bbox.min_x, xmax = ci.bbox.max_x;
                    double ymin = ci.bbox.min_y, ymax = ci.bbox.max_y;
                    double zmin = ci.bbox.min_z, zmax = ci.bbox.max_z;
                    for (int i = 1; i < nc; i++) {
                        if (alea_cell_get_info(app.sys, (size_t)i, &ci) == 0) {
                            if (ci.bbox.min_x < xmin) xmin = ci.bbox.min_x;
                            if (ci.bbox.min_y < ymin) ymin = ci.bbox.min_y;
                            if (ci.bbox.min_z < zmin) zmin = ci.bbox.min_z;
                            if (ci.bbox.max_x > xmax) xmax = ci.bbox.max_x;
                            if (ci.bbox.max_y > ymax) ymax = ci.bbox.max_y;
                            if (ci.bbox.max_z > zmax) zmax = ci.bbox.max_z;
                        }
                    }
                    rc.cam_target[0] = (xmin + xmax) * 0.5;
                    rc.cam_target[1] = (ymin + ymax) * 0.5;
                    rc.cam_target[2] = (zmin + zmax) * 0.5;
                    double dx = xmax - xmin, dy = ymax - ymin, dz = zmax - zmin;
                    rc.cam_distance = sqrt(dx*dx + dy*dy + dz*dz) * 0.8;
                    if (rc.cam_distance < 1.0) rc.cam_distance = 1.0;
                    rc.needs_rerender = true;
                }
            }
        }

        if (rc.worker_busy.load()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.949f, 0.800f, 0.306f, 1.0f), "Rendering...");
        }
    }

    // ---- Image viewport ----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float img_w = avail.x;
    float img_h = avail.y;
    if (img_w < 50) img_w = 50;
    if (img_h < 50) img_h = 50;

    ImVec2 img_pos = ImGui::GetCursorScreenPos();

    if (rc.texture_id && rc.tex_w > 0) {
        ImGui::Image((ImTextureID)(intptr_t)rc.texture_id,
                     ImVec2(img_w, img_h));
    } else {
        ImGui::Dummy(ImVec2(img_w, img_h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(img_pos, ImVec2(img_pos.x+img_w, img_pos.y+img_h),
                    IM_COL32(51, 52, 57, 255));
        dl->AddText(ImVec2(img_pos.x+10, img_pos.y+10), IM_COL32(128, 132, 142, 255),
                    app.sys ? "Rendering..." : "No geometry loaded");
    }

    // ---- Mouse interaction ----
    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();

    if (hovered) {
        // Left-drag = orbit
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f)) {
            ImVec2 delta = io.MouseDelta;
            rc.cam_yaw   -= delta.x * 0.005;
            rc.cam_pitch += delta.y * 0.005;
            // Clamp pitch to avoid gimbal lock
            rc.cam_pitch = std::clamp(rc.cam_pitch, -1.5, 1.5);

            if (!rc.dragging) {
                rc.dragging = true;
                // Interrupt current render for low-res
                if (rc.worker_busy.load()) alea_interrupt();
            }
            rc.needs_rerender = true;
        }

        // Middle-drag = pan target
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 delta = io.MouseDelta;
            double scale = rc.cam_distance * 0.002;

            // Pan in camera right/up plane
            double cy = cos(rc.cam_yaw), sy = sin(rc.cam_yaw);
            double cp = cos(rc.cam_pitch);

            // Right direction (horizontal)
            double rx =  sy, ry = -cy;
            // Up direction (camera up projected)
            double sp = sin(rc.cam_pitch);
            double ux = -sp * cy, uy = -sp * sy, uz = cp;

            rc.cam_target[0] += (-delta.x * rx + delta.y * ux) * scale;
            rc.cam_target[1] += (-delta.x * ry + delta.y * uy) * scale;
            rc.cam_target[2] += delta.y * uz * scale;

            if (!rc.dragging) {
                rc.dragging = true;
                if (rc.worker_busy.load()) alea_interrupt();
            }
            rc.needs_rerender = true;
        }

        // Scroll = zoom (change distance)
        if (io.MouseWheel != 0) {
            double factor = (io.MouseWheel > 0) ? 0.85 : 1.0/0.85;
            rc.cam_distance *= factor;
            if (rc.cam_distance < 0.1) rc.cam_distance = 0.1;
            rc.needs_rerender = true;
        }

        // Left-click = select cell
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f)) {
            ImVec2 mp = io.MousePos;
            if (!rc.cell_ids.empty() && rc.tex_w > 0) {
                int px = (int)((mp.x - img_pos.x) / img_w * rc.tex_w);
                int py = (int)((mp.y - img_pos.y) / img_h * rc.tex_h);
                px = std::clamp(px, 0, rc.tex_w - 1);
                py = std::clamp(py, 0, rc.tex_h - 1);
                int idx = py * rc.tex_w + px;
                int cell_id = rc.cell_ids[idx];
                if (cell_id > 0 && app.sys) {
                    int cidx = alea_cell_find(app.sys, cell_id);
                    if (cidx >= 0) {
                        app.selected_cell_index = cidx;
                    }
                }
            }
        }
    }

    // Detect drag release → request full-res render
    if (rc.dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        rc.dragging = false;
        rc.needs_rerender = true;
    }

    ImGui::End();
}
