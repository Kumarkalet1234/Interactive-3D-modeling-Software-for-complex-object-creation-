// geometry.cpp — primitive mesh builders
#include "aeromash.h"
#include <cmath>

// ──────────────────────────────────────────────
// GPU upload / free
// ──────────────────────────────────────────────
void SceneObject::uploadGPU() {
    if (!vao) glGenVertexArrays(1, &vao);
    if (!vbo) glGenBuffers(1, &vbo);
    if (!ebo) glGenBuffers(1, &ebo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(uint32_t),
        indices.data(), GL_STATIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, pos));
    // normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, normal));
    // uv
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
    gpuDirty = false;
}

void SceneObject::freeGPU() {
    if (vao) { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo) { glDeleteBuffers(1, &vbo); vbo = 0; }
    if (ebo) { glDeleteBuffers(1, &ebo); ebo = 0; }
}

// ──────────────────────────────────────────────
// Helper: compute flat normals from index list
// ──────────────────────────────────────────────
static void computeNormals(SceneObject& o) {
    // zero all normals first
    for (auto& v : o.vertices) v.normal = {0,0,0};

    // accumulate face normals
    for (size_t i = 0; i + 2 < o.indices.size(); i += 3) {
        auto& v0 = o.vertices[o.indices[i]];
        auto& v1 = o.vertices[o.indices[i+1]];
        auto& v2 = o.vertices[o.indices[i+2]];
        glm::vec3 n = glm::normalize(glm::cross(v1.pos - v0.pos, v2.pos - v0.pos));
        v0.normal += n;
        v1.normal += n;
        v2.normal += n;
    }
    for (auto& v : o.vertices)
        v.normal = glm::normalize(v.normal);
}

// ──────────────────────────────────────────────
// Cube (2x2x2, centred at origin, shifted up 1)
// ──────────────────────────────────────────────
void buildCube(SceneObject& o) {
    o.vertices.clear();
    o.indices.clear();

    // 6 faces × 4 verts each
    struct FaceDef { glm::vec3 verts[4]; glm::vec3 normal; };
    FaceDef faces[6] = {
        { {{ {-1,-1, 1},{1,-1, 1},{1,1, 1},{-1,1, 1} }}, { 0, 0, 1} }, // front
        { {{ { 1,-1,-1},{-1,-1,-1},{-1,1,-1},{1,1,-1} }}, { 0, 0,-1} }, // back
        { {{ {-1,-1,-1},{-1,-1, 1},{-1,1, 1},{-1,1,-1} }}, {-1, 0, 0} }, // left
        { {{ { 1,-1, 1},{ 1,-1,-1},{ 1,1,-1},{ 1,1, 1} }}, { 1, 0, 0} }, // right
        { {{ {-1, 1, 1},{ 1, 1, 1},{ 1, 1,-1},{-1, 1,-1} }}, { 0, 1, 0} }, // top
        { {{ {-1,-1,-1},{ 1,-1,-1},{ 1,-1, 1},{-1,-1, 1} }}, { 0,-1, 0} }, // bottom
    };
    glm::vec2 uvs[4] = {{0,0},{1,0},{1,1},{0,1}};

    for (auto& f : faces) {
        uint32_t base = (uint32_t)o.vertices.size();
        for (int j = 0; j < 4; j++) {
            Vertex v;
            v.pos    = f.verts[j] + glm::vec3(0, 1, 0); // shift up
            v.normal = f.normal;
            v.uv     = uvs[j];
            o.vertices.push_back(v);
        }
        // two triangles per face
        o.indices.insert(o.indices.end(), {
            base, base+1, base+2,
            base, base+2, base+3
        });
    }
}

// ──────────────────────────────────────────────
// UV Sphere
// ──────────────────────────────────────────────
void buildSphere(SceneObject& o, int rings, int segs) {
    o.vertices.clear();
    o.indices.clear();

    for (int r = 0; r <= rings; r++) {
        float phi = (float)M_PI * r / rings;
        for (int s = 0; s <= segs; s++) {
            float theta = 2.0f * (float)M_PI * s / segs;
            Vertex v;
            v.pos = {
                std::sin(phi) * std::cos(theta),
                std::cos(phi) + 1.0f,  // shift up by 1
                std::sin(phi) * std::sin(theta)
            };
            v.normal = glm::normalize(v.pos - glm::vec3(0,1,0));
            v.uv     = { (float)s / segs, (float)r / rings };
            o.vertices.push_back(v);
        }
    }

    for (int r = 0; r < rings; r++) {
        for (int s = 0; s < segs; s++) {
            uint32_t a = r * (segs+1) + s;
            uint32_t b = a + segs + 1;
            o.indices.insert(o.indices.end(), {a, b, a+1, b, b+1, a+1});
        }
    }
}

