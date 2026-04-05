#pragma once
#include "AppTypes.hpp"
#include <array>

inline constexpr double APP_RAD_TO_DEG = 180.0 / 3.14159265358979323846;
inline constexpr double APP_MIN_LENGTH = 0.20;
inline constexpr double APP_MAX_LENGTH = 4.20;
inline constexpr double APP_MIN_MASS = 0.10;
inline constexpr double APP_MAX_MASS = 12.00;
inline constexpr double APP_MIN_CONNECTOR_MASS = 0.00;
inline constexpr double APP_MAX_CONNECTOR_MASS = 8.00;
inline constexpr double APP_MIN_LINEAR_DRAG = 0.00;
inline constexpr double APP_MAX_LINEAR_DRAG = 8.00;
inline constexpr double APP_MIN_QUADRATIC_DRAG = 0.00;
inline constexpr double APP_MAX_QUADRATIC_DRAG = 4.00;
inline constexpr double APP_MIN_JOINT_VISCOUS = 0.00;
inline constexpr double APP_MAX_JOINT_VISCOUS = 24.00;
inline constexpr double APP_MIN_JOINT_QUADRATIC = 0.00;
inline constexpr double APP_MAX_JOINT_QUADRATIC = 8.00;
inline constexpr double APP_MIN_JOINT_COULOMB = 0.00;
inline constexpr double APP_MAX_JOINT_COULOMB = 6.00;
inline constexpr double APP_MIN_VELOCITY_VECTOR_SCALE = 0.04;
inline constexpr double APP_MAX_VELOCITY_VECTOR_SCALE = 1.20;
inline constexpr double APP_MIN_FORCE_VECTOR_SCALE = 0.005;
inline constexpr double APP_MAX_FORCE_VECTOR_SCALE = 0.20;
inline constexpr double APP_MIN_ANGLE_DEG = -179.5;
inline constexpr double APP_MAX_ANGLE_DEG = 179.5;
inline constexpr double APP_MIN_OMEGA_DEG = -720.0;
inline constexpr double APP_MAX_OMEGA_DEG = 720.0;
inline constexpr double APP_PHYS_DT = 0.001;
inline constexpr int APP_MAX_STEPS_PER_FRAME = 42;

inline const std::array<FieldSpec, 4> GEOMETRY_FIELDS{{
    {FieldId::LENGTH1, "Upper Reach", &PendulumDraft::l1, APP_MIN_LENGTH, APP_MAX_LENGTH, 2, "m"},
    {FieldId::LENGTH2, "Lower Reach", &PendulumDraft::l2, APP_MIN_LENGTH, APP_MAX_LENGTH, 2, "m"},
    {FieldId::MASS1, "Upper Bob", &PendulumDraft::m1, APP_MIN_MASS, APP_MAX_MASS, 2, "kg"},
    {FieldId::MASS2, "Lower Bob", &PendulumDraft::m2, APP_MIN_MASS, APP_MAX_MASS, 2, "kg"},
}};

inline const std::array<FieldSpec, 2> CONNECTOR_FIELDS{{
    {FieldId::CONNECTOR1_MASS, "Upper Connector", &PendulumDraft::connector1_mass, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS, 2, "kg"},
    {FieldId::CONNECTOR2_MASS, "Lower Connector", &PendulumDraft::connector2_mass, APP_MIN_CONNECTOR_MASS, APP_MAX_CONNECTOR_MASS, 2, "kg"},
}};

