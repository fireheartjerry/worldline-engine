#pragma once

struct PendulumParams {
    double l1 = 1.0;   // rod 1 length (metres)
    double l2 = 1.0;   // rod 2 length (metres)
    double m1 = 1.0;   // bob 1 mass   (kg)
    double m2 = 1.0;   // bob 2 mass   (kg)
    double g  = 9.81;  // gravitational acceleration (m/s²)

    // Phase 3: exponent for the generalised gravity field.
    // Standard Newtonian potential in 3-D corresponds to exponent = 2.0.
    // Hardcoded to 2.0 here; PendulumDerivatives will read it in Phase 3.
    double gravity_exponent = 2.0;
};
