#include "Body.hpp"
#include "IO.hpp"
#include "Physics.hpp"
#include "PN.hpp"
#include "Integrators.hpp"
#include "IAS15.hpp"
#include "AhmadCohen.hpp"
#include "Collisions.hpp"
#include "BarnesHut.hpp"
#include "Diagnostics.hpp"
#include "BinaryIO.hpp"
#include "Renderer.hpp"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

static void compute_forces_dispatch(std::vector<Body>& bodies, const Simulation& sim) {
    if (sim.relativistic) {
        compute_forces_pn(bodies, sim.gravity, sim.c, sim.pn_order);
    } else if (sim.force_method == "barnes_hut") {
        compute_forces_barnes_hut(bodies, sim.gravity, sim.bh_theta);
    } else {
        compute_forces(bodies, sim.gravity);
    }
}

int main(int argc, char* argv[]) {
    std::string preset = (argc > 1) ? argv[1] : "presets/figure_eight.json";
    Simulation sim = load_preset(preset);

    std::cout << "Loaded: " << preset << "\n";
    std::cout << "Bodies: " << sim.n_bodies << "\n";
    std::cout << "G = " << sim.gravity << "\n";
    std::cout << "dt = " << sim.dt << "\n";
    std::cout << "Integrator: " << sim.integrator << "\n";
    std::cout << "Force method: " << sim.force_method << "\n";

    move_to_com(sim.bodies);
    compute_forces_dispatch(sim.bodies, sim);

    IAS15State ias15_state;
    init_ias15(ias15_state);
    std::vector<Particle> particles;
    init_ahmad_cohen(particles, sim.n_bodies);

    mkdir("output", 0755);
    FILE* out = fopen("output/trajectory.bin", "wb");
    if (out) {
        write_header(out, sim);
        write_frame(out, sim);
    }

    Renderer renderer;
    if (!renderer.init(1280, 720, "N-Body Simulation")) {
        std::cerr << "Falha ao inicializar renderer\n";
        return 1;
    }

    int output_every = 100;
    int step = 0;
    double E0 = total_energy(sim);
    double step_accum = 0.0;

    std::cout << "E0 = " << E0 << "\n";

    while (!renderer.should_close()) {
        renderer.process_input(sim);

        step_accum += renderer.speed_mult;
        int steps_this_frame = (int)step_accum;
        step_accum -= steps_this_frame;
        if (steps_this_frame < 1) steps_this_frame = 1;

        for (int s = 0; s < steps_this_frame; s++) {
            if (sim.integrator == "ias15") {
                step_ias15(sim.bodies, ias15_state, sim.gravity, sim.dt,
                           sim.tolerance, 1e-10, 0.01);
            } else if (sim.integrator == "rk4") {
                step_rk4(sim.bodies, sim.gravity, sim.dt);
            } else if (sim.integrator == "ahmad_cohen") {
                ahmad_cohen_step(sim.bodies, particles, sim.gravity, sim.dt);
                compute_forces_dispatch(sim.bodies, sim);
            } else {
                step_yoshida4(sim.bodies, sim.gravity, sim.dt);
            }

            sim.time += sim.dt;
            compute_forces_dispatch(sim.bodies, sim);
            handle_collisions(sim.bodies, sim.collision_mode);
            step++;
        }

        if (step % output_every == 0) {
            if (out) write_frame(out, sim);
            double E = total_energy(sim);
            double drift = (E0 != 0.0) ? std::abs((E - E0) / E0) : 0.0;
            std::cout << "t=" << sim.time << " E=" << E
                      << " drift=" << drift << "\n";
        }

        renderer.render(sim);
        renderer.swap_buffers();
    }

    if (out) fclose(out);
    renderer.cleanup();
    return 0;
}
