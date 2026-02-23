#include "app.h"

#include <SDL.h>
#include <GL/gl.h>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Theme setup — modern dark theme with teal accent
// ---------------------------------------------------------------------------

static void setup_theme(float dpi_scale) {
    ImGuiStyle& style = ImGui::GetStyle();

    // --- Style geometry ---
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.TabRounding       = 4.0f;
    style.GrabRounding      = 4.0f;
    style.ScrollbarRounding = 6.0f;

    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 5);
    style.ItemInnerSpacing  = ImVec2(6, 4);
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.IndentSpacing     = 16.0f;

    // --- Color palette ---
    // Background tones
    ImVec4 bg_darkest    = ImVec4(0.098f, 0.098f, 0.125f, 1.0f);  // #191920
    ImVec4 bg_dark       = ImVec4(0.114f, 0.114f, 0.145f, 1.0f);  // #1D1D25
    ImVec4 bg_mid        = ImVec4(0.145f, 0.149f, 0.161f, 1.0f);  // #252629
    ImVec4 bg_light      = ImVec4(0.180f, 0.184f, 0.200f, 1.0f);  // #2E2F33
    ImVec4 bg_active     = ImVec4(0.235f, 0.235f, 0.259f, 1.0f);  // #3C3C42

    // Text
    ImVec4 text_primary  = ImVec4(0.878f, 0.886f, 0.910f, 1.0f);  // #E0E2E8
    ImVec4 text_disabled = ImVec4(0.502f, 0.518f, 0.557f, 1.0f);  // #80848E

    // Accent (teal)
    ImVec4 accent        = ImVec4(0.180f, 0.588f, 0.678f, 1.0f);  // #2E96AD
    ImVec4 accent_hover  = ImVec4(0.220f, 0.667f, 0.749f, 1.0f);  // #38AABF
    ImVec4 accent_bright = ImVec4(0.306f, 0.788f, 0.831f, 1.0f);  // #4EC9D4

    // Buttons
    ImVec4 btn           = ImVec4(0.180f, 0.188f, 0.216f, 1.0f);  // #2E3037
    ImVec4 btn_hover     = ImVec4(0.239f, 0.247f, 0.278f, 1.0f);  // #3D3F47
    ImVec4 btn_active    = ImVec4(0.290f, 0.298f, 0.333f, 1.0f);  // #4A4C55

    // Border
    ImVec4 border        = ImVec4(0.200f, 0.204f, 0.228f, 1.0f);  // #333339
    ImVec4 border_shadow = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Scrollbar
    ImVec4 scrollbar_bg  = ImVec4(0.110f, 0.110f, 0.140f, 1.0f);
    ImVec4 scrollbar_grab= ImVec4(0.240f, 0.244f, 0.270f, 1.0f);

    ImVec4* c = style.Colors;

    // Window
    c[ImGuiCol_WindowBg]             = bg_darkest;
    c[ImGuiCol_ChildBg]              = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]              = ImVec4(0.118f, 0.118f, 0.149f, 0.96f);

    // Text
    c[ImGuiCol_Text]                 = text_primary;
    c[ImGuiCol_TextDisabled]         = text_disabled;

    // Borders
    c[ImGuiCol_Border]               = border;
    c[ImGuiCol_BorderShadow]         = border_shadow;

    // Frame (input, checkbox, slider backgrounds)
    c[ImGuiCol_FrameBg]              = bg_mid;
    c[ImGuiCol_FrameBgHovered]       = bg_light;
    c[ImGuiCol_FrameBgActive]        = bg_active;

    // Title bar
    c[ImGuiCol_TitleBg]              = bg_darkest;
    c[ImGuiCol_TitleBgActive]        = bg_dark;
    c[ImGuiCol_TitleBgCollapsed]     = bg_darkest;

    // Menu bar
    c[ImGuiCol_MenuBarBg]            = bg_dark;

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = scrollbar_bg;
    c[ImGuiCol_ScrollbarGrab]        = scrollbar_grab;
    c[ImGuiCol_ScrollbarGrabHovered] = btn_hover;
    c[ImGuiCol_ScrollbarGrabActive]  = btn_active;

    // Checkmark / slider grab
    c[ImGuiCol_CheckMark]            = accent_bright;
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent_hover;

    // Buttons
    c[ImGuiCol_Button]               = btn;
    c[ImGuiCol_ButtonHovered]        = btn_hover;
    c[ImGuiCol_ButtonActive]         = btn_active;

    // Headers (collapsing headers, selectable, menu items)
    c[ImGuiCol_Header]               = ImVec4(0.180f, 0.188f, 0.216f, 0.6f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.180f, 0.588f, 0.678f, 0.35f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.180f, 0.588f, 0.678f, 0.50f);

    // Separator
    c[ImGuiCol_Separator]            = border;
    c[ImGuiCol_SeparatorHovered]     = accent;
    c[ImGuiCol_SeparatorActive]      = accent_hover;

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.180f, 0.588f, 0.678f, 0.15f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.180f, 0.588f, 0.678f, 0.40f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.180f, 0.588f, 0.678f, 0.70f);

    // Tabs
    c[ImGuiCol_Tab]                  = bg_dark;
    c[ImGuiCol_TabHovered]           = ImVec4(0.180f, 0.588f, 0.678f, 0.35f);
    c[ImGuiCol_TabSelected]          = ImVec4(0.145f, 0.420f, 0.490f, 1.0f);
    c[ImGuiCol_TabSelectedOverline]  = accent_bright;
    c[ImGuiCol_TabDimmed]            = bg_darkest;
    c[ImGuiCol_TabDimmedSelected]    = bg_mid;
    c[ImGuiCol_TabDimmedSelectedOverline] = text_disabled;

    // Docking
    c[ImGuiCol_DockingPreview]       = ImVec4(0.180f, 0.588f, 0.678f, 0.40f);
    c[ImGuiCol_DockingEmptyBg]       = bg_darkest;

    // Tables
    c[ImGuiCol_TableHeaderBg]        = bg_mid;
    c[ImGuiCol_TableBorderStrong]    = border;
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.200f, 0.204f, 0.228f, 0.5f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1, 1, 1, 0.02f);

    // Text input / selection
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.180f, 0.588f, 0.678f, 0.30f);

    // Drag/drop target
    c[ImGuiCol_DragDropTarget]       = accent_bright;

    // Nav
    c[ImGuiCol_NavCursor]            = accent;
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(1, 1, 1, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.8f, 0.8f, 0.8f, 0.2f);

    // Modal dimming
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0, 0, 0, 0.55f);

    // Plot
    c[ImGuiCol_PlotLines]            = accent;
    c[ImGuiCol_PlotLinesHovered]     = accent_bright;
    c[ImGuiCol_PlotHistogram]        = accent;
    c[ImGuiCol_PlotHistogramHovered] = accent_bright;

    // Input text cursor
    c[ImGuiCol_InputTextCursor] = text_primary;

    // Scale
    style.ScaleAllSizes(dpi_scale);
}

