#include "IAS15.hpp"
#include "Physics.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

void init_ias15(IAS15State& state) {
    state.dt_last = 0.0;
    state.initialized = false;
}

void step_ias15(std::vector<Body>& bodies, IAS15State& state, double G,
                double& dt, double tol, double dt_min, double dt_max) {
    int n = bodies.size();
    std::vector<double> x0(n*3), v0(n*3), a0(n*3);

    for (int i = 0; i < n; i++) {
        x0[i*3]=bodies[i].x;  x0[i*3+1]=bodies[i].y;  x0[i*3+2]=bodies[i].z;
        v0[i*3]=bodies[i].vx; v0[i*3+1]=bodies[i].vy; v0[i*3+2]=bodies[i].vz;
        a0[i*3]=bodies[i].ax; a0[i*3+1]=bodies[i].ay; a0[i*3+2]=bodies[i].az;
    }

    if (!state.initialized) {
        state.initialized = true;
        dt = dt_max * 0.01;
        state.dt_last = dt;
        return;
    }

    double error = 1e10;
    while (error > tol && dt > dt_min) {
        for (int i = 0; i < n; i++) {
            bodies[i].x=x0[i*3]; bodies[i].y=x0[i*3+1]; bodies[i].z=x0[i*3+2];
            bodies[i].vx=v0[i*3]; bodies[i].vy=v0[i*3+1]; bodies[i].vz=v0[i*3+2];
        }

        for (int stage = 0; stage < 6; stage++) {
            double t = (stage < 3) ? (double)stage/5.0 : 1.0;
            for (int i = 0; i < n; i++) {
                bodies[i].x  = x0[i*3]   + dt*t*(v0[i*3]   + 0.5*dt*t*a0[i*3]);
                bodies[i].y  = x0[i*3+1] + dt*t*(v0[i*3+1] + 0.5*dt*t*a0[i*3+1]);
                bodies[i].z  = x0[i*3+2] + dt*t*(v0[i*3+2] + 0.5*dt*t*a0[i*3+2]);
                bodies[i].vx = v0[i*3]   + dt*t*a0[i*3];
                bodies[i].vy = v0[i*3+1] + dt*t*a0[i*3+1];
                bodies[i].vz = v0[i*3+2] + dt*t*a0[i*3+2];
            }
            compute_forces(bodies, G);
        }

        error = 0.0;
        for (int i = 0; i < n; i++) {
            double ex = bodies[i].x - (x0[i*3]+dt*v0[i*3]+0.5*dt*dt*a0[i*3]);
            double ey = bodies[i].y - (x0[i*3+1]+dt*v0[i*3+1]+0.5*dt*dt*a0[i*3+1]);
            double ez = bodies[i].z - (x0[i*3+2]+dt*v0[i*3+2]+0.5*dt*dt*a0[i*3+2]);
            error += ex*ex + ey*ey + ez*ez;
        }
        error = std::sqrt(error / n);
        if (error > tol) dt *= 0.5;
    }

    if (error > 0) {
        double factor = 0.9 * std::pow(tol / error, 1.0/7.0);
        dt = std::clamp(dt * factor, dt_min, dt_max);
    }
    state.dt_last = dt;

    for (int i = 0; i < n; i++) {
        bodies[i].vx = v0[i*3]   + dt*a0[i*3];
        bodies[i].vy = v0[i*3+1] + dt*a0[i*3+1];
        bodies[i].vz = v0[i*3+2] + dt*a0[i*3+2];
    }
    compute_forces(bodies, G);
}
