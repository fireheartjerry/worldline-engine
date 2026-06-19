#pragma once
// AppRuntime — visual/overlay config, presets, canvas-overlay view.
#include "AppDraft.hpp"

inline unsigned char trail_fade_alpha(const VisualDraft& visuals) {
    const double clamped = std::clamp(visuals.trail_memory, 0.0, 1.0);
    const double alpha = 10.0 - clamped * 8.8;
    return static_cast<unsigned char>(std::clamp(alpha, 1.0, 10.0));
}

inline VectorOverlayConfig make_vector_overlay_config(const VisualDraft& visuals) {
    VectorOverlayConfig config;
    config.enabled = visuals.show_vectors;
    config.show_velocity = visuals.show_velocity_vectors;
    config.show_gravity = visuals.show_gravity_vectors;
    config.show_drag = visuals.show_drag_vectors;
    config.show_reaction = visuals.show_reaction_vectors;
    config.show_net = visuals.show_net_vectors;
    config.show_link_drag = visuals.show_link_drag_vectors;
    config.show_joint_torque = visuals.show_joint_torque_vectors;
    config.velocity_scale = visuals.velocity_vector_scale;
    config.force_scale = visuals.force_vector_scale;
    return config;
}

inline const char* overlay_preset_short_label(OverlayPreset preset) {
    switch (preset) {
    case OverlayPreset::CLEAN: return "CLN";
    case OverlayPreset::FORCES: return "FRC";
    case OverlayPreset::RESISTANCE: return "RES";
    case OverlayPreset::FULL: return "ALL";
    case OverlayPreset::CUSTOM: return "CST";
    }
    return "CST";
}

inline const char* overlay_preset_label(OverlayPreset preset) {
    switch (preset) {
    case OverlayPreset::CLEAN: return "Clean";
    case OverlayPreset::FORCES: return "Forces";
    case OverlayPreset::RESISTANCE: return "Resistance";
    case OverlayPreset::FULL: return "Full";
    case OverlayPreset::CUSTOM: return "Custom";
    }
    return "Custom";
}

inline void apply_overlay_preset(VisualDraft& visuals, OverlayPreset preset) {
    visuals.preset = preset;
    switch (preset) {
    case OverlayPreset::CLEAN:
        visuals.show_vectors = false;
        visuals.show_velocity_vectors = false;
        visuals.show_gravity_vectors = false;
        visuals.show_drag_vectors = false;
        visuals.show_reaction_vectors = false;
        visuals.show_net_vectors = false;
        visuals.show_link_drag_vectors = false;
        visuals.show_joint_torque_vectors = false;
        break;
    case OverlayPreset::FORCES:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = true;
        visuals.show_gravity_vectors = true;
        visuals.show_drag_vectors = false;
        visuals.show_reaction_vectors = true;
        visuals.show_net_vectors = true;
        visuals.show_link_drag_vectors = false;
        visuals.show_joint_torque_vectors = false;
        break;
    case OverlayPreset::RESISTANCE:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = false;
        visuals.show_gravity_vectors = false;
        visuals.show_drag_vectors = true;
        visuals.show_reaction_vectors = false;
        visuals.show_net_vectors = false;
        visuals.show_link_drag_vectors = true;
        visuals.show_joint_torque_vectors = true;
        break;
    case OverlayPreset::FULL:
        visuals.show_vectors = true;
        visuals.show_velocity_vectors = true;
        visuals.show_gravity_vectors = true;
        visuals.show_drag_vectors = true;
        visuals.show_reaction_vectors = true;
        visuals.show_net_vectors = true;
        visuals.show_link_drag_vectors = true;
        visuals.show_joint_torque_vectors = true;
        break;
    case OverlayPreset::CUSTOM:
        break;
    }
}

inline const char* connector_mode_label(const PendulumDraft& draft) {
    if (!draft.rigid_connectors) {
        return "Massless ropes";
    }
    if (draft.connector_mass_enabled) {
        return "Rigid rods + mass";
    }
    return "Rigid rods";
}

inline CanvasOverlayView make_canvas_overlay_view(
    const AppState& app,
    const Simulation::VisualDiagnostics* diagnostics,
    double dissipation_power) {
    const PendulumDraft& active_draft =
        (app.mode == RunMode::STOPPED) ? app.draft : app.applied;
    CanvasOverlayView view;
    view.show_vectors = app.visuals.show_vectors;
    view.rigid_mode = app.simulation.rigid_connectors();
    view.mode_label = connector_mode_label(active_draft);
    view.preset_label = overlay_preset_short_label(app.visuals.preset);
    if (diagnostics != nullptr) {
        view.diagnostics = *diagnostics;
    }
    view.dissipation_power = dissipation_power;
    if (app.visuals.show_vectors) {
        view.hint =
            "Overlay is live. Use the inspector, legend, and stage labels to track motion, load paths, and joint torques.";
    } else {
        view.hint =
            app.draft.rigid_connectors
                ? "Wheel over a bob adjusts bob mass. Wheel over connector midpoints adjusts rod mass when enabled."
                : "Wheel over a bob adjusts bob mass. Rope mode can slack and catch again as the run evolves.";
    }
    return view;
}

inline std::string format_number(double value, int precision) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return std::string(buffer);
}

inline Color joint_torque_accent() { return {255, 116, 145, 255}; }

