#include "app.h"
#include <SDL.h>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Worker thread function
// ---------------------------------------------------------------------------

static void do_slice(AppState* app) {
    SliceRequest req;
    {
        std::lock_guard<std::mutex> lock(app->worker_mutex);
        req = app->pending_request;
    }

    int npix = req.width * req.height;
    SliceResult res;
    res.gen    = req.gen;
    res.width  = req.width;
    res.height = req.height;
    res.cell_ids.resize(npix);
    res.material_ids.resize(npix);
    res.errors.resize(npix);

    alea_clear_interrupt();

    // Set up slice view
    alea_slice_view_t view;
    if (req.arbitrary) {
        alea_slice_view_init(&view,
                             req.origin[0], req.origin[1], req.origin[2],
                             req.normal[0], req.normal[1], req.normal[2],
                             req.up[0], req.up[1], req.up[2],
                             req.u_min, req.u_max, req.v_min, req.v_max);
    } else {
        int axis_id = 2;
        switch (req.axis) {
            case SLICE_Z: axis_id = 2; break;
            case SLICE_X: axis_id = 0; break;
            case SLICE_Y: axis_id = 1; break;
        }
        alea_slice_view_axis(&view, axis_id, req.slice_value,
                             req.u_min, req.u_max, req.v_min, req.v_max);
    }

    int rc = alea_find_cells_grid(app->sys, &view,
                                       req.width, req.height, req.universe_depth,
                                       res.cell_ids.data(), res.material_ids.data(),
                                       res.errors.data());
    (void)rc;

    if (!alea_interrupted()) {
        // Extract analytical curves (only if user has them enabled)
        alea_slice_curves_t* curves = nullptr;
        if (req.want_curves || req.want_surface_labels) {
            curves = alea_get_slice_curves(app->sys, &view);
            if (curves) {
                size_t nc = alea_slice_curves_count(curves);
                res.curves.resize(nc);
                for (size_t i = 0; i < nc; i++) {
                    alea_curve_t c;
                    if (alea_slice_curves_get(curves, i, &c) == 0) {
                        StoredCurve& sc = res.curves[i];
                        sc.type = c.type;
                        sc.surface_id = c.surface_id;
                        sc.t_min = c.t_min;
                        sc.t_max = c.t_max;
                        memcpy(&sc.data, &c.data, sizeof(sc.data));
                    }
                }
            }
        }

        // Compute surface label positions
        if (req.want_surface_labels && curves) {
            alea_label_position_t* slabels = nullptr;
            int slabel_count = 0;
            if (alea_find_surface_label_positions(curves,
                        req.u_min, req.u_max, req.v_min, req.v_max,
                        req.width, req.height, 20,
                        &slabels, &slabel_count) == 0 && slabels) {
                res.surface_labels.resize(slabel_count);
                for (int i = 0; i < slabel_count; i++) {
                    StoredLabel& sl = res.surface_labels[i];
                    sl.id = slabels[i].id;
                    double fx = ((double)slabels[i].px + 0.5) / req.width;
                    double fy = ((double)slabels[i].py + 0.5) / req.height;
                    sl.world_u = req.u_min + fx * (req.u_max - req.u_min);
                    sl.world_v = req.v_min + fy * (req.v_max - req.v_min);
                }
                free(slabels);
            }
        }

        if (curves) alea_slice_curves_free(curves);

        // Compute label positions (only if user has them enabled)
        if (req.want_labels) {
            alea_label_position_t* labels = nullptr;
            int label_count = 0;
            if (alea_find_label_positions(res.cell_ids.data(),
                                              req.width, req.height, 100,
                                              &labels, &label_count) == 0 && labels) {
                res.labels.resize(label_count);
                for (int i = 0; i < label_count; i++) {
                    StoredLabel& sl = res.labels[i];
                    sl.id = labels[i].id;
                    double fx = ((double)labels[i].px + 0.5) / req.width;
                    double fy = ((double)labels[i].py + 0.5) / req.height;
                    sl.world_u = req.u_min + fx * (req.u_max - req.u_min);
                    sl.world_v = req.v_min + fy * (req.v_max - req.v_min);
                }
                free(labels);
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(app->worker_mutex);
        app->pending_result = std::move(res);
        app->result_ready.store(true);
        app->worker_busy.store(false);
    }
}

static void worker_func(AppState* app) {
    while (!app->worker_quit.load()) {
        bool did_work = false;

        if (app->worker_busy.load()) {
            do_slice(app);
            did_work = true;
        }

        if (app->rc.worker_busy.load()) {
            raycast_worker_render(app);
            did_work = true;
        }

        if (!did_work) {
            SDL_Delay(5);
        }
    }
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void slice_worker_start(AppState& app) {
    app.worker_quit.store(false);
    app.worker_thread = std::thread(worker_func, &app);
}

void slice_worker_stop(AppState& app) {
    app.worker_quit.store(true);
    alea_interrupt();
    if (app.worker_thread.joinable())
        app.worker_thread.join();
}

void slice_worker_submit(AppState& app) {
    if (app.worker_busy.load()) {
        // Interrupt current render and wait
        alea_interrupt();
        return;
    }

    app.render_gen++;

    // Grid resolution: scale with cell count, cap at 800
    int ncells = app.sys ? (int)alea_cell_count(app.sys) : 0;
    int w = 400;
    if (ncells < 200) w = 800;
    else if (ncells < 1000) w = 600;
    int h = w;

    double u_min = app.view_cx - app.view_half_ext;
    double u_max = app.view_cx + app.view_half_ext;
    double v_min = app.view_cy - app.view_half_ext;
    double v_max = app.view_cy + app.view_half_ext;

    SliceRequest req;
    req.gen         = app.render_gen;
    req.axis        = app.axis;
    req.u_min       = u_min;
    req.u_max       = u_max;
    req.v_min       = v_min;
    req.v_max       = v_max;
    req.slice_value = app.slice_value;
    req.width       = w;
    req.height      = h;
    req.universe_depth = app.universe_depth;
    req.want_curves = app.show_curves;
    req.want_labels = app.show_labels;
    req.want_surface_labels = app.show_surface_labels;

    // Arbitrary slice plane
    req.arbitrary = app.arbitrary_slice;
    if (app.arbitrary_slice) {
        memcpy(req.origin, app.slice_origin, sizeof(req.origin));
        memcpy(req.normal, app.slice_normal, sizeof(req.normal));
        memcpy(req.up,     app.slice_up,     sizeof(req.up));
    }

    {
        std::lock_guard<std::mutex> lock(app.worker_mutex);
        app.pending_request = req;
        app.worker_busy.store(true);
        app.result_ready.store(false);
    }
}

// ---------------------------------------------------------------------------
// Recolor pixels from stored cell/material ID buffers (no re-slice needed)
// ---------------------------------------------------------------------------

void slice_recolor(AppState& app) {
    if (app.cell_ids.empty() || app.tex_w <= 0) return;

    int npix = app.tex_w * app.tex_h;

    // Build universe_id and density lookup from cell info
    std::unordered_map<int, int> univ_map;
    std::unordered_map<int, double> density_map;
    if (app.color_mode == COLOR_BY_UNIVERSE || app.color_mode == COLOR_BY_DENSITY) {
        int nc = (int)alea_cell_count(app.sys);
        for (int i = 0; i < nc; i++) {
            alea_cell_info_t ci;
            if (alea_cell_get_info(app.sys, (size_t)i, &ci) == 0) {
                univ_map[ci.cell_id] = ci.universe_id;
                density_map[ci.cell_id] = ci.density;
            }
        }
    }

    // Convert to RGBA pixels
    app.pixels.resize(npix * 4);
    uint8_t* px = app.pixels.data();

    for (int i = 0; i < npix; i++) {
        int mid = app.material_ids[i];

        // Hidden material -> render as white (void)
        if (!app.hidden_materials.empty() && app.hidden_materials.count(mid)) {
            px[i*4+0] = 255;
            px[i*4+1] = 255;
            px[i*4+2] = 255;
            px[i*4+3] = 255;
            continue;
        }

        uint8_t col[3];

        switch (app.color_mode) {
            case COLOR_BY_CELL:
                color_for_id(app.cell_ids[i], col);
                break;
            case COLOR_BY_MATERIAL:
                color_for_id(mid, col);
                break;
            case COLOR_BY_UNIVERSE: {
                auto it = univ_map.find(app.cell_ids[i]);
                int uid = (it != univ_map.end()) ? it->second : 0;
                color_for_id(uid, col);
                break;
            }
            case COLOR_BY_DENSITY: {
                auto it = density_map.find(app.cell_ids[i]);
                double d = (it != density_map.end()) ? it->second : 0.0;
                color_for_density(d, col);
                break;
            }
        }

        // Error overlay
        if (app.show_errors && i < (int)app.slice_errors.size()) {
            uint8_t err = app.slice_errors[i];
            if (err & 0x02) {
                col[0] = (uint8_t)std::min(255, col[0] + 80);
                col[1] = (uint8_t)(col[1] / 2);
                col[2] = (uint8_t)(col[2] / 2);
            } else if (err & 0x01) {
                col[0] = 200;
                col[1] = 120;
                col[2] = 40;
            }
        }

        px[i*4+0] = col[0];
        px[i*4+1] = col[1];
        px[i*4+2] = col[2];
        px[i*4+3] = 255;
    }

    // Draw cell boundary contours (pixel-based) if contours enabled
    if (app.show_contours) {
        for (int y = 0; y < app.tex_h; y++) {
            for (int x = 0; x < app.tex_w; x++) {
                int idx = y * app.tex_w + x;
                int cid = app.cell_ids[idx];
                int mid = app.material_ids[idx];

                // Skip contours for hidden materials
                bool self_hidden = !app.hidden_materials.empty() && app.hidden_materials.count(mid);
                if (self_hidden) continue;

                bool boundary = false;
                if (x > 0) {
                    int nid = app.cell_ids[idx-1];
                    if (nid != cid) {
                        int nmid = app.material_ids[idx-1];
                        bool neighbor_hidden = !app.hidden_materials.empty() && app.hidden_materials.count(nmid);
                        if (!neighbor_hidden) boundary = true;
                    }
                }
                if (!boundary && x < app.tex_w-1) {
                    int nid = app.cell_ids[idx+1];
                    if (nid != cid) {
                        int nmid = app.material_ids[idx+1];
                        bool neighbor_hidden = !app.hidden_materials.empty() && app.hidden_materials.count(nmid);
                        if (!neighbor_hidden) boundary = true;
                    }
                }
                if (!boundary && y > 0) {
                    int nid = app.cell_ids[idx-app.tex_w];
                    if (nid != cid) {
                        int nmid = app.material_ids[idx-app.tex_w];
                        bool neighbor_hidden = !app.hidden_materials.empty() && app.hidden_materials.count(nmid);
                        if (!neighbor_hidden) boundary = true;
                    }
                }
                if (!boundary && y < app.tex_h-1) {
                    int nid = app.cell_ids[idx+app.tex_w];
                    if (nid != cid) {
                        int nmid = app.material_ids[idx+app.tex_w];
                        bool neighbor_hidden = !app.hidden_materials.empty() && app.hidden_materials.count(nmid);
                        if (!neighbor_hidden) boundary = true;
                    }
                }

                if (boundary) {
                    px[idx*4+0] = 0;
                    px[idx*4+1] = 0;
                    px[idx*4+2] = 0;
                    px[idx*4+3] = 255;
                }
            }
        }
    }

    // Upload texture
    if (!app.texture_id) {
        glGenTextures(1, &app.texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, app.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app.tex_w, app.tex_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, app.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ---------------------------------------------------------------------------
// Consume result on main thread
// ---------------------------------------------------------------------------

void slice_worker_consume(AppState& app) {
    SliceResult res;
    {
        std::lock_guard<std::mutex> lock(app.worker_mutex);
        if (!app.result_ready.load()) return;
        res = std::move(app.pending_result);
        app.result_ready.store(false);
    }

    // Stale check
    if (res.gen != app.render_gen) {
        app.needs_rerender = true;
        return;
    }

    app.cell_ids     = std::move(res.cell_ids);
    app.material_ids = std::move(res.material_ids);
    app.slice_errors = std::move(res.errors);
    app.slice_curves = std::move(res.curves);
    app.slice_labels = std::move(res.labels);
    app.slice_surface_labels = std::move(res.surface_labels);
    app.tex_w = res.width;
    app.tex_h = res.height;

    slice_recolor(app);
}
