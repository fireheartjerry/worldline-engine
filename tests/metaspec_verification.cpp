#include "seed/MetaSpec.hpp"

#include "seed/Seeder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "metaspec_verification failed: " << message << '\n';
        std::exit(1);
    }
}

double sqr(double value) {
    return value * value;
}

bool finite(double value) {
    return std::isfinite(value);
}

double det2(const double matrix[2][2]) {
    return matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
}

double frob2(const double matrix[2][2]) {
    return std::sqrt(
        sqr(matrix[0][0]) +
        sqr(matrix[0][1]) +
        sqr(matrix[1][0]) +
        sqr(matrix[1][1]));
}

double norm2(const double vector[2]) {
    return std::sqrt(sqr(vector[0]) + sqr(vector[1]));
}

double median(std::vector<double> values) {
    require(!values.empty(), "median requires non-empty values");
    std::sort(values.begin(), values.end());
    const std::size_t mid = values.size() / 2u;
    if ((values.size() % 2u) == 0u) {
        return 0.5 * (values[mid - 1u] + values[mid]);
    }
    return values[mid];
}

bool symmetric2(const double matrix[2][2], double tolerance = 1.0e-12) {
    return std::abs(matrix[0][1] - matrix[1][0]) <= tolerance;
}

bool antisymmetric2_exact(const double matrix[2][2]) {
    return matrix[0][0] == 0.0
        && matrix[1][1] == 0.0
        && matrix[0][1] == -matrix[1][0];
}

std::uint64_t canonical_double_bits(double value) {
    if (value == 0.0) {
        return 0u;
    }

    std::uint64_t bits = 0u;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void append_bits(std::vector<std::uint64_t>& bits, const double matrix[2][2]) {
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            bits.push_back(canonical_double_bits(matrix[i][j]));
        }
    }
}

void append_bits(std::vector<std::uint64_t>& bits, const double tensor[2][2][2]) {
    for (int a = 0; a < 2; ++a) {
        append_bits(bits, tensor[a]);
    }
}

std::vector<std::uint64_t> meta_bits(const MetaSpec& ms) {
    std::vector<std::uint64_t> bits;
    bits.reserve(41u);
    append_bits(bits, ms.g);
    append_bits(bits, ms.V);
    append_bits(bits, ms.C);
    append_bits(bits, ms.S);
    append_bits(bits, ms.T);
    append_bits(bits, ms.G);
    append_bits(bits, ms.W);
    bits.push_back(canonical_double_bits(ms.p));
    bits.push_back(canonical_double_bits(ms.q0[0]));
    bits.push_back(canonical_double_bits(ms.q0[1]));
    bits.push_back(canonical_double_bits(ms.qdot0[0]));
    bits.push_back(canonical_double_bits(ms.qdot0[1]));
    return bits;
}

std::vector<double> flat_lanes(double value = 0.5) {
    return std::vector<double>(32u, value);
}

std::vector<double> aligned_nonzero_gyro_lanes() {
    std::vector<double> lanes = flat_lanes();
    lanes[0] = 1.0;
    lanes[1] = 0.0;
    lanes[2] = 0.30;
    lanes[3] = 1.0;
    lanes[4] = 0.0;
    lanes[5] = 0.30;
    lanes[9] = 1.0;
    lanes[10] = 0.0;
    lanes[11] = 0.0;
    lanes[15] = 1.0;
    lanes[16] = 0.0;
    lanes[17] = 0.25;
    return lanes;
}

std::vector<double> misaligned_nonzero_gyro_lanes() {
    std::vector<double> lanes = aligned_nonzero_gyro_lanes();
    lanes[5] = 0.50;
    lanes[29] = 1.0;
    return lanes;
}

std::vector<double> zero_gyro_lanes() {
    std::vector<double> lanes = flat_lanes();
    lanes[0] = 1.0;
    lanes[1] = 0.0;
    lanes[2] = 0.25;
    lanes[3] = 1.0;
    lanes[4] = 0.0;
    lanes[5] = 0.50;
    return lanes;
}

