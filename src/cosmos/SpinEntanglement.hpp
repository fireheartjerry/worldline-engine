// SpinEntanglement.hpp -- the quantum mechanics of spin-1/2 and the quantum
// information that lives on it: Pauli algebra, single-qubit gates and their
// unitarity, two-qubit entanglement (Bell states, partial trace, von Neumann
// entropy, Wootters concurrence), and the CHSH/Bell inequality with its quantum
// Tsirelson bound 2*sqrt(2). This is the genuinely non-classical core of the
// first layer: superposition, measurement, and non-local correlation.
//
// Header-only, pure, deterministic. Complex linear algebra on fixed 2- and
// 4-dimensional state vectors (no allocation).
//
// Sources:
//   - Pauli matrices and the spin algebra [sigma_i, sigma_j] = 2 i eps_ijk sigma_k.
//   - Nielsen & Chuang, "Quantum Computation and Quantum Information" (gates,
//       density matrices, von Neumann entropy, partial trace).
//   - Wootters (1998), concurrence of a two-qubit pure state C = 2|ad - bc|.
//   - Clauser-Horne-Shimony-Holt (1969); Tsirelson (1980) bound S <= 2 sqrt(2).

#ifndef COSMOS_SPINENTANGLEMENT_HPP
#define COSMOS_SPINENTANGLEMENT_HPP

#include <array>
#include <cmath>
#include <complex>

