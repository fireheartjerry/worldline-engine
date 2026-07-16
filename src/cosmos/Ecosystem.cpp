#include "cosmos/Ecosystem.hpp"

#include "cosmos/Astrobio.hpp"
#include "cosmos/Interactions.hpp"
#include "cosmos/Phonology.hpp"
#include "cosmos/Phylogeny.hpp"
#include "cosmos/Spectrum.hpp"

#include <algorithm>
#include <cmath>

namespace cosmos {
namespace eco {

namespace {

double sq(double x) { return x * x; }

Color8 hsv8(double h, double s, double v) {
    h -= std::floor(h);
    const double i = std::floor(h * 6.0);
    const double f = h * 6.0 - i;
    const double p = v * (1.0 - s), q = v * (1.0 - f * s), t = v * (1.0 - (1.0 - f) * s);
    double r = v, g = t, b = p;
    switch (static_cast<int>(i) % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return {static_cast<unsigned char>(r * 255.0), static_cast<unsigned char>(g * 255.0),
            static_cast<unsigned char>(b * 255.0)};
}

constexpr double kB0 = 0.05; // metabolic scale constant (sim units)

} // namespace

const char* role_label(double tau) {
    if (tau < 0.5) return "producer";
    if (tau < 1.5) return "herbivore";
    if (tau < 2.4) return "omnivore";
    return "carnivore";
}

double role_hue(double tau) {
    // green (producers) -> teal/blue (herbivores) -> red (apex), wrapping hue 0.
    const double t = clampd(tau / 3.0, 0.0, 1.0);
    return (t < 0.5) ? lerp(0.33, 0.58, t / 0.5) : lerp(0.58, 1.0, (t - 0.5) / 0.5);
}

Community generate_community(std::uint64_t seed, const BiomeParams& biome,
                            const phon::Language* lang, const phylo::CladePool* pool) {
    Community C;
    C.biome = biome;
    Stream rng(seed, 0xEC051Bull);

    const double npp = std::max(1.0, biome.npp);
    const double Rf = astro::richness_factor(npp);
    const int S_max = std::clamp(static_cast<int>(std::lround(6 + 34 * Rf)), 4, 40);
    const double L_max = std::clamp(2.0 + 1.3 * std::log2(npp / 90.0), 2.0, 5.0);
    const double eff = biome.aquatic ? 0.16 : 0.10;   // Lindeman; aquatic higher
    const double ppmr_lo = biome.aquatic ? 1.5 : 1.0; // log10 predator/prey mass band
    const double ppmr_hi = biome.aquatic ? 2.5 : 3.0;

    std::vector<Species>& sp = C.species;
    sp.reserve(static_cast<std::size_t>(S_max));

    // ── 1. Producers (autotrophs, tau≈0), spaced in body mass (Hutchinson) ────
    const int n_prod = std::clamp(static_cast<int>(std::lround(1 + 5 * Rf)), 1, 8);
    double pm = -5.0 + rng.range(0.0, 0.5);
    for (int i = 0; i < n_prod; ++i) {
        Species s;
        s.t.tau = 0.001 * i;
        s.t.m = std::min(-0.5, pm);
        pm += 0.35 + rng.range(0.0, 1.1);
        s.t.T_opt = rng.gaussian(biome.temp_c, 6.0);
        s.t.T_tol = rng.log_uniform(4.0, 14.0);
        s.t.omega = rng.log_uniform(0.3, 1.0);
        s.t.kappa = std::exp(rng.gaussian(0.0, 0.25));
        s.t.rho = clampd(rng.beta_like(0.75, 0.4), 0.0, 1.0); // producers r-ish
        s.t.phi = rng.unit();
        s.t.defense = rng.range(0.1, 0.5);
        sp.push_back(s);
    }
    C.stats.n_producers = n_prod;

    // ── 2. Radiate consumers into the niche space ─────────────────────────────
    int guard = 0;
    while (static_cast<int>(sp.size()) < S_max && guard++ < S_max * 10) {
        const Species& parent = sp[static_cast<std::size_t>(rng.unit() * sp.size())];
        const double dtau = rng.range(0.5, 1.3);
        const double tau = parent.t.tau + dtau;
        if (tau > L_max + 0.3) continue;

        SpeciesTraits t = parent.t;
        t.tau = tau;
        t.dp = rng.range(ppmr_lo, ppmr_hi);
        t.m = clampd(parent.t.m + t.dp + rng.gaussian(0.0, 0.3), -6.0, 5.0);
        t.omega = rng.log_uniform(0.4, 1.1);
        t.T_opt = parent.t.T_opt + rng.gaussian(0.0, 2.5);
        t.T_tol = rng.log_uniform(4.0, 12.0);
        t.kappa = std::exp(rng.gaussian(0.0, 0.25));
        t.rho = clampd(0.55 - 0.10 * t.m + rng.gaussian(0.0, 0.15), 0.0, 1.0); // small -> r
        t.phi = (rng.unit() < 0.7) ? parent.t.phi + rng.gaussian(0.0, 0.05) : rng.unit();
        t.phi -= std::floor(t.phi);
        t.defense = clampd(rng.beta_like(0.4, 0.3), 0.0, 1.0);

        // Hutchinson spacing vs. same-level, same-climate, same-diel competitors.
        bool spaced = true;
        for (const Species& o : sp) {
            if (std::abs(o.t.tau - t.tau) < 0.5 &&
                std::abs(o.t.T_opt - t.T_opt) < (o.t.T_tol + t.T_tol) * 0.6 &&
                std::abs(o.t.phi - t.phi) < 0.2 && std::abs(o.t.m - t.m) < 0.30) {
                spaced = false; break;
            }
        }
        if (!spaced) t.m += rng.sign() * 0.4; // repair: shift to the nearest free side

        // Prey availability within the size window; else become a detritivore.
        bool has_prey = false;
        for (const Species& o : sp) {
            const double dm = t.m - o.t.m;
            if (dm > 0.2 && dm < t.dp + 2.0) { has_prey = true; break; }
        }
        Species s;
        s.t = t;
        if (!has_prey) { s.detritivore = true; s.t.tau = 1.0 + 0.2 * rng.unit(); }
        sp.push_back(s);
    }

    const int S = static_cast<int>(sp.size());
    for (Species& s : sp) s.mass_kg = std::pow(10.0, s.t.m);

    // ── 3. Food-web wiring: continuous size-ratio x niche kernel ──────────────
    // Flat S*S preference matrix (one allocation, cache-friendly), indexed i*S+j.
    std::vector<double> pref(static_cast<std::size_t>(S) * static_cast<std::size_t>(S), 0.0);
    const auto PF = [&](int i, int j) -> double& {
        return pref[static_cast<std::size_t>(i) * static_cast<std::size_t>(S) + static_cast<std::size_t>(j)];
    };
    for (int i = 0; i < S; ++i) {
        if (sp[static_cast<std::size_t>(i)].t.tau < 0.5 || sp[static_cast<std::size_t>(i)].detritivore)
            continue; // producers/detritivores are not predators
        const SpeciesTraits& pi = sp[static_cast<std::size_t>(i)].t;
        double total = 0.0;
        for (int j = 0; j < S; ++j) {
            if (i == j) continue;
            const SpeciesTraits& pj = sp[static_cast<std::size_t>(j)].t;
            const double dm = pi.m - pj.m;
            if (dm <= 0.1) continue; // predator must be larger
            const double size = std::exp(-0.5 * sq((dm - pi.dp) / std::max(0.2, pi.omega)));
            const double clim = std::exp(-0.5 * sq((pi.T_opt - pj.T_opt) / (pi.T_tol + pj.T_tol)));
            const double diel = 0.5 + 0.5 * std::cos(kTwoPi * (pi.phi - pj.phi));
            const double defp = std::pow(1.0 - pj.defense, 1.0 + 2.0 * pi.rho);
            const double a = size * clim * diel * defp;
            if (a > 0.02) { PF(i, j) = a; total += a; }
        }
        if (total > 0.0)
            for (int j = 0; j < S; ++j) PF(i, j) /= total;
    }

    // ── 4. Realize continuous trophic level from diet (emergent omnivory) ─────
    for (int iter = 0; iter < 5; ++iter) {
        std::vector<double> nt(static_cast<std::size_t>(S), 0.0);
        for (int i = 0; i < S; ++i) {
            if (sp[static_cast<std::size_t>(i)].t.tau < 0.5) { nt[static_cast<std::size_t>(i)] = 0.0; continue; }
            if (sp[static_cast<std::size_t>(i)].detritivore) { nt[static_cast<std::size_t>(i)] = 1.0; continue; }
            double acc = 0.0, w = 0.0;
            for (int j = 0; j < S; ++j) {
                const double p = PF(i, j);
                if (p > 0.0) { acc += p * sp[static_cast<std::size_t>(j)].t.tau; w += p; }
            }
            nt[static_cast<std::size_t>(i)] = (w > 0.0) ? 1.0 + acc : 1.0;
        }
        for (int i = 0; i < S; ++i) sp[static_cast<std::size_t>(i)].t.tau = nt[static_cast<std::size_t>(i)];
    }

    // ── 5. Energy flux (top-down from NPP) + biomass (Kleiber/Damuth) ─────────
    // Producers share NPP by climate fitness; the detritus pool feeds detritivores.
    double fit_sum = 0.0;
    std::vector<double> fit(static_cast<std::size_t>(S), 0.0);
    for (int i = 0; i < S; ++i) {
        if (sp[static_cast<std::size_t>(i)].t.tau < 0.5) {
            fit[static_cast<std::size_t>(i)] =
                std::exp(-0.5 * sq((sp[static_cast<std::size_t>(i)].t.T_opt - biome.temp_c) /
                                   std::max(2.0, sp[static_cast<std::size_t>(i)].t.T_tol)));
            fit_sum += fit[static_cast<std::size_t>(i)];
        }
    }
    const double detritus = npp * 0.3;
    int n_detr = 0;
    for (const Species& s : sp) if (s.detritivore) ++n_detr;
    // Order species by trophic level so suppliers are solved before consumers.
    std::vector<int> order(static_cast<std::size_t>(S));
    for (int i = 0; i < S; ++i) order[static_cast<std::size_t>(i)] = i;
    // stable_sort: equal trophic levels are common (all producers sit at the
    // same tau) and the flux solve below consumes this order, so tie order must
    // not depend on the STL implementation (determinism across platforms).
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return sp[static_cast<std::size_t>(a)].t.tau < sp[static_cast<std::size_t>(b)].t.tau;
    });
    // Total incoming preference on each prey (to split its flux among predators).
    std::vector<double> pressure(static_cast<std::size_t>(S), 0.0);
    for (int i = 0; i < S; ++i)
        for (int j = 0; j < S; ++j) pressure[static_cast<std::size_t>(j)] += PF(i, j);

