#pragma once
#include "Body.hpp"
#include <vector>

struct Particle {
    double irregular_force[3];
    double regular_force[3];
    double dt_irregular, dt_regular;
    double t_next_irregular, t_next_regular;
    double neighbor_radius;
};

void init_ahmad_cohen(std::vector<Particle>& particles, int n);
void ahmad_cohen_step(std::vector<Body>& bodies,
                      std::vector<Particle>& particles, double G, double dt_global);
