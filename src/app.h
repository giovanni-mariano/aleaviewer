#pragma once

#include <string>
#include <vector>
#include <set>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstring>
#include <GL/gl.h>

extern "C" {
#include "alea.h"
#include "alea_slice.h"
#include "alea_raycast.h"
}

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

enum SliceAxis { SLICE_Z = 0, SLICE_X, SLICE_Y };
enum ColorMode { COLOR_BY_CELL = 0, COLOR_BY_MATERIAL, COLOR_BY_UNIVERSE, COLOR_BY_DENSITY };

// ---------------------------------------------------------------------------
// Stored curve data (extracted from alea_curve_t for main thread use)
// ---------------------------------------------------------------------------

struct StoredCurve {
    alea_curve_type_t type;
    int surface_id;
    union {
        struct { double point[2]; double direction[2]; } line;
        struct { double center[2]; double radius; } circle;
        struct { double center[2]; double semi_a, semi_b, angle; } ellipse;
        struct { double vertices[16][2]; int count; int closed; } polygon;
        struct { double point1[2]; double point2[2]; double direction[2]; } parallel_lines;
    } data;
    double t_min, t_max;
};

// ---------------------------------------------------------------------------
// Stored label data
// ---------------------------------------------------------------------------

struct StoredLabel {
    int id;
    double world_u, world_v;   // world coordinates
};

// ---------------------------------------------------------------------------
// Query result from point query
// ---------------------------------------------------------------------------

struct QueryResult {
    double x, y, z;
    std::vector<alea_cell_hit_t> hits;
};

// ---------------------------------------------------------------------------
// Slice request/result (exchanged between main <-> worker)
// ---------------------------------------------------------------------------

struct SliceRequest {
    int gen;
    SliceAxis axis;
    double u_min, u_max, v_min, v_max;
    double slice_value;
    int width, height;
    int universe_depth;
    bool want_curves;
    bool want_labels;
    bool want_surface_labels;
    // Arbitrary slice plane
    bool arbitrary;
    double origin[3];
    double normal[3];
    double up[3];
};

struct SliceResult {
    int gen;
    int width, height;
    std::vector<int> cell_ids;
    std::vector<int> material_ids;
    std::vector<uint8_t> errors;
    std::vector<StoredCurve> curves;
    std::vector<StoredLabel> labels;
    std::vector<StoredLabel> surface_labels;
};

// ---------------------------------------------------------------------------
// Raycast request/result (exchanged between main <-> worker)
// ---------------------------------------------------------------------------

struct RaycastRequest {
    int gen;
    int width, height;
    double cam_pos[3];
    double cam_fwd[3];
    double cam_right[3];
    double cam_up[3];
    double fov_y;
    ColorMode color_mode;
};

struct RaycastResult {
    int gen;
    int width, height;
    std::vector<uint8_t> pixels;
    std::vector<int> cell_ids;
};

struct RaycastState {
    double cam_yaw      = 0.6;
    double cam_pitch    = 0.4;
    double cam_distance = 100.0;
    double cam_target[3] = {0, 0, 0};
    double cam_fov      = 60.0;

    bool dragging        = false;
    bool needs_rerender  = true;
    int  render_gen      = 0;

    // Save-to-file state
    bool save_pending    = false;
    int  save_w          = 0;
    int  save_h          = 0;
    std::string save_path;

    // Pixel buffers and texture
    std::vector<uint8_t> pixels;
    std::vector<int>     cell_ids;
    int    tex_w         = 0;
    int    tex_h         = 0;
    GLuint texture_id    = 0;

    // Worker communication
    std::atomic<bool> worker_busy{false};
    std::atomic<bool> result_ready{false};
    std::mutex        worker_mutex;
    RaycastRequest    pending_request{};
    RaycastResult     pending_result{};
};

// ---------------------------------------------------------------------------
// AppState -- single source of truth
// ---------------------------------------------------------------------------

struct AppState {
    // CSG system
    alea_system_t* sys = nullptr;
    std::string   loaded_file;
    bool          index_built = false;

    // Slice view
    SliceAxis axis      = SLICE_Z;
    double slice_value   = 0.0;
    double view_cx       = 0.0;
    double view_cy       = 0.0;
    double view_half_ext = 50.0;
    ColorMode color_mode = COLOR_BY_CELL;

