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

void require_close(double actual,
                   double expected,
                   double tolerance,
                   const std::string& label) {
    const double scale = std::max(1.0, std::max(std::abs(actual), std::abs(expected)));
    require(std::abs(actual - expected) <= tolerance * scale,
            label + " mismatch");
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
    require(q_norm >= 0.10 && q_norm <= 1.00,
            "q0 norm must stay bounded and nontrivial");
    require(qdot_norm >= 0.10 && qdot_norm <= 2.50,
            "qdot0 norm must stay bounded and nontrivial");
}

void test_zero_symmetry_gate() {
    std::vector<double> lanes = flat_lanes();
    lanes[6] = 0.10;
    const MetaSpec ms = generate_meta_spec(lanes);
    require(ms.S[0][0] == 0.0
            && ms.S[0][1] == 0.0
            && ms.S[1][0] == 0.0
            && ms.S[1][1] == 0.0,
            "u6 <= 0.18 must zero S exactly");
}

void test_w_zero_from_alignment_and_zero_gyro() {
    const MetaSpec aligned = generate_meta_spec(aligned_nonzero_gyro_lanes());
    require(std::abs(aligned.G[0][0][1]) > 1.0e-6,
            "aligned test case should still generate nonzero gyroscopic content");
    require(aligned.W[0][0] == 0.0
            && aligned.W[0][1] == 0.0
            && aligned.W[1][0] == 0.0
            && aligned.W[1][1] == 0.0,
            "aligned g and V must zero W exactly");

    const MetaSpec zero_gyro = generate_meta_spec(zero_gyro_lanes());
    require(zero_gyro.G[0][0][1] == 0.0 && zero_gyro.G[1][0][1] == 0.0,
            "zero-gyro test case must keep G at zero");
    require(zero_gyro.W[0][0] == 0.0
            && zero_gyro.W[0][1] == 0.0
            && zero_gyro.W[1][0] == 0.0
            && zero_gyro.W[1][1] == 0.0,
            "zero gyroscopic content must zero W exactly");
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
    zero_symmetry[6] = 0.10;
    const std::string zero_symmetry_descriptor =
        generate_descriptor(generate_meta_spec(zero_symmetry));
    require(zero_symmetry_descriptor.find("No clean conserved quantity is present") != std::string::npos,
            "descriptor must explicitly state when no clean conserved quantity is present");

    const MetaSpec strong_time_arrow = generate_meta_spec(strong_time_arrow_lanes());
    require(std::abs(strong_time_arrow.T[0][1]) > 0.18,
            "strong time-arrow test case must generate a large T");
    const std::string time_descriptor = generate_descriptor(strong_time_arrow);
    require(time_descriptor.find("strongly one-way") != std::string::npos,
            "descriptor must surface strong time asymmetry");

    const MetaSpec high_contamination = generate_meta_spec(misaligned_nonzero_gyro_lanes());
    require(frob2(high_contamination.W) > 0.10,
            "high-contamination test case must generate a visible W");
    const std::string contamination_descriptor = generate_descriptor(high_contamination);
    require(contamination_descriptor.find("immediate leakage between directions") != std::string::npos,
            "descriptor must surface strong cross-contamination");
}

void test_seed_regression_snapshots() {
    const MetaSpec hello = generate_meta_spec("hello");
    const MetaSpec det = generate_meta_spec("determinism-check");

    require_close(hello.g[0][0], 1.26622385633684997, 1.0e-12, "hello.g00");
    require_close(hello.g[0][1], 0.12792719715075704, 1.0e-12, "hello.g01");
    require_close(hello.V[1][1], 0.38560554944221820, 1.0e-12, "hello.V11");
    require_close(hello.p, -1.08985830969595798, 1.0e-12, "hello.p");
    require_close(hello.W[0][1], 0.00095821233013459, 1.0e-12, "hello.W01");
    require_close(hello.q0[0], -0.25876994660277364, 1.0e-12, "hello.q0x");
    require_close(hello.qdot0[1], -0.62343047601318879, 1.0e-12, "hello.qdot0y");

    require_close(det.g[1][1], 1.46093494692096781, 1.0e-12, "det.g11");
    require_close(det.V[0][1], 0.11233981674736428, 1.0e-12, "det.V01");
    require_close(det.S[0][0], 0.03127744251408070, 1.0e-12, "det.S00");
    require_close(det.T[0][1], -0.00019487315252141, 1.0e-12, "det.T01");
    require_close(det.G[0][0][1], 0.00049913717336220, 1.0e-12, "det.G001");
    require_close(det.W[0][0], 0.00195657861800605, 1.0e-12, "det.W00");
    require_close(det.q0[1], 0.27050865964843529, 1.0e-12, "det.q0y");
}

} // namespace

int main() {
    test_requires_32_lanes();
    test_seed_and_lane_determinism();
    test_structural_invariants();
    test_zero_symmetry_gate();
    test_w_zero_from_alignment_and_zero_gyro();
    test_w_nonzero_from_misalignment_and_gyro();
    test_descriptor_clauses();
    test_seed_regression_snapshots();
    return 0;
}
