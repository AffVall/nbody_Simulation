#include "BinaryIO.hpp"
#include <cstdio>
#include <cstring>
#include <vector>

void write_header(FILE* f, const Simulation& sim) {
    BinHeader h;
    memcpy(h.magic, "NBOD", 4);
    h.version = 1;
    h.n_bodies = sim.n_bodies;
    h.time = sim.time;
    fwrite(&h, sizeof(h), 1, f);
}

void write_frame(FILE* f, const Simulation& sim) {
    fwrite(&sim.time, sizeof(double), 1, f);
    for (auto& b : sim.bodies) {
        double data[8] = {b.x, b.y, b.z, b.vx, b.vy, b.vz, b.m, b.radius};
        fwrite(data, sizeof(double), 8, f);
        fwrite(b.color, sizeof(double), 3, f);
        fwrite(&b.alive, sizeof(bool), 1, f);
    }
}

Simulation load_binary(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    BinHeader h;
    fread(&h, sizeof(h), 1, f);

    Simulation sim;
    sim.n_bodies = h.n_bodies;
    sim.bodies.resize(h.n_bodies);

    while (!feof(f)) {
        double t;
        if (fread(&t, sizeof(double), 1, f) != 1) break;
        sim.time = t;
        for (int i = 0; i < h.n_bodies; i++) {
            double data[8];
            fread(data, sizeof(double), 8, f);
            sim.bodies[i].x=data[0]; sim.bodies[i].y=data[1]; sim.bodies[i].z=data[2];
            sim.bodies[i].vx=data[3]; sim.bodies[i].vy=data[4]; sim.bodies[i].vz=data[5];
            sim.bodies[i].m=data[6]; sim.bodies[i].radius=data[7];
            fread(sim.bodies[i].color, sizeof(double), 3, f);
            fread(&sim.bodies[i].alive, sizeof(bool), 1, f);
        }
    }
    fclose(f);
    return sim;
}
