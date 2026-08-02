#include "IO.hpp"
#include "Constants.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

Simulation load_preset(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open()) {
        std::cerr << "Erro: nao foi abrir " << filename << "\n";
        exit(1);
    }

    json data = json::parse(f);
    Simulation sim;
    sim.time = 0.0;
    sim.dt = data.value("time_step", 0.001);
    sim.gravity = data.value("gravity", Units::PI2);
    sim.integrator = data.value("integrator", "yoshida4");
    sim.relativistic = data.value("relativistic", false);
    sim.pn_order = data.value("pn_order", 0);
    sim.collision_mode = data.value("collision", "none");
    sim.force_method = data.value("force_method", "direct");
    sim.bh_theta = data.value("barnes_hut_theta", 0.5);
    sim.tolerance = data.value("tolerance", 1e-10);
    sim.c = data.value("speed_of_light", Units::C_AU_YR);

    for (auto& b : data["bodies"]) {
        Body body;
        body.name = b["name"];
        body.type = b.value("type", "planet");
        body.m = b["mass"];
        body.radius = b.value("radius", 1e-6);
        body.x = b["position"][0];
        body.y = b["position"][1];
        body.z = b["position"][2];
        body.vx = b["velocity"][0];
        body.vy = b["velocity"][1];
        body.vz = b["velocity"][2];
        body.color[0] = b["color"][0];
        body.color[1] = b["color"][1];
        body.color[2] = b["color"][2];
        body.alive = true;
        sim.bodies.push_back(body);
    }
    sim.n_bodies = sim.bodies.size();
    return sim;
}
