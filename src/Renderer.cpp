#include "Renderer.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>

static GLFWwindow* g_window = nullptr;
static GLuint g_shader = 0;
static GLuint g_grid_shader = 0;
static GLuint g_vao = 0, g_vbo = 0;
static GLuint g_grid_vao = 0, g_grid_vbo = 0;
static int g_grid_count = 0;
static float g_lastX = 640, g_lastY = 360;
static bool g_firstMouse = true;
static bool g_rightMouseDown = false;
static Renderer* g_renderer_instance = nullptr;

static const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in float aSize;
uniform mat4 view;
uniform mat4 projection;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = projection * view * vec4(aPos, 1.0);
    gl_PointSize = aSize;
}
)";

static const char* fragmentShaderSrc = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    vec2 coord = gl_PointCoord - vec2(0.5);
    float r = length(coord);
    if (r > 0.5) discard;
    float alpha = smoothstep(0.5, 0.3, r);
    FragColor = vec4(vColor, alpha);
}
)";

static const char* gridVertSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
uniform mat4 view;
uniform mat4 projection;
out vec3 vColor;
out float vDepth;
void main() {
    vColor = aColor;
    vec4 clip = projection * view * vec4(aPos, 1.0);
    vDepth = clip.w;
    gl_Position = clip;
}
)";

static const char* gridFragSrc = R"(
#version 330 core
in vec3 vColor;
in float vDepth;
out vec4 FragColor;
void main() {
    float fade = clamp(1.0 - (vDepth - 1.0) / 50.0, 0.05, 0.4);
    FragColor = vec4(vColor, fade);
}
)";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::cerr << "Shader compile error: " << log << "\n";
    }
    return s;
}

static GLuint create_program(const char* vsrc, const char* fsrc) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fsrc);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    int ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, 512, nullptr, log);
        std::cerr << "Program link error: " << log << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// --- Camera math ---

float Camera::x() const {
    return focus_x + distance * cosf(glm::radians(pitch)) * cosf(glm::radians(yaw));
}
float Camera::y() const {
    return focus_y + distance * sinf(glm::radians(pitch));
}
float Camera::z() const {
    return focus_z + distance * cosf(glm::radians(pitch)) * sinf(glm::radians(yaw));
}

// --- Callbacks ---

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        g_rightMouseDown = (action == GLFW_PRESS);
        if (g_rightMouseDown) {
            g_firstMouse = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
    if (!g_renderer_instance || !g_rightMouseDown) return;
    float fx = (float)xpos, fy = (float)ypos;
    if (g_firstMouse) {
        g_lastX = fx;
        g_lastY = fy;
        g_firstMouse = false;
    }
    float dx = fx - g_lastX;
    float dy = g_lastY - fy;
    g_lastX = fx;
    g_lastY = fy;

    Camera& cam = g_renderer_instance->camera;
    cam.yaw += dx * cam.sensitivity;
    cam.pitch += dy * cam.sensitivity;
    if (cam.pitch > 89.0f) cam.pitch = 89.0f;
    if (cam.pitch < -89.0f) cam.pitch = -89.0f;
}

static void scroll_callback(GLFWwindow* window, double xoff, double yoff) {
    ImGui_ImplGlfw_ScrollCallback(window, xoff, yoff);
    if (!g_renderer_instance) return;
    Camera& cam = g_renderer_instance->camera;
    cam.distance -= (float)yoff * cam.distance * 0.15f;
    if (cam.distance < 0.01f) cam.distance = 0.01f;
    if (cam.distance > 10000.0f) cam.distance = 10000.0f;
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
}

static void char_callback(GLFWwindow* window, unsigned int c) {
    ImGui_ImplGlfw_CharCallback(window, c);
}

// --- Init ---

