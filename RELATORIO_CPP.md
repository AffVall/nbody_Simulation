# Relatório: Implementação em C++ do Problema dos 3 Corpos

## Sumário

1. [Análise do Projeto Atual (Python)](#1-análise-do-projeto-atual)
2. [Sistema de Unidades](#2-sistema-de-unidades)
3. [Integradores Numéricos](#3-integradores-numéricos)
4. [Equações de Movimento](#4-equações-de-movimento)
5. [Correção Relativística (Toggle)](#5-correção-relativística-toggle)
6. [Passo de Tempo Ahmad-Cohen](#6-passo-de-tempo-ahmad-cohen)
7. [Softening e Colisões](#7-softening-e-colisões)
8. [Cálculo de Forças (Toggle)](#8-cálculo-de-forças-toggle)
9. [Estrutura do Código C++](#9-estrutura-do-código-cpp)
10. [Saída Binária e Leitor](#10-saída-binária-e-leitor)
11. [Visualização OpenGL](#11-visualização-opengl)
12. [Arquitetura Geral](#12-arquitetura-geral)
13. [Referências](#13-referências)

---

## 1. Análise do Projeto Atual

O projeto atual é um **builder** (construtor de presets) em Python/PyQt5 que gera JSON com parâmetros iniciais para uma simulação gravitacional N-corpos. O código Python contém:

- **`builder.py`**: Interface gráfica para editar propriedades de corpos (massa, posição, velocidade, raio, cor)
- **`UNITS.md`**: Documentação completa do sistema de unidades
- **`presets/`**: 4 cenários pré-definidos (Sistema Solar, Figura-8, Estrela Binária, Terra-Lua)

**O que falta**: O código de simulação em si — o builder apenas configura condições iniciais. A simulação real precisa ser implementada.

---

## 2. Sistema de Unidades

**Decisão: SI (kg, m, s)**

Usar unidades do Sistema Internacional para máxima compatibilidade com literatura científica e dados reais.

### Constantes Físicas

| Constante | Valor | Unidade |
|-----------|-------|---------|
| G | 6.67430×10⁻¹¹ | m³/(kg·s²) |
| c | 2.99792458×10⁸ | m/s |
| 1 AU | 1.495978707×10¹¹ | m |
| 1 M☉ | 1.98892×10³⁰ | kg |
| 1 yr | 3.15576×10⁷ | s (Julian year) |

### Fatores de Conversão

```
1 M⊕ = 5.9722×10²⁴ kg
1 M☽ = 7.342×10²² kg
1 R☉ = 6.957×10⁸ m
1 R⊕ = 6.371×10⁶ m
1 R☽ = 1.737×10⁶ m
1 AU = 1.495978707×10¹¹ m
```

### Implementação em C++

```cpp
namespace Units {
    constexpr double G       = 6.67430e-11;    // m³/(kg·s²)
    constexpr double C       = 2.99792458e8;    // m/s
    constexpr double AU      = 1.495978707e11;  // m
    constexpr double M_SUN   = 1.98892e30;      // kg
    constexpr double M_EARTH = 5.9722e24;       // kg
    constexpr double M_MOON  = 7.342e22;        // kg
    constexpr double R_SUN   = 6.957e8;         // m
    constexpr double R_EARTH = 6.371e6;         // m
    constexpr double R_MOON  = 1.737e6;         // m
    constexpr double YEAR    = 3.15576e7;        // s
}
```

---

## 3. Integradores Numéricos

**Decisão: 3 integradores (toggle no builder) — IAS15, Yoshida 4ª ordem, RK4**

O builder permite escolher qual integrador usar para cada simulação.

### 3.1 IAS15 (Integração Adaptativa de 15ª Ordem)

- **Ordem**: 15ª ordem (erro local ~dt¹⁵)
- **Avaliações/step**: ~12
- **Simplesctico**: Não
- **Adaptativo**: Sim (ajusta dt automaticamente)
- **Uso**: Precisão extrema, encontros próximos, validação

```cpp
struct IAS15State {
    // Coeficientes Butcher tableau (simplificado)
    static constexpr int NSTAGE = 12;
    double g[NSTAGE];       // incrementos
    double b[NSTAGE];       // pesos finais
    double c[NSTAGE];       // nós
    double q[NSTAGE][NSTAGE]; // matriz de diferenças divididas
};
```

### 3.2 Yoshida 4ª Ordem

- **Ordem**: 4ª ordem (erro global ~dt⁴)
- **Avaliações/step**: 3
- **Simplesctico**: Sim
- **Adaptativo**: Não (dt fixo)
- **Uso**: Simulação principal, órbitas longas

```cpp
constexpr double W1 = -1.4566124127490405;  // peso sub-step 1 e 3
constexpr double W0 =  1.9131248254980810;  // peso sub-step 2
```

### 3.3 RK4 Clássico

- **Ordem**: 4ª ordem (erro global ~dt⁴)
- **Avaliações/step**: 4
- **Simplesctico**: Não
- **Adaptativo**: Não (dt fixo)
- **Uso**: Validação, comparação, simulações curtas

### Estrutura Comum de Integração

```cpp
enum class IntegratorType { IAS15, YOSHIDA4, RK4 };

struct IntegratorConfig {
    IntegratorType type;
    double dt;           // passo de tempo (s)
    double dt_min;       // limite inferior (para IAS15)
    double dt_max;       // limite superior
    double tolerance;    // tolerância de erro (para IAS15)
};
```

### Seleção no Builder

O JSON do preset inclui o campo `"integrator"`:

```json
{
    "integrator": "yoshida4",
    "dt": 0.001,
    "dt_min": 1e-10,
    "dt_max": 0.01,
    "tolerance": 1e-10
}
```

Valores aceitos: `"ias15"`, `"yoshida4"`, `"rk4"`.

---

## 4. Equações de Movimento

### Força Gravitacional Newtoniana

Para N corpos, a aceleração do corpo i é:

```
a_i = G · Σ_{j≠i} m_j · (r_j - r_i) / |r_j - r_i|³
```

Implementação com Newton's 3rd Law (cada par uma vez):

```cpp
void compute_forces(Body* bodies, int n) {
    for (int i = 0; i < n; i++) {
        bodies[i].ax = 0.0;
        bodies[i].ay = 0.0;
        bodies[i].az = 0.0;
    }

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;

            double r2 = dx*dx + dy*dy + dz*dz + EPS2;
            double r  = sqrt(r2);
            double r3 = r2 * r;
            double f  = Units::G / r3;

            bodies[i].ax += f * bodies[j].m * dx;
            bodies[i].ay += f * bodies[j].m * dy;
            bodies[i].az += f * bodies[j].m * dz;

            bodies[j].ax -= f * bodies[i].m * dx;
            bodies[j].ay -= f * bodies[i].m * dy;
            bodies[j].az -= f * bodies[i].m * dz;
        }
    }
}
```

### Centro de Massa

```cpp
void move_to_com(Body* bodies, int n) {
    double total_m = 0.0;
    double com[3] = {0, 0, 0};
    double com_v[3] = {0, 0, 0};

    for (int i = 0; i < n; i++) {
        total_m += bodies[i].m;
        com[0] += bodies[i].m * bodies[i].x;
        com[1] += bodies[i].m * bodies[i].y;
        com[2] += bodies[i].m * bodies[i].z;
        com_v[0] += bodies[i].m * bodies[i].vx;
        com_v[1] += bodies[i].m * bodies[i].vy;
        com_v[2] += bodies[i].m * bodies[i].vz;
    }

    for (int i = 0; i < n; i++) {
        bodies[i].x  -= com[0] / total_m;
        bodies[i].y  -= com[1] / total_m;
        bodies[i].z  -= com[2] / total_m;
        bodies[i].vx -= com_v[0] / total_m;
        bodies[i].vy -= com_v[1] / total_m;
        bodies[i].vz -= com_v[2] / total_m;
    }
}
```

---

## 5. Correção Relativística (Toggle)

**Decisão: Toggle no builder com escolha de ordem PN**

O builder permite ligar/desligar correções pós-newtonianas e escolher a ordem.

### Estrutura

```json
{
    "relativistic": true,
    "pn_order": 1,
    "speed_of_light": 299792458.0
}
```

Valores de `pn_order`: 0 (off), 1 (1PN), 2 (2PN), 2.5 (2.5PN).

### Implementação

```cpp
struct RelativisticConfig {
    bool enabled;
    int order;          // 0, 1, 2, ou 2.5
    double c;           // velocidade da luz (m/s)
};

void compute_forces_with_pn(Body* bodies, int n, const RelativisticConfig& cfg) {
    // Forças newtonianas
    compute_forces(bodies, n);

    if (!cfg.enabled || cfg.order == 0) return;

    double c2 = cfg.c * cfg.c;

    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            add_pn_correction(bodies[i], bodies[j], c2, cfg.order);
        }
    }
}
```

### Termos 1PN (Einstein-Infeld-Hoffmann)

A correção 1PN na aceleração devido a um par:

```
a_1PN = (G·mⱼ/r²) · n̂ · { (4Gm/r - v² - 2vⱼ² + 4vᵢ·vⱼ) - (3/2)(n̂·vⱼ)² }
```

### Termos 2.5PN (Radiação Gravitacional)

```
a_2.5PN = (8/5) · G²·mᵢ·mⱼ/(r³·c⁵) · { [(11/3)v² - (3/3)(n̂·v)²]·ṙ·n̂ + ... }
```

---

## 6. Passo de Tempo Ahmad-Cohen

**Decisão: Ahmad-Cohen com passos individuais por corpo**

### Princípio

Cada corpo tem dois passos de tempo:
- **dt_irregular**: curto, para a força de vizinhos próximos (muda rapidamente)
- **dt_regular**: longo, para a força de corpos distantes (muda lentamente)

### Implementação

```cpp
struct Particle {
    // Estado
    double x, y, z;
    double vx, vy, vz;
    double ax, ay, az;

    // Ahmad-Cohen
    double irregular_force[3];   // força de vizinhos
    double regular_force[3];     // força distante
    double dt_irregular;
    double dt_regular;
    double t_next_irregular;
    double t_next_regular;
    double t_current;
    double neighbor_radius;      // raio da esfera de vizinhos
};

void ahmad_cohen_step(Simulation& sim) {
    double t_global = sim.time;

    while (t_global < sim.time + sim.dt) {
        // Encontrar corpo com menor t_next
        int idx = find_next_particle(sim);

        Particle& p = sim.particles[idx];

        if (p.t_next_irregular <= p.t_next_regular) {
            // Passo irregular: atualizar só vizinhos
            double dt = p.dt_irregular;
            advance_particle(p, dt);

            // Recalcular força irregular
            compute_irregular_force(sim, idx);
            p.t_next_irregular += dt;
        } else {
            // Passo regular: atualizar todos
            double dt = p.dt_regular;
            advance_particle(p, dt);

            // Recalcular força regular
            compute_regular_force(sim, idx);
            p.t_next_regular += dt;
        }

        t_global = min_t_next(sim);
    }
}
```

### Critérios de Passo

```cpp
void update_timestep(Particle& p, const Simulation& sim) {
    // Irregular: baseado na distância ao vizinho mais próximo
    double min_r = find_min_distance(p, sim);
    p.dt_irregular = ETA_I * sqrt(min_r / max_acceleration(p));

    // Regular: baseado na força total
    double a_total = magnitude(p.regular_force);
    p.dt_regular = ETA_R * sqrt(p.neighbor_radius / a_total);
}
```

---

## 7. Softening e Colisões

### Softening

**Decisão: Plummer**

```
F = G · m₁ · m₂ · r / (r² + ε²)^(3/2)
```

```cpp
constexpr double EPS2 = 1e-6;  // softening² (m²)

double dx = bj.x - bi.x;
double dy = bj.y - bi.y;
double dz = bj.z - bi.z;
double r2 = dx*dx + dy*dy + dz*dz + EPS2;
```

### Colisões

**Decisão: Configurável no builder (off, merge, elástico)**

```cpp
enum class CollisionMode { NONE, MERGE, ELASTIC };

struct CollisionConfig {
    CollisionMode mode;
    double radius_factor;  // fator multiplicativo do raio para detecção
};

void handle_collision(Body& a, Body& b, const CollisionConfig& cfg) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double dz = b.z - a.z;
    double r = sqrt(dx*dx + dy*dy + dz*dz);
    double r_contact = (a.radius + b.radius) * cfg.radius_factor;

    if (r >= r_contact) return;

    switch (cfg.mode) {
        case CollisionMode::NONE:
            break;

        case CollisionMode::MERGE: {
            double total_m = a.m + b.m;
            a.vx = (a.m * a.vx + b.m * b.vx) / total_m;
            a.vy = (a.m * a.vy + b.m * b.vy) / total_m;
            a.vz = (a.m * a.vz + b.m * b.vz) / total_m;
            a.m = total_m;
            a.radius = cbrt(pow(a.radius, 3) + pow(b.radius, 3));
            // Marcar b como removido
            b.m = 0;
            break;
        }

        case CollisionMode::ELASTIC: {
            // Colisão elástica esfera-esfera
            double nx = dx / r, ny = dy / r, nz = dz / r;
            double dvx = b.vx - a.vx;
            double dvy = b.vy - a.vy;
            double dvz = b.vz - a.vz;
            double dvn = dvx*nx + dvy*ny + dvz*nz;

            if (dvn > 0) return; // afastando-se

            double impulse = 2.0 * dvn / (a.m + b.m);
            a.vx += impulse * b.m * nx;
            a.vy += impulse * b.m * ny;
            a.vz += impulse * b.m * nz;
            b.vx -= impulse * a.m * nx;
            b.vy -= impulse * a.m * ny;
            b.vz -= impulse * a.m * nz;
            break;
        }
    }
}
```

---

## 8. Cálculo de Forças (Toggle)

**Decisão: Múltiplos métodos (toggle no builder)**

### 8.1 Direto O(N²)

```cpp
void compute_forces_direct(Body* bodies, int n) {
    // Newton's 3rd law: cada par uma vez
    for (int i = 0; i < n; i++) {
        bodies[i].ax = 0;
        bodies[i].ay = 0;
        bodies[i].az = 0;
    }
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            // ... cálculo de força ...
        }
    }
}
```

### 8.2 Barnes-Hut O(N log N)

Estrutura de árvore octal:

```cpp
struct OctreeNode {
    double center[3];
    double size;
    double total_mass;
    double com[3];         // centro de massa
    int particle_index;    // -1 se nó interno
    int children[8];       // índices dos filhos (-1 se vazio)
};

void compute_forces_barnes_hut(Body* bodies, int n, OctreeNode* tree, double theta) {
    for (int i = 0; i < n; i++) {
        bodies[i].ax = 0;
        bodies[i].ay = 0;
        bodies[i].az = 0;
        traverse_tree(bodies, i, tree, 0, theta);
    }
}

void traverse_tree(Body* bodies, int idx, OctreeNode* tree, int node, double theta) {
    OctreeNode& n = tree[node];

    if (n.particle_index >= 0) {
        // Folha: calcular força diretamente (exceto self)
        if (n.particle_index != idx)
            add_force(bodies[idx], bodies[n.particle_index]);
        return;
    }

    // Critério de abertura
    double d = distance(bodies[idx].pos, n.com);
    if (n.size / d < theta) {
        // Tratar como corpo único
        add_force_com(bodies[idx], n.total_mass, n.com);
    } else {
        // Recursar nos filhos
        for (int c = 0; c < 8; c++)
            if (n.children[c] >= 0)
                traverse_tree(bodies, idx, tree, n.children[c], theta);
    }
}
```

### 8.3 OpenMP Paralelo

```cpp
void compute_forces_parallel(Body* bodies, int n) {
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < n; i++) {
        bodies[i].ax = 0;
        bodies[i].ay = 0;
        bodies[i].az = 0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            // ... cálculo de força (sem Newton's 3rd law) ...
        }
    }
}
```

### Seleção no Builder

```json
{
    "force_method": "direct",
    "barnes_hut_theta": 0.5,
    "openmp_threads": 4
}
```

Valores de `force_method`: `"direct"`, `"barnes_hut"`, `"openmp"`.

---

## 9. Estrutura do Código C++

### Diretórios

```
nbody-sim/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── Body.hpp / Body.cpp
│   ├── Physics.hpp / Physics.cpp
│   ├── Integrators.hpp / Integrators.cpp
│   ├── IAS15.hpp / IAS15.cpp
│   ├── AhmadCohen.hpp / AhmadCohen.cpp
│   ├── Forces.hpp / Forces.cpp
│   ├── BarnesHut.hpp / BarnesHut.cpp
│   ├── Collisions.hpp / Collisions.cpp
│   ├── PN.hpp / PN.cpp
│   ├── IO.hpp / IO.cpp
│   ├── BinaryIO.hpp / BinaryIO.cpp
│   ├── Diagnostics.hpp / Diagnostics.cpp
│   ├── Renderer.hpp / Renderer.cpp
│   └── Constants.hpp
├── presets/
├── output/
└── tests/
```

### Struct de Corpo

```cpp
struct Body {
    std::string name;
    std::string type;    // "star", "planet", "moon"
    double m;            // massa (kg)
    double radius;       // raio (m)
    double x, y, z;      // posição (m)
    double vx, vy, vz;   // velocidade (m/s)
    double ax, ay, az;    // aceleração (m/s²)
    double color[3];     // RGB [0,1]
    bool alive;          // para remoção por colisão
};
```

### Estrutura de Simulação

```cpp
struct Simulation {
    std::vector<Body> bodies;
    int n_bodies;
    double time;         // tempo atual (s)
    double dt;           // passo de tempo base (s)

    // Configurações (do JSON)
    IntegratorType integrator;
    RelativisticConfig relativistic;
    CollisionConfig collision;
    ForceMethod force_method;

    // Para Ahmad-Cohen
    std::vector<Particle> particles;

    // Para Barnes-Hut
    std::vector<OctreeNode> octree;
    double bh_theta;
};
```

---

## 10. Saída Binária e Leitor

### Formato Binário

```cpp
// Cabeçalho
struct FileHeader {
    char magic[4];        // "NBOD"
    uint32_t version;     // 1
    uint32_t n_bodies;
    double time;
    double dt;
    IntegratorType integrator;
    bool relativistic;
    int pn_order;
    CollisionMode collision;
    ForceMethod force_method;
};

// Frame de saída
struct OutputFrame {
    double t;
    struct {
        double x, y, z;
        double vx, vy, vz;
        double m;
        double radius;
        double color[3];
        bool alive;
    } bodies[];           // flexível array
};

// Arquivo completo:
// [FileHeader]
// [OutputFrame t=0]
// [OutputFrame t=dt*N]
// ...
```

### Escrita

```cpp
void write_frame(FILE* f, const Simulation& sim) {
    OutputFrame frame;
    frame.t = sim.time;

    fwrite(&frame, sizeof(OutputFrame) - sizeof(frame.bodies[0]), 1, f);

    for (int i = 0; i < sim.n_bodies; i++) {
        struct {
            double x, y, z;
            double vx, vy, vz;
            double m, radius;
            double color[3];
            bool alive;
        } b = {
            sim.bodies[i].x, sim.bodies[i].y, sim.bodies[i].z,
            sim.bodies[i].vx, sim.bodies[i].vy, sim.bodies[i].vz,
            sim.bodies[i].m, sim.bodies[i].radius,
            sim.bodies[i].color[0], sim.bodies[i].color[1], sim.bodies[i].color[2],
            sim.bodies[i].alive
        };
        fwrite(&b, sizeof(b), 1, f);
    }
}
```

### Leitor (C++)

```cpp
struct SimulationFile {
    FileHeader header;
    std::vector<OutputFrame> frames;

    static SimulationFile load(const std::string& path) {
        SimulationFile sf;
        FILE* f = fopen(path.crb, "rb");

        fread(&sf.header, sizeof(FileHeader), 1, f);

        while (!feof(f)) {
            OutputFrame frame;
            if (fread(&frame.t, sizeof(double), 1, f) != 1) break;

            frame.bodies.resize(sf.header.n_bodies);
            for (int i = 0; i < sf.header.n_bodies; i++) {
                fread(&frame.bodies[i], sizeof(frame.bodies[0]), 1, f);
            }
            sf.frames.push_back(std::move(frame));
        }

        fclose(f);
        return sf;
    }
};
```

### Leitor (Python)

```python
import struct
import numpy as np

def load_nbody_binary(path):
    with open(path, 'rb') as f:
        # Header
        magic = f.read(4)
        assert magic == b'NBOD'
        version, n_bodies = struct.unpack('II', f.read(8))
        time, dt = struct.unpack('dd', f.read(16))
        # ... ler mais campos do header ...

        frames = []
        while True:
            t_bytes = f.read(8)
            if len(t_bytes) < 8:
                break
            t = struct.unpack('d', t_bytes)[0]

            bodies = []
            for _ in range(n_bodies):
                data = struct.unpack('13d ?', f.read(13*8 + 1))
                bodies.append({
                    'x': data[0], 'y': data[1], 'z': data[2],
                    'vx': data[3], 'vy': data[4], 'vz': data[5],
                    'm': data[6], 'radius': data[7],
                    'color': (data[8], data[9], data[10]),
                    'alive': data[12]
                })
            frames.append({'t': t, 'bodies': bodies})

    return {'header': {...}, 'frames': frames}
```

---

## 11. Visualização OpenGL

**Decisão: C++ com OpenGL**

### Dependências

- **GLFW**: janela e inputs
- **GLAD**: loader de OpenGL
- **GLM**: matemática (vetores, matrizes)
- **ImGui**: interface de debug

### Estrutura Básica

```cpp
class Renderer {
public:
    bool init(int width, int height, const char* title);
    void render(const Simulation& sim, const Camera& camera);
    void cleanup();

private:
    GLuint shader_program;
    GLuint VAO, VBO;
    std::vector<float> trail_data;
    int trail_max_points;

    void draw_bodies(const std::vector<Body>& bodies);
    void draw_trails();
    void draw_grid();
};
```

### Loop de Renderização

```cpp
void Renderer::render(const Simulation& sim, const Camera& camera) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Matriz de view/projection
    glm::mat4 view = camera.get_view_matrix();
    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f), 800.0f/600.0f, 0.001f, 1e20f);

    glUseProgram(shader_program);
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "view"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shader_program, "projection"),
                       1, GL_FALSE, glm::value_ptr(projection));

    // Desenhar corpos
    draw_bodies(sim.bodies);

    // Desenhar trails
    draw_trails();

    // Interface ImGui
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Simulation Controls");
    ImGui::Text("Time: %.6f s", sim.time);
    ImGui::Text("Bodies: %d", sim.n_bodies);

    // Seletor de integrador
    const char* integrators[] = {"IAS15", "Yoshida4", "RK4"};
    static int current_integrator = 1;
    if (ImGui::Combo("Integrator", &current_integrator, integrators, 3)) {
        // Mudar integrador
    }

    // Toggle relativístico
    static bool relativistic = false;
    if (ImGui::Checkbox("Relativistic", &relativistic)) {
        // Ativar/desativar PN
    }

    // Toggle método de força
    const char* force_methods[] = {"Direct", "Barnes-Hut", "OpenMP"};
    static int current_force = 0;
    if (ImGui::Combo("Force Method", &current_force, force_methods, 3)) {
        // Mudar método
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
```

### Shaders

**Vertex Shader**:
```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 vertexColor;

void main() {
    vertexColor = aColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    gl_PointSize = 10.0;
}
```

**Fragment Shader**:
```glsl
#version 330 core
in vec3 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vertexColor, 1.0);
}
```

### Câmera

```cpp
class Camera {
public:
    glm::vec3 position;
    float yaw, pitch;
    float speed;
    float sensitivity;

    glm::mat4 get_view_matrix() {
        return glm::lookAt(position,
                           position + get_front(),
                           glm::vec3(0, 1, 0));
    }

    void process_keyboard(GLFWwindow* window, float dt) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            position += get_front() * speed * dt;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            position -= get_front() * speed * dt;
        // ... WASD + mouse ...
    }
};
```

---

## 12. Arquitetura Geral

### Diagrama de Dependências

```
main.cpp
    ├── IO.cpp           (ler JSON, escrever binário)
    ├── Physics.cpp      (forças newtonianas)
    ├── PN.cpp           (correções pós-newtonianas)
    ├── Forces.cpp       (dispatcher: direto/BH/OpenMP)
    ├── BarnesHut.cpp    (árvore octal)
    ├── Integrators.cpp  (dispatcher: IAS15/Yoshida4/RK4)
    ├── IAS15.cpp        (integador adaptativo)
    ├── AhmadCohen.cpp   (passos individuais)
    ├── Collisions.cpp   (detecção e resolução)
    ├── Diagnostics.cpp  (energia, momento)
    ├── BinaryIO.cpp     (leitura/escrita binária)
    ├── Renderer.cpp     (OpenGL + ImGui)
    └── Constants.hpp    (valores físicos)
```

### Fluxo de Execução

```
1. Ler preset JSON (nlohmann/json)
2. Inicializar corpos e partículas
3. Mover para centro de massa
4. Avaliar forças iniciais
5. Inicializar OpenGL/ImGui
6. Loop principal:
   a. Processar input (teclado/mouse)
   b. Passo de integração (IAS15/Yoshida4/RK4)
   c. Se Ahmad-Cohen: passos individuais
   d. Se collisions: detectar e resolver
   e. Calcular diagnósticos
   f. Salvar frame binário (a cada N passos)
   g. Renderizar (OpenGL)
   h. Trocar buffers
7. Salvar resumo final
8. Cleanup
```

### Compilação

```cmake
cmake_minimum_required(VERSION 3.16)
project(nbody-sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json
include(FetchContent)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3)
FetchContent_MakeAvailable(json)

# GLFW
find_package(glfw3 REQUIRED)

# GLAD (gerar ou baixar)
add_library(glad src/glad.c)
target_include_directories(glad PUBLIC include/)

# GLM
find_package(glm REQUIRED)

# ImGui
add_library(imgui
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui/)

# Executável
add_executable(nbody
    src/main.cpp
    src/Body.cpp
    src/Physics.cpp
    src/PN.cpp
    src/Forces.cpp
    src/BarnesHut.cpp
    src/Integrators.cpp
    src/IAS15.cpp
    src/AhmadCohen.cpp
    src/Collisions.cpp
    src/Diagnostics.cpp
    src/BinaryIO.cpp
    src/Renderer.cpp
    src/IO.cpp
)

target_link_libraries(nbody PRIVATE
    nlohmann_json::nlohmann_json
    glfw glad glm imgui
    ${CMAKE_DL_LIBS}
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nbody PRIVATE -O3 -march=native -flto)
endif()
```

---

## 13. Referências

### Artigos Fundamentais

1. **Chenciner & Montgomery (2000)** — "A Remarkable Periodic Solution of the Three-Body Problem in the Case of Equal Masses" — Annals of Mathematics, 152(3), 881-901.

2. **Moore (1993)** — "Braids in Classical Dynamics" — Physical Review Letters, 70(24), 3675-3679.

3. **Šuvakov & Dmitrašinović (2013)** — "Three Classes of Newtonian Three-Body Planar Periodic Orbits" — Phys. Rev. Lett. 110, 114301.

4. **Yoshida (1990)** — "Construction of Higher Order Symplectic Integrators" — Physics Letters A, 150, 262.

5. **Ahmad & Cohen (1973)** — "A Numerical Integration Scheme for the N-Body Gravitational Problem" — J. Comp. Phys. 12, 389.

6. **Rein & Spiegel (2015)** — "IAS15: a 15th-order integrator for orbital dynamics" — MNRAS 446, 1424.

### Bibliotecas de Referência

7. **REBOUND** — https://github.com/hannorein/rebound

8. **SpaceHub** — MNRAS 505, 1053 (2021)

9. **KETJU** — ApJ 887, 71 (2019)

### Catálogo de Validação

10. **Peter (2025)** — "Three-Body Trust Catalog v1.0" — DOI: 10.5281/zenodo.17635887

---

## Resumo das Decisões

| Aspecto | Decisão |
|---------|---------|
| Unidades | SI (kg, m, s) |
| Integradores | IAS15 + Yoshida4 + RK4 (toggle) |
| Correção relativística | Toggle no builder (0, 1, 2, 2.5PN) |
| Passo de tempo | Ahmad-Cohen individual |
| Softening | Plummer |
| Colisões | Configurável (off, merge, elástico) |
| Cálculo de forças | Toggle (Direto, Barnes-Hut, OpenMP) |
| Saída de dados | Binário com leitor |
| Visualização | C++ OpenGL + ImGui |
| JSON | nlohmann/json |
| Build | CMake 3.16+ |
| C++ | C++17 |
