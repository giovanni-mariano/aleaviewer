#include "app.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Deterministic color palette (hashed from id)
// ---------------------------------------------------------------------------

static const uint8_t palette[][3] = {
    {102,194,165}, {252,141, 98}, {141,160,203}, {231,138,195},
    {166,216, 84}, {255,217, 47}, {229,196,148}, {179,179,179},
    { 228, 26, 28}, {  55,126,184}, {  77,175, 74}, {152, 78,163},
    {255,127,  0}, {255,255, 51}, {166, 86, 40}, {247,129,191},
    { 27,158,119}, {217, 95,  2}, {117,112,179}, {231, 41,138},
    {102,166, 30}, {230,171,  2}, {166,118, 29}, {102,102,102},
};
static const int palette_size = sizeof(palette) / sizeof(palette[0]);

void color_for_id(int id, uint8_t out[3]) {
    if (id <= 0) {
        out[0] = out[1] = out[2] = 255;  // void / undefined = white
        return;
    }
    // Simple hash to spread sequential IDs
    unsigned h = (unsigned)id;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    int idx = h % palette_size;
    out[0] = palette[idx][0];
    out[1] = palette[idx][1];
    out[2] = palette[idx][2];
}

// ---------------------------------------------------------------------------
// Density color: white -> yellow -> orange -> red -> dark red
// ---------------------------------------------------------------------------

void color_for_density(double density, uint8_t out[3]) {
    if (density <= 0.0) {
        out[0] = out[1] = out[2] = 40;
        return;
    }
    // Map density to 0..1 range using log scale (typical range 0.001 to 20 g/cc)
    double t = (log10(density) + 3.0) / 4.3; // -3..1.3 -> 0..1
    t = std::clamp(t, 0.0, 1.0);

    // 5-stop gradient: white -> yellow -> orange -> red -> dark red
    struct { double t; uint8_t r, g, b; } stops[] = {
        {0.0,  255, 255, 255},
        {0.25, 255, 255,  80},
        {0.5,  255, 160,  40},
        {0.75, 220,  40,  20},
        {1.0,  100,  10,  10},
    };
    int n = 5;
    int i = 0;
    for (i = 0; i < n - 2; i++) {
        if (t <= stops[i+1].t) break;
    }
    double f = (t - stops[i].t) / (stops[i+1].t - stops[i].t);
    f = std::clamp(f, 0.0, 1.0);
    out[0] = (uint8_t)(stops[i].r + f * (stops[i+1].r - stops[i].r));
    out[1] = (uint8_t)(stops[i].g + f * (stops[i+1].g - stops[i].g));
    out[2] = (uint8_t)(stops[i].b + f * (stops[i+1].b - stops[i].b));
}

// ---------------------------------------------------------------------------
// app lifecycle
// ---------------------------------------------------------------------------

void app_init(AppState& app) {
    memset(app.input_buf, 0, sizeof(app.input_buf));
    commands_init();
    slice_worker_start(app);
}

