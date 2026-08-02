#include "AhmadCohen.hpp"
#include <cmath>
#include <algorithm>

constexpr double ETA_I = 0.02;
constexpr double ETA_R = 0.02;

void init_ahmad_cohen(std::vector<Particle>& particles, int n) {
    particles.resize(n);
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < 3; k++) {
            particles[i].irregular_force[k] = 0;
            particles[i].regular_force[k] = 0;
        }
        particles[i].dt_irregular = 0.001;
        particles[i].dt_regular = 0.01;
        particles[i].t_next_irregular = 0;
        particles[i].t_next_regular = 0;
        particles[i].neighbor_radius = 0.5;
    }
}

static double mag3(const double v[3]) {
    return std::sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static void compute_irr_force(int idx, std::vector<Body>& bodies,
                               std::vector<Particle>& particles, double G) {
    Body& bi = bodies[idx];
    bi.ax = 0; bi.ay = 0; bi.az = 0;
    double nr2 = particles[idx].neighbor_radius * particles[idx].neighbor_radius;

    for (size_t j = 0; j < bodies.size(); j++) {
        if ((int)j == idx || !bodies[j].alive) continue;
        double dx = bodies[j].x - bi.x;
        double dy = bodies[j].y - bi.y;
        double dz = bodies[j].z - bi.z;
        double r2 = dx*dx + dy*dy + dz*dz;
        if (r2 < nr2) {
            double r3 = r2 * std::sqrt(r2) + 1e-6;
            double f = G / r3;
            bi.ax += f * bodies[j].m * dx;
            bi.ay += f * bodies[j].m * dy;
            bi.az += f * bodies[j].m * dz;
        }
    }
    particles[idx].irregular_force[0] = bi.ax;
    particles[idx].irregular_force[1] = bi.ay;
    particles[idx].irregular_force[2] = bi.az;
}

static void update_dt(int idx, std::vector<Body>& bodies,
                      std::vector<Particle>& particles) {
    double min_r = 1e30;
    for (size_t j = 0; j < bodies.size(); j++) {
        if ((int)j == idx || !bodies[j].alive) continue;
        double r = bodies[idx].distance_to(bodies[j]);
        if (r < min_r) min_r = r;
    }
    double a_irr = mag3(particles[idx].irregular_force);
    if (a_irr > 1e-30)
        particles[idx].dt_irregular = ETA_I * std::sqrt(min_r / a_irr);

    double a_reg = mag3(particles[idx].regular_force);
    if (a_reg > 1e-30)
        particles[idx].dt_regular = ETA_R * std::sqrt(
            particles[idx].neighbor_radius / a_reg);
}

void ahmad_cohen_step(std::vector<Body>& bodies,
                      std::vector<Particle>& particles, double G, double dt_global) {
    int n = bodies.size();
    double t_start = 0;

    while (t_start < dt_global) {
        int next_idx = -1;
        double t_next = 1e30;
        bool is_irr = false;

        for (int i = 0; i < n; i++) {
            if (!bodies[i].alive) continue;
            if (particles[i].t_next_irregular < t_next) {
                t_next = particles[i].t_next_irregular;
                next_idx = i; is_irr = true;
            }
            if (particles[i].t_next_regular < t_next) {
                t_next = particles[i].t_next_regular;
                next_idx = i; is_irr = false;
            }
        }
        if (next_idx < 0) break;

        double dt = std::min(t_next - t_start, dt_global - t_start);
        if (dt <= 0) break;

        Body& b = bodies[next_idx];
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.z += b.vz * dt;

        if (is_irr) {
            b.vx += particles[next_idx].irregular_force[0] * dt;
            b.vy += particles[next_idx].irregular_force[1] * dt;
            b.vz += particles[next_idx].irregular_force[2] * dt;
            compute_irr_force(next_idx, bodies, particles, G);
            particles[next_idx].t_next_irregular += particles[next_idx].dt_irregular;
        } else {
            compute_irr_force(next_idx, bodies, particles, G);
            particles[next_idx].t_next_regular += particles[next_idx].dt_regular;
        }

        update_dt(next_idx, bodies, particles);
        t_start = t_next;
    }
}
