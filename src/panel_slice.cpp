#include "app.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Map screen coords to world coords
static void screen_to_world(const AppState& app, float sx, float sy,
                            float img_x, float img_y, float img_w, float img_h,
                            double& wu, double& wv) {
    double u_min = app.view_cx - app.view_half_ext;
    double u_max = app.view_cx + app.view_half_ext;
    double v_min = app.view_cy - app.view_half_ext;
    double v_max = app.view_cy + app.view_half_ext;

    double fx = (sx - img_x) / img_w;
    double fy = (sy - img_y) / img_h;

    wu = u_min + fx * (u_max - u_min);
    wv = v_max - fy * (v_max - v_min); // Y flipped (top = vmax)
}

// Map world coords to screen coords
static void world_to_screen(const AppState& app,
                            double wu, double wv,
                            float img_x, float img_y, float img_w, float img_h,
                            float& sx, float& sy) {
    double u_min = app.view_cx - app.view_half_ext;
    double u_max = app.view_cx + app.view_half_ext;
    double v_min = app.view_cy - app.view_half_ext;
    double v_max = app.view_cy + app.view_half_ext;

    double fx = (wu - u_min) / (u_max - u_min);
    double fy = (v_max - wv) / (v_max - v_min); // Y flipped

    sx = img_x + (float)(fx * img_w);
    sy = img_y + (float)(fy * img_h);
}

// ---------------------------------------------------------------------------
// Draw analytical curve overlay
// ---------------------------------------------------------------------------

