#include "app.h"
#include <SDL.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>

// ---------------------------------------------------------------------------
// Command registry
// ---------------------------------------------------------------------------

using CmdFunc = std::function<void(AppState&, const std::vector<std::string>&)>;

static std::map<std::string, CmdFunc> g_commands;
static std::map<std::string, std::string> g_help;

static void reg(const char* name, const char* help, CmdFunc func) {
    g_commands[name] = func;
    g_help[name] = help;
}

static std::vector<std::string> split_args(const char* line) {
    std::vector<std::string> args;
    std::istringstream iss(line);
    std::string token;
    while (iss >> token) args.push_back(token);
    return args;
}

// ---------------------------------------------------------------------------
// Primitive type name for info output
// ---------------------------------------------------------------------------

static const char* ptype_str(alea_primitive_type_t t) {
    switch (t) {
        case ALEA_PRIMITIVE_PLANE:       return "plane";
        case ALEA_PRIMITIVE_SPHERE:      return "sphere";
        case ALEA_PRIMITIVE_CYLINDER_X:  return "cylinder/x";
        case ALEA_PRIMITIVE_CYLINDER_Y:  return "cylinder/y";
        case ALEA_PRIMITIVE_CYLINDER_Z:  return "cylinder/z";
        case ALEA_PRIMITIVE_CONE_X:      return "cone/x";
        case ALEA_PRIMITIVE_CONE_Y:      return "cone/y";
        case ALEA_PRIMITIVE_CONE_Z:      return "cone/z";
        case ALEA_PRIMITIVE_RPP:         return "box (RPP)";
        case ALEA_PRIMITIVE_QUADRIC:     return "quadric (GQ)";
        case ALEA_PRIMITIVE_TORUS_X:     return "torus/x";
        case ALEA_PRIMITIVE_TORUS_Y:     return "torus/y";
        case ALEA_PRIMITIVE_TORUS_Z:     return "torus/z";
        case ALEA_PRIMITIVE_RCC:         return "RCC";
        case ALEA_PRIMITIVE_BOX: return "BOX";
        case ALEA_PRIMITIVE_TRC:         return "TRC";
        case ALEA_PRIMITIVE_ELL:         return "ELL";
        case ALEA_PRIMITIVE_REC:         return "REC";
        case ALEA_PRIMITIVE_WED:         return "WED";
        case ALEA_PRIMITIVE_RHP:         return "RHP";
        case ALEA_PRIMITIVE_ARB:         return "ARB";
        default:                        return "unknown";
    }
}

// ---------------------------------------------------------------------------
// Command implementations
// ---------------------------------------------------------------------------

static void cmd_help(AppState& app, const std::vector<std::string>& /*args*/) {
    app_log(app, "Available commands:");
    for (auto& [name, help] : g_help) {
        app_log(app, "  %-24s %s", name.c_str(), help.c_str());
    }
}

static void cmd_quit(AppState& /*app*/, const std::vector<std::string>& /*args*/) {
    // Post SDL_QUIT event
    SDL_Event ev;
    ev.type = SDL_QUIT;
    SDL_PushEvent(&ev);
}

static void cmd_load(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        app_log_error(app, "Usage: load <file>");
        return;
    }
    app_load_file(app, args[1]);
}

static void cmd_slice(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        app_log_error(app, "Usage: slice z|y|x [value]");
        return;
    }
    char c = args[1][0];
    if (c == 'z' || c == 'Z') app.axis = SLICE_Z;
    else if (c == 'x' || c == 'X') app.axis = SLICE_X;
    else if (c == 'y' || c == 'Y') app.axis = SLICE_Y;
    else {
        app_log_error(app, "Unknown axis '%s'", args[1].c_str());
        return;
    }
    if (args.size() >= 3) {
        app.slice_value = atof(args[2].c_str());
    }
    app_log(app, "Slice: axis=%c value=%.4f", c, app.slice_value);
    app_request_rerender(app);
}

