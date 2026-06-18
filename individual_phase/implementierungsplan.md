# Implementierungsplan — Individualphase
**Projekt:** Echtzeit-Tsunamis – Interaktive Visualisierung mit OpenGL  
**Team:** Mika Brückner, Yannik Köllmann, Jan Vogt  
**Zeitraum:** 18.06.–08.07.2026

---

## Ist-Stand

| Komponente | Status |
|---|---|
| `WavePropagation2d` | fertig; `getHeight()` / `getBathymetry()` liefern `t_real*` |
| Zeitloop | in `main.cpp` (776 Zeilen); `setGhost → timeStep → write NetCDF` |
| I/O | NetCDF-Writer, CSV, Stations — alles Batch |
| Build | SCons + `shell.nix`; GCC14, NetCDF, pugixml, Catch2 |
| OpenGL | fehlt komplett (kein GLFW/GLAD/ImGui) |

Neues Build-Target: `build/tsunami_lab_viz` — eigener Einstiegspunkt `src/main_viz.cpp`.  
Die bestehende `build/tsunami_lab`-Pipeline bleibt unverändert (CI, Valgrind, Tests laufen weiter).

---

## Phase 0 — Build-Integration (WP1)

### 0.1 `shell.nix`
```nix
glfw   # GLFW3 Fenster + Input
# OpenGL: macOS via Frameworks, Linux via libGL aus nixpkgs
```

### 0.2 Drittanbieter-Dateien im Repo
```
thirdparty/
  glad/
    include/glad/glad.h
    include/KHR/khrplatform.h
    src/glad.c                  # generiert für OpenGL 3.3 Core
  imgui/                        # Git-Submodule oder kopiert
    imgui.h / imgui.cpp
    imgui_draw.cpp / imgui_widgets.cpp / imgui_tables.cpp
    backends/imgui_impl_glfw.h/.cpp
    backends/imgui_impl_opengl3.h/.cpp
```

### 0.3 `SConstruct` — neue Abschnitte
```python
# GLFW via pkg-config (analog nc-config)
try:
    glfw_cflags = subprocess.check_output(['pkg-config','--cflags','glfw3']).split()
    glfw_libs   = subprocess.check_output(['pkg-config','--libs',  'glfw3']).split()
    env.Append(CXXFLAGS=glfw_cflags, LINKFLAGS=glfw_libs)
except: pass

# macOS OpenGL Frameworks
if platform.system() == 'Darwin':
    env.Append(LINKFLAGS=['-framework','OpenGL','-framework','Cocoa',
                           '-framework','IOKit','-framework','CoreVideo'])

# GLAD (C-Datei, kein C++)
glad_env = env.Clone()
glad_env.Append(CXXFLAGS=['-w'], CPPPATH=['thirdparty/glad/include'])
l_glad = glad_env.Object('build/glad', 'thirdparty/glad/src/glad.c')

# ImGui
imgui_env = env.Clone()
imgui_env.Append(CXXFLAGS=['-w'],
                 CPPPATH=['thirdparty/imgui','thirdparty/imgui/backends',
                          'thirdparty/glad/include'])
imgui_srcs = ['thirdparty/imgui/imgui.cpp', 'thirdparty/imgui/imgui_draw.cpp',
              'thirdparty/imgui/imgui_widgets.cpp','thirdparty/imgui/imgui_tables.cpp',
              'thirdparty/imgui/backends/imgui_impl_glfw.cpp',
              'thirdparty/imgui/backends/imgui_impl_opengl3.cpp']
l_imgui = [imgui_env.Object(f'build/imgui_{i}', s) for i,s in enumerate(imgui_srcs)]

# Viz-Executable
env.Program('build/tsunami_lab_viz',
            source = viz_sources + [l_glad] + l_imgui + l_pugixml)
```

---

## Phase 1 — OpenGL Grundgerüst (WP1 / WP2)