void app_shutdown(AppState& app) {
    // Wait for any in-progress file load
    if (app.load_thread.joinable()) {
        alea_interrupt();
        app.load_thread.join();
    }
    if (app.loaded_sys) {
        alea_destroy(app.loaded_sys);
        app.loaded_sys = nullptr;
    }

    slice_worker_stop(app);
    if (app.texture_id) {
        glDeleteTextures(1, &app.texture_id);
        app.texture_id = 0;
    }
    if (app.sys) {
        alea_destroy(app.sys);
        app.sys = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Zoom to fit
// ---------------------------------------------------------------------------

void app_zoom_fit(AppState& app) {
    if (!app.sys) return;
    int nc = (int)alea_cell_count(app.sys);
    if (nc == 0) return;

    alea_cell_info_t ci;
    if (alea_cell_get_info(app.sys, 0, &ci) != 0) return;
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

    double u_min = xmin, u_max = xmax, v_min = ymin, v_max = ymax;
    switch (app.axis) {
        case SLICE_Z: u_min=xmin; u_max=xmax; v_min=ymin; v_max=ymax;
                      app.slice_value = (zmin+zmax)*0.5; break;
        case SLICE_X: u_min=ymin; u_max=ymax; v_min=zmin; v_max=zmax;
                      app.slice_value = (xmin+xmax)*0.5; break;
        case SLICE_Y: u_min=xmin; u_max=xmax; v_min=zmin; v_max=zmax;
                      app.slice_value = (ymin+ymax)*0.5; break;
    }
    app.view_cx = (u_min + u_max) * 0.5;
    app.view_cy = (v_min + v_max) * 0.5;
    app.view_half_ext = std::max(u_max - u_min, v_max - v_min) * 0.55;
    if (app.view_half_ext < 1.0) app.view_half_ext = 1.0;
    app_request_rerender(app);
}

// ---------------------------------------------------------------------------
// Load a geometry file (MCNP or OpenMC)
// ---------------------------------------------------------------------------

bool app_load_file(AppState& app, const std::string& path) {
    if (app.loading.load()) {
        app_log(app, "Already loading a file, please wait...");
        return false;
    }

    // Join previous load thread if any
    if (app.load_thread.joinable())
        app.load_thread.join();
    if (app.loaded_sys) {
        alea_destroy(app.loaded_sys);
        app.loaded_sys = nullptr;
    }

    app.load_path = path;
    app.load_error.clear();
    app.load_done.store(false);
    app.loading.store(true);

    app.load_thread = std::thread([&app, path]() {
        alea_clear_interrupt();

        // Determine format from extension
        alea_system_t* sys = nullptr;
        if (path.size() > 4 && path.substr(path.size()-4) == ".xml") {
            sys = alea_load_openmc(path.c_str());
        } else {
            sys = alea_load_mcnp(path.c_str());
        }

        if (!sys) {
            std::lock_guard<std::mutex> lock(app.load_mutex);
            app.load_error = alea_error() ? alea_error() : "Unknown error";
            app.load_done.store(true);
            return;
        }

        // Build indices (heavy work)
        alea_build_universe_index(sys);
        alea_build_spatial_index(sys);

        {
            std::lock_guard<std::mutex> lock(app.load_mutex);
            app.loaded_sys = sys;
        }
        app.load_done.store(true);
    });

    return true;
}

void app_load_consume(AppState& app) {
    app.load_done.store(false);

    // Join the loader thread
    if (app.load_thread.joinable())
        app.load_thread.join();

    std::lock_guard<std::mutex> lock(app.load_mutex);

    if (!app.load_error.empty()) {
        app_log_error(app, "Failed to load '%s': %s",
                      app.load_path.c_str(), app.load_error.c_str());
        app.loading.store(false);
        return;
    }

    // Swap in the new system
    if (app.sys) alea_destroy(app.sys);
    app.sys = app.loaded_sys;
    app.loaded_sys = nullptr;
    app.loaded_file = app.load_path;
    app.index_built = true;

    app_log(app, "Loaded '%s': %d cells, %d surfaces",
            app.load_path.c_str(),
            (int)alea_cell_count(app.sys),
            (int)alea_surface_count(app.sys));

    // Reset view state
    app.selected_cell_index = -1;
    app.selected_surface_index = -1;
    app.slice_curves.clear();
    app.slice_labels.clear();
    app.slice_surface_labels.clear();
    app.query_results.clear();
    app.hidden_materials.clear();

    // Zoom to fit
    app_zoom_fit(app);
    app_request_rerender(app);

    app.loading.store(false);
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void app_log(AppState& app, const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app.log_lines.push_back(std::string(buf));
    app.scroll_to_bottom = true;
}

void app_log_error(AppState& app, const char* fmt, ...) {
    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    app.log_lines.push_back(std::string("[ERROR] ") + buf);
    app.scroll_to_bottom = true;
}

void app_request_rerender(AppState& app) {
    app.needs_rerender = true;
    app.render_gen++;
}
