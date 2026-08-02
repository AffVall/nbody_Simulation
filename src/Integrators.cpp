#include "Integrators.hpp"
#include "Physics.hpp"
#include <vector>
#include <functional>

constexpr double W1 = -1.4566124127490405;
constexpr double W0 =  1.9131248254980810;

static void verlet_step(std::vector<Body>& bodies, double G, double h,
                        std::function<void(std::vector<Body>&, double)> force_fn) {
    int n = bodies.size();
    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        bodies[i].vx += 0.5 * bodies[i].ax * h;
        bodies[i].vy += 0.5 * bodies[i].ay * h;
        bodies[i].vz += 0.5 * bodies[i].az * h;
    }
    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        bodies[i].x += bodies[i].vx * h;
        bodies[i].y += bodies[i].vy * h;
        bodies[i].z += bodies[i].vz * h;
    }
    force_fn(bodies, G);
    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        bodies[i].vx += 0.5 * bodies[i].ax * h;
        bodies[i].vy += 0.5 * bodies[i].ay * h;
        bodies[i].vz += 0.5 * bodies[i].az * h;
    }
}

void step_yoshida4(std::vector<Body>& bodies, double G, double dt) {
    auto fn = [](std::vector<Body>& b, double g) { compute_forces(b, g); };
    verlet_step(bodies, G, W1 * dt, fn);
    verlet_step(bodies, G, W0 * dt, fn);
    verlet_step(bodies, G, W1 * dt, fn);
}

struct State { double x, y, z, vx, vy, vz; };

static std::vector<State> get_state(const std::vector<Body>& b) {
    std::vector<State> s(b.size());
    for (size_t i = 0; i < b.size(); i++)
        s[i] = {b[i].x,b[i].y,b[i].z,b[i].vx,b[i].vy,b[i].vz};
    return s;
}

static void set_state(std::vector<Body>& bodies, const std::vector<State>& s) {
    for (size_t i = 0; i < bodies.size(); i++) {
        bodies[i].x=s[i].x; bodies[i].y=s[i].y; bodies[i].z=s[i].z;
        bodies[i].vx=s[i].vx; bodies[i].vy=s[i].vy; bodies[i].vz=s[i].vz;
    }
}

static void add_state(std::vector<Body>& bodies, const std::vector<State>& s, double h) {
    for (size_t i = 0; i < bodies.size(); i++) {
        bodies[i].x+=h*s[i].x; bodies[i].y+=h*s[i].y; bodies[i].z+=h*s[i].z;
        bodies[i].vx+=h*s[i].vx; bodies[i].vy+=h*s[i].vy; bodies[i].vz+=h*s[i].vz;
    }
}

void step_rk4(std::vector<Body>& bodies, double G, double dt) {
    auto orig = get_state(bodies);
    int n = bodies.size();

    compute_forces(bodies, G);
    auto k1 = get_state(bodies);

    set_state(bodies, orig);
    add_state(bodies, k1, 0.5*dt);
    compute_forces(bodies, G);
    auto k2 = get_state(bodies);

    set_state(bodies, orig);
    add_state(bodies, k2, 0.5*dt);
    compute_forces(bodies, G);
    auto k3 = get_state(bodies);

    set_state(bodies, orig);
    add_state(bodies, k3, dt);
    compute_forces(bodies, G);
    auto k4 = get_state(bodies);

    set_state(bodies, orig);
    for (int i = 0; i < n; i++) {
        bodies[i].x  += (dt/6.0)*(k1[i].x  + 2*k2[i].x  + 2*k3[i].x  + k4[i].x);
        bodies[i].y  += (dt/6.0)*(k1[i].y  + 2*k2[i].y  + 2*k3[i].y  + k4[i].y);
        bodies[i].z  += (dt/6.0)*(k1[i].z  + 2*k2[i].z  + 2*k3[i].z  + k4[i].z);
        bodies[i].vx += (dt/6.0)*(k1[i].vx + 2*k2[i].vx + 2*k3[i].vx + k4[i].vx);
        bodies[i].vy += (dt/6.0)*(k1[i].vy + 2*k2[i].vy + 2*k3[i].vy + k4[i].vy);
        bodies[i].vz += (dt/6.0)*(k1[i].vz + 2*k2[i].vz + 2*k3[i].vz + k4[i].vz);
    }
    compute_forces(bodies, G);
}