**Neue Dateien:** `src/visualization/`

### `Window.h/.cpp`
```cpp
class Window {
public:
    Window(int width, int height, const char* title);
    ~Window();
    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();
    GLFWwindow* handle() { return m_window; }
private:
    GLFWwindow* m_window;
};
```
- `glfwInit()`, GLAD laden, OpenGL 3.3 Core Context
- Fehler-Callback auf stderr

### `src/main_viz.cpp` (Einstiegspunkt)
```cpp
int main() {
    Window window(1280, 720, "Tsunami Lab");
    while (!window.shouldClose()) {
        window.pollEvents();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        window.swapBuffers();
    }
}
```

> **Checkpoint M1 (Mitte Woche 1):** Fenster öffnet sich, läuft stabil, baut in SCons.

---

## Phase 2 — Kamera, Szene, UI (WP2)

### `Camera.h/.cpp`
Arcball-Kamera — Rotation per Maus-Drag, Zoom per Scroll, Pan per Middle-Click:
```cpp
class Camera {
public:
    glm::mat4 view() const;
    glm::mat4 projection(float aspect) const;
    void onMouseDrag(float dx, float dy);
    void onScroll(float dy);
    Ray unproject(float mouseX, float mouseY, int screenW, int screenH) const;
private:
    glm::vec3 m_target{0,0,0};
    float     m_distance{10.f}, m_azimuth{0.f}, m_elevation{0.3f};
};
```

### `Ui.h/.cpp`
ImGui initialisieren und pro Frame wrappen:
```cpp
class Ui {
public:
    void init(GLFWwindow*);
    void beginFrame();
    void endFrame();   // ImGui::Render() + RenderDrawData
    void shutdown();
};
```

### `AppState` enum
```cpp
enum class AppState { REGION_SELECT, LOADING, SIMULATING };
```
Steuert was im Render-Loop passiert und was ImGui anzeigt.

---

## Phase 3 — SimBuffer + Solver-Thread (WP6)

### `SimBuffer.h`
```cpp
class SimBuffer {
public:
    SimBuffer(t_idx nx, t_idx ny);

    // Solver-Thread schreibt:
    void write(const t_real* height, t_idx n);  // kopiert in back-Buffer, setzt dirty

    // Render-Thread liest:
    bool swap();                    // wenn dirty: front<->back, gibt true zurück
    const t_real* front() const;    // Render-Thread liest immer front

private:
    std::vector<t_real> m_buf[2];   // [0]=front, [1]=back
    std::mutex          m_mtx;      // nur für swap(), ~5ns kritischer Abschnitt
    std::atomic<bool>   m_dirty{false};
    t_idx               m_nx, m_ny;
};
```

### `SolverThread.h/.cpp`
```cpp
class SolverThread {
public:
    SolverThread(SimBuffer& buf, t_idx nx, t_idx ny, t_real dxy);
    void start();
    void stop();
    void requestDisplacement(t_idx cx, t_idx cy, float strength, float depth);

private:
    void run();

    std::thread                              m_thread;
    SimBuffer&                               m_buffer;
    tsunami_lab::patches::WavePropagation2d  m_solver;
    std::atomic<bool>                        m_running{false};

    // Displacement-Request (atomic, kein Mutex nötig)
    std::atomic<bool>  m_dispatchDisplacement{false};
    t_idx  m_dispCx, m_dispCy;
    float  m_dispStrength, m_dispDepth;
};
```

**`run()`-Schleife — kein NetCDF:**
```cpp
void SolverThread::run() {
    while (m_running) {
        if (m_dispatchDisplacement.exchange(false))
            applyGaussDisplacement(m_dispCx, m_dispCy, m_dispStrength, m_dispDepth);

        m_solver.setGhost(BoundaryCondition::Outflow, BoundaryCondition::Outflow);
        m_solver.timeStep(m_scaling, "fwave");

        m_buffer.write(m_solver.getHeight(), m_nx * m_ny);
        // kein NetCDF, kein CSV
    }
}
```