// ──────────────────────────────────────────────
// Cylinder
// ──────────────────────────────────────────────
void buildCylinder(SceneObject& o, int segs) {
    o.vertices.clear();
    o.indices.clear();

    float h = 1.0f; // half height
    float r = 0.8f;

    // side verts
    for (int i = 0; i <= segs; i++) {
        float a = 2.0f * (float)M_PI * i / segs;
        float cx = r * std::cos(a);
        float cz = r * std::sin(a);
        glm::vec3 n = glm::normalize(glm::vec3(cx, 0, cz));
        // bottom ring
        o.vertices.push_back({ {cx, 0, cz}, n, {(float)i/segs, 0} });
        // top ring
        o.vertices.push_back({ {cx, 2*h, cz}, n, {(float)i/segs, 1} });
    }

    // side indices
    for (int i = 0; i < segs; i++) {
        uint32_t b  = i * 2;
        o.indices.insert(o.indices.end(), { b, b+2, b+1, b+1, b+2, b+3 });
    }

    // cap centres
    uint32_t botC = (uint32_t)o.vertices.size();
    o.vertices.push_back({ {0, 0,    0}, {0,-1,0}, {0.5f,0.5f} });
    uint32_t topC = (uint32_t)o.vertices.size();
    o.vertices.push_back({ {0, 2*h, 0}, {0, 1,0}, {0.5f,0.5f} });

    // cap ring verts
    uint32_t botRing = (uint32_t)o.vertices.size();
    for (int i = 0; i <= segs; i++) {
        float a  = 2.0f * (float)M_PI * i / segs;
        float cx = r * std::cos(a), cz = r * std::sin(a);
        o.vertices.push_back({ {cx, 0,    cz}, {0,-1,0}, {0.5f+0.5f*std::cos(a), 0.5f+0.5f*std::sin(a)} });
        o.vertices.push_back({ {cx, 2*h, cz}, {0, 1,0}, {0.5f+0.5f*std::cos(a), 0.5f+0.5f*std::sin(a)} });
    }

    for (int i = 0; i < segs; i++) {
        uint32_t bi = botRing + i*2;
        uint32_t ti = botRing + i*2 + 1;
        o.indices.insert(o.indices.end(), { botC, bi+2, bi });
        o.indices.insert(o.indices.end(), { topC, ti,   ti+2 });
    }
}

// ──────────────────────────────────────────────
// Plane (XZ, lying flat)
// ──────────────────────────────────────────────
void buildPlane(SceneObject& o, int div) {
    o.vertices.clear();
    o.indices.clear();

    for (int row = 0; row <= div; row++) {
        for (int col = 0; col <= div; col++) {
            float x = -1.0f + 2.0f * col / div;
            float z = -1.0f + 2.0f * row / div;
            Vertex v;
            v.pos    = { x, 0.002f, z }; // just above y=0
            v.normal = { 0, 1, 0 };
            v.uv     = { (float)col/div, (float)row/div };
            o.vertices.push_back(v);
        }
    }

    for (int row = 0; row < div; row++) {
        for (int col = 0; col < div; col++) {
            uint32_t a = row*(div+1)+col;
            o.indices.insert(o.indices.end(), { a, a+1, a+(uint32_t)(div+1)+1, a, a+(uint32_t)(div+1)+1, a+(uint32_t)(div+1) });
        }
    }
}

// ──────────────────────────────────────────────
// Cone
// ──────────────────────────────────────────────
void buildCone(SceneObject& o, int segs) {
    o.vertices.clear();
    o.indices.clear();

    float r = 1.0f, h = 2.0f;

    // apex
    uint32_t apex = 0;
    o.vertices.push_back({ {0, h+1, 0}, {0,1,0}, {0.5f,1.0f} }); // apex at y=3

    // base ring
    uint32_t baseStart = 1;
    for (int i = 0; i <= segs; i++) {
        float a  = 2.0f * (float)M_PI * i / segs;
        float cx = r * std::cos(a), cz = r * std::sin(a);
        glm::vec3 n = glm::normalize(glm::vec3(cx, r/h, cz));
        o.vertices.push_back({ {cx, 1, cz}, n, {(float)i/segs, 0} });
    }

    // sides
    for (int i = 0; i < segs; i++) {
        o.indices.insert(o.indices.end(), { apex, baseStart+(uint32_t)i+1, baseStart+(uint32_t)i });
    }

    // base cap
    uint32_t cen = (uint32_t)o.vertices.size();
    o.vertices.push_back({ {0, 1, 0}, {0,-1,0}, {0.5f,0.5f} });
    for (int i = 0; i < segs; i++) {
        o.indices.insert(o.indices.end(), { cen, baseStart+(uint32_t)i, baseStart+(uint32_t)i+1 });
    }
}