std::vector<double> strong_time_arrow_lanes() {
    std::vector<double> lanes = flat_lanes();
    lanes[21] = 1.0;
    lanes[22] = 0.0;
    lanes[23] = 0.00;
    lanes[24] = 1.0;
    lanes[25] = 0.0;
    lanes[26] = 0.25;
    return lanes;
}

void test_requires_32_lanes() {
    bool threw = false;
    try {
        generate_meta_spec(std::vector<double>(31u, 0.5));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    require(threw, "generate_meta_spec must reject lane vectors that are not length 32");
}

void test_seed_and_lane_determinism() {
    const std::vector<double> lanes = {
        0.41068849862530315, 0.45679622816347887, 0.45413201826992808, 0.86108233068689523,
        0.93400948633955236, 0.27661447718133058, 0.49907230856467468, 0.12215313469035055,
        0.90853125426403677, 0.81950849987640430, 0.02157909070287716, 0.86963685770788486,
        0.29877704701393888, 0.63064116363649211, 0.74623259658850543, 0.47643557586677554,
        0.35703869655201483, 0.13758714038568229, 0.11168591435979410, 0.30356848677779235,
        0.33676771342716734, 0.41552858218868921, 0.44350795734424855, 0.26172305888254928,
        0.14310363861466632, 0.68584895190206775, 0.33518480692096131, 0.75593647738963480,
        0.49739809121487576, 0.57039399388072501, 0.49403000961085519, 0.14329094735842365
    };

    const MetaSpec a = generate_meta_spec(lanes);
    const MetaSpec b = generate_meta_spec(lanes);
    require(meta_bits(a) == meta_bits(b),
            "lane-based generation must be bitwise deterministic");

    const MetaSpec from_seed_a = generate_meta_spec("hello");
    const MetaSpec from_seed_b = generate_meta_spec("hello");
    require(meta_bits(from_seed_a) == meta_bits(from_seed_b),
            "seed-based generation must be bitwise deterministic");
    require(meta_bits(from_seed_a) == meta_bits(generate_meta_spec(generate("hello"))),
            "seed wrapper must match the lane wrapper");

    require(generate_descriptor(from_seed_a) == generate_descriptor(from_seed_b),
            "descriptor generation must be deterministic");
}

void test_structural_invariants() {
    const MetaSpec ms = generate_meta_spec("determinism-check");

    require(finite(ms.p), "p must be finite");
    require(ms.g[0][0] > 0.0, "g(0,0) must be positive");
    require(det2(ms.g) > 0.0, "g must be positive definite");

    require(symmetric2(ms.g), "g must be symmetric");
    require(symmetric2(ms.V), "V must be symmetric");
    require(symmetric2(ms.S), "S must be symmetric");
    require(symmetric2(ms.W), "W must be symmetric");
    require(symmetric2(ms.C[0]), "C[0] must be symmetric in its last two indices");
    require(symmetric2(ms.C[1]), "C[1] must be symmetric in its last two indices");

    require(antisymmetric2_exact(ms.T), "T must be antisymmetric");
    require(antisymmetric2_exact(ms.G[0]), "G[0] must be antisymmetric");
    require(antisymmetric2_exact(ms.G[1]), "G[1] must be antisymmetric");

    require(ms.p >= -3.0 && ms.p <= 3.0,
            "p must stay inside [-3, 3]");

    const double q_norm = norm2(ms.q0);
    const double qdot_norm = norm2(ms.qdot0);
    require(finite(q_norm) && finite(qdot_norm),
            "initial conditions must be finite");
    require(q_norm >= 0.10 && q_norm <= 0.69 + 1.0e-12,
            "q0 norm must stay bounded and nontrivial");
    require(qdot_norm >= 0.10 && qdot_norm <= 1.40,
            "qdot0 norm must stay bounded and nontrivial");
}

void test_soft_symmetry_floor() {
    std::vector<double> lanes = flat_lanes();
    lanes[6] = 0.0;
    const MetaSpec ms = generate_meta_spec(lanes);
    require(frob2(ms.S) > 1.0e-4,
            "S should retain a weak but nonzero floor");
}

void test_w_subtle_from_alignment_and_zero_gyro() {
    const MetaSpec aligned = generate_meta_spec(aligned_nonzero_gyro_lanes());
    require(std::abs(aligned.G[0][0][1]) > 1.0e-6,
            "aligned test case should still generate nonzero gyroscopic content");
    require(frob2(aligned.W) > 1.0e-4,
            "aligned test case should still permit subtle W activity");

    const MetaSpec zero_gyro = generate_meta_spec(zero_gyro_lanes());
    require(frob2(zero_gyro.W) >= 0.0,
            "zero-gyro test case must still produce a finite W");
}

void test_w_nonzero_from_misalignment_and_gyro() {
    const MetaSpec ms = generate_meta_spec(misaligned_nonzero_gyro_lanes());
    require(std::abs(ms.G[0][0][1]) > 1.0e-6,
            "misaligned test case must produce nonzero gyroscopic content");
    require(frob2(ms.W) > 1.0e-6,
            "misaligned g and V with nonzero G must produce nonzero W");
}

void test_descriptor_clauses() {
    std::vector<double> zero_symmetry = flat_lanes();
    zero_symmetry[6] = 0.0;
    const std::string zero_symmetry_descriptor =
        generate_descriptor(generate_meta_spec(zero_symmetry));
    require(!zero_symmetry_descriptor.empty(),
            "descriptor must generate for weak-symmetry universes");
    require(zero_symmetry_descriptor.find("UNIVERSE CHARACTER") != std::string::npos,
            "descriptor must contain a universe character section");
    require(zero_symmetry_descriptor.find("PHYSICAL READING") != std::string::npos,
            "descriptor must contain a physical reading section");
    require(zero_symmetry_descriptor.find("CONSTRUCTION PATH") != std::string::npos,
            "descriptor must contain a construction path section");
    require(zero_symmetry_descriptor.find("GLOSSARY") != std::string::npos,
            "descriptor must contain a glossary section");

    const MetaSpec strong_time_arrow = generate_meta_spec(strong_time_arrow_lanes());
    require(std::abs(strong_time_arrow.T[0][1]) > 0.18,
            "strong time-arrow test case must generate a large T");
    const std::string time_descriptor = generate_descriptor(strong_time_arrow);
    require(time_descriptor.find("time-asymmetry term") != std::string::npos,
            "descriptor must surface strong time asymmetry");

    const MetaSpec high_contamination = generate_meta_spec(misaligned_nonzero_gyro_lanes());
    require(frob2(high_contamination.W) > 0.03,
            "high-contamination test case must generate a visible W");
    const std::string contamination_descriptor = generate_descriptor(high_contamination);
    require(contamination_descriptor.find("Warp contamination") != std::string::npos,
            "descriptor should discuss warp contamination explicitly");
}

void test_seed_regression_envelopes() {
    const MetaSpec hello = generate_meta_spec("hello");
    const MetaSpec det = generate_meta_spec("determinism-check");

    require(det2(hello.g) > 0.0 && det2(det.g) > 0.0,
            "regression seeds must keep g positive definite");
    require(frob2(hello.W) >= 0.015 && frob2(det.W) >= 0.015,
            "regression seeds should retain visible but subtle W activity");
    require(norm2(hello.q0) >= 0.16 && norm2(hello.q0) <= 0.69 + 1.0e-12,
            "hello q0 must stay within tuned radius band");
    require(norm2(det.q0) >= 0.16 && norm2(det.q0) <= 0.69 + 1.0e-12,
            "det q0 must stay within tuned radius band");
    require(norm2(hello.qdot0) >= 0.42 && norm2(hello.qdot0) <= 1.40,
            "hello qdot0 must stay within tuned speed band");
    require(norm2(det.qdot0) >= 0.42 && norm2(det.qdot0) <= 1.40,
            "det qdot0 must stay within tuned speed band");
}

void test_distribution_richness_corpus() {
    std::vector<double> w_norms;
    std::vector<double> c_norms;
    std::vector<double> s_norms;
    std::vector<double> tg_norms;
    std::vector<double> gv_norms;
    std::vector<double> descriptor_lengths;
    int near_zero_w = 0;
    int strong_combo_count = 0;
    int extreme_count = 0;

    for (int index = 0; index < 200; ++index) {
        const std::string seed = "richness-seed-" + std::to_string(index);
        const MetaSpec ms = generate_meta_spec(seed);
        const std::string descriptor = generate_descriptor(ms);

        const double w = frob2(ms.W);
        const double c = frob2(ms.C[0]) + frob2(ms.C[1]);
        const double s = frob2(ms.S);
        const double tg = std::abs(ms.T[0][1]) + std::abs(ms.G[0][0][1]) + std::abs(ms.G[1][0][1]);
        const double gv = frob2(ms.g) + frob2(ms.V);
        const double q = norm2(ms.q0);
        const double qd = norm2(ms.qdot0);
        const double extreme_score =
            0.32 * std::min(1.0, w / 0.24) +
            0.20 * std::min(1.0, tg / 0.42) +
            0.20 * std::min(1.0, c / 1.35) +
            0.14 * std::min(1.0, qd / 1.30) +
            0.14 * std::min(1.0, std::abs(ms.p) / 3.0);

        w_norms.push_back(w);
        c_norms.push_back(c);
        s_norms.push_back(s);
        tg_norms.push_back(tg);
        gv_norms.push_back(gv);
        descriptor_lengths.push_back(static_cast<double>(descriptor.size()));

        if (w < 0.02) {
            ++near_zero_w;
        }
        if (c + tg + w > 2.9) {
            ++strong_combo_count;
        }
        if (extreme_score > 0.40) {
            ++extreme_count;
        }

        require(det2(ms.g) > 0.0, "corpus g must remain positive definite");
        require(finite(ms.p), "corpus p must be finite");
        require(q >= 0.16 && q <= 0.69 + 1.0e-12, "corpus q0 norm must stay within tuned band");
        require(qd >= 0.42 && qd <= 1.40, "corpus qdot0 norm must stay within tuned band");
        require(descriptor.find("UNIVERSE CHARACTER") != std::string::npos, "corpus descriptor missing universe character section");
        require(descriptor.find("PHYSICAL READING") != std::string::npos, "corpus descriptor missing physical reading section");
        require(descriptor.find("CONSTRUCTION PATH") != std::string::npos, "corpus descriptor missing construction path section");
        require(descriptor.find("GLOSSARY") != std::string::npos, "corpus descriptor missing glossary section");
    }

    require(median(w_norms) > 0.02, "W median should rise above near-zero baseline");
    require(median(w_norms) < median(c_norms), "W median should stay below coupling median");
    require(near_zero_w < 60, "too many seeds still produce near-zero W");
    require(median(s_norms) > 0.03, "S should participate more often across the corpus");
    require(median(tg_norms) < median(gv_norms), "T/G should stay secondary to g/V on average");
    require(strong_combo_count < 30, "too many seeds produce unreadably large combined C+T/G+W");    require(extreme_count >= 1 && extreme_count <= 80, "extreme universes should appear rarely but nonzero");
    require(median(descriptor_lengths) > 2500.0, "descriptor should be substantially richer and more detailed");
}

} // namespace

int main() {
    test_requires_32_lanes();
    test_seed_and_lane_determinism();
    test_structural_invariants();
    test_soft_symmetry_floor();
    test_w_subtle_from_alignment_and_zero_gyro();
    test_w_nonzero_from_misalignment_and_gyro();
    test_descriptor_clauses();
    test_seed_regression_envelopes();
    test_distribution_richness_corpus();
    return 0;
}



