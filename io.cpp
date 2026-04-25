// io.cpp — OBJ / STL / JSON import and export
#include "aeromash.h"
#include <fstream>
#include <sstream>
#include <cstring>

// ──────────────────────────────────────────────
// OBJ import (positions + normals + faces)
// ──────────────────────────────────────────────
bool importOBJ(const std::string& path) {
    std::ifstream f(path);
    if (!f) { g.flash("OBJ: can't open " + path); return false; }

    std::vector<glm::vec3> pos, nrm;
    std::vector<glm::vec2> uvs;

    SceneObject o;
    o.id      = ++g.objCounter;
    o.name    = path.substr(path.rfind('/')+1);
    o.typeStr = "imported";

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0]=='#') continue;
        std::istringstream ss(line);
        std::string tok; ss >> tok;

        if (tok == "v") {
            glm::vec3 p; ss >> p.x >> p.y >> p.z; pos.push_back(p);
        } else if (tok == "vn") {
            glm::vec3 n; ss >> n.x >> n.y >> n.z; nrm.push_back(n);
        } else if (tok == "vt") {
            glm::vec2 uv; ss >> uv.x >> uv.y; uvs.push_back(uv);
        } else if (tok == "f") {
            // Parse face: v/vt/vn  or  v//vn  or  v
            std::vector<uint32_t> fv;
            std::string elem;
            while (ss >> elem) {
                // grab position index
                uint32_t pi = 0, ui = 0, ni = 0;
                sscanf(elem.c_str(), "%u/%u/%u", &pi, &ui, &ni);
                if (!ni) sscanf(elem.c_str(), "%u//%u", &pi, &ni);
                if (!pi) sscanf(elem.c_str(), "%u",     &pi);

                Vertex vx;
                if (pi && pi <= pos.size()) vx.pos    = pos[pi-1];
                if (ui && ui <= uvs.size()) vx.uv     = uvs[ui-1];
                if (ni && ni <= nrm.size()) vx.normal = nrm[ni-1];

                fv.push_back((uint32_t)o.vertices.size());
                o.vertices.push_back(vx);
            }
            // fan-triangulate the face
            for (uint32_t i = 1; i + 1 < fv.size(); i++) {
                o.indices.push_back(fv[0]);
                o.indices.push_back(fv[i]);
                o.indices.push_back(fv[i+1]);
            }
        }
    }

    if (o.vertices.empty()) { g.flash("OBJ: no geometry in " + path); return false; }

    // If no normals were supplied, compute smooth normals
    bool hasNormals = false;
    for (auto& v : o.vertices) if (glm::length(v.normal) > 0.01f) { hasNormals = true; break; }
    if (!hasNormals) {
        for (size_t i = 0; i+2 < o.indices.size(); i+=3) {
            auto& v0 = o.vertices[o.indices[i]];
            auto& v1 = o.vertices[o.indices[i+1]];
            auto& v2 = o.vertices[o.indices[i+2]];
            glm::vec3 n = glm::normalize(glm::cross(v1.pos-v0.pos, v2.pos-v0.pos));
            v0.normal += n; v1.normal += n; v2.normal += n;
        }
        for (auto& v : o.vertices) v.normal = glm::normalize(v.normal);
    }

    o.gpuDirty = true;
    g.saveState();
    g.objects.push_back(std::move(o));
    g.active = &g.objects.back();
    g.active->selected = true;
    g.selected = { g.active };
    g.flash("Imported " + g.active->name);
    return true;
}

// ──────────────────────────────────────────────
// OBJ export
// ──────────────────────────────────────────────
bool exportOBJ(const std::string& path) {
    std::ofstream f(path);
    if (!f) { g.flash("OBJ: can't write " + path); return false; }

    f << "# AeroMash OBJ export\n";
    uint32_t vOff = 1;

    for (auto& o : g.objects) {
        if (o.objType != ObjType::Mesh) continue;
        f << "o " << o.name << "\n";
        f << "mtllib " << o.name << ".mtl\n";
        f << "usemtl " << o.material.name << "\n";

        glm::mat4 M = o.getModelMatrix();
        for (auto& v : o.vertices) {
            glm::vec4 wp = M * glm::vec4(v.pos, 1);
            f << "v " << wp.x << " " << wp.y << " " << wp.z << "\n";
        }
        for (auto& v : o.vertices) {
            glm::vec3 n = glm::normalize(glm::mat3(M) * v.normal);
            f << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        }
        for (auto& v : o.vertices)
            f << "vt " << v.uv.x << " " << v.uv.y << "\n";

        for (size_t i = 0; i+2 < o.indices.size(); i+=3) {
            uint32_t a = o.indices[i]  +vOff;
            uint32_t b = o.indices[i+1]+vOff;
            uint32_t c = o.indices[i+2]+vOff;
            f << "f " << a<<"/"<<a<<"/"<<a << " "
                      << b<<"/"<<b<<"/"<<b << " "
                      << c<<"/"<<c<<"/"<<c << "\n";
        }
        vOff += (uint32_t)o.vertices.size();
    }

    g.flash("Exported → " + path);
    return true;
}

