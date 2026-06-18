#pragma once

#include "cosmos/NBodySystem.hpp"
#include "math/Vec2.hpp"

#include <cstdint>
#include <vector>

namespace cosmos {

// A single autonomous observer probe. It drifts toward local matter
// concentrations, sampling what it finds.
struct Observer {
    Vec2 pos;
    Vec2 vel;
    double local_density = 0.0; // bodies per unit area within sense radius
    int nearest_type = -1;      // object index of the closest body
    double nearest_dist = 0.0;
};

// Aggregate readout of the whole fleet — the "observation" surface.
struct FleetReport {
    int agents = 0;
    double mean_density = 0.0;
    double peak_density = 0.0;
    double coverage = 0.0;   // fraction of agents currently near matter
    int species_seen = 0;    // distinct nearest object types
    double mean_speed = 0.0;
    bool finite = true;
};

// A deterministic fleet of probes that explore an NBodySystem.
class ObserverFleet {
public:
    std::vector<Observer> agents;
    double sense_radius = 2.2;

    void deploy(int count, double radius, std::uint64_t seed);
    void update(const NBodySystem& sys, double dt);
    FleetReport report() const;

private:
    void sample(Observer& a, const NBodySystem& sys) const;
};

} // namespace cosmos
