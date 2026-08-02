#include "PN.hpp"
#include "Physics.hpp"
#include <cmath>

void compute_forces_pn(std::vector<Body>& bodies, double G, double c, int order) {
    compute_forces(bodies, G);
    if (order <= 0) return;

    double c2 = c * c;
    int n = bodies.size();

    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        for (int j = i + 1; j < n; j++) {
            if (!bodies[j].alive) continue;

            double dx = bodies[j].x - bodies[i].x;
            double dy = bodies[j].y - bodies[i].y;
            double dz = bodies[j].z - bodies[i].z;
            double r2 = dx*dx + dy*dy + dz*dz;
            double r  = std::sqrt(r2);
            double rn = 1.0 / r;

            double nx = dx * rn, ny = dy * rn, nz = dz * rn;
            double M = bodies[i].m + bodies[j].m;
            double GM = G * M;

            double dvx = bodies[j].vx - bodies[i].vx;
            double dvy = bodies[j].vy - bodies[i].vy;
            double dvz = bodies[j].vz - bodies[i].vz;
            double v_rel2 = dvx*dvx + dvy*dvy + dvz*dvz;
            double n_dot_v = nx*dvx + ny*dvy + nz*dvz;

            // 1PN (Einstein-Infeld-Hoffmann)
            if (order >= 1) {
                double factor = G * M / (r2 * c2);
                double amp = 4.0 * GM * rn - v_rel2;
                double apn_x = factor * nx * amp;
                double apn_y = factor * ny * amp;
                double apn_z = factor * nz * amp;
                double tm = bodies[i].m + bodies[j].m;
                bodies[i].ax += apn_x * (bodies[j].m / tm);
                bodies[i].ay += apn_y * (bodies[j].m / tm);
                bodies[i].az += apn_z * (bodies[j].m / tm);
                bodies[j].ax -= apn_x * (bodies[i].m / tm);
                bodies[j].ay -= apn_y * (bodies[i].m / tm);
                bodies[j].az -= apn_z * (bodies[i].m / tm);
            }

            // 2.5PN (gravitational radiation)
            if (order >= 2) {
                double factor = 8.0/5.0 * G * G
                    * bodies[i].m * bodies[j].m / (r2*r*c2*c2*c);
                double drdt = (dx*dvx + dy*dvy + dz*dvz) * rn;
                double amp25 = ((11.0/3.0)*v_rel2 - (33.0/3.0)*n_dot_v*n_dot_v) * drdt;
                bodies[i].ax += factor * amp25 * nx;
                bodies[i].ay += factor * amp25 * ny;
                bodies[i].az += factor * amp25 * nz;
                bodies[j].ax -= factor * amp25 * nx;
                bodies[j].ay -= factor * amp25 * ny;
                bodies[j].az -= factor * amp25 * nz;
            }
        }
    }
}
