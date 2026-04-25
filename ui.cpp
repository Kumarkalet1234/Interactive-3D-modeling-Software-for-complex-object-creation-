// ui.cpp — all ImGui panels and 3D viewport rendering
#include "aeromash.h"
#include <cstdio>
#include <cmath>

// ── Forward declarations for file dialog (platform specific) ──────────────
static std::string openFileDialog(const char* filter);
static std::string saveFileDialog(const char* defaultName);

// ──────────────────────────────────────────────
// Menu bar
// ──────────────────────────────────────────────
void drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    // File
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene",      "Ctrl+N")) {
            g.saveState();
            for (auto& o : g.objects) o.freeGPU();
            g.objects.clear();
            g.selected.clear();
            g.active = nullptr;
            g.objCounter = 0;
            g.flash("New Scene");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Import OBJ...",  "Ctrl+I")) {
            auto p = openFileDialog(".obj");
            if (!p.empty()) importOBJ(p);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Export OBJ...",  "Ctrl+E")) {
            auto p = saveFileDialog("export.obj");
            if (!p.empty()) exportOBJ(p);
        }
        if (ImGui::MenuItem("Export STL...")) {
            auto p = saveFileDialog("export.stl");
            if (!p.empty()) exportSTL(p);
        }
        if (ImGui::MenuItem("Export JSON...")) {
            auto p = saveFileDialog("export.json");
            if (!p.empty()) exportJSON(p);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Alt+F4")) glfwSetWindowShouldClose(g.window, true);
        ImGui::EndMenu();
    }

    // Edit
    if (ImGui::BeginMenu("Edit")) {
        if (ImGui::MenuItem("Undo",        "Ctrl+Z",     false, !g.undoStack.empty())) g.undo();
        if (ImGui::MenuItem("Redo",        "Ctrl+Shift+Z",false,!g.redoStack.empty())) g.redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Select All",  "A"))  g.selectAll();
        if (ImGui::MenuItem("Deselect All","Alt+A")) g.deselectAll();
        if (ImGui::MenuItem("Duplicate",   "Shift+D")) g.duplicateSelected();
        if (ImGui::MenuItem("Delete",      "X"))  g.deleteSelected();
        ImGui::EndMenu();
    }

    // Add
    if (ImGui::BeginMenu("Add")) {
        if (ImGui::BeginMenu("Mesh")) {
            auto addMesh = [](const char* t, const char* name) {
                g.saveState();
                auto& o = g.addObject(t, name);
                // Scatter position a bit if other objects exist
                if (g.objects.size() > 1) {
                    o.position.x += ((float)rand()/RAND_MAX - 0.5f) * 2.0f;
                    o.position.z += ((float)rand()/RAND_MAX - 0.5f) * 2.0f;
                }
                g.deselectAll();
                g.active = &g.objects.back();
                g.active->selected = true;
                g.selected = {g.active};
                g.flash(std::string("Added ") + name);
            };
            if (ImGui::MenuItem("Cube"))      addMesh("cube",      "Cube");
            if (ImGui::MenuItem("Sphere"))    addMesh("sphere",    "Sphere");
            if (ImGui::MenuItem("Cylinder"))  addMesh("cylinder",  "Cylinder");
            if (ImGui::MenuItem("Plane"))     addMesh("plane",     "Plane");
            if (ImGui::MenuItem("Cone"))      addMesh("cone",      "Cone");
            if (ImGui::MenuItem("Torus"))     addMesh("torus",     "Torus");
            if (ImGui::MenuItem("Ico Sphere"))addMesh("icosphere", "IcoSphere");
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    // View
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Front",        "Numpad 1")) { g.cam.theta=0;             g.cam.phi=(float)M_PI/2; }
        if (ImGui::MenuItem("Right",        "Numpad 3")) { g.cam.theta=(float)M_PI/2; g.cam.phi=(float)M_PI/2; }
        if (ImGui::MenuItem("Top",          "Numpad 7")) { g.cam.theta=0;             g.cam.phi=0.001f; }
        if (ImGui::MenuItem("Back",         "Numpad 9")) { g.cam.theta=(float)M_PI;   g.cam.phi=(float)M_PI/2; }
        ImGui::Separator();
        if (ImGui::MenuItem("Toggle Orthographic","Numpad 5")) {
            g.cam.ortho = !g.cam.ortho;
            g.flash(g.cam.ortho?"Orthographic":"Perspective");
        }
        if (ImGui::MenuItem("Toggle Wireframe","Z")) g.showWireframe = !g.showWireframe;
        if (ImGui::MenuItem("Toggle X-Ray"))  g.showXRay = !g.showXRay;
        if (ImGui::MenuItem("Toggle Grid"))   g.showGrid = !g.showGrid;
        ImGui::EndMenu();
    }

    // Mode indicator on right
    {
        const char* modeLabel = g.mode==AppMode::Edit   ? "EDIT MODE" :
                                 g.mode==AppMode::Sculpt ? "SCULPT MODE" :
                                                            "OBJECT MODE";
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 200);
        ImGui::TextColored(Colors::orange, "%s", modeLabel);

        // Stats
        int verts = 0, tris = 0;
        for (auto& o : g.objects) {
            verts += (int)o.vertices.size();
            tris  += (int)(o.indices.size() / 3);
        }
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 100);
        ImGui::TextColored(Colors::text3, "V:%d T:%d", verts, tris);
    }

    ImGui::EndMainMenuBar();
}