static void draw_curve_overlay(AppState& app, ImDrawList* dl,
                               float img_x, float img_y, float img_w, float img_h) {
    ImU32 curve_col = IM_COL32(78, 201, 212, 200);  // teal accent bright

    for (auto& c : app.slice_curves) {
        switch (c.type) {
            case ALEA_CURVE_LINE: {
                // Line: point + direction, clipped to viewport
                double px = c.data.line.point[0];
                double py = c.data.line.point[1];
                double dx = c.data.line.direction[0];
                double dy = c.data.line.direction[1];

                double u_min = app.view_cx - app.view_half_ext;
                double u_max = app.view_cx + app.view_half_ext;
                double v_min = app.view_cy - app.view_half_ext;
                double v_max = app.view_cy + app.view_half_ext;

                // Find t range that clips to viewport
                double t0 = -1e10, t1 = 1e10;
                if (fabs(dx) > 1e-15) {
                    double ta = (u_min - px) / dx;
                    double tb = (u_max - px) / dx;
                    if (ta > tb) std::swap(ta, tb);
                    t0 = std::max(t0, ta);
                    t1 = std::min(t1, tb);
                }
                if (fabs(dy) > 1e-15) {
                    double ta = (v_min - py) / dy;
                    double tb = (v_max - py) / dy;
                    if (ta > tb) std::swap(ta, tb);
                    t0 = std::max(t0, ta);
                    t1 = std::min(t1, tb);
                }
                if (t0 >= t1) break;

                float sx0, sy0, sx1, sy1;
                world_to_screen(app, px + dx*t0, py + dy*t0, img_x, img_y, img_w, img_h, sx0, sy0);
                world_to_screen(app, px + dx*t1, py + dy*t1, img_x, img_y, img_w, img_h, sx1, sy1);
                dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1), curve_col, 1.0f);
                break;
            }

            case ALEA_CURVE_CIRCLE: {
                float cx_s, cy_s;
                world_to_screen(app, c.data.circle.center[0], c.data.circle.center[1],
                               img_x, img_y, img_w, img_h, cx_s, cy_s);
                float scale = img_w / (float)(2.0 * app.view_half_ext);
                float r_s = (float)(c.data.circle.radius * scale);
                if (r_s > 1.0f && r_s < img_w * 5.0f) {
                    dl->AddCircle(ImVec2(cx_s, cy_s), r_s, curve_col, 0, 1.0f);
                }
                break;
            }

            case ALEA_CURVE_ELLIPSE: {
                // Approximate ellipse with polyline
                float scale = img_w / (float)(2.0 * app.view_half_ext);
                double cx = c.data.ellipse.center[0];
                double cy = c.data.ellipse.center[1];
                double sa = c.data.ellipse.semi_a;
                double sb = c.data.ellipse.semi_b;
                double angle = c.data.ellipse.angle;
                double cos_a = cos(angle), sin_a = sin(angle);

                int nsegs = std::clamp((int)(std::max(sa, sb) * scale * 0.3), 32, 256);
                ImVec2 pts[257];
                for (int i = 0; i <= nsegs; i++) {
                    double t = 2.0 * M_PI * i / nsegs;
                    double eu = sa * cos(t);
                    double ev = sb * sin(t);
                    double wu = cx + eu * cos_a - ev * sin_a;
                    double wv = cy + eu * sin_a + ev * cos_a;
                    world_to_screen(app, wu, wv, img_x, img_y, img_w, img_h, pts[i].x, pts[i].y);
                }
                dl->AddPolyline(pts, nsegs + 1, curve_col, ImDrawFlags_None, 1.0f);
                break;
            }

            case ALEA_CURVE_POLYGON: {
                int n = c.data.polygon.count;
                if (n < 2 || n > 16) break;
                ImVec2 pts[17];
                for (int i = 0; i < n; i++) {
                    world_to_screen(app, c.data.polygon.vertices[i][0],
                                   c.data.polygon.vertices[i][1],
                                   img_x, img_y, img_w, img_h, pts[i].x, pts[i].y);
                }
                if (c.data.polygon.closed) {
                    pts[n] = pts[0];
                    dl->AddPolyline(pts, n + 1, curve_col, ImDrawFlags_None, 1.0f);
                } else {
                    dl->AddPolyline(pts, n, curve_col, ImDrawFlags_None, 1.0f);
                }
                break;
            }

            case ALEA_CURVE_PARALLEL_LINES: {
                double u_min = app.view_cx - app.view_half_ext;
                double u_max = app.view_cx + app.view_half_ext;
                double v_min = app.view_cy - app.view_half_ext;
                double v_max = app.view_cy + app.view_half_ext;

                double dx = c.data.parallel_lines.direction[0];
                double dy = c.data.parallel_lines.direction[1];

                for (int li = 0; li < 2; li++) {
                    double px = (li == 0) ? c.data.parallel_lines.point1[0]
                                          : c.data.parallel_lines.point2[0];
                    double py = (li == 0) ? c.data.parallel_lines.point1[1]
                                          : c.data.parallel_lines.point2[1];

                    double t0 = -1e10, t1 = 1e10;
                    if (fabs(dx) > 1e-15) {
                        double ta = (u_min - px) / dx;
                        double tb = (u_max - px) / dx;
                        if (ta > tb) std::swap(ta, tb);
                        t0 = std::max(t0, ta);
                        t1 = std::min(t1, tb);
                    }
                    if (fabs(dy) > 1e-15) {
                        double ta = (v_min - py) / dy;
                        double tb = (v_max - py) / dy;
                        if (ta > tb) std::swap(ta, tb);
                        t0 = std::max(t0, ta);
                        t1 = std::min(t1, tb);
                    }
                    if (t0 >= t1) continue;

                    float sx0, sy0, sx1, sy1;
                    world_to_screen(app, px + dx*t0, py + dy*t0, img_x, img_y, img_w, img_h, sx0, sy0);
                    world_to_screen(app, px + dx*t1, py + dy*t1, img_x, img_y, img_w, img_h, sx1, sy1);
                    dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy1), curve_col, 1.0f);
                }
                break;
            }

            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Draw label overlay (cell labels — white text)
// ---------------------------------------------------------------------------

static void draw_label_overlay(AppState& app, ImDrawList* dl,
                               float img_x, float img_y, float img_w, float img_h) {
    ImU32 text_col = IM_COL32(224, 226, 232, 230);  // text primary
    ImU32 bg_col   = IM_COL32(25, 25, 32, 180);     // bg darkest

    for (auto& lbl : app.slice_labels) {
        if (lbl.id <= 0) continue;

        float sx, sy;
        world_to_screen(app, lbl.world_u, lbl.world_v, img_x, img_y, img_w, img_h, sx, sy);

        if (sx < img_x || sx > img_x + img_w || sy < img_y || sy > img_y + img_h)
            continue;

        char txt[32];
        snprintf(txt, sizeof(txt), "%d", lbl.id);
        ImVec2 text_size = ImGui::CalcTextSize(txt);

        dl->AddRectFilled(ImVec2(sx - 1, sy - 1),
                         ImVec2(sx + text_size.x + 1, sy + text_size.y + 1),
                         bg_col);
        dl->AddText(ImVec2(sx, sy), text_col, txt);
    }
}

// ---------------------------------------------------------------------------
// Draw surface label overlay (surface IDs — cyan text)
// ---------------------------------------------------------------------------

