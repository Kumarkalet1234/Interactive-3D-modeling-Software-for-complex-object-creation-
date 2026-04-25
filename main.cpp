// main.cpp — AeroMash entry point
#include "aeromash.h"

App g; // single global app instance

// ──────────────────────────────────────────────
// GLFW callbacks
// ──────────────────────────────────────────────
static void cb_resize(GLFWwindow*, int w, int h) {
    g.winW = w; g.winH = h;
    glViewport(0, 0, w, h);
}

static void cb_scroll(GLFWwindow*, double, double dy) {
    if (!g.vpHovered) return;
    float factor = dy > 0 ? 0.88f : 1.13f;
    g.cam.r = std::clamp(g.cam.r * factor, 0.3f, 400.0f);
}

static void cb_mousebutton(GLFWwindow*, int btn, int action, int) {
    // Let ImGui handle UI clicks first
    if (ImGui::GetIO().WantCaptureMouse) return;
    bool pressed = (action == GLFW_PRESS);
    if (btn == GLFW_MOUSE_BUTTON_MIDDLE) g.mbMid   = pressed;
    if (btn == GLFW_MOUSE_BUTTON_RIGHT)  g.mbRight = pressed;
    if (btn == GLFW_MOUSE_BUTTON_LEFT)   g.mbLeft  = pressed;
}

static void cb_cursor(GLFWwindow*, double mx, double my) {
    float dx = (float)mx - g.lastMX;
    float dy = (float)my - g.lastMY;
    g.lastMX = (float)mx;
    g.lastMY = (float)my;

    if (!g.vpHovered) return;

    // Middle drag = orbit
    if (g.mbMid && !ImGui::GetIO().WantCaptureMouse) {
        if (glfwGetKey(g.window, GLFW_KEY_LEFT_SHIFT)) {
            // Pan
            glm::vec3 right  = glm::normalize(glm::cross(g.cam.eyePos() - g.cam.target, {0,1,0}));
            glm::vec3 up     = glm::normalize(glm::cross(right, g.cam.eyePos() - g.cam.target));
            float     speed  = g.cam.r * 0.002f;
            g.cam.target    -= right * dx * speed;
            g.cam.target    += up    * dy * speed;
        } else {
            g.cam.theta -= dx * 0.008f;
            g.cam.phi    = std::clamp(g.cam.phi - dy * 0.008f, 0.05f, (float)M_PI - 0.05f);
        }
    }
}