    for (int oi = 0; oi < S; ++oi) {
        const int i = order[static_cast<std::size_t>(oi)];
        Species& s = sp[static_cast<std::size_t>(i)];
        if (s.t.tau < 0.5) {
            s.flux = (fit_sum > 0.0) ? npp * fit[static_cast<std::size_t>(i)] / fit_sum : npp / std::max(1, S);
        } else if (s.detritivore) {
            s.flux = eff * detritus / std::max(1, n_detr);
        } else {
            double f = 0.0;
            for (int j = 0; j < S; ++j) {
                const double p = PF(i, j);
                if (p > 0.0 && pressure[static_cast<std::size_t>(j)] > 0.0) {
                    f += (p / pressure[static_cast<std::size_t>(j)]) * sp[static_cast<std::size_t>(j)].flux;
                }
            }
            s.flux = eff * f;
        }
        const double Bind = s.t.kappa * kB0 * std::pow(s.mass_kg, astro::kKleiberExp); // per-individual
        const double turnover = 0.5 + s.t.rho;
        s.density = (Bind > 0.0) ? std::max(0.0, s.flux) / (Bind * turnover) : 0.0; // Damuth emerges
        s.biomass = s.density * s.mass_kg;
        s.population = s.density * biome.area_m2;
        s.xeq = std::max(1.0e-9, s.biomass);
        s.x = s.xeq;
    }

