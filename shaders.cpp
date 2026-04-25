// shaders.cpp — GLSL source and compiler
#include "aeromash.h"

// ──────────────────────────────────────────────
// Solid / lit shader (Blinn-Phong, no textures)
// ──────────────────────────────────────────────
static const char* SOLID_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;
uniform mat3 uNormalMat;

out vec3 fragPos;
out vec3 fragNorm;
out vec2 fragUV;

void main() {
    vec4 world  = uModel * vec4(aPos, 1.0);
    fragPos     = world.xyz;
    fragNorm    = normalize(uNormalMat * aNorm);
    fragUV      = aUV;
    gl_Position = uProj * uView * world;
}
)";

static const char* SOLID_FRAG = R"(
#version 330 core
in vec3 fragPos;
in vec3 fragNorm;
in vec2 fragUV;

uniform vec3  uColor;
uniform float uRoughness;
uniform float uMetalness;
uniform float uOpacity;
uniform bool  uSelected;
uniform vec3  uCamPos;

// two lights like Blender's default scene
const vec3 LIGHT1_DIR = normalize(vec3( 1.0, 2.0, 1.5));
const vec3 LIGHT1_COL = vec3(1.0, 0.97, 0.90);
const vec3 LIGHT2_DIR = normalize(vec3(-1.5,-1.0,-1.0));
const vec3 LIGHT2_COL = vec3(0.25, 0.28, 0.35);
const vec3 AMBIENT    = vec3(0.09, 0.09, 0.11);

out vec4 outColor;

void main() {
    vec3 N = normalize(fragNorm);
    vec3 V = normalize(uCamPos - fragPos);

    // Diffuse
    float d1 = max(dot(N, LIGHT1_DIR), 0.0);
    float d2 = max(dot(N,-LIGHT2_DIR), 0.0);

    // Blinn-Phong specular
    float shininess = mix(4.0, 128.0, 1.0 - uRoughness);
    vec3  H1   = normalize(LIGHT1_DIR + V);
    float spec = pow(max(dot(N, H1), 0.0), shininess) * (1.0 - uRoughness);

    vec3 diffuse  = uColor * (d1*LIGHT1_COL + d2*LIGHT2_COL + AMBIENT);
    vec3 specCol  = mix(vec3(spec), uColor*spec, uMetalness);
    vec3 col      = diffuse + specCol;

    // Selection rim
    if (uSelected) {
        float rim = 1.0 - max(dot(N, V), 0.0);
        col += vec3(0.22, 0.42, 0.87) * pow(rim, 3.0) * 1.4;
    }

    outColor = vec4(col, uOpacity);
}
)";

// ──────────────────────────────────────────────
// Wireframe / overlay shader (flat colour)
// ──────────────────────────────────────────────
static const char* WIRE_VERT = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); }
)";

static const char* WIRE_FRAG = R"(
#version 330 core
uniform vec4 uColor;
out vec4 outColor;
void main() { outColor = uColor; }
)";

// ──────────────────────────────────────────────
// Compile helper
// ──────────────────────────────────────────────
GLuint compileShader(const char* vert, const char* frag) {
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512]; glGetShaderInfoLog(s, 512, nullptr, log);
            fprintf(stderr, "Shader compile error:\n%s\n", log);
        }
        return s;
    };

    GLuint v = compile(GL_VERTEX_SHADER,   vert);
    GLuint f = compile(GL_FRAGMENT_SHADER, frag);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512]; glGetProgramInfoLog(p, 512, nullptr, log);
        fprintf(stderr, "Shader link error:\n%s\n", log);
    }
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

void buildShaders() {
    g.solidShader   = compileShader(SOLID_VERT, SOLID_FRAG);
    g.wireShader    = compileShader(WIRE_VERT,  WIRE_FRAG);
    g.overlayShader = g.wireShader; // reuse
}
