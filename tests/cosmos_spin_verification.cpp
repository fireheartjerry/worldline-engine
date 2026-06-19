// Verifies cosmos/SpinEntanglement.hpp: the Pauli algebra, gate unitarity, the
// Bloch sphere, two-qubit entanglement (entropy, concurrence, partial trace), and
// the CHSH/Bell inequality reaching the Tsirelson bound 2*sqrt(2).

#include "cosmos/SpinEntanglement.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace cosmos::spin;

namespace {
int g_failures = 0;
void check(bool cond, const std::string &what) {
    if (!cond) {
        std::cerr << "cosmos_spin_verification FAILED: " << what << "\n";
        ++g_failures;
    }
}
void close(double got, double want, double tol, const std::string &what) {
    if (std::abs(got - want) > tol) {
        std::cerr << "cosmos_spin_verification FAILED: " << what << " got=" << got
                  << " want=" << want << "\n";
        ++g_failures;
    }
}
bool mat_close(const Mat2 &A, const Mat2 &B, double tol) {
    for (int i = 0; i < 4; ++i)
        if (std::abs(A[i] - B[i]) > tol)
            return false;
    return true;
}
} // namespace

int main() {
    // --- Pauli algebra ------------------------------------------------------
    // sigma_i^2 = I.
    check(mat_close(mat_mul(pauli_x(), pauli_x()), identity2(), 1e-12), "sigma_x^2 = I");
    check(mat_close(mat_mul(pauli_y(), pauli_y()), identity2(), 1e-12), "sigma_y^2 = I");
    check(mat_close(mat_mul(pauli_z(), pauli_z()), identity2(), 1e-12), "sigma_z^2 = I");
    // [sigma_x, sigma_y] = 2 i sigma_z.
    const Mat2 comm = commutator(pauli_x(), pauli_y());
    const Mat2 want = scalar_mul(Complex(0, 2), pauli_z());
    check(mat_close(comm, want, 1e-12), "[sigma_x,sigma_y] = 2 i sigma_z");
    // Hermitian and traceless.
    check(is_hermitian(pauli_x()) && is_hermitian(pauli_y()) && is_hermitian(pauli_z()),
          "Pauli matrices Hermitian");
    close(trace(pauli_z()).real(), 0.0, 1e-12, "sigma_z traceless");

    // --- Gate unitarity -----------------------------------------------------
    check(is_unitary(hadamard()), "Hadamard unitary");
    check(is_unitary(pauli_x()), "X gate unitary");
    check(is_unitary(phase_gate(1.234)), "phase gate unitary");
    check(is_unitary(rotation_z(0.77)), "Rz unitary");
    // H^2 = I.
    check(mat_close(mat_mul(hadamard(), hadamard()), identity2(), 1e-12), "H^2 = I");

    // --- Single-qubit states & Bloch sphere ---------------------------------
    const Ket2 zero = {Complex(1, 0), Complex(0, 0)};
    const Ket2 plus = apply_gate(hadamard(), zero); // (|0>+|1>)/sqrt2
    close(norm2(plus), 1.0, 1e-12, "H|0> normalised");
    close(prob_zero(plus), 0.5, 1e-12, "H|0> measures 0 with p=1/2");
    // |0> points to +z; |+> points to +x.
    const auto bz = bloch_vector(zero);
    close(bz[2], 1.0, 1e-12, "|0> Bloch vector = +z");
    const auto bx = bloch_vector(plus);
    close(bx[0], 1.0, 1e-12, "|+> Bloch vector = +x");
    close(bx[2], 0.0, 1e-12, "|+> has no z-component");

    // --- Entanglement -------------------------------------------------------
    const Ket4 bell = bell_phi_plus();
    // Maximally entangled: entropy = ln 2, concurrence = 1, reduced state = I/2.
    close(entanglement_entropy(bell), std::log(2.0), 1e-12, "Bell entropy = ln 2");
    close(concurrence(bell), 1.0, 1e-12, "Bell concurrence = 1");
    const Mat2 rhoA = reduced_density_A(bell);
    close(rhoA[0].real(), 0.5, 1e-12, "Bell reduced rho_A = I/2 (diagonal)");
    close(std::abs(rhoA[1]), 0.0, 1e-12, "Bell reduced rho_A off-diagonal = 0");
    // The singlet is also maximally entangled.
    close(concurrence(bell_psi_minus()), 1.0, 1e-12, "singlet concurrence = 1");
    // A product state is separable: zero entropy, zero concurrence.
    const Ket4 prod = product_state(zero, plus);
    close(entanglement_entropy(prod), 0.0, 1e-12, "product state entropy = 0");
    close(concurrence(prod), 0.0, 1e-12, "product state concurrence = 0");

    // --- CHSH / Bell inequality ---------------------------------------------
    // Optimal angles a=0, a'=pi/2, b=pi/4, b'=3pi/4 reach the Tsirelson bound.
    const double pi = 3.14159265358979323846;
    const double S = chsh_S(0.0, pi / 2.0, pi / 4.0, 3.0 * pi / 4.0);
    close(std::abs(S), kTsirelsonBound, 1e-12, "CHSH saturates 2 sqrt(2)");
    check(std::abs(S) > kClassicalBound, "quantum CHSH beats the classical bound 2");
    close(kTsirelsonBound, 2.0 * std::sqrt(2.0), 1e-12, "Tsirelson = 2 sqrt(2)");
    // A classically-correlated choice of angles stays within the bound.
    check(std::abs(chsh_S(0.0, 0.0, 0.0, pi / 2.0)) <= kClassicalBound + 1e-12,
          "aligned analyzers respect the classical bound");

    // --- Determinism --------------------------------------------------------
    check(entanglement_entropy(bell) == entanglement_entropy(bell), "entropy deterministic");

    if (g_failures != 0) {
        std::cerr << "cosmos_spin_verification: " << g_failures << " failure(s)\n";
        return EXIT_FAILURE;
    }
    std::cout << "cosmos_spin_verification: all checks passed\n";
    return EXIT_SUCCESS;
}
