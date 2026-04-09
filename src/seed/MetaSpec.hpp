#pragma once

#include <string>
#include <vector>

struct MetaSpec {
    double g[2][2]{};
    double V[2][2]{};
    double C[2][2][2]{};
    double S[2][2]{};
    double T[2][2]{};
    double G[2][2][2]{};
    double W[2][2]{};
    double p = 0.0;
    bool p_dynamic = false;   // ADD
    double p_beta = 0.0;     // ADD — self-mod rate, used by integrator
    double q0[2]{};
    double qdot0[2]{};
};

MetaSpec generate_meta_spec(const std::vector<double>& lanes);
MetaSpec generate_meta_spec(const std::string& seed);
std::string generate_descriptor(const MetaSpec& ms);