// ---------------------------------------------------------------------------
// File dialog via zenity (returns empty on cancel)
// ---------------------------------------------------------------------------

static std::string file_dialog_open(const char* title, const char* filter) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title='%s' --file-filter='%s' 2>/dev/null",
             title, filter);
    FILE* f = popen(cmd, "r");
    if (!f) return {};
    char buf[4096];
    std::string result;
    while (fgets(buf, sizeof(buf), f)) result += buf;
    pclose(f);
    // Trim trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

static std::string file_dialog_save(const char* title, const char* filter) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --save --confirm-overwrite --title='%s' --file-filter='%s' 2>/dev/null",
             title, filter);
    FILE* f = popen(cmd, "r");
    if (!f) return {};
    char buf[4096];
    std::string result;
    while (fgets(buf, sizeof(buf), f)) result += buf;
    pclose(f);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

static void render_menu_bar(AppState& app, bool& quit) {
    if (!ImGui::BeginMenuBar()) return;

    // --- File ---
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open MCNP...", "Ctrl+O")) {
            std::string path = file_dialog_open("Open MCNP File", "MCNP files | *");
            if (!path.empty()) app_load_file(app, path);
        }
        if (ImGui::MenuItem("Open OpenMC...")) {
            std::string path = file_dialog_open("Open OpenMC File", "*.xml");
            if (!path.empty()) app_load_file(app, path);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export MCNP...", nullptr, false, app.sys != nullptr)) {
            std::string path = file_dialog_save("Export MCNP", "*.inp");
            if (!path.empty() && app.sys) {
                if (alea_export_mcnp(app.sys, path.c_str()) == 0)
                    app_log(app, "Exported to '%s'", path.c_str());
                else
                    app_log_error(app, "Export failed: %s", alea_error());
            }
        }
        if (ImGui::MenuItem("Export OpenMC...", nullptr, false, app.sys != nullptr)) {
            std::string path = file_dialog_save("Export OpenMC", "*.xml");
            if (!path.empty() && app.sys) {
                if (alea_export_openmc(app.sys, path.c_str()) == 0)
                    app_log(app, "Exported to '%s'", path.c_str());
                else
                    app_log_error(app, "Export failed: %s", alea_error());
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
            quit = true;
        }
        ImGui::EndMenu();
    }

    // --- View ---
    if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Browser", nullptr, &app.show_left_panel);
        ImGui::MenuItem("Properties", nullptr, &app.show_right_panel);
        ImGui::MenuItem("Console", nullptr, &app.show_bottom_panel);
        ImGui::Separator();
        if (ImGui::MenuItem("Zoom to Fit", "Home", false, app.sys != nullptr)) {
            app_zoom_fit(app);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Color Scheme")) {
            if (ImGui::MenuItem("Cell", nullptr, app.color_mode == COLOR_BY_CELL)) {
                app.color_mode = COLOR_BY_CELL; app_request_rerender(app);
            }
            if (ImGui::MenuItem("Material", nullptr, app.color_mode == COLOR_BY_MATERIAL)) {
                app.color_mode = COLOR_BY_MATERIAL; app_request_rerender(app);
            }
            if (ImGui::MenuItem("Universe", nullptr, app.color_mode == COLOR_BY_UNIVERSE)) {
                app.color_mode = COLOR_BY_UNIVERSE; app_request_rerender(app);
            }
            if (ImGui::MenuItem("Density", nullptr, app.color_mode == COLOR_BY_DENSITY)) {
                app.color_mode = COLOR_BY_DENSITY; app_request_rerender(app);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        ImGui::MenuItem("Grid", "G", &app.show_grid);
        ImGui::MenuItem("Labels", "L", &app.show_labels);
        ImGui::MenuItem("Contours", "C", &app.show_contours);
        ImGui::MenuItem("Surface Curves", "S", &app.show_curves);
        ImGui::MenuItem("Surface Labels", nullptr, &app.show_surface_labels);
        if (ImGui::MenuItem("Errors", "E", &app.show_errors)) {
            app.needs_recolor = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Arbitrary Slice", nullptr, app.arbitrary_slice)) {
            app.arbitrary_slice = !app.arbitrary_slice;
            app_request_rerender(app);
        }
        ImGui::EndMenu();
    }

    // --- Analysis ---
    if (ImGui::BeginMenu("Analysis")) {
        if (ImGui::MenuItem("Statistics", nullptr, false, app.sys != nullptr)) {
            commands_execute(app, "stats");
        }
        if (ImGui::MenuItem("Find Overlaps", nullptr, false, app.sys != nullptr)) {
            commands_execute(app, "find_overlaps");
        }
        ImGui::EndMenu();
    }

    // --- Help ---
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About")) {
            app_log(app, "aleaviewer - Geometry Editor for MCNP/OpenMC");
            app_log(app, "CSG Library: %s", alea_version());
        }
        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();
}

