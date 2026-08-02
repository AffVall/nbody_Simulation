#pragma once
#include "Body.hpp"

void step_yoshida4(std::vector<Body>& bodies, double G, double dt);
void step_rk4(std::vector<Body>& bodies, double G, double dt);