// ──────────────────────────────────────────────
// STL export (binary)
// ──────────────────────────────────────────────
bool exportSTL(const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { g.flash("STL: can't write " + path); return false; }

    // Count total triangles
    uint32_t triCount = 0;
    for (auto& o : g.objects)
        if (o.objType == ObjType::Mesh)
            triCount += (uint32_t)(o.indices.size() / 3);

    // 80-byte header
    char header[80] = {};
    strncpy(header, "AeroMash STL Export", 79);
    f.write(header, 80);
    f.write(reinterpret_cast<const char*>(&triCount), 4);

    for (auto& o : g.objects) {
        if (o.objType != ObjType::Mesh) continue;
        glm::mat4 M = o.getModelMatrix();

        for (size_t i = 0; i+2 < o.indices.size(); i+=3) {
            auto& v0 = o.vertices[o.indices[i]];
            auto& v1 = o.vertices[o.indices[i+1]];
            auto& v2 = o.vertices[o.indices[i+2]];

            glm::vec3 p0 = glm::vec3(M * glm::vec4(v0.pos,1));
            glm::vec3 p1 = glm::vec3(M * glm::vec4(v1.pos,1));
            glm::vec3 p2 = glm::vec3(M * glm::vec4(v2.pos,1));
            glm::vec3 n  = glm::normalize(glm::cross(p1-p0, p2-p0));

            f.write(reinterpret_cast<const char*>(&n),  12);
            f.write(reinterpret_cast<const char*>(&p0), 12);
            f.write(reinterpret_cast<const char*>(&p1), 12);
            f.write(reinterpret_cast<const char*>(&p2), 12);
            uint16_t attr = 0;
            f.write(reinterpret_cast<const char*>(&attr), 2);
        }
    }

    g.flash("Exported STL → " + path);
    return true;
}

// ──────────────────────────────────────────────
// Minimal JSON scene export
// ──────────────────────────────────────────────
bool exportJSON(const std::string& path) {
    std::ofstream f(path);
    if (!f) { g.flash("JSON: can't write " + path); return false; }

    f << "{\n  \"aeromash\": \"1.5\",\n  \"objects\": [\n";

    for (size_t oi = 0; oi < g.objects.size(); oi++) {
        auto& o = g.objects[oi];
        f << "    {\n";
        f << "      \"id\": " << o.id << ",\n";
        f << "      \"name\": \"" << o.name << "\",\n";
        f << "      \"type\": \"" << o.typeStr << "\",\n";
        f << "      \"position\": [" << o.position.x<<","<<o.position.y<<","<<o.position.z << "],\n";
        f << "      \"rotation\": [" << o.rotation.x<<","<<o.rotation.y<<","<<o.rotation.z << "],\n";
        f << "      \"scale\":    [" << o.scale.x   <<","<<o.scale.y   <<","<<o.scale.z    << "],\n";

        // vertices
        f << "      \"vertices\": [";
        for (size_t vi = 0; vi < o.vertices.size(); vi++) {
            auto& v = o.vertices[vi];
            if (vi) f << ",";
            f << "[" << v.pos.x<<","<<v.pos.y<<","<<v.pos.z << "]";
        }
        f << "],\n";

        // indices
        f << "      \"indices\": [";
        for (size_t ii = 0; ii < o.indices.size(); ii++) {
            if (ii) f << ",";
            f << o.indices[ii];
        }
        f << "]\n";

        f << "    }";
        if (oi+1 < g.objects.size()) f << ",";
        f << "\n";
    }

    f << "  ]\n}\n";
    g.flash("Exported JSON → " + path);
    return true;
}