// ---------------------------------------------------------------------------
// Keyboard shortcuts (processed before ImGui eats events)
// ---------------------------------------------------------------------------

static void handle_keyboard_shortcuts(AppState& app, bool& quit) {
    ImGuiIO& io = ImGui::GetIO();

    // Don't process shortcuts when typing in text input
    if (io.WantTextInput) return;

    bool ctrl = io.KeyCtrl;

    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        std::string path = file_dialog_open("Open File", "All files | *");
        if (!path.empty()) app_load_file(app, path);
        return;
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Q)) {
        quit = true;
        return;
    }

    // 1/2/3 -> XY/XZ/YZ plane
    if (ImGui::IsKeyPressed(ImGuiKey_1)) {
        app.axis = SLICE_Z;
        app_zoom_fit(app);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_2)) {
        app.axis = SLICE_X;
        app_zoom_fit(app);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_3)) {
        app.axis = SLICE_Y;
        app_zoom_fit(app);
    }

    // +/- step slice value
    if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd)) {
        app.slice_value += app.slice_step;
        app_request_rerender(app);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract)) {
        app.slice_value -= app.slice_step;
        app_request_rerender(app);
    }

    // Home -> Zoom to fit
    if (ImGui::IsKeyPressed(ImGuiKey_Home)) {
        app_zoom_fit(app);
    }

    // Toggle overlays
    if (ImGui::IsKeyPressed(ImGuiKey_G)) {
        app.show_grid = !app.show_grid;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_L)) {
        app.show_labels = !app.show_labels;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_C)) {
        app.show_contours = !app.show_contours;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_S)) {
        app.show_curves = !app.show_curves;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_E)) {
        app.show_errors = !app.show_errors;
        app_request_rerender(app);
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // SDL init
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "aleaviewer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1600, 900,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_MAXIMIZED);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(1); // vsync

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "imgui.ini";

    // Detect display DPI scale
    float dpi_scale = 1.0f;
    {
        // Method 1: SDL display DPI
        float ddpi = 0, hdpi = 0, vdpi = 0;
        int display_index = SDL_GetWindowDisplayIndex(window);
        if (display_index >= 0 && SDL_GetDisplayDPI(display_index, &ddpi, &hdpi, &vdpi) == 0) {
            float s = ddpi / 96.0f;
            if (s > dpi_scale) dpi_scale = s;
        }
        // Method 2: framebuffer vs window size ratio (Wayland HiDPI)
        int win_w, fb_w;
        SDL_GetWindowSize(window, &win_w, nullptr);
        SDL_GL_GetDrawableSize(window, &fb_w, nullptr);
        if (win_w > 0) {
            float s = (float)fb_w / (float)win_w;
            if (s > dpi_scale) dpi_scale = s;
        }
        // Method 3: GNOME/GDK scale factor (compositor-level scaling)
        const char* gdk_scale = getenv("GDK_SCALE");
        if (gdk_scale) {
            float s = (float)atof(gdk_scale);
            if (s > dpi_scale) dpi_scale = s;
        }
        // Method 4: Qt scale factor
        const char* qt_scale = getenv("QT_SCALE_FACTOR");
        if (qt_scale) {
            float s = (float)atof(qt_scale);
            if (s > dpi_scale) dpi_scale = s;
        }
        if (dpi_scale < 1.0f) dpi_scale = 1.0f;
        fprintf(stderr, "DPI scale: %.2f\n", dpi_scale);
    }

    setup_theme(dpi_scale);

    // Load font at scaled size
    io.Fonts->Clear();
    float font_size = 15.0f * dpi_scale;

    // Try loading Inter, fall back to system fonts, then default
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 2;
    font_cfg.OversampleV = 2;
    font_cfg.PixelSnapH = true;

    const char* font_paths[] = {
        "/usr/share/fonts/opentype/inter/Inter-Regular.otf",
        "/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        nullptr
    };

    ImFont* font = nullptr;
    for (int i = 0; font_paths[i] && !font; i++) {
        FILE* f = fopen(font_paths[i], "rb");
        if (f) {
            fclose(f);
            font = io.Fonts->AddFontFromFileTTF(font_paths[i], font_size, &font_cfg);
            if (font) fprintf(stderr, "Font: %s @ %.0fpx\n", font_paths[i], font_size);
        }
    }

    if (!font) {
        // Fallback to built-in default
        font_cfg.SizePixels = font_size;
        io.Fonts->AddFontDefault(&font_cfg);
        fprintf(stderr, "Font: built-in default @ %.0fpx\n", font_size);
    }

    ImGui_ImplSDL2_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 130");

    // App init
    AppState app;
    app_init(app);

    // Load file from CLI arg
    if (argc > 1) {
        app_load_file(app, argv[1]);
    } else {
        app_log(app, "aleaviewer ready. Use 'load <file>' to open a geometry.");
    }

    // Main loop
    bool quit = false;
    int frames_until_maximize = 3;  // Wayland needs window mapped before maximize; dock layout waits for this
    while (!quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                quit = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                quit = true;
        }

        // Consume async slice results
        if (app.result_ready.load()) {
            slice_worker_consume(app);
        }

        // Consume async file load results
        if (app.load_done.load()) {
            app_load_consume(app);
        }



        // Start new frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Keyboard shortcuts
        handle_keyboard_shortcuts(app, quit);

        // Full-window DockSpace with menu bar (leave room for status bar)
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float status_bar_h = ImGui::GetFrameHeight() + 8.0f;  // matches panel_status padding (12,4) -> 4*2
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - status_bar_h));
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGuiWindowFlags dock_flags = ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;
        ImGui::Begin("DockHost", nullptr, dock_flags);
        ImGui::PopStyleVar(3);

        // Menu bar
        render_menu_bar(app, quit);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");

        if (app.first_frame && frames_until_maximize <= 0) {
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->WorkSize);

            ImGuiID dock_main = dockspace_id;
            ImGuiID dock_right = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.30f, nullptr, &dock_main);
            ImGuiID dock_left = ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.30f, nullptr, &dock_main);
            ImGuiID dock_right_bottom = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Down, 0.40f, nullptr, &dock_right);
            ImGui::DockBuilderDockWindow("2D Slice", dock_main);
            ImGui::DockBuilderDockWindow("Browser", dock_left);
            ImGui::DockBuilderDockWindow("Properties", dock_right);
            ImGui::DockBuilderDockWindow("Console", dock_right_bottom);

            ImGui::DockBuilderFinish(dockspace_id);
            app.first_frame = false;
        }

        ImGui::DockSpace(dockspace_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);
        ImGui::End();

        // Panels
        panel_slice(app);

        if (app.show_left_panel) panel_tree(app);
        if (app.show_right_panel) panel_properties(app);
        if (app.show_bottom_panel) panel_cli(app);

        panel_status(app);

        // Loading overlay
        if (app.loading.load()) {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 16));
            if (ImGui::Begin("##Loading", nullptr,
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav)) {
                // Animated spinner dots
                const char* dots[] = {".", "..", "..."};
                int phase = (int)(ImGui::GetTime() / 0.4) % 3;

                // Extract filename from path
                std::string fname = app.load_path;
                auto pos = fname.find_last_of("/\\");
                if (pos != std::string::npos) fname = fname.substr(pos + 1);

                ImGui::Text("Loading %s%s", fname.c_str(), dots[phase]);
            }
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        // Fast recolor path (no re-slice needed)
        if (app.needs_recolor && !app.cell_ids.empty()) {
            slice_recolor(app);
            app.needs_recolor = false;
        }

        // Submit renders
        if (app.sys && app.index_built) {
            if (app.worker_busy.load() && app.needs_rerender) {
                // View changed while rendering — interrupt so we can start fresh
                alea_interrupt();
            } else if (!app.worker_busy.load() && app.needs_rerender) {
                slice_worker_submit(app);
                app.needs_rerender = false;
            }


        }

        // Render
        ImGui::Render();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.098f, 0.098f, 0.125f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        // Wayland: maximize after window is mapped and visible
        if (frames_until_maximize > 0 && --frames_until_maximize == 0) {
            SDL_MaximizeWindow(window);
        }
    }

    // Cleanup
    app_shutdown(app);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
