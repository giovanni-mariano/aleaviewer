#include "app.h"
#include <SDL.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Camera helpers
// ---------------------------------------------------------------------------

static void compute_camera(const RaycastState& rc, double pos[3],
                           double fwd[3], double right[3], double up[3]) {
    double cy = cos(rc.cam_yaw),   sy = sin(rc.cam_yaw);
    double cp = cos(rc.cam_pitch), sp = sin(rc.cam_pitch);

    pos[0] = rc.cam_target[0] + rc.cam_distance * cp * cy;
    pos[1] = rc.cam_target[1] + rc.cam_distance * cp * sy;
    pos[2] = rc.cam_target[2] + rc.cam_distance * sp;

    double dx = rc.cam_target[0] - pos[0];
    double dy = rc.cam_target[1] - pos[1];
    double dz = rc.cam_target[2] - pos[2];
    double len = sqrt(dx*dx + dy*dy + dz*dz);
    if (len < 1e-12) len = 1.0;
    fwd[0] = dx/len; fwd[1] = dy/len; fwd[2] = dz/len;

    right[0] =  fwd[1];
    right[1] = -fwd[0];
    right[2] =  0.0;
    double rlen = sqrt(right[0]*right[0] + right[1]*right[1]);
    if (rlen < 1e-12) {
        right[0] = 1.0; right[1] = 0.0; right[2] = 0.0;
    } else {
        right[0] /= rlen; right[1] /= rlen;
    }

    up[0] = right[1]*fwd[2] - right[2]*fwd[1];
    up[1] = right[2]*fwd[0] - right[0]*fwd[2];
    up[2] = right[0]*fwd[1] - right[1]*fwd[0];
}

// ---------------------------------------------------------------------------
// Render function — called from the single worker thread
// ---------------------------------------------------------------------------

