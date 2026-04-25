#pragma once

// AeroMash 1.5 Alpha - C++ / OpenGL port
// Requires: GLFW3, GLAD, GLM, Dear ImGui, tinyobjloader, stb_image_write
//
// Build:
//   g++ -std=c++17 src/*.cpp -lglfw -lGL -ldl -o AeroMash
//   (or use the provided CMakeLists.txt)

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <optional>
#include <cmath>

// ──────────────────────────────────────────────
// Colour palette (matches Blender dark theme)
// ──────────────────────────────────────────────
namespace Colors {
    inline ImVec4 bg       = {0.11f, 0.11f, 0.11f, 1.f};
    inline ImVec4 bg2      = {0.14f, 0.14f, 0.14f, 1.f};
    inline ImVec4 bg3      = {0.16f, 0.16f, 0.16f, 1.f};
    inline ImVec4 header   = {0.15f, 0.15f, 0.15f, 1.f};
    inline ImVec4 border   = {0.07f, 0.07f, 0.07f, 1.f};
    inline ImVec4 btn      = {0.22f, 0.22f, 0.22f, 1.f};
    inline ImVec4 text     = {0.89f, 0.89f, 0.89f, 1.f};
    inline ImVec4 text2    = {0.65f, 0.65f, 0.65f, 1.f};
    inline ImVec4 text3    = {0.41f, 0.41f, 0.41f, 1.f};
    inline ImVec4 orange   = {0.878f,0.443f,0.125f, 1.f};
    inline ImVec4 sel      = {0.14f, 0.20f, 0.32f, 1.f};
    inline ImVec4 selBdr   = {0.24f, 0.42f, 0.67f, 1.f};
}

// ──────────────────────────────────────────────
// Math helpers
// ──────────────────────────────────────────────
inline glm::vec3 sphericalToCart(float theta, float phi, float r) {
    return {
        r * std::sin(phi) * std::sin(theta),
        r * std::cos(phi),
        r * std::sin(phi) * std::cos(theta)
    };
}

// ──────────────────────────────────────────────
// Mesh vertex
// ──────────────────────────────────────────────
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

// ──────────────────────────────────────────────
// Material
// ──────────────────────────────────────────────
struct Material {
    std::string name = "Material";
    glm::vec3   color       = {0.8f, 0.5f, 0.2f};
    float       roughness   = 0.6f;
    float       metalness   = 0.05f;
    float       opacity     = 1.0f;
    bool        wireframe   = false;
};

// ──────────────────────────────────────────────
// Modifier types
// ──────────────────────────────────────────────
enum class ModType { Subdivision, Mirror, Array, Solidify, Bevel };

struct Modifier {
    ModType     type;
    std::string name;
    bool        enabled = true;
    // params (union-like via named fields)
    int   subdivLevels = 1;
    int   arrayCount   = 2;
    float arrayOffset  = 2.0f;
    float solidifyThickness = 0.1f;
    float bevelWidth   = 0.1f;
    int   mirrorAxis   = 0; // 0=X 1=Y 2=Z
};

// ──────────────────────────────────────────────
// Keyframe
// ──────────────────────────────────────────────
struct Keyframe {
    int         frame;
    int         objectId;
    glm::vec3   position;
    glm::vec3   rotation; // euler radians
    glm::vec3   scale;
};

// ──────────────────────────────────────────────
// Scene object
// ──────────────────────────────────────────────
enum class ObjType { Mesh, Light, Empty, Camera };

struct SceneObject {
    int         id;
    std::string name;
    std::string typeStr; // "cube", "sphere", etc.
    ObjType     objType = ObjType::Mesh;

    glm::vec3   position = {0,0,0};
    glm::vec3   rotation = {0,0,0}; // euler radians XYZ
    glm::vec3   scale    = {1,1,1};

    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    Material              material;
    std::vector<Modifier> modifiers;

    bool        visible  = true;
    bool        selected = false;

    // GPU handles
    GLuint vao = 0, vbo = 0, ebo = 0;
    bool   gpuDirty = true;

    glm::mat4 getModelMatrix() const {
        glm::mat4 m = glm::mat4(1.0f);
        m = glm::translate(m, position);
        m = m * glm::eulerAngleXYZ(rotation.x, rotation.y, rotation.z);
        m = glm::scale(m, scale);
        return m;
    }

    void uploadGPU();
    void freeGPU();
};

// ──────────────────────────────────────────────
// Edit-mode selection element
// ──────────────────────────────────────────────
struct EditVert {
    glm::vec3 pos;
    bool      selected = false;
    int       origIdx;
};

struct EditEdge {
    int a, b;
    bool selected = false;
};

struct EditFace {
    std::vector<int> verts;
    bool selected = false;
};