static void cb_key(GLFWwindow* win, int key, int, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT)   != 0;

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // ── Numeric-pad views ──────────────────────
    if (key == GLFW_KEY_KP_1) { g.cam.theta = 0;           g.cam.phi = (float)M_PI/2; g.flash("Front"); }
    if (key == GLFW_KEY_KP_3) { g.cam.theta = (float)M_PI/2; g.cam.phi=(float)M_PI/2; g.flash("Right"); }
    if (key == GLFW_KEY_KP_7) { g.cam.theta = 0;           g.cam.phi = 0.001f;       g.flash("Top");   }
    if (key == GLFW_KEY_KP_9) { // back
        g.cam.theta = (float)M_PI; g.cam.phi = (float)M_PI/2; g.flash("Back");
    }
    if (key == GLFW_KEY_KP_5) { g.cam.ortho = !g.cam.ortho; g.flash(g.cam.ortho?"Orthographic":"Perspective"); }

    // ── Modes ──────────────────────────────────
    if (key == GLFW_KEY_TAB && g.mode == AppMode::Object && g.active) {
        g.enterEditMode();
    } else if (key == GLFW_KEY_TAB && g.mode == AppMode::Edit) {
        g.exitEditMode();
    }

    // ── Tools ──────────────────────────────────
    if (!ctrl && !shift) {
        if (key == GLFW_KEY_G) g.txActive = true, g.txType = 'g', g.txAxis = 0, g.txNumStr = "";
        if (key == GLFW_KEY_R) g.txActive = true, g.txType = 'r', g.txAxis = 0, g.txNumStr = "";
        if (key == GLFW_KEY_S) g.txActive = true, g.txType = 's', g.txAxis = 0, g.txNumStr = "";
        if (key == GLFW_KEY_X && !g.txActive) g.deleteSelected();
        if (key == GLFW_KEY_A && g.mode == AppMode::Object) {
            if (g.selected.empty()) g.selectAll(); else g.deselectAll();
        }
    }

    // Axis constrain during transform
    if (g.txActive) {
        if (key == GLFW_KEY_X) g.txAxis = 'x';
        if (key == GLFW_KEY_Y) g.txAxis = 'y';
        if (key == GLFW_KEY_Z) g.txAxis = 'z';
        if (key == GLFW_KEY_ESCAPE) { g.txActive = false; g.txNumStr = ""; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) g.txActive = false;
    }

    // ── Duplicates ──
    if (shift && key == GLFW_KEY_D) g.duplicateSelected();

    // ── Undo / Redo ──
    if (ctrl && !shift && key == GLFW_KEY_Z) g.undo();
    if (ctrl &&  shift && key == GLFW_KEY_Z) g.redo();

    // ── Import / Export ──
    if (ctrl && !shift && key == GLFW_KEY_E) exportOBJ("export.obj");
    if (ctrl && !shift && key == GLFW_KEY_I) {
        // Open a simple file dialog via the OS (basic: just try a hardcoded path for now)
        g.flash("Use File > Import to load an OBJ");
    }

    // ── Focus on selection ──
    if (key == GLFW_KEY_KP_DECIMAL && g.active) {
        g.cam.target = g.active->position;
        g.flash("Focus selected");
    }

    // ── Toggle overlays ──
    if (key == GLFW_KEY_Z && !ctrl && !shift) g.showWireframe = !g.showWireframe;

    // ── Sculpt tool shortcuts ──
    if (g.mode == AppMode::Sculpt) {
        if (key == GLFW_KEY_D) g.sculptTool = SculptTool::Draw;
        if (key == GLFW_KEY_S) g.sculptTool = SculptTool::Smooth;
        if (key == GLFW_KEY_I) g.sculptTool = SculptTool::Inflate;
        if (key == GLFW_KEY_P) g.sculptTool = SculptTool::Pinch;
    }

    // ── Timeline spacebar ──
    if (key == GLFW_KEY_SPACE) g.tlPlaying = !g.tlPlaying;
}

// ──────────────────────────────────────────────
// App state helpers
// ──────────────────────────────────────────────
void App::flash(const std::string& msg) {
    flashMsg  = msg;
    flashTime = glfwGetTime();
}

