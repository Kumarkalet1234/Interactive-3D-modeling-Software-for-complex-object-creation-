// sculpt.cpp — sculpt brush operations
#include "aeromash.h"

// ──────────────────────────────────────────────
// Sculpt brush — modifies vertex positions in world space
// mx, my are normalised device coordinates [-1,+1]
// ──────────────────────────────────────────────
void sculptBrush(float mx, float my) {
    if (!g.active || g.mode != AppMode::Sculpt) return;
    SceneObject& obj = *g.active;

    // Build a ray from the camera
    float aspect    = (float)g.vpW / (float)g.vpH;
    glm::mat4 proj  = g.cam.proj(aspect);
    glm::mat4 view  = g.cam.view();
    glm::mat4 VP    = proj * view;
    glm::mat4 invVP = glm::inverse(VP);

    // Two points on the ray
    glm::vec4 ndcNear = {mx, my,  -1, 1};
    glm::vec4 ndcFar  = {mx, my,   1, 1};
    glm::vec4 wn = invVP * ndcNear; wn /= wn.w;
    glm::vec4 wf = invVP * ndcFar;  wf /= wf.w;

    glm::vec3 rayOrigin = g.cam.eyePos();
    glm::vec3 rayDir    = glm::normalize(glm::vec3(wf) - glm::vec3(wn));

    // Find closest intersection with object's bounding sphere
    // (cheap; good enough for sculpting)
    glm::vec3 oc = rayOrigin - obj.position;
    float b = glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - 9.0f; // radius^2 = 3
    float disc = b*b - c;
    if (disc < 0) return;
    float t = -b - std::sqrt(disc);
    if (t < 0) t = -b + std::sqrt(disc);
    if (t < 0) return;

    glm::vec3 hitWorld = rayOrigin + t * rayDir;

    // Transform hit point into object local space
    glm::mat4 inv = glm::inverse(obj.getModelMatrix());
    glm::vec3 hitLocal = glm::vec3(inv * glm::vec4(hitWorld, 1));

    float rad    = g.sculptRadius;
    float str    = g.sculptStrength;
    bool  dirty  = false;

    for (auto& v : obj.vertices) {
        float dist = glm::length(v.pos - hitLocal);
        if (dist >= rad) continue;

        // Smooth falloff
        float f = (1.0f - dist / rad) * str;
        dirty = true;

        switch (g.sculptTool) {
            case SculptTool::Draw: {
                // Push outward along normal
                glm::vec3 dir = glm::length(v.normal) > 0.001f
                    ? glm::normalize(v.normal) : glm::normalize(v.pos);
                v.pos += dir * f;
                break;
            }
            case SculptTool::Smooth: {
                // Average with neighbours within radius
                glm::vec3 avg = v.pos;
                int cnt = 1;
                for (auto& v2 : obj.vertices) {
                    if (glm::length(v2.pos - v.pos) < rad * 0.5f) {
                        avg += v2.pos; cnt++;
                    }
                }
                avg /= (float)cnt;
                v.pos = glm::mix(v.pos, avg, f * 2.0f);
                break;
            }
            case SculptTool::Inflate: {
                glm::vec3 dir = glm::length(v.normal) > 0.001f
                    ? glm::normalize(v.normal) : glm::normalize(v.pos);
                v.pos += dir * f * 0.5f;
                break;
            }
            case SculptTool::Pinch: {
                v.pos = glm::mix(v.pos, hitLocal, f * 0.4f);
                break;
            }
        }
    }

    if (dirty) {
        // Recompute normals
        for (auto& v : obj.vertices) v.normal = {0,0,0};
        for (size_t i = 0; i+2 < obj.indices.size(); i+=3) {
            auto& v0 = obj.vertices[obj.indices[i]];
            auto& v1 = obj.vertices[obj.indices[i+1]];
            auto& v2 = obj.vertices[obj.indices[i+2]];
            glm::vec3 n = glm::normalize(glm::cross(v1.pos - v0.pos, v2.pos - v0.pos));
            v0.normal += n; v1.normal += n; v2.normal += n;
        }
        for (auto& v : obj.vertices) v.normal = glm::normalize(v.normal);

        obj.gpuDirty = true;
    }
}
