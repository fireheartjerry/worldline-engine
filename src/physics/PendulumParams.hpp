#pragma once

enum class ConnectorMode {
    RIGID,
    ROPE
};

struct DragCoefficients {
    double linear = 0.0;     // viscous drag coefficient (kg/s)
    double quadratic = 0.0;  // quadratic drag coefficient (kg/m)
};

struct DirectionalDragCoefficients {
    DragCoefficients axial{};
    DragCoefficients normal{};
};

struct JointResistance {
    double viscous = 0.0;    // linear torque damping (N m s)
    double quadratic = 0.0;  // quadratic torque damping (N m s^2)
    double coulomb = 0.0;    // dry friction torque magnitude (N m)
};

struct FlowFieldParams {
    double wind_x = 0.0;   // uniform x-directed fluid velocity (m/s)
    double wind_y = 0.0;   // uniform y-directed fluid velocity (m/s)
    double shear_x = 0.0;  // x-flow gradient with vertical position: u_x += shear_x * y
    double shear_y = 0.0;  // y-flow gradient with horizontal position: u_y += shear_y * x
    double swirl = 0.0;    // rigid-body swirl rate about the pivot: u += swirl * (-y, x)
};

struct PendulumParams {
    double l1 = 1.0;   // rod 1 length (metres)
    double l2 = 1.0;   // rod 2 length (metres)
    double m1 = 1.0;   // bob 1 mass   (kg)
    double m2 = 1.0;   // bob 2 mass   (kg)
    double connector1_mass = 0.0; // rigid connector 1 mass (kg)
    double connector2_mass = 0.0; // rigid connector 2 mass (kg)
    double g  = 9.81;  // gravitational acceleration (m/s²)
    DragCoefficients bob1_drag{};
    DragCoefficients bob2_drag{};
    DirectionalDragCoefficients connector1_drag{};
    DirectionalDragCoefficients connector2_drag{};
    JointResistance pivot_resistance{};
    JointResistance elbow_resistance{};
    FlowFieldParams flow_field{};
    double adaptive_tolerance = 1e-7;
    double min_substep = 2.5e-5;
    int max_refinements = 10;
    ConnectorMode connector_mode = ConnectorMode::RIGID;

    // Phase 3: exponent for the generalised gravity field.
    // Standard Newtonian potential in 3-D corresponds to exponent = 2.0.
    // Hardcoded to 2.0 here; PendulumDerivatives will read it in Phase 3.
    double gravity_exponent = 2.0;
};