namespace cosmos {
namespace spin {

using Complex = std::complex<double>;
using Mat2 = std::array<Complex, 4>; // row-major [a b; c d] -> {a,b,c,d}
using Ket2 = std::array<Complex, 2>; // single qubit
using Ket4 = std::array<Complex, 4>; // two qubits: |00>,|01>,|10>,|11>

inline constexpr Complex I_{0.0, 1.0};

// ---------------------------------------------------------------------------
// 2x2 complex matrix algebra
// ---------------------------------------------------------------------------

inline Mat2 identity2() {
    return {Complex(1, 0), 0, 0, Complex(1, 0)};
}
inline Mat2 pauli_x() {
    return {0, Complex(1, 0), Complex(1, 0), 0};
}
inline Mat2 pauli_y() {
    return {0, -I_, I_, 0};
}
inline Mat2 pauli_z() {
    return {Complex(1, 0), 0, 0, Complex(-1, 0)};
}

inline Mat2 mat_mul(const Mat2 &A, const Mat2 &B) {
    return {A[0] * B[0] + A[1] * B[2], A[0] * B[1] + A[1] * B[3], A[2] * B[0] + A[3] * B[2],
            A[2] * B[1] + A[3] * B[3]};
}
inline Mat2 mat_sub(const Mat2 &A, const Mat2 &B) {
    return {A[0] - B[0], A[1] - B[1], A[2] - B[2], A[3] - B[3]};
}
inline Mat2 scalar_mul(Complex s, const Mat2 &A) {
    return {s * A[0], s * A[1], s * A[2], s * A[3]};
}
inline Mat2 dagger(const Mat2 &A) {
    return {std::conj(A[0]), std::conj(A[2]), std::conj(A[1]), std::conj(A[3])};
}
inline Complex trace(const Mat2 &A) {
    return A[0] + A[3];
}
inline Mat2 commutator(const Mat2 &A, const Mat2 &B) {
    return mat_sub(mat_mul(A, B), mat_mul(B, A));
}

// Is U unitary (U U^dagger == I) to a tolerance?
inline bool is_unitary(const Mat2 &U, double tol = 1e-12) {
    const Mat2 p = mat_mul(U, dagger(U));
    const Mat2 id = identity2();
    for (int i = 0; i < 4; ++i)
        if (std::abs(p[i] - id[i]) > tol)
            return false;
    return true;
}

// Is M Hermitian (M == M^dagger)?
inline bool is_hermitian(const Mat2 &M, double tol = 1e-12) {
    const Mat2 d = dagger(M);
    for (int i = 0; i < 4; ++i)
        if (std::abs(M[i] - d[i]) > tol)
            return false;
    return true;
}

// ---------------------------------------------------------------------------
// Single-qubit gates
// ---------------------------------------------------------------------------

inline Mat2 hadamard() {
    const double s = 1.0 / std::sqrt(2.0);
    return {Complex(s, 0), Complex(s, 0), Complex(s, 0), Complex(-s, 0)};
}
inline Mat2 phase_gate(double phi) {
    return {Complex(1, 0), 0, 0, std::exp(I_ * phi)};
}
// Rotation about the Bloch z-axis by angle theta: diag(e^-i th/2, e^+i th/2).
inline Mat2 rotation_z(double theta) {
    return {std::exp(-I_ * (theta / 2.0)), 0, 0, std::exp(I_ * (theta / 2.0))};
}

inline Ket2 apply_gate(const Mat2 &U, const Ket2 &psi) {
    return {U[0] * psi[0] + U[1] * psi[1], U[2] * psi[0] + U[3] * psi[1]};
}

inline double norm2(const Ket2 &psi) {
    return std::norm(psi[0]) + std::norm(psi[1]);
}

// Probability of measuring |0> (i.e. |<0|psi>|^2).
inline double prob_zero(const Ket2 &psi) {
    return std::norm(psi[0]);
}

// Expectation value <psi|M|psi> of a Hermitian observable (returns the real part).
inline double expectation(const Mat2 &M, const Ket2 &psi) {
    const Ket2 mpsi = {M[0] * psi[0] + M[1] * psi[1], M[2] * psi[0] + M[3] * psi[1]};
    const Complex v = std::conj(psi[0]) * mpsi[0] + std::conj(psi[1]) * mpsi[1];
    return v.real();
}

// Bloch vector (<sigma_x>, <sigma_y>, <sigma_z>) of a single-qubit state.
inline std::array<double, 3> bloch_vector(const Ket2 &psi) {
    return {expectation(pauli_x(), psi), expectation(pauli_y(), psi), expectation(pauli_z(), psi)};
}

// ---------------------------------------------------------------------------
// Two-qubit entanglement
// ---------------------------------------------------------------------------

// The four maximally-entangled Bell states.
inline Ket4 bell_phi_plus() {
    const double s = 1.0 / std::sqrt(2.0);
    return {Complex(s, 0), 0, 0, Complex(s, 0)}; // (|00> + |11>)/sqrt2
}
inline Ket4 bell_psi_minus() {
    const double s = 1.0 / std::sqrt(2.0);
    return {0, Complex(s, 0), Complex(-s, 0), 0}; // (|01> - |10>)/sqrt2 (singlet)
}

// A separable product state |a> tensor |b>.
inline Ket4 product_state(const Ket2 &a, const Ket2 &b) {
    return {a[0] * b[0], a[0] * b[1], a[1] * b[0], a[1] * b[1]};
}

// Reduced density matrix of qubit A: rho_A = Tr_B(|psi><psi|).
inline Mat2 reduced_density_A(const Ket4 &c) {
    // index = 2*a + b. rho_A[i][j] = sum_b c_{i,b} conj(c_{j,b}).
    const Complex r00 = std::norm(c[0]) + std::norm(c[1]);
    const Complex r11 = std::norm(c[2]) + std::norm(c[3]);
    const Complex r01 = c[0] * std::conj(c[2]) + c[1] * std::conj(c[3]);
    const Complex r10 = std::conj(r01);
    return {r00, r01, r10, r11};
}

// Von Neumann entropy S = -Tr(rho ln rho) of a 2x2 density matrix, in nats.
// Uses the closed-form eigenvalues lambda = tr/2 +/- sqrt((tr/2)^2 - det).
inline double von_neumann_entropy(const Mat2 &rho) {
    const double tr = trace(rho).real();
    const double det = (rho[0] * rho[3] - rho[1] * rho[2]).real();
    double disc = (tr * tr) / 4.0 - det;
    if (disc < 0.0)
        disc = 0.0;
    const double root = std::sqrt(disc);
    const double l1 = tr / 2.0 + root;
    const double l2 = tr / 2.0 - root;
    auto term = [](double l) { return (l > 1e-15) ? -l * std::log(l) : 0.0; };
    return term(l1) + term(l2);
}

// Entanglement entropy of a two-qubit pure state = S(rho_A).
inline double entanglement_entropy(const Ket4 &psi) {
    return von_neumann_entropy(reduced_density_A(psi));
}

// Wootters concurrence of a two-qubit PURE state |psi> = a|00>+b|01>+c|10>+d|11>:
//   C = 2|ad - bc|, in [0,1]. 0 = separable, 1 = maximally entangled.
inline double concurrence(const Ket4 &s) {
    return 2.0 * std::abs(s[0] * s[3] - s[1] * s[2]);
}

// ---------------------------------------------------------------------------
// Bell / CHSH inequality
// ---------------------------------------------------------------------------

// Quantum spin correlation for the singlet at two analyzer angles: E = -cos(a-b).
inline double singlet_correlation(double angle_a, double angle_b) {
    return -std::cos(angle_a - angle_b);
}

// CHSH combination S = E(a,b) - E(a,b') + E(a',b) + E(a',b'). Local-hidden-
// variable theories obey |S| <= 2; quantum mechanics reaches |S| = 2 sqrt(2).
inline double chsh_S(double a, double ap, double b, double bp) {
    return singlet_correlation(a, b) - singlet_correlation(a, bp) + singlet_correlation(ap, b) +
           singlet_correlation(ap, bp);
}

inline constexpr double kClassicalBound = 2.0;
inline const double kTsirelsonBound = 2.0 * std::sqrt(2.0); // ~2.8284

} // namespace spin
} // namespace cosmos

#endif // COSMOS_SPINENTANGLEMENT_HPP