`WavePropagation2d` wird **unverändert** übernommen — kein Refactoring nötig.

---

## Phase 4 — Erde + Gebietsauswahl (WP3)

### Konzept
```
AppState::REGION_SELECT:
  Globale GEBCO low-res (5°-Raster) als flaches Mesh
  → equirektangulare Projektion: lon → x, lat → y
  → Colorcoding: Tiefsee blau, Festland braun/grün
  → Nutzer klickt + zieht → Rechteck in lon/lat-Koordinaten
  → ImGui: Bestätigen-Button → AppState::LOADING
```

### `GlobeView.h/.cpp`
```cpp
class GlobeView {
public:
    void init(const char* gebcoLowResPath);  // NetCDF, 5°-Auflösung
    void draw(const Camera&);
    void onMouseDown(float lon, float lat);
    void onMouseMove(float lon, float lat);
    BBox getSelection() const;               // {lon_min, lon_max, lat_min, lat_max}
private:
    GLuint m_vao, m_vbo, m_ebo;
    BBox   m_selection;
    bool   m_selecting{false};
};
```

Low-res GEBCO-Datei (< 1 MB) liegt unter `data/gebco_lowres.nc` als Fallback im Repo.

### `GebcoLoader.h/.cpp` (WP3 — Jan)
```cpp
class GebcoLoader {
public:
    // Lädt high-res GEBCO für Bbox, resampled auf nx×ny Solver-Gitter
    static std::vector<t_real> load(BBox bbox, t_idx nx, t_idx ny);
    // Fallback: vorbereitete lokale NetCDF-Ausschnitte
    static std::vector<t_real> loadLocal(const std::string& region);
};
```
Nutzt `NetCDF::read()` — bereits vorhanden.

### Übergang LOADING → SIMULATING
```cpp
auto bath = GebcoLoader::load(selection, nx, ny);

for (t_idx iy = 0; iy < ny; iy++)
    for (t_idx ix = 0; ix < nx; ix++)
        solver.setBathymetry(ix, iy, bath[iy*nx + ix]);
// Wasserinitialisierung: h = max(-b, 0)

renderer.uploadBathymetry(bath.data(), nx, ny);  // VBO, einmalig
solverThread.start();
state = AppState::SIMULATING;
```

---

## Phase 5 — Simulations-Ansicht (WP2)

### Zwei Meshes, ein Grid

Beide Meshes teilen die gleichen `nx × ny` Gitterpositionen in X/Z; nur Y (Höhe) unterscheidet sich.

```
Vertices : nx × ny
Indices  : (nx-1) × (ny-1) × 2 Dreiecke (Index Buffer)
```

### `Mesh.h/.cpp`
```cpp
class Mesh {
public:
    void init(t_idx nx, t_idx ny, float dx, float dy);
    void uploadHeights(const t_real* heights, t_idx stride);  // glBufferSubData
    void draw(GLuint shader);
private:
    GLuint m_vao, m_vbo_pos, m_vbo_height, m_ebo;
    t_idx  m_nx, m_ny;
};
```

Positionen (X/Z) werden einmal beim Init hochgeladen.  
Nur die Y-Werte werden per Frame via `glBufferSubData` aktualisiert.

### Shader — Colorcoding

**Bathymetrie-Fragment-Shader:**
```glsl
float h = bathymetry;
vec3 color = h < 0.0
    ? mix(vec3(0.0,0.1,0.4), vec3(0.2,0.5,0.8), clamp(h / -6000.0, 0.0, 1.0))
    : mix(vec3(0.3,0.6,0.2), vec3(0.6,0.4,0.2), clamp(h /  3000.0, 0.0, 1.0));
```

