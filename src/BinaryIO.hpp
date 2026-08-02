#pragma once
#include "Body.hpp"
#include <string>
#include <cstdint>

struct BinHeader {
    char magic[4];      // "NBOD"
    uint32_t version;
    uint32_t n_bodies;
    double time;
};

void write_header(FILE* f, const Simulation& sim);
void write_frame(FILE* f, const Simulation& sim);
Simulation load_binary(const std::string& path);
