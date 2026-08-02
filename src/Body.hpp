#pragma once
#include <string>
#include <vector>
#include <cmath>

struct Body {
    std::string name;
    std::string type;
    double m;            // kg
    double radius;       // m
    double x, y, z;      // m
    double vx, vy, vz;   // m/s
    double ax, ay, az;    // m/s2
    double color[3];
    bool alive;

    double distance_to(const Body& other) const {
        double dx = other.x - x;
        double dy = other.y - y;
        double dz = other.z - z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
};

struct Simulation {
    std::vector<Body> bodies;
    int n_bodies;
    double time;
    double dt;
    double gravity;          // G in simulation units (e.g. 4*pi^2 for AU,M_sun,yr)
    std::string integrator;
    bool relativistic;
    int pn_order;
    std::string collision_mode;
    std::string force_method;
    double bh_theta;
    double tolerance;
    double c;                // speed of light in simulation units
};

void move_to_com(std::vector<Body>& bodies);