**Wasserhöhen-Fragment-Shader:**
```glsl
float anomaly = waterHeight - restHeight;
vec3 color = mix(vec3(0.0,0.3,0.7), vec3(1.0,0.2,0.0),
                 clamp((anomaly + 5.0) / 10.0, 0.0, 1.0));
```

### Render-Loop im SIMULATING-Zustand
```cpp
if (simBuffer.swap())
    waterMesh.uploadHeights(simBuffer.front(), stride);

bathMesh.draw(bathShader);
waterMesh.draw(waterShader);
ui.draw();   // ImGui als 2D-Overlay
```

> **Checkpoint M2 (Ende Woche 1):** Wasseroberfläche animiert live, Solver läuft im Hintergrund.

---

## Phase 6 — Interaktive Auslösung (WP4)

### Picking: Mausklick → Gitterzelle
```cpp
Ray ray = camera.unproject(mouseX, mouseY, screenW, screenH);

// Ray × y=0-Ebene (Meeresboden-Mittelwert)
float t = -ray.origin.y / ray.dir.y;
glm::vec3 worldPos = ray.origin + t * ray.dir;

t_idx cx = (t_idx)((worldPos.x - originX) / dx);
t_idx cy = (t_idx)((worldPos.z - originY) / dy);
```

### Validierung
```cpp
bool isPlausible(t_idx cx, t_idx cy) {
    return bathymetry[cy*nx + cx] < 0.0f
        && isSubductionZone(cx*dx + originX, cy*dy + originY);
}
```

Subduktionszonen als `constexpr`-Array (erweiterbar auf Bird-GeoJSON):
```cpp
struct SubductionZone { float lon_min, lon_max, lat_min, lat_max; };
constexpr SubductionZone ZONES[] = {
    { 130, 150,  30,  45 },   // Japangraben (Tohoku)
    { -76, -68, -45, -15 },   // Chile/Peru
    {  95, 106,  -6,   6 },   // Sunda/Sumatra
    {-130,-122,  40,  50 },   // Cascadia
};
```

### Gauß-Verschiebung
```cpp
void applyGaussDisplacement(t_idx cx, t_idx cy, float strength, float sigma) {
    for (t_idx iy = 0; iy < ny; iy++)
        for (t_idx ix = 0; ix < nx; ix++) {
            float ddx = (float)(ix - cx), ddy = (float)(iy - cy);
            float disp = strength * std::exp(-(ddx*ddx + ddy*ddy) / (2*sigma*sigma));
            if (bathymetry[iy*nx + ix] < 0.0f)
                solver.setHeight(ix, iy, solver.getHeight()[iy*stride + ix] + disp);
        }
}
```

Aufruf immer über `SolverThread::requestDisplacement()` — nie direkt aus dem Render-Thread in den Solver schreiben.

### ImGui-Panel
```
[Stärke   ] ████████░░░░  8.5
[Tiefe    ] ████░░░░░░░░  15 km
[Ausricht.] ████████████  45°
[Auslösen ]   ← nur aktiv wenn Zonen-Check positiv
```

> **Checkpoint M3 (Ende Woche 2):** Klick löst Welle aus, Welle läuft stabil, GEBCO-Gebiet ladbar.

---

## Phase 7 — Szenario-System (WP5)

```cpp
struct Scenario {
    std::string name;
    BBox        region;
    t_idx       cx, cy;
    float       magnitude;
    float       depth;
    float       strike;
};

constexpr Scenario SCENARIOS[] = {
    {"Tohoku 2011", {138,148,35,42}, ..., 9.0f, 29.f, 193.f},
    {"Chile 1960",  {-76,-68,-45,-35},..., 9.5f, 25.f, 175.f},
};
```

ImGui-Dropdown: Szenario auswählen → Region laden → Solver starten → Displacement automatisch triggern.

> **Checkpoint M4 (Ende Woche 3):** Szenario lädt, läuft live, Bericht + Tests abgeschlossen.

---

## Neue Dateistruktur