    // ── 6. Links list + interaction strengths + May stability margin ──────────
    // Predators iterate in order (outer i), so the link list is already grouped
    // by predator — exactly the order CSR needs (counting-sort is implicit).
    double mean = 0.0, mean2 = 0.0;
    int L = 0;
    C.links.reserve(static_cast<std::size_t>(2 * S));
    C.csr_off.assign(static_cast<std::size_t>(S) + 1, 0);
    for (int i = 0; i < S; ++i) {
        C.csr_off[static_cast<std::size_t>(i)] = L;
        for (int j = 0; j < S; ++j) {
            const double p = PF(i, j);
            if (p <= 0.0) continue;
            Link lk;
            lk.pred = i; lk.prey = j; lk.pref = p;
            lk.alpha = p * std::pow(sp[static_cast<std::size_t>(i)].mass_kg, -0.25);
            C.links.push_back(lk);
            C.csr_prey.push_back(j);
            C.csr_alpha.push_back(lk.alpha);
            mean += lk.alpha; mean2 += lk.alpha * lk.alpha; ++L;
        }
    }
    C.csr_off[static_cast<std::size_t>(S)] = L;
    // Precompute 1/max(eps, xeq[prey]) per edge for the hot step() loop.
    C.csr_inv_xeq.resize(static_cast<std::size_t>(L));
    for (int eidx = 0; eidx < L; ++eidx) {
        const int prey = C.csr_prey[static_cast<std::size_t>(eidx)];
        C.csr_inv_xeq[static_cast<std::size_t>(eidx)] =
            1.0 / std::max(1.0e-9, sp[static_cast<std::size_t>(prey)].xeq);
    }
    C.dx_scratch.assign(static_cast<std::size_t>(S), 0.0);
    const double Cc = (S > 0) ? static_cast<double>(L) / (static_cast<double>(S) * S) : 0.0;
    double sigma = 0.0;
    if (L > 1) { mean /= L; sigma = std::sqrt(std::max(0.0, mean2 / L - mean * mean)); }
    C.stats.stability_margin = 1.0 - sigma * std::sqrt(static_cast<double>(S) * Cc);

