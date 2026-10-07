// Tier analysis deck for the Cosmos descent view: a strip of real scientific
// instruments along the bottom of the stage, one set per level. Split out of
// CosmosDescent.cpp (navigation + map rendering); shares its geometry through
// CosmosDescent.hpp.
#include "ui/CosmosDescent.hpp"

#include "ui/CosmosExplorerInternal.hpp" // helpers + draw_universe_backdrop decl

#include "ui/UiCharts.hpp"

#include "cosmos/Astrobio.hpp"
#include "cosmos/Cosmography.hpp"
#include "cosmos/CosmoStats.hpp"
#include "cosmos/Demography.hpp"
#include "cosmos/EcoSim.hpp"
#include "cosmos/Organism.hpp"
#include "cosmos/Phenology.hpp"

#include <algorithm>
#include <cmath>
#include <string>

using namespace cosmos;

namespace cosmos_ui {

// ── Tier analysis deck: a strip of real scientific instruments per level ──────
// Every plot is driven by the engine's own physics (CosmoStats / Cosmography /
// Astrobio / Phenology / Organism / Demography / the live ecosystem), so the
// console reads the actual model, not decoration.
void draw_descent_analysis(CosmosState& cosmos, Rectangle stage, float ui) {
    using namespace charts;
    DescentState& d = cosmos.descent;
    if (!d.analysis_open) return;
    const ProcNode& f = d.focus();
    const float t = static_cast<float>(GetTime());

    const float gap = kDescentDeckGap * ui;
    const float ch = descent_deck_height(stage, ui);
    const float cw = (stage.width - 4.0f * gap) / 3.0f;
    const float cy = stage.y + stage.height - ch - gap;
    const Rectangle C0 = {stage.x + gap, cy, cw, ch};
    const Rectangle C1 = {C0.x + cw + gap, cy, cw, ch};
    const Rectangle C2 = {C1.x + cw + gap, cy, cw, ch};

    if (f.kind == NodeKind::Universe) {
        // (1) LambdaCDM energy budget pie.
        Rectangle p = plot_card(C0, "ENERGY BUDGET", ui, WL::VIOLET_CORE);
        draw_pie(p, {static_cast<float>(cstat::kOmegaB), static_cast<float>(cstat::kOmegaC),
                     static_cast<float>(cstat::kOmegaLambda)},
                 {WL::XENON_CORE, WL::CYAN_CORE, WL::VIOLET_CORE});
        draw_text("baryon 5%", {C0.x + 9.0f * ui, C0.y + C0.height - 40.0f * ui}, 8.0f * ui, WL::XENON_CORE);
        draw_text("dark matter 27%", {C0.x + 9.0f * ui, C0.y + C0.height - 28.0f * ui}, 8.0f * ui, WL::CYAN_CORE);
        draw_text("dark energy 68%", {C0.x + 9.0f * ui, C0.y + C0.height - 16.0f * ui}, 8.0f * ui, WL::VIOLET_CORE);

        // (2) Matter power spectrum P(k) (log-log) with turnover + BAO scale.
        Axes a;
        a.plot = plot_card(C1, "MATTER POWER SPECTRUM P(k)", ui);
        a.logx = a.logy = true;
        a.x0 = std::log10(1e-3); a.x1 = std::log10(1.0);
        a.y0 = std::log10(1e2);  a.y1 = std::log10(1e6);
        draw_grid(a, ui, "k [h/Mpc]", "P(k)");
        draw_function(a, [](double k) { return cstat::power_spectrum_pk(k) * 1e4; }, WL::CYAN_CORE);
        const float kt = map_x(a, 0.018);
        DrawLineEx({kt, a.plot.y}, {kt, a.plot.y + a.plot.height}, 1.0f, with_alpha(WL::XENON_CORE, 120));
        draw_text("turnover", {kt + 2.0f * ui, a.plot.y + 2.0f * ui}, 8.0f * ui, with_alpha(WL::XENON_CORE, 200));

        // (3) Cosmic-web census (void/wall/filament/node volume share). The
        // 3000-point sample is static per seed, so it is computed once and
        // cached — never per frame.
        Rectangle q = plot_card(C2, "COSMIC WEB CENSUS", ui, WL::PLASMA_GREEN);
        if (d.web_census_seed != f.seed) {
            const std::vector<cosmoweb::WebPoint> web =
                cosmoweb::sample_cosmic_web(f.seed, 3000, 200.0);
            for (float& c : d.web_census) c = 0.0f;
            for (const auto& w : web) d.web_census[static_cast<int>(w.cls)] += 1.0f;
            d.web_census_seed = f.seed;
        }
        const float* cnt = d.web_census;
        const float tot = std::max(1.0f, cnt[0] + cnt[1] + cnt[2] + cnt[3]);
        draw_hbars(q, {cnt[0] / tot, cnt[1] / tot, cnt[2] / tot, cnt[3] / tot},
                   {"void", "wall", "filament", "node"},
                   {WL::VOID_SURFACE, WL::CYAN_DIM, WL::CYAN_CORE, WL::XENON_CORE}, ui);
        return;
    }

    if (f.kind == NodeKind::Galaxy) {
        const double mstar = std::max(1e7, f.phys_mass);
        // (1) Rotation curve: flat (with dark-matter halo) vs declining (baryons only).
        Axes a;
        a.plot = plot_card(C0, "ROTATION CURVE", ui);
        a.x0 = 0; a.x1 = 30; a.y0 = 0; a.y1 = 260;
        draw_grid(a, ui, "r [kpc]", "v [km/s]");
        const double vflat = 180.0 * std::pow(mstar / 5e10, 0.25);
        draw_function(a, [&](double r) { return vflat * std::sqrt(1.0 - std::exp(-r / 4.0)); },
                      WL::CYAN_CORE);                                   // with dark matter -> flat
        draw_function(a, [&](double r) { return vflat * std::sqrt(3.0 / (r + 3.0)); },
                      with_alpha(WL::XENON_CORE, 200));                  // baryons only -> Keplerian fall
        draw_text("halo (DM)", {a.plot.x + 4.0f * ui, a.plot.y + 2.0f * ui}, 8.0f * ui, WL::CYAN_CORE);
        draw_text("baryons", {a.plot.x + 4.0f * ui, a.plot.y + 12.0f * ui}, 8.0f * ui, WL::XENON_CORE);

        // (2) NFW dark-matter density profile (log-log; r^-1 inner, r^-3 outer).
        Axes b;
        b.plot = plot_card(C1, "NFW HALO PROFILE", ui);
        b.logx = b.logy = true;
        b.x0 = std::log10(0.05); b.x1 = std::log10(50.0);
        b.y0 = std::log10(1e-4); b.y1 = std::log10(1e2);
        draw_grid(b, ui, "r / r_s", "rho");
        draw_function(b, [](double r) { return cstat::nfw_density(r, 1.0, 1.0); }, WL::VIOLET_CORE);

        // (3) Galaxy stellar mass function (Schechter) with this galaxy marked.
        Axes c;
        c.plot = plot_card(C2, "STELLAR MASS FUNCTION", ui, WL::PLASMA_GREEN);
        c.logx = c.logy = true;
        c.x0 = std::log10(1e8); c.x1 = std::log10(3e11);
        c.y0 = std::log10(1e-5); c.y1 = std::log10(1e-1);
        draw_grid(c, ui, "M* [Msun]", "phi");
        const double Mstar = std::pow(10.0, 10.66);
        draw_function(c, [&](double M) { return cstat::schechter_phi(M, Mstar, -1.2, 3.96e-3); },
                      WL::PLASMA_GREEN);
        draw_marker(c, mstar, cstat::schechter_phi(mstar, Mstar, -1.2, 3.96e-3), WL::CYAN_CORE, ui, "this");
        return;
    }

    if (f.kind == NodeKind::StarSystem) {
        const double L = std::max(1e-4, f.luminosity);
        const double Teff = std::max(2200.0, f.phys_radius);
        // (1) HR diagram (Teff reversed) with the main sequence + this star.
        Axes a;
        a.plot = plot_card(C0, "HR DIAGRAM", ui, WL::XENON_CORE);
        a.logx = a.logy = true;
        a.x0 = std::log10(45000.0); a.x1 = std::log10(2200.0); // hot left -> cool right
        a.y0 = std::log10(1e-4); a.y1 = std::log10(1e6);
        draw_grid(a, ui, "Teff [K]", "L/Lsun");
        const auto& tbl = astro::star_table();
        std::vector<double> ms_t, ms_l;
        for (const auto& s : tbl) { ms_t.push_back(s.temp_k); ms_l.push_back(astro::star_luminosity(s.mass_sun)); }
        draw_series(a, ms_t, ms_l, with_alpha(WL::CYAN_DIM, 220), 1.6f);
        draw_marker(a, Teff, L, WL::XENON_CORE, ui, "star");

        // (2) Blackbody spectral energy distribution.
        Axes b;
        b.plot = plot_card(C1, "BLACKBODY SED", ui);
        b.logx = true; b.x0 = std::log10(1e-7); b.x1 = std::log10(3e-6);
        b.y0 = 0; b.y1 = 1.05;
        draw_grid(b, ui, "lambda [m]", "B");
        const double lam_pk = 2.898e-3 / Teff;
        auto planck = [&](double lam) {
            const double c2 = 1.4388e-2; // m K
            const double v = (1.0 / std::pow(lam, 5)) / (std::exp(c2 / (lam * Teff)) - 1.0);
            const double vp = (1.0 / std::pow(lam_pk, 5)) / (std::exp(c2 / (lam_pk * Teff)) - 1.0);
            return v / std::max(1e-300, vp);
        };
        draw_area(b, planck, palette_color(f.color, 255));
        draw_function(b, planck, palette_color(f.color, 255));

        // (3) Mass-luminosity relation with this star marked.
        Axes c;
        c.plot = plot_card(C2, "MASS-LUMINOSITY", ui, WL::PLASMA_GREEN);
        c.logx = c.logy = true;
        c.x0 = std::log10(0.08); c.x1 = std::log10(40.0);
        c.y0 = std::log10(1e-4); c.y1 = std::log10(1e6);
        draw_grid(c, ui, "M [Msun]", "L/Lsun");
        draw_function(c, [](double m) { return cstat::stellar_luminosity(m); }, WL::PLASMA_GREEN);
        draw_marker(c, std::max(0.08, f.phys_mass), L, WL::CYAN_CORE, ui, "");
        return;
    }

    if (f.kind == NodeKind::Planet) {
        // (1) Mass-radius diagram (Chen-Kipping) with this planet + regime lines.
        Axes a;
        a.plot = plot_card(C0, "MASS-RADIUS", ui);
        a.logx = a.logy = true;
        a.x0 = std::log10(0.1); a.x1 = std::log10(1e4);
        a.y0 = std::log10(0.3); a.y1 = std::log10(20.0);
        draw_grid(a, ui, "M [Mearth]", "R [Rearth]");
        draw_function(a, [](double m) { return cstat::planet_radius_earth(m); }, WL::CYAN_CORE);
        for (double bx : {2.04, 132.0}) {
            const float xx = map_x(a, bx);
            DrawLineEx({xx, a.plot.y}, {xx, a.plot.y + a.plot.height}, 1.0f, with_alpha(WL::GLASS_BORDER, 70));
        }
        if (f.phys_mass > 0.0)
            draw_marker(a, f.phys_mass, f.phys_radius, WL::XENON_CORE, ui, "world");

        // (2) Interior structure: differentiated core / mantle / crust / atmosphere.
        Rectangle q = plot_card(C1, "INTERIOR STRUCTURE", ui, WL::XENON_CORE);
        const Vector2 ic = {q.x + q.width * 0.5f, q.y + q.height * 0.5f};
        const float R = std::min(q.width, q.height) * 0.46f;
        const bool rocky = (f.phys_radius > 0.0 && f.phys_radius < 2.2);
        const Color shell[4] = {WL::XENON_CORE, {180, 90, 40, 255}, {110, 92, 70, 255}, WL::CYAN_DIM};
        float frac[4];
        if (rocky) { frac[0] = 0.50f; frac[1] = 0.85f; frac[2] = 0.97f; frac[3] = 1.0f; }
        else       { frac[0] = 0.25f; frac[1] = 0.55f; frac[2] = 0.80f; frac[3] = 1.0f; }
        for (int i = 3; i >= 0; --i)
            DrawCircleV(ic, R * frac[i], with_alpha(shell[i], 200));
        DrawCircleLines(static_cast<int>(ic.x), static_cast<int>(ic.y), R, with_alpha(WL::CYAN_DIM, 120));
        draw_text(rocky ? "rocky: Fe core" : "volatile envelope",
                  {q.x + 8.0f * ui, q.y + q.height - 16.0f * ui}, 8.0f * ui, WL::TEXT_TERTIARY);

        // (3) Insolation vs latitude with this world's regions plotted.
        Axes c;
        c.plot = plot_card(C2, "INSOLATION x LATITUDE", ui, WL::PLASMA_GREEN);
        c.x0 = -90; c.x1 = 90; c.y0 = 0; c.y1 = 1.05;
        draw_grid(c, ui, "latitude", "rel. insol.");
        draw_function(c, [](double lat) { return pheno::annual_mean_insolation(lat); }, WL::PLASMA_GREEN);
        for (const ChildRef& rg : f.children) {
            const double lat = -static_cast<double>(rg.y) * 110.0;
            draw_marker(c, lat, pheno::annual_mean_insolation(lat), to_raylib(rg.color), ui);
        }
        return;
    }

    if (f.kind == NodeKind::Ecosystem && d.live_seed == f.seed) {
        const eco::Community& Cm = d.live.community;
        const int S = static_cast<int>(Cm.species.size());
        // (1) Population time-series (all species, from the live history rings).
        Axes a;
        a.plot = plot_card(C0, "POPULATION TIME SERIES", ui, WL::PLASMA_GREEN);
        a.x0 = 0; a.x1 = static_cast<double>(ecosim::PopRing::N); a.y0 = 0; a.y1 = 1;
        // y auto-scaled per draw by normalizing each ring to its own max.
        draw_grid(a, ui, "time", "N/Nmax");
        for (int i = 0; i < S && i < static_cast<int>(d.live.history.size()); ++i) {
            const ecosim::PopRing& rg = d.live.history[static_cast<std::size_t>(i)];
            if (rg.filled < 2) continue;
            float mx = 1e-12f;
            for (int k = 0; k < rg.filled; ++k) mx = std::max(mx, rg.at(k));
            std::vector<double> xs, ys;
            for (int k = 0; k < rg.filled; ++k) { xs.push_back(k); ys.push_back(rg.at(k) / mx); }
            draw_series(a, xs, ys, with_alpha(to_raylib(Cm.species[static_cast<std::size_t>(i)].color), 200), 1.1f);
        }

        // (2) Predator-prey phase portrait (the keystone predator vs its main prey).
        Axes b;
        b.plot = plot_card(C1, "PHASE PORTRAIT", ui, WL::CYAN_CORE);
        int pred = -1, prey = -1;
        for (const eco::Link& lk : Cm.links)
            if (lk.pred < S && lk.prey < S && Cm.species[static_cast<std::size_t>(lk.pred)].t.tau > 1.5) {
                pred = lk.pred; prey = lk.prey; break;
            }
        b.x0 = 0; b.x1 = 1.6; b.y0 = 0; b.y1 = 1.6;
        draw_grid(b, ui, "prey N/Neq", "pred N/Neq");
        if (pred >= 0 && prey >= 0 && pred < static_cast<int>(d.live.history.size()) &&
            prey < static_cast<int>(d.live.history.size())) {
            const ecosim::PopRing& rpr = d.live.history[static_cast<std::size_t>(prey)];
            const ecosim::PopRing& rpd = d.live.history[static_cast<std::size_t>(pred)];
            const double xeq_pr = std::max(1e-9, Cm.species[static_cast<std::size_t>(prey)].xeq);
            const double xeq_pd = std::max(1e-9, Cm.species[static_cast<std::size_t>(pred)].xeq);
            const int nn = std::min(rpr.filled, rpd.filled);
            std::vector<double> xs, ys;
            for (int k = 0; k < nn; ++k) { xs.push_back(rpr.at(k) / xeq_pr); ys.push_back(rpd.at(k) / xeq_pd); }
            draw_series(b, xs, ys, WL::CYAN_CORE, 1.2f);
            if (nn > 0) draw_marker(b, xs.back(), ys.back(), WL::XENON_CORE, ui);
        }

        // (3) Community interaction matrix (predation strengths, heatmap).
        Rectangle q = plot_card(C2, "INTERACTION MATRIX", ui, WL::VIOLET_CORE);
        const int n = std::min(S, 16);
        std::vector<float> mat(static_cast<std::size_t>(n * n), 0.0f);
        for (const eco::Link& lk : Cm.links)
            if (lk.pred < n && lk.prey < n)
                mat[static_cast<std::size_t>(lk.pred * n + lk.prey)] = static_cast<float>(lk.pref);
        draw_heatmap(q, mat, n, n, 0.0f, 1.0f, Color{6, 14, 24, 255}, WL::CYAN_CORE);
        return;
    }

    if (f.kind == NodeKind::Creature) {
        // Recover this creature's species traits from the (frozen) parent ecosystem.
        const eco::SpeciesTraits* tr = nullptr;
        if (d.depth() > 0) {
            const ProcNode& ecop = d.path[static_cast<std::size_t>(d.depth() - 1)];
            if (ecop.kind == NodeKind::Ecosystem && d.live_seed == ecop.seed) {
                for (std::size_t k = 0; k < ecop.children.size() &&
                     k < d.live.community.species.size(); ++k)
                    if (ecop.children[k].seed == f.seed) { tr = &d.live.community.species[k].t; break; }
            }
        }
        if (!tr) return;
        const bio::Organism org = bio::derive_organism(*tr, tr->T_opt);
        const double M = std::pow(10.0, tr->m);

        // (1) Allometric scaling: metabolic rate vs body mass (Kleiber) with marker.
        Axes a;
        a.plot = plot_card(C0, "ALLOMETRY: BMR vs MASS", ui, WL::XENON_CORE);
        a.logx = a.logy = true;
        a.x0 = std::log10(1e-4); a.x1 = std::log10(1e5);
        a.y0 = std::log10(1e-3); a.y1 = std::log10(1e6);
        draw_grid(a, ui, "M [kg]", "BMR [W]");
        draw_function(a, [](double m) { return 3.4 * std::pow(m, 0.75); }, WL::CYAN_DIM); // Kleiber line
        draw_marker(a, M, org.bmr_w, WL::XENON_CORE, ui, "");

        // (2) von Bertalanffy growth curve L(age).
        Axes b;
        b.plot = plot_card(C1, "GROWTH (von BERTALANFFY)", ui, WL::PLASMA_GREEN);
        b.x0 = 0; b.x1 = std::max(1.0, org.lifespan_yr); b.y0 = 0; b.y1 = 1.05;
        draw_grid(b, ui, "age [yr]", "L/Linf");
        draw_function(b, [&](double age) { return 1.0 - std::exp(-org.growth_k * age); }, WL::PLASMA_GREEN);
        const float am = map_x(b, org.age_maturity_yr);
        DrawLineEx({am, b.plot.y}, {am, b.plot.y + b.plot.height}, 1.0f, with_alpha(WL::XENON_CORE, 130));
        draw_text("maturity", {am + 2.0f * ui, b.plot.y + 2.0f * ui}, 8.0f * ui, with_alpha(WL::XENON_CORE, 200));

        // (3) Demography: stable age structure (Lefkovitch) as a pyramid.
        Rectangle q = plot_card(C2, "DEMOGRAPHY (LEFKOVITCH)", ui, WL::CYAN_CORE);
        const demo::StageModel sm = demo::stage_model(*tr, org);
        draw_hbars(q, {static_cast<float>(sm.stable[2]), static_cast<float>(sm.stable[1]),
                       static_cast<float>(sm.stable[0])},
                   {"adult", "subadult", "juvenile"},
                   {WL::CYAN_CORE, {120, 200, 120, 255}, WL::PLASMA_GREEN}, ui, true);
        const std::string st = sm.survivorship > 2.3 ? "Type III" : (sm.survivorship < 1.5 ? "Type I" : "Type II");
        draw_text(st + " survivorship", {q.x + 8.0f * ui, q.y + q.height - 15.0f * ui}, 8.0f * ui,
                  with_alpha(WL::TEXT_TERTIARY, 220));
        return;
    }
    (void)t;
}

} // namespace cosmos_ui