    // Texture
    GLuint texture_id    = 0;
    int    tex_w         = 0;
    int    tex_h         = 0;
    std::vector<uint8_t> pixels;       // RGBA
    std::vector<int>     cell_ids;     // per-pixel cell id (from last result)
    std::vector<int>     material_ids; // per-pixel material id

    bool needs_rerender  = true;
    int  render_gen      = 0;

    // Cursor
    double cursor_u = 0.0, cursor_v = 0.0;
    int    cursor_cell = -1;
    int    cursor_mat  = 0;
    bool   cursor_in_viewport = false;

    // Tree panel
    int selected_cell_index = -1;
    int selected_surface_index = -1;
    int hovered_surface_id  = -1;

    // CLI
    char input_buf[1024] = {};
    std::vector<std::string> log_lines;
    std::vector<std::string> cmd_history;
    int history_pos = -1;
    bool scroll_to_bottom = false;

    // Async worker
    std::thread       worker_thread;
    std::atomic<bool> worker_busy{false};
    std::atomic<bool> result_ready{false};
    std::atomic<bool> worker_quit{false};
    std::mutex        worker_mutex;
    SliceRequest      pending_request{};
    SliceResult       pending_result{};

    // Whether the first layout has been applied
    bool first_frame = true;

    // --- New state fields ---

    // Layer visibility toggles
    bool show_contours = true;
    bool show_curves   = false;
    bool show_labels   = false;
    bool show_errors   = true;
    bool show_grid     = false;

    // Panel visibility
    bool show_left_panel   = true;
    bool show_right_panel  = true;
    bool show_bottom_panel = true;

    // Universe depth for slice (-1 = innermost)
    int universe_depth = -1;

    // Slice step size
    double slice_step = 1.0;

    // Query results storage
    std::vector<QueryResult> query_results;

    // Bottom panel tab: 0=Log, 1=Query Results
    int bottom_tab = 0;

    // Stored curves and labels from last slice result
    std::vector<StoredCurve> slice_curves;
    std::vector<StoredLabel> slice_labels;

    // Feature: Surface labels on curves
    bool show_surface_labels = false;
    std::vector<StoredLabel> slice_surface_labels;

    // Feature: Material visibility filter
    std::set<int> hidden_materials;
    std::vector<uint8_t> slice_errors; // stored for recolor path
    bool needs_recolor = false;

    // Feature: Arbitrary slice planes
    bool arbitrary_slice = false;
    double slice_origin[3] = {0, 0, 0};
    double slice_normal[3] = {0, 0, 1};
    double slice_up[3]     = {0, 1, 0};


    // Async file loading
    std::atomic<bool> loading{false};
    std::atomic<bool> load_done{false};
    std::mutex        load_mutex;
    alea_system_t*    loaded_sys = nullptr;
    std::string       load_path;
    std::string       load_error;
    std::thread       load_thread;

    // 3D raycast state
    RaycastState rc;
};

// ---------------------------------------------------------------------------
// Module functions
// ---------------------------------------------------------------------------

// app.cpp
void app_init(AppState& app);
void app_shutdown(AppState& app);
bool app_load_file(AppState& app, const std::string& path);
void app_load_consume(AppState& app);
void app_log(AppState& app, const char* fmt, ...);
void app_log_error(AppState& app, const char* fmt, ...);
void app_request_rerender(AppState& app);
void app_zoom_fit(AppState& app);

// Color helpers
void color_for_id(int id, uint8_t out[3]);
void color_for_density(double density, uint8_t out[3]);

// panel_slice.cpp
void panel_slice(AppState& app);

// panel_tree.cpp
void panel_tree(AppState& app);

// panel_properties.cpp
void panel_properties(AppState& app);

// panel_cli.cpp
void panel_cli(AppState& app);

// panel_status.cpp
void panel_status(AppState& app);

// commands.cpp
void commands_init();
void commands_execute(AppState& app, const char* line);

// panel_3d.cpp
void panel_3d(AppState& app);

// slice_worker.cpp
void slice_worker_start(AppState& app);
void slice_worker_stop(AppState& app);
void slice_worker_submit(AppState& app);
void slice_worker_consume(AppState& app);
void slice_recolor(AppState& app);

// raycast_worker.cpp
void raycast_worker_render(AppState* app);
void raycast_worker_submit(AppState& app);
void raycast_worker_consume(AppState& app);