void App::saveState() {
    UndoSnapshot snap;
    snap.objects = objects;
    undoStack.push_back(snap);
    if (undoStack.size() > 64) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void App::undo() {
    if (undoStack.empty()) return;
    UndoSnapshot snap;
    snap.objects = objects;
    redoStack.push_back(snap);
    objects = undoStack.back().objects;
    undoStack.pop_back();
    active   = nullptr;
    selected.clear();
    flash("Undo");
}

void App::redo() {
    if (redoStack.empty()) return;
    UndoSnapshot snap;
    snap.objects = objects;
    undoStack.push_back(snap);
    objects = redoStack.back().objects;
    redoStack.pop_back();
    active   = nullptr;
    selected.clear();
    flash("Redo");
}

void App::selectAll() {
    selected.clear();
    for (auto& o : objects) {
        o.selected = true;
        selected.push_back(&o);
    }
    if (!objects.empty()) active = &objects.back();
}

void App::deselectAll() {
    for (auto& o : objects) o.selected = false;
    selected.clear();
    active = nullptr;
}

void App::deleteSelected() {
    saveState();
    for (auto* o : selected) {
        o->freeGPU();
        objects.erase(std::remove_if(objects.begin(), objects.end(),
            [&](const SceneObject& s){ return s.id == o->id; }), objects.end());
    }
    selected.clear();
    active = nullptr;
    flash("Deleted");
}

void App::duplicateSelected() {
    if (!active) return;
    saveState();
    SceneObject dup = *active;
    dup.id       = ++objCounter;
    dup.name     = active->name + ".001";
    dup.position += glm::vec3(0.5f, 0.0f, 0.5f);
    dup.vao = dup.vbo = dup.ebo = 0;
    dup.gpuDirty = true;
    objects.push_back(dup);
    deselectAll();
    active            = &objects.back();
    active->selected  = true;
    selected.push_back(active);
    flash("Duplicated");
}

// ──────────────────────────────────────────────
// Camera matrices
// ──────────────────────────────────────────────
glm::mat4 OrbCamera::view() const {
    return glm::lookAt(eyePos(), target, glm::vec3(0,1,0));
}

glm::mat4 OrbCamera::proj(float aspect) const {
    if (ortho) {
        float h = r * 0.5f;
        float w = h * aspect;
        return glm::ortho(-w, w, -h, h, 0.01f, 1000.0f);
    }
    return glm::perspective(glm::radians(fov), aspect, 0.01f, 1000.0f);
}

// ──────────────────────────────────────────────
// main()
// ──────────────────────────────────────────────
int main() {
    // Init GLFW
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    g.window = glfwCreateWindow(g.winW, g.winH, g.title.c_str(), nullptr, nullptr);
    if (!g.window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(g.window);
    glfwSwapInterval(1); // vsync

    // Load OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return 1;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // GLFW callbacks
    glfwSetWindowSizeCallback  (g.window, cb_resize);
    glfwSetScrollCallback      (g.window, cb_scroll);
    glfwSetMouseButtonCallback (g.window, cb_mousebutton);
    glfwSetCursorPosCallback   (g.window, cb_cursor);
    glfwSetKeyCallback         (g.window, cb_key);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.Fonts->AddFontDefault();

    // Blender-like dark style
    ImGui::StyleColorsDark();
    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding   = 2.0f;
    st.FrameRounding    = 2.0f;
    st.FramePadding     = {4, 3};
    st.ItemSpacing      = {6, 4};
    st.ScrollbarRounding = 2.0f;
    st.Colors[ImGuiCol_WindowBg]       = Colors::bg;
    st.Colors[ImGuiCol_FrameBg]        = Colors::bg2;
    st.Colors[ImGuiCol_Header]         = Colors::sel;
    st.Colors[ImGuiCol_HeaderHovered]  = Colors::btn;
    st.Colors[ImGuiCol_Button]         = Colors::btn;
    st.Colors[ImGuiCol_ButtonHovered]  = {0.27f,0.27f,0.27f,1.f};
    st.Colors[ImGuiCol_TitleBg]        = Colors::header;
    st.Colors[ImGuiCol_TitleBgActive]  = Colors::bg3;
    st.Colors[ImGuiCol_CheckMark]      = Colors::orange;
    st.Colors[ImGuiCol_SliderGrab]     = Colors::orange;
    st.Colors[ImGuiCol_SliderGrabActive] = {1.0f,0.55f,0.2f,1.f};

    ImGui_ImplGlfw_InitForOpenGL(g.window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Compile shaders
    buildShaders();

    // Add a default cube on startup
    g.saveState();
    auto& cube = g.addObject("cube", "Cube");
    buildCube(cube);
    cube.gpuDirty = true;
    g.active = &g.objects.back();

    g.flash("AeroMash 1.5 Alpha — Ready");

    // ── Main loop ──────────────────────────────
    while (!glfwWindowShouldClose(g.window)) {
        glfwPollEvents();

        // Timeline playback
        if (g.tlPlaying) {
            double now = glfwGetTime();
            if (now - g.tlLastTime >= 1.0 / g.tlFps) {
                g.tlLastTime = now;
                g.tlFrame    = (g.tlFrame + 1) % g.tlFrameMax;
                // Apply any keyframes at this frame
                for (auto& kf : g.keyframes) {
                    if (kf.frame == g.tlFrame) {
                        for (auto& o : g.objects) {
                            if (o.id == kf.objectId) {
                                o.position = kf.position;
                                o.rotation = kf.rotation;
                                o.scale    = kf.scale;
                            }
                        }
                    }
                }
            }
        }

        // Upload dirty GPU meshes
        for (auto& o : g.objects) {
            if (o.gpuDirty) o.uploadGPU();
        }

        // New ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Draw all UI panels
        drawMenuBar();
        drawToolbar();
        drawLeftToolbar();
        drawViewport();
        drawPropertiesPanel();
        drawOutliner();
        drawTimeline();
        drawFlash();

        // Render
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.11f, 0.11f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(g.window);
    }

    // Cleanup
    for (auto& o : g.objects) o.freeGPU();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(g.window);
    glfwTerminate();
    return 0;
}
