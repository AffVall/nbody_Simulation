#include "Diagnostics.hpp"
#include <cmath>

double total_energy(const Simulation& sim) {
    double KE = 0, PE = 0;
    for (auto& b : sim.bodies) {
        if (!b.alive) continue;
        KE += 0.5 * b.m * (b.vx*b.vx + b.vy*b.vy + b.vz*b.vz);
    }
    for (size_t i = 0; i < sim.bodies.size(); i++) {
        if (!sim.bodies[i].alive) continue;
        for (size_t j = i+1; j < sim.bodies.size(); j++) {
            if (!sim.bodies[j].alive) continue;
            double r = sim.bodies[i].distance_to(sim.bodies[j]);
            PE -= sim.gravity * sim.bodies[i].m * sim.bodies[j].m / r;
        }
    }
    return KE + PE;
}

double momentum_magnitude(const Simulation& sim) {
    double px=0, py=0, pz=0;
    for (auto& b : sim.bodies) {
        if (!b.alive) continue;
        px += b.m * b.vx;
        py += b.m * b.vy;
        pz += b.m * b.vz;
    }
    return std::sqrt(px*px + py*py + pz*pz);
}