bool Renderer::init(int w, int h, const char* title) {
    g_renderer_instance = this;
    window_w = w;
    window_h = h;

    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    g_window = glfwCreateWindow(w, h, title, nullptr, nullptr);
    if (!g_window) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(g_window);
    glfwSetCursorPosCallback(g_window, mouse_callback);
    glfwSetScrollCallback(g_window, scroll_callback);
    glfwSetMouseButtonCallback(g_window, mouse_button_callback);
    glfwSetKeyCallback(g_window, key_callback);
    glfwSetCharCallback(g_window, char_callback);
    glfwSetInputMode(g_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(g_window, false);
    ImGui_ImplOpenGL3_Init("#version 330");
    ImGui::StyleColorsDark();

    g_shader = create_program(vertexShaderSrc, fragmentShaderSrc);
    g_grid_shader = create_program(gridVertSrc, gridFragSrc);

    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 7 * 256, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)(sizeof(double) * 3));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)(sizeof(double) * 6));
    glEnableVertexAttribArray(2);

    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenVertexArrays(1, &g_grid_vao);
    glGenBuffers(1, &g_grid_vbo);
    glBindVertexArray(g_grid_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_grid_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 100000, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 6, (void*)(sizeof(float) * 3));
    glEnableVertexAttribArray(1);

    return true;
}

// --- Input ---