static void cmd_zoom(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        app_log_error(app, "Usage: zoom <extent> | zoom fit");
        return;
    }
    if (args[1] == "fit") {
        app_zoom_fit(app);
        app_log(app, "Zoomed to fit.");
    } else {
        double ext = atof(args[1].c_str());
        if (ext <= 0) { app_log_error(app, "Invalid extent"); return; }
        app.view_half_ext = ext * 0.5;
        app_log(app, "Zoom extent: %.4f", ext);
        app_request_rerender(app);
    }
}

static void cmd_pan(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        app_log_error(app, "Usage: pan <u> <v>");
        return;
    }
    app.view_cx = atof(args[1].c_str());
    app.view_cy = atof(args[2].c_str());
    app_log(app, "Pan to (%.4f, %.4f)", app.view_cx, app.view_cy);
    app_request_rerender(app);
}

static void cmd_color(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 2) {
        app_log_error(app, "Usage: color cells|materials|universe|density");
        return;
    }
    if (args[1] == "cells" || args[1] == "cell") {
        app.color_mode = COLOR_BY_CELL;
        app_log(app, "Color mode: cells");
    } else if (args[1] == "materials" || args[1] == "material" || args[1] == "mat") {
        app.color_mode = COLOR_BY_MATERIAL;
        app_log(app, "Color mode: materials");
    } else if (args[1] == "universe" || args[1] == "univ") {
        app.color_mode = COLOR_BY_UNIVERSE;
        app_log(app, "Color mode: universe");
    } else if (args[1] == "density" || args[1] == "dens") {
        app.color_mode = COLOR_BY_DENSITY;
        app_log(app, "Color mode: density");
    } else {
        app_log_error(app, "Unknown color mode '%s'", args[1].c_str());
        return;
    }
    app_request_rerender(app);
}

static void cmd_select(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 3 || args[1] != "cell") {
        app_log_error(app, "Usage: select cell <id>");
        return;
    }
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }
    int cell_id = atoi(args[2].c_str());
    int idx = alea_cell_find(app.sys, cell_id);
    if (idx < 0) {
        app_log_error(app, "Cell %d not found.", cell_id);
        return;
    }
    app.selected_cell_index = idx;
    app.selected_surface_index = -1;
    app_log(app, "Selected cell %d (index %d)", cell_id, idx);
}

static void cmd_info(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        app_log_error(app, "Usage: info cell <id> | info surface <id>");
        return;
    }
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }

    if (args[1] == "cell") {
        int cell_id = atoi(args[2].c_str());
        alea_cell_info_t info;
        if (alea_cell_find_info(app.sys, cell_id, &info) != 0) {
            app_log_error(app, "Cell %d not found.", cell_id);
            return;
        }
        app_log(app, "Cell %d:", info.cell_id);
        app_log(app, "  Material: %d  Density: %.6g", info.material_id, info.density);
        app_log(app, "  Universe: %d  Fill: %d", info.universe_id, info.fill_universe);
        app_log(app, "  BBox: [%.4f,%.4f] x [%.4f,%.4f] x [%.4f,%.4f]",
                info.bbox.min_x, info.bbox.max_x,
                info.bbox.min_y, info.bbox.max_y,
                info.bbox.min_z, info.bbox.max_z);
        if (info.lat_type > 0) {
            app_log(app, "  Lattice: type=%d pitch=[%.4f,%.4f,%.4f]",
                    info.lat_type, info.lat_pitch[0], info.lat_pitch[1], info.lat_pitch[2]);
        }
    } else if (args[1] == "surface" || args[1] == "surf") {
        int surf_id = atoi(args[2].c_str());
        int idx = alea_surface_find(app.sys, surf_id);
        if (idx < 0) {
            app_log_error(app, "Surface %d not found.", surf_id);
            return;
        }
        int sid = 0;
        alea_primitive_type_t ptype = (alea_primitive_type_t)0;
        alea_node_id_t pos_node = 0;
        alea_surface_get(app.sys, (size_t)idx, &sid, &ptype, &pos_node, nullptr, nullptr);

        app_log(app, "Surface %d:", sid);
        app_log(app, "  Type: %s", ptype_str(ptype));

        alea_primitive_data_t pdata;
        if (alea_node_primitive_data(app.sys, pos_node, &pdata) == 0) {
            switch (ptype) {
                case ALEA_PRIMITIVE_PLANE:
                    app_log(app, "  Coeffs: %.6g %.6g %.6g %.6g",
                            pdata.plane.a, pdata.plane.b, pdata.plane.c, pdata.plane.d);
                    break;
                case ALEA_PRIMITIVE_SPHERE:
                    app_log(app, "  Center: (%.6g, %.6g, %.6g)  R=%.6g",
                            pdata.sphere.center_x, pdata.sphere.center_y,
                            pdata.sphere.center_z, pdata.sphere.radius);
                    break;
                case ALEA_PRIMITIVE_CYLINDER_Z:
                    app_log(app, "  Center: (%.6g, %.6g)  R=%.6g",
                            pdata.cyl_z.center_x, pdata.cyl_z.center_y, pdata.cyl_z.radius);
                    break;
                default:
                    break;
            }
        }
    } else {
        app_log_error(app, "Usage: info cell <id> | info surface <id>");
    }
}

