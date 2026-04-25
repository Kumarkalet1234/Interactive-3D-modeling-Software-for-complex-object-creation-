# AeroMash — C++ / OpenGL Port

A C++ reimplementation of the AeroMash 3D modeller, using OpenGL 3.3 + Dear ImGui.

## Features

| Feature | Status |
|---|---|
| Orbit / pan / zoom camera | ✅ |
| Perspective & orthographic views | ✅ |
| Add mesh primitives (cube, sphere, cylinder, plane, cone, torus, ico-sphere) | ✅ |
| Object mode — select, move, rotate, scale | ✅ |
| Edit mode — enter/exit, vertex editing | ✅ |
| Sculpt mode — draw, smooth, inflate, pinch brushes | ✅ |
| Undo / redo stack (64 levels) | ✅ |
| Duplicate / delete objects | ✅ |
| Properties panel — transform, material, modifiers | ✅ |
| Outliner — scene hierarchy | ✅ |
| Timeline — keyframes, scrubber, playback | ✅ |
| Import OBJ | ✅ |
| Export OBJ / STL (binary) / JSON | ✅ |
| Solid shading with Blinn-Phong lighting | ✅ |
| Wireframe overlay | ✅ |
| X-Ray mode | ✅ |
| Grid + axis overlay | ✅ |
| Snap to grid | ✅ |
| Flash status messages | ✅ |
| Dark Blender-style theme | ✅ |

---

## Dependencies

You need these libraries (all free / open source):

| Library | Purpose | Get it |
|---|---|---|
| **GLFW 3** | Window & input | https://www.glfw.org |
| **GLAD** | OpenGL loader | https://glad.dav1d.de (GL 3.3 core) |
| **GLM** | Math (vectors, matrices) | https://github.com/g-truc/glm |
| **Dear ImGui** | UI | https://github.com/ocornut/imgui |

All four are header-mostly or single-header, so they're easy to vendor.

---

## Project layout

```
aeromash/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── aeromash.h        ← All types, structs, and declarations
│   ├── main.cpp          ← Entry point, GLFW callbacks, main loop
│   ├── geometry.cpp      ← Primitive mesh builders + GPU upload
│   ├── shaders.cpp       ← GLSL source + shader compiler
│   ├── io.cpp            ← OBJ / STL / JSON import & export
│   ├── sculpt.cpp        ← Sculpt brush operations
│   └── ui.cpp            ← All ImGui panels + 3D viewport rendering
└── vendor/               ← Put GLAD, ImGui, GLM, GLFW here
    ├── glad/
    ├── imgui/
    ├── glm/
    └── glfw3/            ← Only needed if not installed system-wide
```

---

## Quick start (Linux / Mac)

```bash
# 1 — Clone or copy the repo

# 2 — Install GLFW (if not vendored)
#   Ubuntu/Debian:  sudo apt install libglfw3-dev
#   macOS:          brew install glfw

# 3 — Download vendors
mkdir vendor && cd vendor
git clone --depth=1 https://github.com/ocornut/imgui
git clone --depth=1 https://github.com/g-truc/glm
# GLAD: generate at https://glad.dav1d.de with GL 3.3 core, then unzip here
cd ..

# 4 — Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4

# 5 — Run
./build/AeroMash
```

## Quick start (Windows / MSVC)

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
build\Release\AeroMash.exe
```

---

## Keyboard shortcuts

| Key | Action |
|---|---|
| `G` | Grab / move |
| `R` | Rotate |
| `S` | Scale |
| `X/Y/Z` after G/R/S | Constrain to axis |
| `Tab` | Toggle Object / Edit mode |
| `Numpad 1/3/7/9` | Front / Right / Top / Back view |
| `Numpad 5` | Toggle orthographic |
| `Z` | Toggle wireframe |
| `A` | Select / deselect all |
| `Shift+D` | Duplicate |
| `X` or `Delete` | Delete selected |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` | Redo |
| `Numpad .` | Focus on selection |
| `Space` | Play / pause timeline |
| (Sculpt) `D/S/I/P` | Draw / Smooth / Inflate / Pinch |
| `Ctrl+E` | Export OBJ |
| Mouse wheel | Zoom |
| Middle drag | Orbit |
| Shift + Middle drag | Pan |

---

## Notes

- The file dialog currently falls back to stdin. For a proper native dialog, drop in
  [tinyfiledialogs](https://sourceforge.net/projects/tinyfiledialogs/) or
  [nativefiledialog](https://github.com/mlabbe/nativefiledialog) and replace the two
  stub functions at the bottom of `ui.cpp`.
- Modifier stack (Subdivision, Mirror, Array…) stores parameters in the properties panel
  but does not yet auto-apply geometry. Hook the modifier loop in `geometry.cpp` to
  rebuild the mesh when settings change.
- FBX import/export is intentionally omitted — the FBX SDK requires a proprietary licence.
  Use OBJ or STL instead, or integrate [OpenFBX](https://github.com/nem0/OpenFBX).