void raycast_worker_render(AppState* app) {
    RaycastRequest req;
    {
        std::lock_guard<std::mutex> lock(app->rc.worker_mutex);
        req = app->rc.pending_request;
    }

    int w = req.width, h = req.height;
    int npix = w * h;

    RaycastResult res;
    res.gen    = req.gen;
    res.width  = w;
    res.height = h;
    res.pixels.resize(npix * 4);
    res.cell_ids.resize(npix);

    alea_clear_interrupt();

    // Warm-up: single-threaded ray to force cache construction before OpenMP
    {
        double dummy_t;
        alea_ray_first_cell(app->sys,
            req.cam_pos[0], req.cam_pos[1], req.cam_pos[2],
            req.cam_fwd[0], req.cam_fwd[1], req.cam_fwd[2],
            0.0, &dummy_t);
    }

    double half_h = tan(req.fov_y * 0.5);
    double aspect = (double)w / (double)h;
    double half_w = half_h * aspect;

    uint8_t* px = res.pixels.data();
    int* cids = res.cell_ids.data();

    // Pre-build cell_id → material_id lookup (few cells, avoids per-thread caches)
    std::unordered_map<int, int> mat_map;
    if (req.color_mode == COLOR_BY_MATERIAL) {
        int nc = (int)alea_cell_count(app->sys);
        for (int i = 0; i < nc; i++) {
            alea_cell_info_t ci;
            if (alea_cell_get_info(app->sys, (size_t)i, &ci) == 0)
                mat_map[ci.cell_id] = ci.material_id;
        }
    }

    #pragma omp parallel for schedule(dynamic, 4)
    for (int y = 0; y < h; y++) {
        if (alea_interrupted()) continue;

        double vy = half_h * (1.0 - 2.0 * ((double)y + 0.5) / (double)h);

        for (int x = 0; x < w; x++) {
            double vx = half_w * (2.0 * ((double)x + 0.5) / (double)w - 1.0);

            double dx = req.cam_fwd[0] + vx * req.cam_right[0] + vy * req.cam_up[0];
            double dy = req.cam_fwd[1] + vx * req.cam_right[1] + vy * req.cam_up[1];
            double dz = req.cam_fwd[2] + vx * req.cam_right[2] + vy * req.cam_up[2];

            int idx = y * w + x;
            double t = 0;
            int cell_id = alea_ray_first_cell(app->sys,
                req.cam_pos[0], req.cam_pos[1], req.cam_pos[2],
                dx, dy, dz, 0.0, &t);

            if (cell_id > 0) {
                int color_id = cell_id;
                if (req.color_mode == COLOR_BY_MATERIAL) {
                    auto it = mat_map.find(cell_id);
                    if (it != mat_map.end()) color_id = it->second;
                }

                uint8_t col[3];
                color_for_id(color_id, col);

                double depth_factor = 1.0 / (1.0 + 0.003 * t);
                px[idx*4+0] = (uint8_t)(col[0] * depth_factor);
                px[idx*4+1] = (uint8_t)(col[1] * depth_factor);
                px[idx*4+2] = (uint8_t)(col[2] * depth_factor);
                px[idx*4+3] = 255;
                cids[idx] = cell_id;
            } else {
                px[idx*4+0] = 30; px[idx*4+1] = 30;
                px[idx*4+2] = 32; px[idx*4+3] = 255;
                cids[idx] = -1;
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(app->rc.worker_mutex);
        app->rc.pending_result = std::move(res);
        app->rc.result_ready.store(true);
        app->rc.worker_busy.store(false);
    }
}

// ---------------------------------------------------------------------------
// Public interface (no thread — called from single worker in slice_worker.cpp)
// ---------------------------------------------------------------------------

void raycast_worker_submit(AppState& app) {
    if (app.rc.worker_busy.load()) {
        alea_interrupt();
        return;
    }

    app.rc.render_gen++;

    int w, h;
    if (app.rc.save_pending) {
        w = app.rc.save_w;
        h = app.rc.save_h;
    } else {
        w = app.rc.dragging ? 128 : 400;
        h = w;
    }

    double pos[3], fwd[3], right[3], up[3];
    compute_camera(app.rc, pos, fwd, right, up);

    RaycastRequest req{};
    req.gen = app.rc.render_gen;
    req.width = w;
    req.height = h;
    memcpy(req.cam_pos, pos, sizeof(pos));
    memcpy(req.cam_fwd, fwd, sizeof(fwd));
    memcpy(req.cam_right, right, sizeof(right));
    memcpy(req.cam_up, up, sizeof(up));
    req.fov_y = app.rc.cam_fov * (M_PI / 180.0);
    req.color_mode = app.color_mode;

    {
        std::lock_guard<std::mutex> lock(app.rc.worker_mutex);
        app.rc.pending_request = req;
        app.rc.worker_busy.store(true);
        app.rc.result_ready.store(false);
    }
}

void raycast_worker_consume(AppState& app) {
    RaycastResult res;
    {
        std::lock_guard<std::mutex> lock(app.rc.worker_mutex);
        if (!app.rc.result_ready.load()) return;
        res = std::move(app.rc.pending_result);
        app.rc.result_ready.store(false);
    }

    if (res.gen != app.rc.render_gen) {
        app.rc.needs_rerender = true;
        return;
    }

    app.rc.pixels   = std::move(res.pixels);
    app.rc.cell_ids = std::move(res.cell_ids);
    app.rc.tex_w = res.width;
    app.rc.tex_h = res.height;

    // Save to file if requested
    if (app.rc.save_pending) {
        app.rc.save_pending = false;
        const std::string& path = app.rc.save_path;
        int sw = app.rc.tex_w, sh = app.rc.tex_h;
        const uint8_t* px = app.rc.pixels.data();

        bool ok = false;
        bool is_bmp = (path.size() > 4 && path.substr(path.size()-4) == ".bmp");

        if (is_bmp) {
            // BMP (24-bit, bottom-up)
            FILE* f = fopen(path.c_str(), "wb");
            if (f) {
                int row_bytes = sw * 3;
                int pad = (4 - (row_bytes % 4)) % 4;
                int data_size = (row_bytes + pad) * sh;
                int file_size = 54 + data_size;

                uint8_t hdr[54] = {};
                hdr[0] = 'B'; hdr[1] = 'M';
                hdr[2] = file_size & 0xff; hdr[3] = (file_size>>8)&0xff;
                hdr[4] = (file_size>>16)&0xff; hdr[5] = (file_size>>24)&0xff;
                hdr[10] = 54;
                hdr[14] = 40;
                hdr[18] = sw&0xff; hdr[19] = (sw>>8)&0xff;
                hdr[20] = (sw>>16)&0xff; hdr[21] = (sw>>24)&0xff;
                hdr[22] = sh&0xff; hdr[23] = (sh>>8)&0xff;
                hdr[24] = (sh>>16)&0xff; hdr[25] = (sh>>24)&0xff;
                hdr[26] = 1; hdr[28] = 24;
                hdr[34] = data_size&0xff; hdr[35] = (data_size>>8)&0xff;
                hdr[36] = (data_size>>16)&0xff; hdr[37] = (data_size>>24)&0xff;

                fwrite(hdr, 1, 54, f);
                uint8_t padding[3] = {0,0,0};
                for (int y = sh - 1; y >= 0; y--) {
                    for (int x = 0; x < sw; x++) {
                        int i = (y * sw + x) * 4;
                        uint8_t bgr[3] = { px[i+2], px[i+1], px[i+0] };
                        fwrite(bgr, 1, 3, f);
                    }
                    if (pad) fwrite(padding, 1, pad, f);
                }
                fclose(f);
                ok = true;
            }
        } else {
            // PPM (P6)
            FILE* f = fopen(path.c_str(), "wb");
            if (f) {
                fprintf(f, "P6\n%d %d\n255\n", sw, sh);
                for (int i = 0; i < sw * sh; i++) {
                    fwrite(&px[i*4], 1, 3, f);
                }
                fclose(f);
                ok = true;
            }
        }

        if (ok)
            app_log(app, "Saved %dx%d render to '%s'", sw, sh, path.c_str());
        else
            app_log_error(app, "Failed to write '%s'", path.c_str());
    }

    // Upload texture
    if (!app.rc.texture_id) {
        glGenTextures(1, &app.rc.texture_id);
    }
    glBindTexture(GL_TEXTURE_2D, app.rc.texture_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, app.rc.tex_w, app.rc.tex_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, app.rc.pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}