static void cmd_point(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 4) {
        app_log_error(app, "Usage: point <x> <y> <z>");
        return;
    }
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }

    double x = atof(args[1].c_str());
    double y = atof(args[2].c_str());
    double z = atof(args[3].c_str());

    alea_cell_hit_t hits[32];
    int nhits = alea_find_all_cells(app.sys, x, y, z, hits, 32);

    if (nhits <= 0) {
        app_log(app, "Point (%.4f, %.4f, %.4f): no cells found", x, y, z);
    } else {
        app_log(app, "Point (%.4f, %.4f, %.4f): %d cell(s)", x, y, z, nhits);
        for (int i = 0; i < nhits; i++) {
            app_log(app, "  Cell %d  mat=%d  u=%d  depth=%d",
                    hits[i].cell_id, hits[i].material_id,
                    hits[i].universe_id, hits[i].depth);
        }
    }

    // Also store as query result
    QueryResult qr;
    qr.x = x; qr.y = y; qr.z = z;
    for (int i = 0; i < nhits; i++) qr.hits.push_back(hits[i]);
    app.query_results.push_back(std::move(qr));
}

static void cmd_export(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        app_log_error(app, "Usage: export mcnp|openmc <file>");
        return;
    }
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }

    int rc;
    if (args[1] == "mcnp") {
        rc = alea_export_mcnp(app.sys, args[2].c_str());
    } else if (args[1] == "openmc") {
        rc = alea_export_openmc(app.sys, args[2].c_str());
    } else {
        app_log_error(app, "Unknown format '%s'. Use mcnp or openmc.", args[1].c_str());
        return;
    }
    if (rc == 0) {
        app_log(app, "Exported to '%s'", args[2].c_str());
    } else {
        app_log_error(app, "Export failed: %s", alea_error());
    }
}

static void cmd_stats(AppState& app, const std::vector<std::string>& /*args*/) {
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }
    app_log(app, "System statistics:");
    app_log(app, "  Cells:     %d", (int)alea_cell_count(app.sys));
    app_log(app, "  Surfaces:  %d", (int)alea_surface_count(app.sys));
    app_log(app, "  Universes: %d", (int)alea_universe_count(app.sys));
    app_log(app, "  File:      %s", app.loaded_file.c_str());

    // Compute overall bbox
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
        app_log(app, "  BBox:      [%.2f, %.2f] x [%.2f, %.2f] x [%.2f, %.2f]",
                xmin, xmax, ymin, ymax, zmin, zmax);
    }
}

static void cmd_origin(AppState& app, const std::vector<std::string>& args) {
    double x = 0, y = 0, z = 0;
    if (args.size() >= 4) {
        x = atof(args[1].c_str());
        y = atof(args[2].c_str());
        z = atof(args[3].c_str());
    }
    switch (app.axis) {
        case SLICE_Z: app.view_cx = x; app.view_cy = y; app.slice_value = z; break;
        case SLICE_X: app.view_cx = y; app.view_cy = z; app.slice_value = x; break;
        case SLICE_Y: app.view_cx = x; app.view_cy = z; app.slice_value = y; break;
    }
    app_log(app, "View centered at (%.4f, %.4f, %.4f)", x, y, z);
    app_request_rerender(app);
}