// ──────────────────────────────────────────────
// Header toolbar (mode switcher + common tools)
// ──────────────────────────────────────────────
void drawToolbar() {
    ImGui::SetNextWindowPos({0, ImGui::GetFrameHeight()});
    ImGui::SetNextWindowSize({(float)g.winW, 32});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("##toolbar", nullptr, f);

    // Mode buttons
    auto modeBtn = [](const char* label, AppMode m) {
        bool on = (g.mode == m);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, Colors::sel);
        if (ImGui::Button(label)) {
            if (m == AppMode::Edit && g.mode != AppMode::Edit && g.active) g.enterEditMode();
            else if (m != AppMode::Edit && g.mode == AppMode::Edit)         g.exitEditMode();
            else g.mode = m;
        }
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };

    modeBtn("Object Mode",  AppMode::Object);
    modeBtn("Edit Mode",    AppMode::Edit);
    modeBtn("Sculpt Mode",  AppMode::Sculpt);

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Active tool buttons
    auto toolBtn = [](const char* label, ActiveTool t) {
        bool on = (g.activeTool == t);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, Colors::sel);
        if (ImGui::Button(label)) g.activeTool = t;
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    toolBtn("Select[Q]", ActiveTool::Select);
    toolBtn("Move[G]",   ActiveTool::Move);
    toolBtn("Rotate[R]", ActiveTool::Rotate);
    toolBtn("Scale[S]",  ActiveTool::Scale);

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Snap toggle
    ImGui::Checkbox("Snap", &g.snapOn);
    if (g.snapOn) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::DragFloat("##snap", &g.snapGrid, 0.1f, 0.1f, 10.0f, "%.2f");
    }

    // Sculpt tools (when in sculpt mode)
    if (g.mode == AppMode::Sculpt) {
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();
        auto scBtn = [](const char* label, SculptTool t) {
            bool on = (g.sculptTool == t);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, Colors::sel);
            if (ImGui::Button(label)) g.sculptTool = t;
            if (on) ImGui::PopStyleColor();
            ImGui::SameLine();
        };
        scBtn("Draw[D]",    SculptTool::Draw);
        scBtn("Smooth[S]",  SculptTool::Smooth);
        scBtn("Inflate[I]", SculptTool::Inflate);
        scBtn("Pinch[P]",   SculptTool::Pinch);
        ImGui::SetNextItemWidth(70);
        ImGui::SliderFloat("Radius", &g.sculptRadius, 0.1f, 3.0f);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        ImGui::SliderFloat("Str",    &g.sculptStrength, 0.005f, 0.2f);
    }

    ImGui::End();
}