```
src/
  main_viz.cpp
  visualization/
    Window.h/.cpp
    Camera.h/.cpp
    Mesh.h/.cpp
    Shader.h/.cpp           # GLSL laden + kompilieren
    Ui.h/.cpp               # ImGui wrapper
    SimBuffer.h/.cpp        # Double-Buffer
    SolverThread.h/.cpp     # Background-Thread
    GlobeView.h/.cpp        # Erde + Selektion
    GebcoLoader.h/.cpp      # GEBCO NetCDF → Solver-Gitter
    Renderer.h/.cpp         # koordiniert Meshes + Shader
    Scenario.h              # Szenario-Daten (constexpr)
  shaders/
    bath.vert / bath.frag
    water.vert / water.frag
    globe.vert / globe.frag
thirdparty/
  glad/
  imgui/
data/
  gebco_lowres.nc           # Fallback-Datensatz (< 1 MB)
```

---

## Aufgabenverteilung nach Phase

| Phase | Yannik | Mika | Jan |
|---|---|---|---|
| 0 Build | gemeinsam | gemeinsam | gemeinsam |
| 1 Grundgerüst | Window, main_viz | | |
| 2 Kamera/UI | Camera, Ui | | |
| 3 SimBuffer/Thread | SimBuffer | SolverThread | |
| 4 Gebietsauswahl | GlobeView | | GebcoLoader |
| 5 Simulations-Ansicht | Mesh, Shader, Renderer | | |
| 6 Auslösung | Picking | Gauß/Okada, ImGui-Panel | |
| 7 Szenarien | | | Scenario-Daten, Parameter |
| WP7 Tests/Doku | alle | alle | alle |

---

## Review gegen Projektplan

| Anforderung | Abgedeckt | Anmerkung |
|---|---|---|
| 3D-Rendering Bathymetrie + Wasser | ✅ Phase 5 | Zwei Meshes, Colorcoding |
| Solver im Hintergrund-Thread | ✅ Phase 3 | `SolverThread` + `SimBuffer` |
| Doppelbuffer ohne Render-Blockierung | ✅ Phase 3 | Mutex nur beim Swap |
| Okada/Gauß-Verschiebung per Klick | ✅ Phase 6 | Gauß zuerst, Okada optional |
| Parameter-Eingabe (Stärke, Tiefe, Ausrichtung) | ✅ Phase 6 | ImGui-Slider |
| GEBCO-Anbindung | ✅ Phase 4 | inkl. lokalem Fallback |
| Gebietsauswahl (Rechteck) | ✅ Phase 4 | `GlobeView` + BBox |
| Szenarien Tohoku / Chile | ✅ Phase 7 | `constexpr Scenario[]` |
| Catch2-Tests, Valgrind, CI | ✅ bestehend | neue Klassen bekommen Tests |
| SCons-Build | ✅ Phase 0 | separates `viz`-Target |
| kein NetCDF im Viz-Modus | ✅ Phase 3 | `run()` ohne I/O |
| **Nicht-Ziel:** vollständiges Okada | ✅ ausgelassen | inkrementell |
| **Nicht-Ziel:** GPU-Parallelisierung | ✅ nicht geplant | |
| **Nicht-Ziel:** persistente Speicherung | ✅ nicht geplant | |

### Offene Risiken

| Risiko | Gegenmaßnahme |
|---|---|
| GEBCO-Live-API blockiert WP2/WP6 | `gebco_lowres.nc` + lokale Ausschnitte von Anfang an im Repo |
| Okada-Kopplung physikalisch nicht identisch mit `deltaXPsi` | im Bericht transparent machen; Gauß als explizite Vereinfachung deklarieren |
| Displacement aus Render-Thread → Race Condition | immer über `SolverThread::requestDisplacement()`, nie direkt `setHeight()` aus Main-Thread |
| OpenGL 4.1 max auf macOS | 3.3 Core reicht für alles geplante |