// ──────────────────────────────────────────────
// Orbital camera
// ──────────────────────────────────────────────
struct OrbCamera {
    float     theta  = 0.8f;  // azimuth
    float     phi    = 1.1f;  // polar
    float     r      = 8.0f;  // distance
    glm::vec3 target = {0,0,0};
    bool      ortho  = false;
    float     fov    = 60.0f;

    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;
    glm::vec3 eyePos() const {
        return target + sphericalToCart(theta, phi, r);
    }
};

// ──────────────────────────────────────────────
// App modes
// ──────────────────────────────────────────────
enum class AppMode   { Object, Edit, Sculpt };
enum class EditMode  { Vertex, Edge, Face };
enum class SculptTool{ Draw, Smooth, Inflate, Pinch };
enum class ActiveTool{ Select, Move, Rotate, Scale };

// ──────────────────────────────────────────────
// Undo snapshot (simple whole-scene copy)
// ──────────────────────────────────────────────
struct UndoSnapshot {
    std::vector<SceneObject> objects;
};

// ──────────────────────────────────────────────
// App state — one global instance
// ──────────────────────────────────────────────
struct App {
    // window
    GLFWwindow* window = nullptr;
    int         winW = 1440, winH = 900;
    std::string title = "(Unsaved) — AeroMash 1.5 Alpha";

    // scene
    std::vector<SceneObject> objects;
    std::vector<SceneObject*> selected;
    SceneObject* active = nullptr;
    int objCounter = 0;

    // camera
    OrbCamera cam;

    // viewport framebuffer
    GLuint vpFBO = 0, vpColorTex = 0, vpDepthRBO = 0;
    int    vpW = 0, vpH = 0;

    // shaders
    GLuint solidShader   = 0;
    GLuint wireShader    = 0;
    GLuint overlayShader = 0;

    // mode
    AppMode   mode       = AppMode::Object;
    EditMode  editMode   = EditMode::Vertex;
    SculptTool sculptTool = SculptTool::Draw;
    float     sculptRadius = 0.8f;
    float     sculptStrength = 0.04f;
    ActiveTool activeTool = ActiveTool::Select;

    // edit mode buffers
    std::vector<EditVert> editVerts;
    std::vector<EditEdge> editEdges;
    std::vector<EditFace> editFaces;
    SceneObject* editMesh = nullptr;

    // overlays
    bool showGrid       = true;
    bool showXRay       = false;
    bool showWireframe  = false;

    // timeline
    int   tlFrame    = 0;
    int   tlFrameMax = 250;
    bool  tlPlaying  = false;
    float tlFps      = 24.0f;
    double tlLastTime = 0;
    std::vector<Keyframe> keyframes;

    // undo
    std::vector<UndoSnapshot> undoStack;
    std::vector<UndoSnapshot> redoStack;

    // transform gizmo
    bool  txActive = false;
    char  txType   = 0;   // 'g','r','s'
    char  txAxis   = 0;   // 'x','y','z', 0=all
    std::string txNumStr;
    glm::vec3   txOrigin;
    glm::vec2   txMouseStart;

    // snap
    bool  snapOn   = false;
    float snapGrid = 1.0f;

    // flash message
    std::string flashMsg;
    double      flashTime = 0;

    // panel visibility
    bool showProperties = true;
    bool showOutliner   = true;
    bool showTimeline   = true;

    // right panel tab: 0=object 1=material 2=modifiers
    int propTab = 0;

    // viewport input state
    bool  mbMid = false, mbRight = false, mbLeft = false;
    float lastMX = 0, lastMY = 0;
    bool  vpHovered = false;

    // helpers
    void flash(const std::string& msg);
    void saveState();
    void undo();
    void redo();

    SceneObject& addObject(const std::string& typeStr, const std::string& name, ObjType ot = ObjType::Mesh);
    void deleteSelected();
    void selectAll();
    void deselectAll();
    void duplicateSelected();

    void enterEditMode();
    void exitEditMode();

    glm::vec3 screenToWorld(float mx, float my, float vpX, float vpY, float vpW, float vpH);
};

extern App g;

// ──────────────────────────────────────────────
// Function declarations (defined in other .cpp files)
// ──────────────────────────────────────────────

// geometry.cpp
void buildCube(SceneObject& o);
void buildSphere(SceneObject& o, int rings=16, int segs=32);
void buildCylinder(SceneObject& o, int segs=32);
void buildPlane(SceneObject& o, int div=1);
void buildCone(SceneObject& o, int segs=32);
void buildTorus(SceneObject& o, int rings=20, int segs=48);
void buildIcoSphere(SceneObject& o, int subdivisions=2);

// io.cpp
bool importOBJ(const std::string& path);
bool exportOBJ(const std::string& path);
bool exportSTL(const std::string& path);
bool exportJSON(const std::string& path);

// shaders.cpp
GLuint compileShader(const char* vert, const char* frag);
void   buildShaders();

// ui.cpp
void drawMenuBar();
void drawToolbar();
void drawLeftToolbar();
void drawViewport();
void drawPropertiesPanel();
void drawOutliner();
void drawTimeline();
void drawFlash();

// sculpt.cpp
void sculptBrush(float mx, float my);