// ──────────────────────────────────────────────
// Torus
// ──────────────────────────────────────────────
void buildTorus(SceneObject& o, int rings, int segs) {
    o.vertices.clear();
    o.indices.clear();

    float R = 0.8f, r = 0.3f; // major/minor radii

    for (int ri = 0; ri <= rings; ri++) {
        float phi = 2.0f * (float)M_PI * ri / rings;
        glm::vec3 centre = { R * std::cos(phi), 1.0f, R * std::sin(phi) };

        for (int si = 0; si <= segs; si++) {
            float theta = 2.0f * (float)M_PI * si / segs;
            glm::vec3 n = {
                std::cos(phi) * std::cos(theta),
                std::sin(theta),
                std::sin(phi) * std::cos(theta)
            };
            Vertex v;
            v.pos    = centre + r * n;
            v.normal = glm::normalize(n);
            v.uv     = { (float)ri/rings, (float)si/segs };
            o.vertices.push_back(v);
        }
    }

    for (int ri = 0; ri < rings; ri++) {
        for (int si = 0; si < segs; si++) {
            uint32_t a = ri*(segs+1)+si;
            uint32_t b = a + segs + 1;
            o.indices.insert(o.indices.end(), { a, b, a+1, b, b+1, a+1 });
        }
    }
}

// ──────────────────────────────────────────────
// Icosphere (subdivided icosahedron)
// ──────────────────────────────────────────────
static uint32_t icoMidpoint(std::vector<Vertex>& verts,
    std::unordered_map<uint64_t,uint32_t>& cache,
    uint32_t a, uint32_t b)
{
    uint64_t key = a < b ? ((uint64_t)a<<32)|b : ((uint64_t)b<<32)|a;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    glm::vec3 mid = glm::normalize((verts[a].pos + verts[b].pos) * 0.5f);
    Vertex v; v.pos = mid + glm::vec3(0,1,0); v.normal = mid;
    v.uv = { std::atan2(mid.z, mid.x)/(2*(float)M_PI)+0.5f,
              std::asin(mid.y)/(float)M_PI+0.5f };
    uint32_t idx = (uint32_t)verts.size();
    verts.push_back(v);
    cache[key] = idx;
    return idx;
}

void buildIcoSphere(SceneObject& o, int subdivisions) {
    o.vertices.clear();
    o.indices.clear();

    float t = (1.0f + std::sqrt(5.0f)) / 2.0f;

    // 12 initial verts on unit sphere
    std::vector<glm::vec3> pts = {
        {-1, t, 0},{1, t, 0},{-1,-t, 0},{1,-t, 0},
        { 0,-1, t},{0, 1, t},{ 0,-1,-t},{0, 1,-t},
        { t, 0,-1},{t, 0, 1},{-t, 0,-1},{-t, 0, 1}
    };
    for (auto& p : pts) {
        p = glm::normalize(p);
        Vertex v; v.pos = p + glm::vec3(0,1,0); v.normal = p;
        v.uv = { std::atan2(p.z, p.x) / (2*(float)M_PI) + 0.5f,
                 std::asin(p.y) / (float)M_PI + 0.5f };
        o.vertices.push_back(v);
    }

    std::vector<uint32_t> faces = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };

    std::unordered_map<uint64_t,uint32_t> cache;
    for (int s = 0; s < subdivisions; s++) {
        std::vector<uint32_t> next;
        for (size_t i = 0; i+2 < faces.size(); i+=3) {
            uint32_t a = faces[i], b = faces[i+1], c = faces[i+2];
            uint32_t ab = icoMidpoint(o.vertices, cache, a, b);
            uint32_t bc = icoMidpoint(o.vertices, cache, b, c);
            uint32_t ca = icoMidpoint(o.vertices, cache, c, a);
            next.insert(next.end(), {a,ab,ca, b,bc,ab, c,ca,bc, ab,bc,ca});
        }
        faces = next;
        cache.clear();
    }

    o.indices = faces;
}

// ──────────────────────────────────────────────
// Add a new object to scene and pick geometry
// ──────────────────────────────────────────────
SceneObject& App::addObject(const std::string& typeStr, const std::string& name, ObjType ot) {
    SceneObject o;
    o.id      = ++objCounter;
    o.name    = name;
    o.typeStr = typeStr;
    o.objType = ot;
    o.position = {0, 0, 0};

    // Assign a nice colour from palette
    static const glm::vec3 pal[] = {
        {0.87f,0.44f,0.12f},{0.28f,0.55f,0.82f},{0.45f,0.72f,0.35f},
        {0.72f,0.35f,0.72f},{0.85f,0.78f,0.22f},{0.35f,0.72f,0.72f},
    };
    o.material.color = pal[objects.size() % 6];

    if      (typeStr == "cube")      buildCube(o);
    else if (typeStr == "sphere")    buildSphere(o);
    else if (typeStr == "cylinder")  buildCylinder(o);
    else if (typeStr == "plane")     buildPlane(o);
    else if (typeStr == "cone")      buildCone(o);
    else if (typeStr == "torus")     buildTorus(o);
    else if (typeStr == "icosphere") buildIcoSphere(o);
    else                             buildCube(o);

    objects.push_back(std::move(o));
    return objects.back();
}
