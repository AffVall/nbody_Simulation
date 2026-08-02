#include "BarnesHut.hpp"
#include <cmath>
#include <vector>

struct OctNode {
    double cx, cy, cz, size;
    double mass, comx, comy, comz;
    int particle;  // -1 if internal
    int children[8];
};

static int build_tree(std::vector<Body>& bodies, std::vector<OctNode>& tree,
                      int node, double cx, double cy, double cz, double sz) {
    tree[node].cx = cx; tree[node].cy = cy; tree[node].cz = cz;
    tree[node].size = sz;
    for (int c = 0; c < 8; c++) tree[node].children[c] = -1;
    tree[node].particle = -1;
    tree[node].mass = 0; tree[node].comx = 0; tree[node].comy = 0; tree[node].comz = 0;

    int count = 0;
    for (size_t i = 0; i < bodies.size(); i++) {
        if (!bodies[i].alive) continue;
        double dx = bodies[i].x - cx, dy = bodies[i].y - cy, dz = bodies[i].z - cz;
        if (std::abs(dx) < sz/2 && std::abs(dy) < sz/2 && std::abs(dz) < sz/2)
            count++;
    }

    if (count == 0) return node;
    if (count == 1) {
        for (size_t i = 0; i < bodies.size(); i++) {
            if (!bodies[i].alive) continue;
            double dx = bodies[i].x - cx, dy = bodies[i].y - cy, dz = bodies[i].z - cz;
            if (std::abs(dx) < sz/2 && std::abs(dy) < sz/2 && std::abs(dz) < sz/2) {
                tree[node].particle = i;
                tree[node].mass = bodies[i].m;
                tree[node].comx = bodies[i].x;
                tree[node].comy = bodies[i].y;
                tree[node].comz = bodies[i].z;
                return node;
            }
        }
    }

    double hs = sz / 4.0;
    for (int oct = 0; oct < 8; oct++) {
        double ncx = cx + ((oct & 1) ? hs : -hs);
        double ncy = cy + ((oct & 2) ? hs : -hs);
        double ncz = cz + ((oct & 4) ? hs : -hs);
        int child = tree.size();
        tree.push_back(OctNode());
        tree[node].children[oct] = child;
        build_tree(bodies, tree, child, ncx, ncy, ncz, sz/2);
    }

    for (int c = 0; c < 8; c++) {
        int ci = tree[node].children[c];
        if (ci < 0 || tree[ci].mass == 0) continue;
        double tm = tree[node].mass + tree[ci].mass;
        tree[node].comx = (tree[node].mass*tree[node].comx + tree[ci].mass*tree[ci].comx)/tm;
        tree[node].comy = (tree[node].mass*tree[node].comy + tree[ci].mass*tree[ci].comy)/tm;
        tree[node].comz = (tree[node].mass*tree[node].comz + tree[ci].mass*tree[ci].comz)/tm;
        tree[node].mass = tm;
    }
    return node;
}

static void traverse(std::vector<Body>& bodies, int idx, std::vector<OctNode>& tree,
                     int node, double G, double theta) {
    if (node < 0 || tree[node].mass == 0) return;
    if (tree[node].particle >= 0) {
        if (tree[node].particle != idx) {
            int j = tree[node].particle;
            double dx=bodies[j].x-bodies[idx].x, dy=bodies[j].y-bodies[idx].y;
            double dz=bodies[j].z-bodies[idx].z;
            double r2=dx*dx+dy*dy+dz*dz+1e-6;
            double r3=r2*std::sqrt(r2);
            double f=G/r3;
            bodies[idx].ax += f*bodies[j].m*dx;
            bodies[idx].ay += f*bodies[j].m*dy;
            bodies[idx].az += f*bodies[j].m*dz;
        }
        return;
    }
    double dx=tree[node].comx-bodies[idx].x;
    double dy=tree[node].comy-bodies[idx].y;
    double dz=tree[node].comz-bodies[idx].z;
    double d=std::sqrt(dx*dx+dy*dy+dz*dz);
    if (d > 0 && tree[node].size/d < theta) {
        double r2=dx*dx+dy*dy+dz*dz+1e-6;
        double r3=r2*std::sqrt(r2);
        double f=G/r3;
        bodies[idx].ax += f*tree[node].mass*dx;
        bodies[idx].ay += f*tree[node].mass*dy;
        bodies[idx].az += f*tree[node].mass*dz;
    } else {
        for (int c = 0; c < 8; c++)
            traverse(bodies, idx, tree, tree[node].children[c], G, theta);
    }
}

void compute_forces_barnes_hut(std::vector<Body>& bodies, double G, double theta) {
    for (auto& b : bodies) { b.ax=0; b.ay=0; b.az=0; }
    if (bodies.empty()) return;

    double maxr = 0;
    for (auto& b : bodies) {
        if (!b.alive) continue;
        maxr = std::max(maxr, std::max(std::abs(b.x), std::max(std::abs(b.y), std::abs(b.z))));
    }
    maxr *= 2;

    std::vector<OctNode> tree(1);
    build_tree(bodies, tree, 0, 0, 0, 0, maxr);

    for (size_t i = 0; i < bodies.size(); i++)
        if (bodies[i].alive)
            traverse(bodies, i, tree, 0, G, theta);
}