static void draw_surface_label_overlay(AppState& app, ImDrawList* dl,
                                       float img_x, float img_y, float img_w, float img_h) {
    ImU32 text_col = IM_COL32(56, 170, 191, 230);   // accent hover
    ImU32 bg_col   = IM_COL32(20, 50, 60, 180);     // dark teal tinted

    for (auto& lbl : app.slice_surface_labels) {
        if (lbl.id <= 0) continue;

        float sx, sy;
        world_to_screen(app, lbl.world_u, lbl.world_v, img_x, img_y, img_w, img_h, sx, sy);

        if (sx < img_x || sx > img_x + img_w || sy < img_y || sy > img_y + img_h)
            continue;

        char txt[32];
        snprintf(txt, sizeof(txt), "S%d", lbl.id);
        ImVec2 text_size = ImGui::CalcTextSize(txt);

        dl->AddRectFilled(ImVec2(sx - 1, sy - 1),
                         ImVec2(sx + text_size.x + 1, sy + text_size.y + 1),
                         bg_col);
        dl->AddText(ImVec2(sx, sy), text_col, txt);
    }
}

// ---------------------------------------------------------------------------
// Draw grid overlay
// ---------------------------------------------------------------------------

static void draw_grid_lines(AppState& app, ImDrawList* dl,
                            float img_x, float img_y, float img_w, float img_h) {
    double ext = app.view_half_ext * 2.0;
    double u_min = app.view_cx - app.view_half_ext;
    double v_min = app.view_cy - app.view_half_ext;
    double u_max = app.view_cx + app.view_half_ext;
    double v_max = app.view_cy + app.view_half_ext;

    double raw_step = ext / 10.0;
    double log_step = floor(log10(raw_step));
    double step = pow(10.0, log_step);
    if (ext / step > 20) step *= 5;
    else if (ext / step > 10) step *= 2;

    ImU32 grid_col = IM_COL32(78, 201, 212, 35);    // teal grid lines
    ImU32 axis_col = IM_COL32(78, 201, 212, 80);    // teal axis lines

    double u_start = ceil(u_min / step) * step;
    for (double u = u_start; u <= u_max; u += step) {
        float sx0, sy0, sx1, sy1;
        world_to_screen(app, u, v_min, img_x, img_y, img_w, img_h, sx0, sy1);
        world_to_screen(app, u, v_max, img_x, img_y, img_w, img_h, sx1, sy0);
        bool is_origin = fabs(u) < step * 0.01;
        dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx0, sy1), is_origin ? axis_col : grid_col, 1.0f);
    }

    double v_start = ceil(v_min / step) * step;
    for (double v = v_start; v <= v_max; v += step) {
        float sx0, sy0, sx1, sy1;
        world_to_screen(app, u_min, v, img_x, img_y, img_w, img_h, sx0, sy0);
        world_to_screen(app, u_max, v, img_x, img_y, img_w, img_h, sx1, sy1);
        bool is_origin = fabs(v) < step * 0.01;
        dl->AddLine(ImVec2(sx0, sy0), ImVec2(sx1, sy0), is_origin ? axis_col : grid_col, 1.0f);
    }
}

static void draw_grid_labels(AppState& app, ImDrawList* dl,
                             float img_x, float img_y, float img_w, float img_h) {
    double ext = app.view_half_ext * 2.0;
    double u_min = app.view_cx - app.view_half_ext;
    double v_min = app.view_cy - app.view_half_ext;
    double u_max = app.view_cx + app.view_half_ext;
    double v_max = app.view_cy + app.view_half_ext;

    double raw_step = ext / 10.0;
    double log_step = floor(log10(raw_step));
    double step = pow(10.0, log_step);
    if (ext / step > 20) step *= 5;
    else if (ext / step > 10) step *= 2;

    ImU32 text_col = IM_COL32(224, 226, 232, 220);
    float font_h = ImGui::GetFontSize();

    // Bottom axis labels (U values)
    double u_start = ceil(u_min / step) * step;
    for (double u = u_start; u <= u_max; u += step) {
        float sx, sy;
        world_to_screen(app, u, v_min, img_x, img_y, img_w, img_h, sx, sy);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "%.4g", u);
        ImVec2 tsz = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(sx - tsz.x * 0.5f, img_y + img_h + 2), text_col, lbl);
    }

    // Left axis labels (V values)
    double v_start = ceil(v_min / step) * step;
    for (double v = v_start; v <= v_max; v += step) {
        float sx, sy;
        world_to_screen(app, u_min, v, img_x, img_y, img_w, img_h, sx, sy);
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "%.4g", v);
        ImVec2 tsz = ImGui::CalcTextSize(lbl);
        dl->AddText(ImVec2(img_x - tsz.x - 4, sy - font_h * 0.5f), text_col, lbl);
    }
}

