#include "Body.hpp"

void move_to_com(std::vector<Body>& bodies) {
    double total_m = 0.0;
    double com[3] = {0, 0, 0};
    double com_v[3] = {0, 0, 0};

    for (auto& b : bodies) {
        if (!b.alive) continue;
        total_m += b.m;
        com[0] += b.m * b.x;
        com[1] += b.m * b.y;
        com[2] += b.m * b.z;
        com_v[0] += b.m * b.vx;
        com_v[1] += b.m * b.vy;
        com_v[2] += b.m * b.vz;
    }

    for (auto& b : bodies) {
        if (!b.alive) continue;
        b.x  -= com[0] / total_m;
        b.y  -= com[1] / total_m;
        b.z  -= com[2] / total_m;
        b.vx -= com_v[0] / total_m;
        b.vy -= com_v[1] / total_m;
        b.vz -= com_v[2] / total_m;
    }
}
