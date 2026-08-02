#include "Collisions.hpp"
#include <cmath>

void handle_collisions(std::vector<Body>& bodies, const std::string& mode) {
    if (mode == "none") return;
    int n = bodies.size();

    for (int i = 0; i < n; i++) {
        if (!bodies[i].alive) continue;
        for (int j = i + 1; j < n; j++) {
            if (!bodies[j].alive) continue;
            double r = bodies[i].distance_to(bodies[j]);
            double rc = bodies[i].radius + bodies[j].radius;
            if (r >= rc) continue;

            if (mode == "merge") {
                double tm = bodies[i].m + bodies[j].m;
                bodies[i].vx = (bodies[i].m*bodies[i].vx + bodies[j].m*bodies[j].vx)/tm;
                bodies[i].vy = (bodies[i].m*bodies[i].vy + bodies[j].m*bodies[j].vy)/tm;
                bodies[i].vz = (bodies[i].m*bodies[i].vz + bodies[j].m*bodies[j].vz)/tm;
                bodies[i].m = tm;
                bodies[i].radius = cbrt(pow(bodies[i].radius,3)+pow(bodies[j].radius,3));
                bodies[j].alive = false;
            } else if (mode == "elastic") {
                double dx=bodies[j].x-bodies[i].x, dy=bodies[j].y-bodies[i].y;
                double dz=bodies[j].z-bodies[i].z;
                double nx=dx/r, ny=dy/r, nz=dz/r;
                double dvx=bodies[j].vx-bodies[i].vx;
                double dvy=bodies[j].vy-bodies[i].vy;
                double dvz=bodies[j].vz-bodies[i].vz;
                double dvn=dvx*nx+dvy*ny+dvz*nz;
                if (dvn > 0) continue;
                double imp = 2.0*dvn/(bodies[i].m+bodies[j].m);
                bodies[i].vx += imp*bodies[j].m*nx;
                bodies[i].vy += imp*bodies[j].m*ny;
                bodies[i].vz += imp*bodies[j].m*nz;
                bodies[j].vx -= imp*bodies[i].m*nx;
                bodies[j].vy -= imp*bodies[i].m*ny;
                bodies[j].vz -= imp*bodies[i].m*nz;
            }
        }
    }
}
