#pragma once
#include "Body.hpp"
#include <vector>

void compute_forces_barnes_hut(std::vector<Body>& bodies, double G, double theta);