    // ── 7. Names + continuous role colors ─────────────────────────────────────
    // Lineage language (so creatures sound like their world) or a seed-derived
    // fallback for direct callers. If a phylogeny pool is supplied, each species
    // is assigned to a clade tip and named from that clade's sub-language, so a
    // planet's biota splits into related clades (sister species sound alike). The
    // per-species mix2 keeps intra-clade variety. Trait-relatedness is layered on
    // separately; here we realize the clade-coherent naming structure.
    const phon::Language base = lang ? *lang : phon::make_language(seed ^ 0x5EC0DEull);
    const bool use_pool = (pool != nullptr && pool->n_tips > 0 &&
                           !pool->lang.empty() && !pool->tip_node.empty());
    for (int i = 0; i < S; ++i) {
        Species& s = sp[static_cast<std::size_t>(i)];
        const phon::Language* nl = &base;
        if (use_pool) {
            const int tip = i % pool->n_tips;
            const int node = pool->tip_node[static_cast<std::size_t>(tip)];
            if (node >= 0 && node < static_cast<int>(pool->lang.size()))
                nl = &pool->lang[static_cast<std::size_t>(node)];
        }
        s.name = phon::generate_name(*nl, mix2(seed, static_cast<std::uint64_t>(i) + 1), 1.2f, 0.3f);
        s.color = hsv8(role_hue(s.t.tau), 0.5 + 0.4 * s.t.defense, 0.92);
    }

    // ── 8. Stats (richness, levels, connectance, biomass, keystone) ───────────
    C.stats.n_species = S;
    C.stats.n_links = L;
    C.stats.connectance = Cc;
    double maxtau = 0.0, taus = 0.0;
    int nc = 0;
    double tot_bio = 0.0;
    for (const Species& s : sp) {
        maxtau = std::max(maxtau, s.t.tau);
        if (s.t.tau >= 0.5) { taus += s.t.tau; ++nc; }
        tot_bio += s.biomass;
    }
    C.stats.n_levels = maxtau;
    C.stats.mean_chain = (nc > 0) ? taus / nc : 0.0;
    C.stats.total_biomass = tot_bio;
    // Keystone = species whose removal severs the most (weighted) diet dependence.
    double best = -1.0;
    for (int j = 0; j < S; ++j) {
        double impact = 0.0;
        for (int i = 0; i < S; ++i)
            impact += PF(i, j) * sp[static_cast<std::size_t>(i)].biomass;
        impact /= (sp[static_cast<std::size_t>(j)].biomass + 1.0e-12); // outsized effect vs own biomass
        if (impact > best) { best = impact; C.stats.keystone = j; }
    }