// ──────────────────────────────────────────────
// Left toolbar (vertical icon strip)
// ──────────────────────────────────────────────
void drawLeftToolbar() {
    float menuH    = ImGui::GetFrameHeight() + 32;
    float tlHeight = g.showTimeline ? 130.0f : 0.0f;
    float height   = g.winH - menuH - tlHeight;

    ImGui::SetNextWindowPos({0, menuH});
    ImGui::SetNextWindowSize({36, height});
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##ltb", nullptr, f);

    auto ltbBtn = [](const char* icon, const char* tip, ActiveTool t) {
        bool on = (g.activeTool == t);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, Colors::orange);
        if (ImGui::Button(icon, {24,24})) g.activeTool = t;
        if (on) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    };

    ltbBtn("Q", "Select",  ActiveTool::Select);
    ltbBtn("G", "Move",    ActiveTool::Move);
    ltbBtn("R", "Rotate",  ActiveTool::Rotate);
    ltbBtn("S", "Scale",   ActiveTool::Scale);

    ImGui::End();
}

// ──────────────────────────────────────────────
// 3D viewport (renders into an FBO, shown as an ImGui image)
// ──────────────────────────────────────────────
static void ensureVPFBO(int w, int h) {
    if (g.vpFBO && g.vpW == w && g.vpH == h) return;

    if (g.vpFBO) {
        glDeleteFramebuffers(1, &g.vpFBO);
        glDeleteTextures(1, &g.vpColorTex);
        glDeleteRenderbuffers(1, &g.vpDepthRBO);
    }

    g.vpW = w; g.vpH = h;
    glGenFramebuffers(1, &g.vpFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, g.vpFBO);

    glGenTextures(1, &g.vpColorTex);
    glBindTexture(GL_TEXTURE_2D, g.vpColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g.vpColorTex, 0);

    glGenRenderbuffers(1, &g.vpDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, g.vpDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g.vpDepthRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void renderGrid() {
    if (!g.showGrid || !g.wireShader) return;
    // Draw a simple grid using GL_LINES via a temporary VAO
    static GLuint gridVAO = 0, gridVBO = 0;
    static int    gridBuilt = 0;
    const  int    GRID_HALF = 10;
    const  int    LINES     = (GRID_HALF*2+1)*2*2;

    if (!gridBuilt) {
        gridBuilt = 1;
        std::vector<glm::vec3> pts;
        for (int i = -GRID_HALF; i <= GRID_HALF; i++) {
            pts.push_back({(float)i, 0, -(float)GRID_HALF});
            pts.push_back({(float)i, 0,  (float)GRID_HALF});
            pts.push_back({-(float)GRID_HALF, 0, (float)i});
            pts.push_back({ (float)GRID_HALF, 0, (float)i});
        }
        glGenVertexArrays(1, &gridVAO);
        glGenBuffers(1, &gridVBO);
        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, pts.size()*sizeof(glm::vec3), pts.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
        glBindVertexArray(0);
    }

    glUseProgram(g.wireShader);
    float aspect = (float)g.vpW / (float)g.vpH;
    glm::mat4 mvp = g.cam.proj(aspect) * g.cam.view();
    glUniformMatrix4fv(glGetUniformLocation(g.wireShader, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniform4f(glGetUniformLocation(g.wireShader, "uColor"), 0.25f, 0.25f, 0.25f, 1.0f);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, LINES);

    // Highlight X and Z axes
    // X axis — red
    static GLuint axVAO = 0, axVBO = 0;
    if (!axVAO) {
        glm::vec3 axPts[4] = { {-10,0,0},{10,0,0},{0,0,-10},{0,0,10} };
        glGenVertexArrays(1, &axVAO);
        glGenBuffers(1, &axVBO);
        glBindVertexArray(axVAO);
        glBindBuffer(GL_ARRAY_BUFFER, axVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(axPts), axPts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
        glBindVertexArray(0);
    }
    glBindVertexArray(axVAO);
    glUniform4f(glGetUniformLocation(g.wireShader, "uColor"), 0.6f, 0.1f, 0.1f, 1.0f);
    glDrawArrays(GL_LINES, 0, 2);
    glUniform4f(glGetUniformLocation(g.wireShader, "uColor"), 0.1f, 0.1f, 0.6f, 1.0f);
    glDrawArrays(GL_LINES, 2, 2);

    glBindVertexArray(0);
}

static void renderObjects() {
    if (!g.solidShader) return;

    float aspect = (float)g.vpW / (float)g.vpH;
    glm::mat4 proj = g.cam.proj(aspect);
    glm::mat4 view = g.cam.view();
    glm::vec3 eye  = g.cam.eyePos();

    for (auto& o : g.objects) {
        if (!o.visible || o.vertices.empty() || !o.vao) continue;

        glm::mat4 model   = o.getModelMatrix();
        glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(model)));

        // ── Solid pass ──
        if (!g.showWireframe) {
            glUseProgram(g.solidShader);
            glUniformMatrix4fv(glGetUniformLocation(g.solidShader,"uModel"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(g.solidShader,"uView"),  1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(glGetUniformLocation(g.solidShader,"uProj"),  1, GL_FALSE, glm::value_ptr(proj));
            glUniformMatrix3fv(glGetUniformLocation(g.solidShader,"uNormalMat"), 1, GL_FALSE, glm::value_ptr(normMat));
            glUniform3fv(glGetUniformLocation(g.solidShader,"uColor"),    1, glm::value_ptr(o.material.color));
            glUniform1f (glGetUniformLocation(g.solidShader,"uRoughness"), o.material.roughness);
            glUniform1f (glGetUniformLocation(g.solidShader,"uMetalness"), o.material.metalness);
            float opacity = g.showXRay ? 0.35f : o.material.opacity;
            glUniform1f (glGetUniformLocation(g.solidShader,"uOpacity"),   opacity);
            glUniform1i (glGetUniformLocation(g.solidShader,"uSelected"),  o.selected ? 1 : 0);
            glUniform3fv(glGetUniformLocation(g.solidShader,"uCamPos"),    1, glm::value_ptr(eye));

            if (g.showXRay) {
                glEnable(GL_BLEND);
                glDepthMask(GL_FALSE);
            }

            glBindVertexArray(o.vao);
            glDrawElements(GL_TRIANGLES, (GLsizei)o.indices.size(), GL_UNSIGNED_INT, nullptr);

            if (g.showXRay) {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
        }

        // ── Wireframe overlay ──
        if (g.showWireframe || o.selected) {
            glUseProgram(g.wireShader);
            glm::mat4 mvp = proj * view * model;
            glUniformMatrix4fv(glGetUniformLocation(g.wireShader,"uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));

            glm::vec4 wCol = o.selected
                ? glm::vec4(0.24f, 0.42f, 0.87f, 1.0f)
                : glm::vec4(0.5f,  0.5f,  0.5f,  0.6f);
            glUniform4fv(glGetUniformLocation(g.wireShader,"uColor"), 1, glm::value_ptr(wCol));

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glPolygonOffset(-1.0f, -1.0f);
            glEnable(GL_POLYGON_OFFSET_LINE);
            glBindVertexArray(o.vao);
            glDrawElements(GL_TRIANGLES, (GLsizei)o.indices.size(), GL_UNSIGNED_INT, nullptr);
            glDisable(GL_POLYGON_OFFSET_LINE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
    }
    glBindVertexArray(0);
}

void drawViewport() {
    float menuH    = ImGui::GetFrameHeight() + 32;
    float panelW   = g.showProperties ? 258.0f : 0.0f;
    float outlW    = g.showOutliner   ? 220.0f : 0.0f;
    float tlH      = g.showTimeline   ? 130.0f : 0.0f;
    float x        = 36.0f;
    float y        = menuH;
    float w        = g.winW - x - panelW - outlW;
    float h        = g.winH - y - tlH;

    // Render 3D scene into FBO
    int iw = std::max(1,(int)w), ih = std::max(1,(int)h);
    ensureVPFBO(iw, ih);

    glBindFramebuffer(GL_FRAMEBUFFER, g.vpFBO);
    glViewport(0, 0, iw, ih);
    glClearColor(0.18f, 0.18f, 0.18f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    renderGrid();
    renderObjects();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, g.winW, g.winH);

    // Show as ImGui image window
    ImGui::SetNextWindowPos({x, y});
    ImGui::SetNextWindowSize({w, h});
    ImGuiWindowFlags vf = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoBringToFrontOnFocus
                        | ImGuiWindowFlags_NoScrollbar;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
    ImGui::Begin("##viewport", nullptr, vf);

    g.vpHovered = ImGui::IsWindowHovered();
    ImGui::Image((ImTextureID)(intptr_t)g.vpColorTex, {w, h}, {0,1},{1,0});

    // Transform gizmo overlay text
    if (g.txActive) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s%s  %s  | LMB confirm · RMB cancel",
            g.txType=='g'?"Grab":g.txType=='r'?"Rotate":"Scale",
            g.txAxis ? (std::string(" | ")+g.txAxis).c_str() : "",
            g.txNumStr.c_str());
        ImGui::SetCursorPos({10, h-30});
        ImGui::TextColored(Colors::orange, "%s", buf);
    }

    // Mode overlay
    if (g.mode == AppMode::Sculpt) {
        ImGui::SetCursorPos({10, 10});
        ImGui::TextColored(Colors::orange, "-- SCULPT MODE --");
    } else if (g.mode == AppMode::Edit) {
        ImGui::SetCursorPos({10, 10});
        ImGui::TextColored({0.55f,0.85f,1.0f,1.0f}, "-- EDIT MODE --");
    }

    // Handle sculpt brush on drag in viewport
    if (g.mode == AppMode::Sculpt && g.mbLeft && g.vpHovered) {
        ImVec2 mp   = ImGui::GetMousePos();
        ImVec2 wp   = ImGui::GetWindowPos();
        ImVec2 ws   = ImGui::GetWindowSize();
        float  nx   = (mp.x - wp.x) / ws.x * 2.0f - 1.0f;
        float  ny   = -((mp.y - wp.y) / ws.y * 2.0f - 1.0f);
        sculptBrush(nx, ny);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

// ──────────────────────────────────────────────
// Properties panel (right side)
// ──────────────────────────────────────────────
void drawPropertiesPanel() {
    if (!g.showProperties) return;

    float menuH = ImGui::GetFrameHeight() + 32;
    float tlH   = g.showTimeline ? 130.0f : 0.0f;
    float outlW = g.showOutliner ? 220.0f : 0.0f;
    float h     = g.winH - menuH - tlH;

    ImGui::SetNextWindowPos({(float)g.winW - 258.0f - outlW, menuH});
    ImGui::SetNextWindowSize({258.0f, h});
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##props", nullptr, f);

    // Tab buttons
    const char* tabs[] = {"Object", "Material", "Modifiers"};
    for (int i = 0; i < 3; i++) {
        if (i > 0) ImGui::SameLine();
        bool on = (g.propTab == i);
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, Colors::orange);
        char lbl[32]; snprintf(lbl, 32, "%s##tab%d", tabs[i], i);
        if (ImGui::Button(lbl, {78, 22})) g.propTab = i;
        if (on) ImGui::PopStyleColor();
    }
    ImGui::Separator();

    if (!g.active) {
        ImGui::TextColored(Colors::text3, "Nothing selected.");
        ImGui::End();
        return;
    }
    SceneObject& o = *g.active;

    // ── Object tab ──
    if (g.propTab == 0) {
        char nameBuf[128];
        strncpy(nameBuf, o.name.c_str(), 127);
        ImGui::Text("Name");
        ImGui::SameLine(70);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##name", nameBuf, 127)) o.name = nameBuf;

        ImGui::Separator();
        ImGui::TextColored(Colors::text3, "Transform");

        auto v3 = [](const char* label, glm::vec3& v, float spd = 0.01f) {
            ImGui::Text("%s", label); ImGui::SameLine(70);
            ImGui::SetNextItemWidth(-1);
            char id[32]; snprintf(id, 32, "##%s", label);
            if (ImGui::DragFloat3(id, glm::value_ptr(v), spd)) {}
        };
        v3("Location", o.position, 0.05f);
        v3("Rotation", o.rotation, 0.01f); // radians, todo: show degrees
        v3("Scale",    o.scale,    0.01f);

        ImGui::Separator();
        ImGui::TextColored(Colors::text3, "Geometry");
        ImGui::Text("Vertices: %d", (int)o.vertices.size());
        ImGui::Text("Triangles: %d", (int)(o.indices.size()/3));
        ImGui::Text("Type: %s", o.typeStr.c_str());

        ImGui::Separator();
        if (ImGui::Button("Apply All Transforms", {-1, 0})) {
            g.saveState();
            // Bake transforms into vertex positions
            glm::mat4 M = o.getModelMatrix();
            glm::mat3 N = glm::transpose(glm::inverse(glm::mat3(M)));
            for (auto& v : o.vertices) {
                v.pos    = glm::vec3(M * glm::vec4(v.pos, 1));
                v.normal = glm::normalize(N * v.normal);
            }
            o.position = {0,0,0};
            o.rotation = {0,0,0};
            o.scale    = {1,1,1};
            o.gpuDirty = true;
            g.flash("Applied Transforms");
        }
        if (ImGui::Button("Duplicate", {-1, 0})) g.duplicateSelected();
        if (ImGui::Button("Delete",    {-1, 0})) g.deleteSelected();
    }

    // ── Material tab ──
    else if (g.propTab == 1) {
        char mbuf[128]; strncpy(mbuf, o.material.name.c_str(), 127);
        ImGui::Text("Name"); ImGui::SameLine(70);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputText("##mname", mbuf, 127)) o.material.name = mbuf;

        ImGui::ColorEdit3("Base Color", glm::value_ptr(o.material.color));
        ImGui::SliderFloat("Roughness", &o.material.roughness, 0, 1);
        ImGui::SliderFloat("Metalness", &o.material.metalness, 0, 1);
        ImGui::SliderFloat("Opacity",   &o.material.opacity,   0, 1);
        ImGui::Checkbox("Wireframe",    &o.material.wireframe);
    }

    // ── Modifiers tab ──
    else if (g.propTab == 2) {
        if (ImGui::Button("+ Add Modifier", {-1, 0})) {
            ImGui::OpenPopup("add_mod");
        }
        if (ImGui::BeginPopup("add_mod")) {
            auto addMod = [&](ModType t, const char* name) {
                Modifier m; m.type = t; m.name = name;
                o.modifiers.push_back(m);
                ImGui::CloseCurrentPopup();
                g.flash(std::string("Added ") + name);
            };
            if (ImGui::MenuItem("Subdivision"))  addMod(ModType::Subdivision, "Subdivision");
            if (ImGui::MenuItem("Mirror"))        addMod(ModType::Mirror,      "Mirror");
            if (ImGui::MenuItem("Array"))         addMod(ModType::Array,       "Array");
            if (ImGui::MenuItem("Solidify"))      addMod(ModType::Solidify,    "Solidify");
            if (ImGui::MenuItem("Bevel"))         addMod(ModType::Bevel,       "Bevel");
            ImGui::EndPopup();
        }
        ImGui::Separator();

        int delIdx = -1;
        for (int mi = 0; mi < (int)o.modifiers.size(); mi++) {
            auto& m = o.modifiers[mi];
            ImGui::PushID(mi);

            // Header
            bool open = ImGui::CollapsingHeader(m.name.c_str());
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 18);
            if (ImGui::SmallButton("X")) delIdx = mi;

            if (open) {
                ImGui::Checkbox("Enabled", &m.enabled);
                switch (m.type) {
                    case ModType::Subdivision:
                        ImGui::SliderInt("Levels", &m.subdivLevels, 1, 4);
                        break;
                    case ModType::Mirror:
                        ImGui::RadioButton("X", &m.mirrorAxis, 0); ImGui::SameLine();
                        ImGui::RadioButton("Y", &m.mirrorAxis, 1); ImGui::SameLine();
                        ImGui::RadioButton("Z", &m.mirrorAxis, 2);
                        break;
                    case ModType::Array:
                        ImGui::SliderInt("Count",  &m.arrayCount,  2, 10);
                        ImGui::DragFloat("Offset", &m.arrayOffset, 0.1f, 0.1f, 20.0f);
                        break;
                    case ModType::Solidify:
                        ImGui::DragFloat("Thickness", &m.solidifyThickness, 0.01f, 0.001f, 2.0f);
                        break;
                    case ModType::Bevel:
                        ImGui::DragFloat("Width", &m.bevelWidth, 0.01f, 0.001f, 1.0f);
                        break;
                }
            }
            ImGui::PopID();
        }
        if (delIdx >= 0) o.modifiers.erase(o.modifiers.begin() + delIdx);
    }

    ImGui::End();
}

// ──────────────────────────────────────────────
// Outliner (right side, above properties)
// ──────────────────────────────────────────────
void drawOutliner() {
    if (!g.showOutliner) return;

    float menuH = ImGui::GetFrameHeight() + 32;
    float tlH   = g.showTimeline ? 130.0f : 0.0f;
    float h     = g.winH - menuH - tlH;

    ImGui::SetNextWindowPos({(float)g.winW - 220.0f, menuH});
    ImGui::SetNextWindowSize({220.0f, h});
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##outliner", nullptr, f);

    ImGui::TextColored(Colors::orange, "Outliner");
    ImGui::TextColored(Colors::text3,  "(%d objects)", (int)g.objects.size());
    ImGui::Separator();

    for (auto& o : g.objects) {
        bool sel = o.selected;
        if (sel) ImGui::PushStyleColor(ImGuiCol_Header, Colors::sel);

        char lbl[128]; snprintf(lbl, 128, "%s##%d", o.name.c_str(), o.id);
        if (ImGui::Selectable(lbl, sel)) {
            // Ctrl = additive select
            bool ctrl = ImGui::GetIO().KeyCtrl;
            if (!ctrl) g.deselectAll();
            auto* ptr = const_cast<SceneObject*>(&o);
            ptr->selected = true;
            if (std::find(g.selected.begin(), g.selected.end(), ptr) == g.selected.end())
                g.selected.push_back(ptr);
            g.active = ptr;
        }

        if (sel) ImGui::PopStyleColor();
    }

    ImGui::End();
}

// ──────────────────────────────────────────────
// Timeline
// ──────────────────────────────────────────────
void drawTimeline() {
    if (!g.showTimeline) return;

    float tlH = 130.0f;
    ImGui::SetNextWindowPos({0, (float)g.winH - tlH});
    ImGui::SetNextWindowSize({(float)g.winW, tlH});
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("##timeline", nullptr, f);

    ImGui::TextColored(Colors::orange, "Timeline");
    ImGui::SameLine();
    ImGui::TextColored(Colors::text3, "Frame %d / %d", g.tlFrame, g.tlFrameMax);

    // Transport controls
    if (ImGui::Button("|<")) g.tlFrame = 0;
    ImGui::SameLine();
    if (ImGui::Button("<"))  g.tlFrame = std::max(0, g.tlFrame - 1);
    ImGui::SameLine();
    if (ImGui::Button(g.tlPlaying ? "Pause" : "Play")) g.tlPlaying = !g.tlPlaying;
    ImGui::SameLine();
    if (ImGui::Button(">"))  g.tlFrame = std::min(g.tlFrameMax-1, g.tlFrame + 1);
    ImGui::SameLine();
    if (ImGui::Button(">|")) g.tlFrame = g.tlFrameMax - 1;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    ImGui::DragInt("FPS", (int*)&g.tlFps, 1, 1, 120);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::DragInt("End", &g.tlFrameMax, 1, 1, 9999);

    if (g.active) {
        ImGui::SameLine();
        if (ImGui::Button("Insert Keyframe")) {
            Keyframe kf;
            kf.frame    = g.tlFrame;
            kf.objectId = g.active->id;
            kf.position = g.active->position;
            kf.rotation = g.active->rotation;
            kf.scale    = g.active->scale;
            // Replace existing kf at same frame/obj if exists
            bool replaced = false;
            for (auto& k : g.keyframes) {
                if (k.frame == kf.frame && k.objectId == kf.objectId) {
                    k = kf; replaced = true; break;
                }
            }
            if (!replaced) g.keyframes.push_back(kf);
            g.flash("Keyframe @ " + std::to_string(g.tlFrame));
        }
    }

    // Scrubber
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderInt("##frame", &g.tlFrame, 0, g.tlFrameMax - 1)) {
        g.tlPlaying = false;
        // Apply keyframes at this frame
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

    // Keyframe markers
    ImVec2 p    = ImGui::GetCursorScreenPos();
    float  barW = ImGui::GetContentRegionAvail().x;
    float  barH = 18.0f;
    auto* dl    = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, {p.x+barW, p.y+barH}, IM_COL32(30,30,30,255));
    for (auto& kf : g.keyframes) {
        float xp = p.x + (float)kf.frame / g.tlFrameMax * barW;
        dl->AddCircleFilled({xp, p.y+barH*0.5f}, 4.0f, IM_COL32(224,113,32,255));
    }
    // Playhead
    float ph = p.x + (float)g.tlFrame / g.tlFrameMax * barW;
    dl->AddLine({ph, p.y}, {ph, p.y+barH}, IM_COL32(255,255,255,200), 2.0f);

    ImGui::End();
}

// ──────────────────────────────────────────────
// Flash message overlay
// ──────────────────────────────────────────────
void drawFlash() {
    if (g.flashMsg.empty()) return;
    double age = glfwGetTime() - g.flashTime;
    if (age > 2.5) { g.flashMsg.clear(); return; }

    float alpha = (float)std::min(1.0, (2.5 - age) / 0.5);
    ImGui::SetNextWindowPos({(float)g.winW * 0.5f, (float)g.winH - 170.0f},
        ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowBgAlpha(0.7f * alpha);
    ImGuiWindowFlags f = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove
                       | ImGuiWindowFlags_NoBringToFrontOnFocus
                       | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##flash", nullptr, f);
    ImGui::TextColored({Colors::text.x, Colors::text.y, Colors::text.z, alpha},
        "%s", g.flashMsg.c_str());
    ImGui::End();
}

// ──────────────────────────────────────────────
// Edit mode stubs (full implementation omitted for brevity)
// ──────────────────────────────────────────────
void App::enterEditMode() {
    mode     = AppMode::Edit;
    editMesh = active;
    // Copy mesh vertices into edit-mode arrays
    editVerts.clear();
    if (!active) return;
    for (int i = 0; i < (int)active->vertices.size(); i++) {
        EditVert ev;
        ev.pos     = active->vertices[i].pos;
        ev.origIdx = i;
        editVerts.push_back(ev);
    }
    flash("Edit Mode");
}

void App::exitEditMode() {
    // Write back edited verts
    if (editMesh) {
        for (auto& ev : editVerts) {
            if (ev.origIdx < (int)editMesh->vertices.size())
                editMesh->vertices[ev.origIdx].pos = ev.pos;
        }
        editMesh->gpuDirty = true;
    }
    mode     = AppMode::Object;
    editMesh = nullptr;
    editVerts.clear();
    flash("Object Mode");
}

// ──────────────────────────────────────────────
// Minimal file dialog (cross-platform via stdin for now)
// Production: replace with tinyfd / NFD / Win32 dialog
// ──────────────────────────────────────────────
static std::string openFileDialog(const char* filter) {
    // On platforms with a terminal, fall back to prompt
    // A real app would use nativefiledialog or tinyfiledialogs here
    char buf[512] = {};
    printf("Open file (%s): ", filter);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
    }
    return buf;
}

static std::string saveFileDialog(const char* defaultName) {
    char buf[512] = {};
    printf("Save file [%s]: ", defaultName);
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = 0;
    }
    if (strlen(buf) == 0) return defaultName;
    return buf;
}