inline const std::array<FieldSpec, 4> BOB_RESISTANCE_FIELDS{{
    {FieldId::BOB1_LINEAR_DRAG, "Upper Bob Linear", &PendulumDraft::bob1_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::BOB1_QUADRATIC_DRAG, "Upper Bob Quad", &PendulumDraft::bob1_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
    {FieldId::BOB2_LINEAR_DRAG, "Lower Bob Linear", &PendulumDraft::bob2_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::BOB2_QUADRATIC_DRAG, "Lower Bob Quad", &PendulumDraft::bob2_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
}};

inline const std::array<FieldSpec, 8> CONNECTOR_DRAG_FIELDS{{
    {FieldId::CONNECTOR1_AXIAL_LINEAR_DRAG, "Upper Axial Linear", &PendulumDraft::connector1_axial_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::CONNECTOR1_AXIAL_QUADRATIC_DRAG, "Upper Axial Quad", &PendulumDraft::connector1_axial_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
    {FieldId::CONNECTOR1_NORMAL_LINEAR_DRAG, "Upper Normal Linear", &PendulumDraft::connector1_normal_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::CONNECTOR1_NORMAL_QUADRATIC_DRAG, "Upper Normal Quad", &PendulumDraft::connector1_normal_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
    {FieldId::CONNECTOR2_AXIAL_LINEAR_DRAG, "Lower Axial Linear", &PendulumDraft::connector2_axial_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::CONNECTOR2_AXIAL_QUADRATIC_DRAG, "Lower Axial Quad", &PendulumDraft::connector2_axial_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
    {FieldId::CONNECTOR2_NORMAL_LINEAR_DRAG, "Lower Normal Linear", &PendulumDraft::connector2_normal_linear_drag, APP_MIN_LINEAR_DRAG, APP_MAX_LINEAR_DRAG, 2, "kg/s"},
    {FieldId::CONNECTOR2_NORMAL_QUADRATIC_DRAG, "Lower Normal Quad", &PendulumDraft::connector2_normal_quadratic_drag, APP_MIN_QUADRATIC_DRAG, APP_MAX_QUADRATIC_DRAG, 2, "kg/m"},
}};

inline const std::array<FieldSpec, 6> JOINT_RESISTANCE_FIELDS{{
    {FieldId::PIVOT_VISCOUS, "Pivot Viscous", &PendulumDraft::pivot_viscous, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS, 2, "Nms"},
    {FieldId::PIVOT_QUADRATIC, "Pivot Quadratic", &PendulumDraft::pivot_quadratic, APP_MIN_JOINT_QUADRATIC, APP_MAX_JOINT_QUADRATIC, 2, "Nms^2"},
    {FieldId::PIVOT_COULOMB, "Pivot Coulomb", &PendulumDraft::pivot_coulomb, APP_MIN_JOINT_COULOMB, APP_MAX_JOINT_COULOMB, 2, "Nm"},
    {FieldId::ELBOW_VISCOUS, "Elbow Viscous", &PendulumDraft::elbow_viscous, APP_MIN_JOINT_VISCOUS, APP_MAX_JOINT_VISCOUS, 2, "Nms"},
    {FieldId::ELBOW_QUADRATIC, "Elbow Quadratic", &PendulumDraft::elbow_quadratic, APP_MIN_JOINT_QUADRATIC, APP_MAX_JOINT_QUADRATIC, 2, "Nms^2"},
    {FieldId::ELBOW_COULOMB, "Elbow Coulomb", &PendulumDraft::elbow_coulomb, APP_MIN_JOINT_COULOMB, APP_MAX_JOINT_COULOMB, 2, "Nm"},
}};

inline const std::array<FieldSpec, 0> FLOW_FIELDS{{}};

inline const std::array<FieldSpec, 4> LAUNCH_FIELDS{{
    {FieldId::ANGLE1, "Upper Angle", &PendulumDraft::theta1_deg, APP_MIN_ANGLE_DEG, APP_MAX_ANGLE_DEG, 1, "deg"},
    {FieldId::ANGLE2, "Lower Angle", &PendulumDraft::theta2_deg, APP_MIN_ANGLE_DEG, APP_MAX_ANGLE_DEG, 1, "deg"},
    {FieldId::OMEGA1, "Upper Spin", &PendulumDraft::omega1_deg, APP_MIN_OMEGA_DEG, APP_MAX_OMEGA_DEG, 1, "deg/s"},
    {FieldId::OMEGA2, "Lower Spin", &PendulumDraft::omega2_deg, APP_MIN_OMEGA_DEG, APP_MAX_OMEGA_DEG, 1, "deg/s"},
}};

inline const std::array<VisualFieldSpec, 3> VISUAL_FIELDS{{
    {FieldId::TRAIL_MEMORY, "Trail Persistence", &VisualDraft::trail_memory, 0.0, 1.0, 2, "0-1"},
    {FieldId::VELOCITY_VECTOR_SCALE, "Velocity Scale", &VisualDraft::velocity_vector_scale, APP_MIN_VELOCITY_VECTOR_SCALE, APP_MAX_VELOCITY_VECTOR_SCALE, 2, "x"},
    {FieldId::FORCE_VECTOR_SCALE, "Force Scale", &VisualDraft::force_vector_scale, APP_MIN_FORCE_VECTOR_SCALE, APP_MAX_FORCE_VECTOR_SCALE, 3, "x"},
}};

inline const FieldSpec* find_field_spec(FieldId id) {
    for (const FieldSpec& spec : GEOMETRY_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : CONNECTOR_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : BOB_RESISTANCE_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : CONNECTOR_DRAG_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : JOINT_RESISTANCE_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : FLOW_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    for (const FieldSpec& spec : LAUNCH_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}

inline const VisualFieldSpec* find_visual_field_spec(FieldId id) {
    for (const VisualFieldSpec& spec : VISUAL_FIELDS) {
        if (spec.id == id) {
            return &spec;
        }
    }
    return nullptr;
}
