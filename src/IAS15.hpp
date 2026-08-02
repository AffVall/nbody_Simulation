#pragma once
#include "Body.hpp"

struct IAS15State {
    double dt_last;
    bool initialized;
};

void init_ias15(IAS15State& state);
void step_ias15(std::vector<Body>& bodies, IAS15State& state, double G, double& dt,
                double tol, double dt_min, double dt_max);
