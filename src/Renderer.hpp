#pragma once
#include "Body.hpp"

struct Camera {
    float focus_x = 0.0f, focus_y = 0.0f, focus_z = 0.0f;
    float distance = 5.0f;
    float yaw = -90.0f, pitch = 30.0f;
    float speed = 2.0f;
    float sensitivity = 0.3f;
    float fov = 45.0f;

    float x() const;
    float y() const;
    float z() const;
};

enum GridMode { GRID_NONE = 0, GRID_FLAT = 1, GRID_RELATIVITY_2D = 2, GRID_RELATIVITY_3D = 3 };

struct Renderer {
    bool init(int w, int h, const char* title);
    void render(const Simulation& sim);
    void cleanup();
    bool should_close();
    void swap_buffers();
    void process_input(const Simulation& sim);

    Camera camera;
    bool follow_body = false;
    int follow_body_index = -1;
    float follow_free_focus_x = 0.0f;
    float follow_free_focus_y = 0.0f;
    float follow_free_focus_z = 0.0f;
    int window_w = 1280, window_h = 720;
    bool show_shortcuts = true;
    bool shortcuts_collapsed = false;
    bool show_camera_config = false;
    int grid_mode = GRID_FLAT;
    float speed_mult = 1.0f;
};