    // ── 9. Signed-interaction structure + nutrient pools (beyond predation) ────
    // Classify every species pair into the broader community matrix (competition,
    // mutualism, ...) so the ecosystem reads as a real interaction web, not just a
    // food chain. These are weak relative to predation (kept so for May stability).
    int n_comp = 0, n_mut = 0;
    for (int i = 0; i < S; ++i)
        for (int j = i + 1; j < S; ++j) {
            const interact::PairEffect pe =
                interact::classify(sp[static_cast<std::size_t>(i)].t, sp[static_cast<std::size_t>(j)].t);
            if (pe.type == interact::Interaction::Competition) ++n_comp;
            else if (pe.type == interact::Interaction::Mutualism) ++n_mut;
        }
    C.stats.n_competition = n_comp;
    C.stats.n_mutualism = n_mut;
    // Standing nutrient stocks: carbon ~50% of dry biomass; nitrogen via a C:N
    // ratio (terrestrial ~11, aquatic closer to Redfield ~6.6).
    C.stats.carbon = 0.5 * tot_bio;
    C.stats.nitrogen = C.stats.carbon / (biome.aquatic ? 6.6 : 11.0);
    return C;
}

void step_community(Community& C, double dt, double season_factor) {
    const int S = static_cast<int>(C.species.size());
    if (S == 0) return;
    dt = std::min(dt, 0.05);
    season_factor = std::min(2.0, std::max(0.1, season_factor)); // bounded forcing
    // Reuse the per-community scratch buffer (no per-frame heap allocation).
    if (static_cast<int>(C.dx_scratch.size()) != S) C.dx_scratch.assign(static_cast<std::size_t>(S), 0.0);
    std::vector<double>& dx = C.dx_scratch;
    std::fill(dx.begin(), dx.end(), 0.0);

    for (int i = 0; i < S; ++i) {
        Species& s = C.species[static_cast<std::size_t>(i)];
        const double r = 0.6 * std::pow(std::max(1.0e-6, s.mass_kg), -0.25); // allometric rate
        if (s.t.tau < 0.5) {
            // Logistic producer: grows toward a seasonally-forced carrying capacity.
            const double K = std::max(1.0e-9, s.xeq * season_factor);
            dx[static_cast<std::size_t>(i)] += r * s.x * (1.0 - s.x / K);
        } else {
            // Consumer: damped toward equilibrium with self-limitation (stable).
            dx[static_cast<std::size_t>(i)] += r * s.x * (1.0 - s.x / std::max(1.0e-9, s.xeq))
                                               - 0.05 * r * (s.x - s.xeq);
        }
    }
    // Predation coupling (CSR walk) produces the predator-lags-prey breathing,
    // bounded by the self-limitation above. Numerically identical to iterating
    // the link list; csr_inv_xeq precomputes 1/max(eps, xeq[prey]).
    if (!C.csr_off.empty()) {
        for (int pred = 0; pred < S; ++pred) {
            const double xp = C.species[static_cast<std::size_t>(pred)].x;
            const int e0 = C.csr_off[static_cast<std::size_t>(pred)];
            const int e1 = C.csr_off[static_cast<std::size_t>(pred) + 1];
            for (int e = e0; e < e1; ++e) {
                const int prey = C.csr_prey[static_cast<std::size_t>(e)];
                const double flow = 0.15 * C.csr_alpha[static_cast<std::size_t>(e)] * xp *
                                    C.species[static_cast<std::size_t>(prey)].x *
                                    C.csr_inv_xeq[static_cast<std::size_t>(e)];
                dx[static_cast<std::size_t>(prey)] -= flow;
                dx[static_cast<std::size_t>(pred)] += 0.1 * flow;
            }
        }
    }
    // Robust, positivity-preserving update: never propagate NaN/inf, and confine
    // every population to a bounded basin [1e-6*xeq, 8*xeq] so stiff predator-prey
    // pairs can oscillate but can neither blow up nor flicker to zero.
    for (int i = 0; i < S; ++i) {
        Species& s = C.species[static_cast<std::size_t>(i)];
        double xn = s.x + dx[static_cast<std::size_t>(i)] * dt;
        if (!std::isfinite(xn)) xn = s.xeq;
        const double floor = 1.0e-6 * s.xeq;
        const double ceil = 8.0 * s.xeq;
        s.x = std::min(ceil, std::max(floor, xn));
    }
}

} // namespace eco
} // namespace cosmos