// ---------------------------------------------------------------------------
// Toggle button helper (accent bg when on, dim bg + secondary text when off)
// ---------------------------------------------------------------------------

static bool toggle_button(const char* label, bool active, const char* tooltip = nullptr) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.180f, 0.588f, 0.678f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.220f, 0.667f, 0.749f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.306f, 0.788f, 0.831f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.878f, 0.886f, 0.910f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.180f, 0.188f, 0.216f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.239f, 0.247f, 0.278f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.290f, 0.298f, 0.333f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.502f, 0.518f, 0.557f, 1.0f));
    }
    bool clicked = ImGui::SmallButton(label);
    ImGui::PopStyleColor(4);
    if (tooltip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return clicked;
}

// ---------------------------------------------------------------------------
// Slice panel
// ---------------------------------------------------------------------------

void panel_slice(AppState& app) {
    ImGui::Begin("2D Slice");

    // ---- Toolbar Row 1: Axis, Value, Step buttons ----
    {
        if (!app.arbitrary_slice) {
            const char* axes[] = { "XY (Z)", "YZ (X)", "XZ (Y)" };
            int ax = (int)app.axis;
            ImGui::SetNextItemWidth(200);
            if (ImGui::Combo("##Axis", &ax, axes, 3)) {
                app.axis = (SliceAxis)ax;
                app_zoom_fit(app);
            }
            ImGui::SameLine();

            // Step buttons (guillemets)
            if (ImGui::Button("\xC2\xAB##back10")) {
                app.slice_value -= app.slice_step * 10;
                app_request_rerender(app);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step back x10");
            ImGui::SameLine();
            if (ImGui::Button("<##back1")) {
                app.slice_value -= app.slice_step;
                app_request_rerender(app);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step back");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(100);
            if (ImGui::DragScalar("##Value", ImGuiDataType_Double, &app.slice_value, 0.5f)) {
                app_request_rerender(app);
            }
            ImGui::SameLine();

            if (ImGui::Button(">##fwd1")) {
                app.slice_value += app.slice_step;
                app_request_rerender(app);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step forward");
            ImGui::SameLine();
            if (ImGui::Button("\xC2\xBB##fwd10")) {
                app.slice_value += app.slice_step * 10;
                app_request_rerender(app);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Step forward x10");
            ImGui::SameLine();

            ImGui::SetNextItemWidth(60);
            ImGui::DragScalar("Step", ImGuiDataType_Double, &app.slice_step, 0.1f);
        } else {
            // Arbitrary mode: origin + normal + up inputs
            float label_w = ImGui::CalcTextSize("O ").x;
            float avail = ImGui::GetContentRegionAvail().x;
            float w = (avail - label_w * 3 - 24) / 3.0f;
            if (w < 140) w = 140;

            ImGui::Text("O");  ImGui::SameLine();
            ImGui::SetNextItemWidth(w);
            if (ImGui::DragScalarN("##origin", ImGuiDataType_Double, app.slice_origin, 3, 0.5f, nullptr, nullptr, "%.3f"))
                app_request_rerender(app);
            ImGui::SameLine();
            ImGui::Text("N");  ImGui::SameLine();
            ImGui::SetNextItemWidth(w);
            if (ImGui::DragScalarN("##normal", ImGuiDataType_Double, app.slice_normal, 3, 0.01f, nullptr, nullptr, "%.4f"))
                app_request_rerender(app);
            ImGui::SameLine();
            ImGui::Text("U");  ImGui::SameLine();
            ImGui::SetNextItemWidth(w);
            if (ImGui::DragScalarN("##up", ImGuiDataType_Double, app.slice_up, 3, 0.01f, nullptr, nullptr, "%.4f"))
                app_request_rerender(app);
        }
    }

    // ---- Toolbar Row 2: Zoom | Depth | Color | Toggles ----
    {
        if (ImGui::Button("Fit")) {
            app_zoom_fit(app);
        }
        ImGui::SameLine();
        if (ImGui::Button("1:1")) {
            app.view_half_ext = 400.0;
            app_request_rerender(app);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Universe depth
        const char* depth_labels[] = { "All", "Root", "1", "2", "3", "4", "5" };
        int depth_idx = (app.universe_depth == -1) ? 0 : (app.universe_depth + 1);
        if (depth_idx > 6) depth_idx = 6;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Depth", &depth_idx, depth_labels, 7)) {
            app.universe_depth = (depth_idx == 0) ? -1 : (depth_idx - 1);
            app_request_rerender(app);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Color mode
        const char* cmodes[] = { "Cell", "Material", "Universe", "Density" };
        int cm = (int)app.color_mode;
        ImGui::SetNextItemWidth(220);
        if (ImGui::Combo("##Color", &cm, cmodes, 4)) {
            app.color_mode = (ColorMode)cm;
            app_request_rerender(app);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Layer toggle buttons
        if (toggle_button("C", app.show_contours, "Contours (cell boundaries)"))
            { app.show_contours = !app.show_contours; app.needs_recolor = true; }
        ImGui::SameLine();

        if (toggle_button("S", app.show_curves, "Surface curves (analytical)")) {
            app.show_curves = !app.show_curves;
            if (app.show_curves && app.slice_curves.empty())
                app_request_rerender(app);
        }
        ImGui::SameLine();

        if (toggle_button("SL", app.show_surface_labels, "Surface labels")) {
            app.show_surface_labels = !app.show_surface_labels;
            if (app.show_surface_labels && app.slice_surface_labels.empty())
                app_request_rerender(app);
        }
        ImGui::SameLine();

        if (toggle_button("L", app.show_labels, "Labels")) {
            app.show_labels = !app.show_labels;
            if (app.show_labels && app.slice_labels.empty())
                app_request_rerender(app);
        }
        ImGui::SameLine();

        if (toggle_button("G", app.show_grid, "Grid"))
            { app.show_grid = !app.show_grid; }
        ImGui::SameLine();

        if (toggle_button("E", app.show_errors, "Errors"))
            { app.show_errors = !app.show_errors; app.needs_recolor = true; }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        // Arbitrary slice toggle (orange accent when on)
        if (app.arbitrary_slice) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.50f, 0.20f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.80f, 0.60f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.90f, 0.70f, 0.35f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.878f, 0.886f, 0.910f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.180f, 0.188f, 0.216f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.239f, 0.247f, 0.278f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.290f, 0.298f, 0.333f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,           ImVec4(0.502f, 0.518f, 0.557f, 1.0f));
        }
        if (ImGui::SmallButton("Arb")) {
            app.arbitrary_slice = !app.arbitrary_slice;
            app_request_rerender(app);
        }
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Arbitrary slice plane");

        if (app.worker_busy.load()) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.949f, 0.800f, 0.306f, 1.0f), "Rendering...");
        }
    }

    // ---- Image viewport ----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    // Reserve margin for grid axis labels when grid is on
    float margin_left = app.show_grid ? 48.0f : 0.0f;
    float margin_bottom = app.show_grid ? (ImGui::GetFontSize() + 4.0f) : 0.0f;
    float img_size = std::min(avail.x - margin_left, avail.y - margin_bottom);
    if (img_size < 50) img_size = 50;

    // Offset cursor to make room for left margin
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 img_pos(cursor.x + margin_left, cursor.y);
    float img_w = img_size, img_h = img_size;

    ImGui::SetCursorScreenPos(img_pos);

    if (app.texture_id && app.tex_w > 0) {
        // Flip V: texture row 0 = v_min (bottom), but screen top = v_max
        ImGui::Image((ImTextureID)(intptr_t)app.texture_id,
                     ImVec2(img_w, img_h), ImVec2(0,1), ImVec2(1,0));
    } else {
        ImGui::Dummy(ImVec2(img_w, img_h));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRect(img_pos, ImVec2(img_pos.x+img_w, img_pos.y+img_h),
                    IM_COL32(51, 52, 57, 255));
        dl->AddText(ImVec2(img_pos.x+10, img_pos.y+10), IM_COL32(128, 132, 142, 255),
                    app.sys ? "Rendering..." : "No geometry loaded");
    }

    // ---- Draw overlays on top of the image ----
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Clip overlays to image area
    dl->PushClipRect(img_pos, ImVec2(img_pos.x + img_w, img_pos.y + img_h), true);

    if (app.show_grid) {
        draw_grid_lines(app, dl, img_pos.x, img_pos.y, img_w, img_h);
    }

    if (app.show_curves && !app.slice_curves.empty()) {
        draw_curve_overlay(app, dl, img_pos.x, img_pos.y, img_w, img_h);
    }

    if (app.show_labels && !app.slice_labels.empty()) {
        draw_label_overlay(app, dl, img_pos.x, img_pos.y, img_w, img_h);
    }

    if (app.show_surface_labels && !app.slice_surface_labels.empty()) {
        draw_surface_label_overlay(app, dl, img_pos.x, img_pos.y, img_w, img_h);
    }

    dl->PopClipRect();

    // Grid tick labels drawn outside the image clip rect
    if (app.show_grid) {
        draw_grid_labels(app, dl, img_pos.x, img_pos.y, img_w, img_h);
    }

    // ---- Mouse interaction ----
    ImGuiIO& io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();

    if (hovered) {
        ImVec2 mp = io.MousePos;

        // Update cursor world position
        double wu, wv;
        screen_to_world(app, mp.x, mp.y, img_pos.x, img_pos.y, img_w, img_h, wu, wv);
        app.cursor_u = wu;
        app.cursor_v = wv;
        app.cursor_in_viewport = true;

        // Look up cell under cursor from grid data (V is flipped)
        if (!app.cell_ids.empty() && app.tex_w > 0) {
            int px = (int)((mp.x - img_pos.x) / img_w * app.tex_w);
            float fy = (mp.y - img_pos.y) / img_h;
            int py = (int)((1.0f - fy) * app.tex_h);
            px = std::clamp(px, 0, app.tex_w-1);
            py = std::clamp(py, 0, app.tex_h-1);
            int idx = py * app.tex_w + px;
            app.cursor_cell = app.cell_ids[idx];
            app.cursor_mat  = app.material_ids[idx];
        }

        // Scroll = zoom toward cursor
        if (io.MouseWheel != 0) {
            double factor = (io.MouseWheel > 0) ? 0.85 : 1.0/0.85;
            // Zoom toward cursor
            double new_ext = app.view_half_ext * factor;
            double t = 1.0 - factor;
            app.view_cx += t * (wu - app.view_cx);
            app.view_cy += t * (wv - app.view_cy);
            app.view_half_ext = new_ext;
            app_request_rerender(app);
        }

        // Middle-drag = pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
            ImVec2 delta = io.MouseDelta;
            double scale = (2.0 * app.view_half_ext) / img_w;
            app.view_cx -= delta.x * scale;
            app.view_cy += delta.y * scale; // Y flipped
            app_request_rerender(app);
        }

        // Left-click = select cell in tree
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && app.cursor_cell > 0 && app.sys) {
            int idx = alea_cell_find(app.sys, app.cursor_cell);
            if (idx >= 0) {
                app.selected_cell_index = idx;
                app.selected_surface_index = -1;
            }
        }

        // Right-click context menu
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup("SliceContextMenu");
        }
    } else {
        app.cursor_in_viewport = false;
    }

    // Context menu
    if (ImGui::BeginPopup("SliceContextMenu")) {
        if (ImGui::MenuItem("Query this point") && app.sys) {
            double x = 0, y = 0, z = 0;
            if (app.arbitrary_slice) {
                // In arbitrary mode, reconstruct 3D point from UV + plane basis
                // origin + u * U_axis + v * V_axis
                // U_axis and V_axis are orthogonal on the plane; for now use the origin
                // as a rough approximation — full reconstruction requires the view basis vectors
                x = app.slice_origin[0];
                y = app.slice_origin[1];
                z = app.slice_origin[2];
            } else {
                switch (app.axis) {
                    case SLICE_Z: x = app.cursor_u; y = app.cursor_v; z = app.slice_value; break;
                    case SLICE_X: x = app.slice_value; y = app.cursor_u; z = app.cursor_v; break;
                    case SLICE_Y: x = app.cursor_u; y = app.slice_value; z = app.cursor_v; break;
                }
            }

            alea_cell_hit_t hits[32];
            int nhits = alea_find_all_cells(app.sys, x, y, z, hits, 32);

            QueryResult qr;
            qr.x = x; qr.y = y; qr.z = z;
            for (int i = 0; i < nhits; i++) {
                qr.hits.push_back(hits[i]);
            }
            app.query_results.push_back(std::move(qr));
            app.bottom_tab = 1; // Switch to query results tab

            app_log(app, "Query (%.4f, %.4f, %.4f): %d hits", x, y, z, nhits);
        }
        if (ImGui::MenuItem("Zoom to fit")) {
            app_zoom_fit(app);
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}
