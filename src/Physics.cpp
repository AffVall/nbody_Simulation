#include "Physics.hpp"
#include <cmath>

constexpr double EPS2 = 1e-6;

void compute_forces(std::vector<Body>& bodies, double G) {
    int n = bodies.size();

    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        bodies[i].ax = 0;
        bodies[i].ay = 0;
        bodies[i].az = 0;
    }

    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        for (int j = i + 1; j < n; j++) {
            if (!bodies[j].alive) continue;

            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;

            double r2 = dx*dx + dy*dy + dz*dz + EPS2;
            double r  = std::sqrt(r2);
            double r3 = r2 * r;
            double f  = G / r3;

            bodies[i].ax += f * bodies[j].m * dx;
            bodies[i].ay += f * bodies[j].m * dy;
            bodies[i].az += f * bodies[j].m * dz;

            bodies[j].ax -= f * bodies[i].m * dx;
            bodies[j].ay -= f * bodies[i].m * dy;
            bodies[j].az -= f * bodies[i].m * dz;
        }
    }
}