static void cmd_find_overlaps(AppState& app, const std::vector<std::string>& /*args*/) {
    if (!app.sys) { app_log_error(app, "No geometry loaded."); return; }

    int pairs[256];
    int np = alea_find_overlaps(app.sys, pairs, 128);
    if (np < 0) {
        app_log_error(app, "Find overlaps failed: %s", alea_error());
    } else if (np == 0) {
        app_log(app, "No overlapping cell pairs found.");
    } else {
        app_log(app, "Found %d overlapping cell pair(s):", np);
        for (int i = 0; i < np; i++) {
            app_log(app, "  Cell %d <-> Cell %d", pairs[i*2], pairs[i*2+1]);
        }
    }
}

// ---------------------------------------------------------------------------
// Arbitrary slice plane command
// ---------------------------------------------------------------------------

static void cmd_slice_plane(AppState& app, const std::vector<std::string>& args) {
    if (args.size() < 10) {
        app_log_error(app, "Usage: slice_plane ox oy oz nx ny nz ux uy uz");
        return;
    }
    app.arbitrary_slice = true;
    app.slice_origin[0] = atof(args[1].c_str());
    app.slice_origin[1] = atof(args[2].c_str());
    app.slice_origin[2] = atof(args[3].c_str());
    app.slice_normal[0] = atof(args[4].c_str());
    app.slice_normal[1] = atof(args[5].c_str());
    app.slice_normal[2] = atof(args[6].c_str());
    app.slice_up[0]     = atof(args[7].c_str());
    app.slice_up[1]     = atof(args[8].c_str());
    app.slice_up[2]     = atof(args[9].c_str());
    app_log(app, "Arbitrary slice: origin=(%.4f,%.4f,%.4f) normal=(%.4f,%.4f,%.4f) up=(%.4f,%.4f,%.4f)",
            app.slice_origin[0], app.slice_origin[1], app.slice_origin[2],
            app.slice_normal[0], app.slice_normal[1], app.slice_normal[2],
            app.slice_up[0], app.slice_up[1], app.slice_up[2]);
    app_request_rerender(app);
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void commands_init() {
    reg("help",             "Show this help",                      cmd_help);
    reg("quit",             "Exit the editor",                     cmd_quit);
    reg("load",             "load <file>         Load geometry",   cmd_load);
    reg("slice",            "slice z|y|x [val]   Set slice",       cmd_slice);
    reg("zoom",             "zoom <ext>|fit      Set zoom",        cmd_zoom);
    reg("pan",              "pan <u> <v>         Center viewport",  cmd_pan);
    reg("origin",           "origin [x y z]      Center view at point", cmd_origin);
    reg("color",            "color cells|materials|universe|density", cmd_color);
    reg("select",           "select cell <id>    Select cell",      cmd_select);
    reg("info",             "info cell|surface <id>  Show details", cmd_info);
    reg("point",            "point <x> <y> <z>   Query point",     cmd_point);
    reg("export",           "export mcnp|openmc <file>  Export",    cmd_export);
    reg("stats",            "stats               System summary",   cmd_stats);
    reg("find_overlaps",    "find_overlaps        Find cell overlaps", cmd_find_overlaps);
    reg("slice_plane",     "slice_plane ox oy oz nx ny nz ux uy uz", cmd_slice_plane);
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

void commands_execute(AppState& app, const char* line) {
    auto args = split_args(line);
    if (args.empty()) return;

    std::string cmd = args[0];
    // Lowercase
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    auto it = g_commands.find(cmd);
    if (it != g_commands.end()) {
        it->second(app, args);
    } else {
        app_log_error(app, "Unknown command: '%s'. Type 'help' for commands.", cmd.c_str());
    }
}