void Renderer::process_input(const Simulation& sim) {
    if (!g_window) return;
    glfwPollEvents();

    static bool h_was_pressed = false;
    static bool c_was_pressed = false;
    static bool g_was_pressed = false;
    static bool f_was_pressed = false;
    static bool plus_was_pressed = false;
    static bool minus_was_pressed = false;
    bool h_pressed = glfwGetKey(g_window, GLFW_KEY_H) == GLFW_PRESS;
    bool c_pressed = glfwGetKey(g_window, GLFW_KEY_C) == GLFW_PRESS;
    bool g_pressed = glfwGetKey(g_window, GLFW_KEY_G) == GLFW_PRESS;
    bool f_pressed = glfwGetKey(g_window, GLFW_KEY_F) == GLFW_PRESS;
    bool plus_pressed = glfwGetKey(g_window, GLFW_KEY_EQUAL) == GLFW_PRESS;
    bool minus_pressed = glfwGetKey(g_window, GLFW_KEY_MINUS) == GLFW_PRESS;
    if (h_pressed && !h_was_pressed) shortcuts_collapsed = !shortcuts_collapsed;
    if (c_pressed && !c_was_pressed) show_camera_config = !show_camera_config;
    if (g_pressed && !g_was_pressed) grid_mode = (grid_mode + 1) % 4;
    if (f_pressed && !f_was_pressed) {
        std::vector<int> alive_indices;
        alive_indices.reserve(sim.bodies.size());
        for (int i = 0; i < (int)sim.bodies.size(); ++i) {
            if (sim.bodies[i].alive) alive_indices.push_back(i);
        }

        if (!alive_indices.empty()) {
            if (!follow_body) {
                follow_free_focus_x = camera.focus_x;
                follow_free_focus_y = camera.focus_y;
                follow_free_focus_z = camera.focus_z;
                follow_body = true;
                follow_body_index = alive_indices[0];
            } else {
                bool found = false;
                for (size_t i = 0; i < alive_indices.size(); ++i) {
                    if (alive_indices[i] == follow_body_index) {
                        if (i + 1 < alive_indices.size()) {
                            follow_body_index = alive_indices[i + 1];
                        } else {
                            follow_body = false;
                            follow_body_index = -1;
                            camera.focus_x = follow_free_focus_x;
                            camera.focus_y = follow_free_focus_y;
                            camera.focus_z = follow_free_focus_z;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    follow_body = true;
                    follow_body_index = alive_indices[0];
                }
            }
        }
    }
    if (plus_pressed && !plus_was_pressed)  speed_mult = std::min(speed_mult * 2.0f, 128.0f);
    if (minus_pressed && !minus_was_pressed) speed_mult = std::max(speed_mult / 2.0f, 0.015625f);
    h_was_pressed = h_pressed;
    c_was_pressed = c_pressed;
    g_was_pressed = g_pressed;
    f_was_pressed = f_pressed;
    plus_was_pressed = plus_pressed;
    minus_was_pressed = minus_pressed;

    if (glfwGetKey(g_window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(g_window, true);

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return;

    Camera& cam = camera;
    float dt = 0.016f;

    float spd = cam.speed * dt;
    if (glfwGetKey(g_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) spd *= 3.0f;

    if (follow_body && follow_body_index >= 0 && follow_body_index < (int)sim.bodies.size() && sim.bodies[follow_body_index].alive) {
        const Body& target = sim.bodies[follow_body_index];
        cam.focus_x = target.x;
        cam.focus_y = target.y;
        cam.focus_z = target.z;
    } else {
        // WASD moves the focus point on the XZ plane
        glm::vec3 front;
        front.x = cosf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        front.y = sinf(glm::radians(cam.pitch));
        front.z = sinf(glm::radians(cam.yaw)) * cosf(glm::radians(cam.pitch));
        front = glm::normalize(front);
        glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0, 1, 0)));
        glm::vec3 flatFront = glm::normalize(glm::vec3(front.x, 0, front.z));

        if (glfwGetKey(g_window, GLFW_KEY_W) == GLFW_PRESS) {
            cam.focus_x += flatFront.x * spd;
            cam.focus_z += flatFront.z * spd;
        }
        if (glfwGetKey(g_window, GLFW_KEY_S) == GLFW_PRESS) {
            cam.focus_x -= flatFront.x * spd;
            cam.focus_z -= flatFront.z * spd;
        }
        if (glfwGetKey(g_window, GLFW_KEY_A) == GLFW_PRESS) {
            cam.focus_x -= right.x * spd;
            cam.focus_z -= right.z * spd;
        }
        if (glfwGetKey(g_window, GLFW_KEY_D) == GLFW_PRESS) {
            cam.focus_x += right.x * spd;
            cam.focus_z += right.z * spd;
        }
        if (glfwGetKey(g_window, GLFW_KEY_Q) == GLFW_PRESS) cam.focus_y -= spd;
        if (glfwGetKey(g_window, GLFW_KEY_E) == GLFW_PRESS) cam.focus_y += spd;
    }
}

// --- Render ---

void Renderer::render(const Simulation& sim) {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Camera& cam = camera;
    glm::vec3 camPos(cam.x(), cam.y(), cam.z());
    glm::vec3 focus(cam.focus_x, cam.focus_y, cam.focus_z);
    glm::mat4 view = glm::lookAt(camPos, focus, glm::vec3(0, 1, 0));
    float aspect = (float)window_w / (float)window_h;
    glm::mat4 projection = glm::perspective(glm::radians(cam.fov), aspect, 0.001f, 10000.0f);

    // Draw grid
    if (grid_mode != GRID_NONE) {
        glUseProgram(g_grid_shader);
        glUniformMatrix4fv(glGetUniformLocation(g_grid_shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(g_grid_shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glBindVertexArray(g_grid_vao);

        const float grid_radius = 60.0f;
        float grid_step = 2.0f;

        bool has_body = false;
        float min_x = 0.0f, max_x = 0.0f;
        float min_z = 0.0f, max_z = 0.0f;
        for (const auto& b : sim.bodies) {
            if (!b.alive) continue;
            if (!has_body) {
                min_x = max_x = (float)b.x;
                min_z = max_z = (float)b.z;
                has_body = true;
            } else {
                min_x = std::min(min_x, (float)b.x);
                max_x = std::max(max_x, (float)b.x);
                min_z = std::min(min_z, (float)b.z);
                max_z = std::max(max_z, (float)b.z);
            }
        }

        if (has_body) {
            float span_x = std::max(1.0f, max_x - min_x);
            float span_z = std::max(1.0f, max_z - min_z);
            float span = std::max(span_x, span_z);
            if (span > 20.0f) {
                int target_lines = 31 + (int)std::ceil((span - 20.0f) / 5.0f);
                target_lines = std::min(121, target_lines);
                grid_step = std::max(0.5f, (2.0f * grid_radius) / (float)(target_lines - 1));
            }
        }

        std::vector<float> gv;

        if (grid_mode == GRID_FLAT) {
            for (float i = -grid_radius; i <= grid_radius; i += grid_step) {
                gv.insert(gv.end(), {i, 0, -grid_radius, 0.25f, 0.25f, 0.4f});
                gv.insert(gv.end(), {i, 0,  grid_radius, 0.25f, 0.25f, 0.4f});
                gv.insert(gv.end(), {-grid_radius, 0, i, 0.25f, 0.25f, 0.4f});
                gv.insert(gv.end(), { grid_radius, 0, i, 0.25f, 0.25f, 0.4f});
            }
        } else if (grid_mode == GRID_RELATIVITY_2D) {
            float warp_strength = 0.15f;
            int res = 80;
            float span = grid_radius;
            float step = (2.0f * span) / res;
            for (int ix = 0; ix <= res; ix++) {
                for (int iz = 0; iz < res; iz++) {
                    float x0 = -span + ix * step;
                    float z0 = -span + iz * step;
                    float z1 = -span + (iz + 1) * step;
                    float y0 = 0, y1 = 0;
                    for (auto& b : sim.bodies) {
                        if (!b.alive) continue;
                        float dx0 = x0 - (float)b.x;
                        float dz0 = z0 - (float)b.z;
                        float r0 = std::sqrt(dx0*dx0 + dz0*dz0) + 0.01f;
                        y0 -= warp_strength * (float)b.m / r0;
                        float dx1 = x0 - (float)b.x;
                        float dz1 = z1 - (float)b.z;
                        float r1 = std::sqrt(dx1*dx1 + dz1*dz1) + 0.01f;
                        y1 -= warp_strength * (float)b.m / r1;
                    }
                    float c0 = 0.15f + 0.35f * std::clamp(-y0 / 0.5f, 0.0f, 1.0f);
                    float c1 = 0.15f + 0.35f * std::clamp(-y1 / 0.5f, 0.0f, 1.0f);
                    gv.insert(gv.end(), {x0, y0, z0, c0, c0, 0.6f + 0.2f * c0});
                    gv.insert(gv.end(), {x0, y1, z1, c1, c1, 0.6f + 0.2f * c1});
                }
            }
            for (int iz = 0; iz <= res; iz++) {
                for (int ix = 0; ix < res; ix++) {
                    float x0 = -span + ix * step;
                    float x1 = -span + (ix + 1) * step;
                    float z  = -span + iz * step;
                    float y0 = 0, y1 = 0;
                    for (auto& b : sim.bodies) {
                        if (!b.alive) continue;
                        float dx0 = x0 - (float)b.x;
                        float dz0 = z - (float)b.z;
                        float r0 = std::sqrt(dx0*dx0 + dz0*dz0) + 0.01f;
                        y0 -= warp_strength * (float)b.m / r0;
                        float dx1 = x1 - (float)b.x;
                        float dz1 = z - (float)b.z;
                        float r1 = std::sqrt(dx1*dx1 + dz1*dz1) + 0.01f;
                        y1 -= warp_strength * (float)b.m / r1;
                    }
                    float c0 = 0.15f + 0.35f * std::clamp(-y0 / 0.5f, 0.0f, 1.0f);
                    float c1 = 0.15f + 0.35f * std::clamp(-y1 / 0.5f, 0.0f, 1.0f);
                    gv.insert(gv.end(), {x0, y0, z, c0, c0, 0.6f + 0.2f * c0});
                    gv.insert(gv.end(), {x1, y1, z, c1, c1, 0.6f + 0.2f * c1});
                }
            }
        } else if (grid_mode == GRID_RELATIVITY_3D) {
            float warp_strength = 0.12f;
            int res = 30;
            float span = grid_radius * 0.6f;
            float step = (2.0f * span) / res;
            auto warp = [&](float wx, float wy, float wz) -> float {
                float d = 0;
                for (auto& b : sim.bodies) {
                    if (!b.alive) continue;
                    float dx = wx - (float)b.x;
                    float dy = wy - (float)b.y;
                    float dz = wz - (float)b.z;
                    float r = std::sqrt(dx*dx + dy*dy + dz*dz) + 0.01f;
                    d -= warp_strength * (float)b.m / r;
                }
                return d;
            };
            for (int i = 0; i <= res; i++) {
                float t = -span + i * step;
                for (int j = 0; j < res; j++) {
                    float a0 = -span + j * step;
                    float a1 = -span + (j + 1) * step;
                    float g0 = 0.2f, g1 = 0.2f;
                    float d0 = warp(t, a0, 0);
                    float d1 = warp(t, a1, 0);
                    gv.insert(gv.end(), {t, a0 + d0, 0, g0, g0 + 0.1f, 0.5f});
                    gv.insert(gv.end(), {t, a1 + d1, 0, g1, g1 + 0.1f, 0.5f});

                    d0 = warp(a0, t, 0);
                    d1 = warp(a1, t, 0);
                    gv.insert(gv.end(), {a0 + d0, t, 0, g0, g0, 0.6f});
                    gv.insert(gv.end(), {a1 + d1, t, 0, g1, g1, 0.6f});

                    d0 = warp(t, 0, a0);
                    d1 = warp(t, 0, a1);
                    gv.insert(gv.end(), {t, d0, a0, 0.5f, g0, g0});
                    gv.insert(gv.end(), {t, d1, a1, 0.5f, g1, g1});
                }
            }
        }

        g_grid_count = (int)(gv.size() / 6);
        if (g_grid_count > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, g_grid_vbo);
            glBufferData(GL_ARRAY_BUFFER, gv.size() * sizeof(float), gv.data(), GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, g_grid_count);
        }
    }

    // Draw bodies as points
    glUseProgram(g_shader);
    glUniformMatrix4fv(glGetUniformLocation(g_shader, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(g_shader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

    std::vector<double> pointData;
    for (auto& b : sim.bodies) {
        if (!b.alive) continue;
        pointData.push_back(b.x);
        pointData.push_back(b.y);
        pointData.push_back(b.z);
        pointData.push_back(b.color[0]);
        pointData.push_back(b.color[1]);
        pointData.push_back(b.color[2]);
        double size = std::max(4.0, std::min(64.0, b.radius * 5000.0));
        pointData.push_back(size);
    }

    if (!pointData.empty()) {
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glBufferData(GL_ARRAY_BUFFER, pointData.size() * sizeof(double),
                     pointData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)(sizeof(double) * 3));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_DOUBLE, GL_FALSE, sizeof(double) * 7, (void*)(sizeof(double) * 6));
        glEnableVertexAttribArray(2);
        glDrawArrays(GL_POINTS, 0, (GLsizei)(pointData.size() / 7));
    }

    // ImGui overlay
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("N-Body Simulation");
    ImGui::Text("Time: %.6f", sim.time);
    ImGui::Text("Bodies: %d", sim.n_bodies);
    ImGui::Text("G = %.4f", sim.gravity);
    ImGui::Text("dt = %.6f", sim.dt);
    ImGui::Text("Integrator: %s", sim.integrator.c_str());
    ImGui::Text("Forces: %s", sim.force_method.c_str());
    ImGui::Separator();
    ImGui::Text("Focus: (%.2f, %.2f, %.2f)", cam.focus_x, cam.focus_y, cam.focus_z);
    if (follow_body && follow_body_index >= 0 && follow_body_index < (int)sim.bodies.size() && sim.bodies[follow_body_index].alive) {
        ImGui::Text("Seguindo: %s", sim.bodies[follow_body_index].name.c_str());
    } else {
        ImGui::Text("Seguindo: livre");
    }
    ImGui::Text("Camera: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
    ImGui::Text("Dist: %.2f", cam.distance);
    ImGui::Separator();

    const char* grid_names[] = {"Nenhum", "Plano", "Relatividade 2D", "Relatividade 3D"};
    ImGui::Text("Grid: %s", grid_names[grid_mode]);
    ImGui::SameLine();
    if (ImGui::SmallButton("<##grid")) {
        grid_mode = (grid_mode - 1 + 4) % 4;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(">##grid")) {
        grid_mode = (grid_mode + 1) % 4;
    }
    ImGui::Separator();
    ImGui::Text("Velocidade: %.4fx", speed_mult);
    if (ImGui::SmallButton("-##speed"))  speed_mult = std::max(speed_mult / 2.0f, 0.015625f);
    ImGui::SameLine();
    if (ImGui::SmallButton("+##speed"))  speed_mult = std::min(speed_mult * 2.0f, 128.0f);
    ImGui::Separator();

    for (auto& b : sim.bodies) {
        if (!b.alive) continue;
        ImGui::TextColored(ImVec4((float)b.color[0], (float)b.color[1], (float)b.color[2], 1.0f),
                           "%s: (%.4f, %.4f, %.4f)", b.name.c_str(), b.x, b.y, b.z);
    }
    ImGui::End();

    if (show_shortcuts) {
        ImGui::SetNextWindowCollapsed(shortcuts_collapsed, ImGuiCond_Always);
        ImGui::Begin("Atalhos", nullptr,
            ImGuiWindowFlags_NoCollapse);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Camera");
        ImGui::BulletText("Botao direito + arrastar: orbitar");
        ImGui::BulletText("Scroll: zoom in/out");
        ImGui::BulletText("WASD: mover foco (plano XZ)");
        ImGui::BulletText("Q / E: descer / subir");
        ImGui::BulletText("F: seguir próximo corpo (volta ao livre no último)");
        ImGui::BulletText("Shift: velocidade 3x");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Janela");
        ImGui::BulletText("H: mostrar/ocultar atalhos");
        ImGui::BulletText("C: configurar camera");
        ImGui::BulletText("G: ciclar modo do grid");
        ImGui::BulletText("+ / -: acelerar / desacelerar");
        ImGui::BulletText("Esc: fechar simulacao");
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Pressione H para fechar esta janela");
        ImGui::End();
    }

    if (show_camera_config) {
        ImGui::Begin("Config Camera", &show_camera_config);
        ImGui::SliderFloat("Distancia", &cam.distance, 0.1f, 1000.0f);
        ImGui::SliderFloat("Sensibilidade", &cam.sensitivity, 0.01f, 2.0f);
        ImGui::SliderFloat("Velocidade", &cam.speed, 0.1f, 50.0f);
        ImGui::SliderFloat("FOV", &cam.fov, 10.0f, 120.0f);
        ImGui::Separator();
        ImGui::InputFloat3("Foco", &cam.focus_x);
        if (ImGui::Button("Reset camera")) {
            cam.distance = 5.0f;
            cam.yaw = -90.0f;
            cam.pitch = 30.0f;
            cam.focus_x = 0.0f;
            cam.focus_y = 0.0f;
            cam.focus_z = 0.0f;
            cam.fov = 45.0f;
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Renderer::swap_buffers() {
    if (g_window) glfwSwapBuffers(g_window);
}

bool Renderer::should_close() {
    return g_window ? glfwWindowShouldClose(g_window) : true;
}

void Renderer::cleanup() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    if (g_window) {
        glfwDestroyWindow(g_window);
        glfwTerminate();
    }
    g_renderer_instance = nullptr;
}
