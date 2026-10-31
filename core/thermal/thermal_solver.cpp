#include "core/thermal/thermal_solver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace nexus::thermal {
namespace {

bool finite(double value) { return std::isfinite(value); }

std::vector<double> solve_linear_system(std::vector<std::vector<double>> matrix,
                                        std::vector<double> rhs) {
    const std::size_t n = rhs.size();
    for (std::size_t pivot = 0; pivot < n; ++pivot) {
        std::size_t pivot_row = pivot;
        for (std::size_t row = pivot + 1; row < n; ++row) {
            if (std::abs(matrix[row][pivot]) > std::abs(matrix[pivot_row][pivot])) {
                pivot_row = row;
            }
        }
        if (std::abs(matrix[pivot_row][pivot]) <= std::numeric_limits<double>::epsilon()) {
            throw std::runtime_error("Thermal stiffness matrix is singular");
        }
        std::swap(matrix[pivot], matrix[pivot_row]);
        std::swap(rhs[pivot], rhs[pivot_row]);

        for (std::size_t row = pivot + 1; row < n; ++row) {
            const double factor = matrix[row][pivot] / matrix[pivot][pivot];
            for (std::size_t column = pivot; column < n; ++column) {
                matrix[row][column] -= factor * matrix[pivot][column];
            }
            rhs[row] -= factor * rhs[pivot];
        }
    }

    std::vector<double> solution(n, 0.0);
    for (std::size_t row = n; row-- > 0;) {
        double value = rhs[row];
        for (std::size_t column = row + 1; column < n; ++column) {
            value -= matrix[row][column] * solution[column];
        }
        solution[row] = value / matrix[row][row];
    }
    return solution;
}

void apply_prescribed_temperature(std::vector<std::vector<double>>& stiffness,
                                  std::vector<double>& load,
                                  std::size_t node,
                                  double temperature_C) {
    for (std::size_t row = 0; row < load.size(); ++row) {
        if (row != node) load[row] -= stiffness[row][node] * temperature_C;
        if (row != node) stiffness[row][node] = 0.0;
    }
    for (std::size_t column = 0; column < load.size(); ++column) {
        if (column != node) stiffness[node][column] = 0.0;
    }
    stiffness[node][node] = 1.0;
    load[node] = temperature_C;
}

} // namespace

void validate_parameters(const ThermalParameters& p, std::size_t element_count) {
    if (element_count == 0 || element_count > 10000) {
        throw std::invalid_argument("element_count must be between 1 and 10000");
    }
    if (!finite(p.length_m) || p.length_m <= 0.0) throw std::invalid_argument("length_m must be finite and positive");
    if (!finite(p.area_m2) || p.area_m2 <= 0.0) throw std::invalid_argument("area_m2 must be finite and positive");
    if (!finite(p.conductivity_W_mK) || p.conductivity_W_mK <= 0.0) throw std::invalid_argument("conductivity_W_mK must be finite and positive");
    if (!finite(p.heat_generation_W_m3)) throw std::invalid_argument("heat_generation_W_m3 must be finite");
    if (!finite(p.left_temperature_C) || !finite(p.right_temperature_C)) throw std::invalid_argument("boundary temperatures must be finite");
}

double analytical_temperature(const ThermalParameters& p, double x_m) {
    return p.left_temperature_C + (p.right_temperature_C - p.left_temperature_C) * x_m / p.length_m
        + (p.heat_generation_W_m3 / (2.0 * p.conductivity_W_mK)) * x_m * (p.length_m - x_m);
}

ThermalResult solve(const ThermalParameters& p, std::size_t element_count) {
    validate_parameters(p, element_count);
    const std::size_t node_count = element_count + 1;
    const double element_length = p.length_m / static_cast<double>(element_count);
    const double stiffness_factor = p.conductivity_W_mK * p.area_m2 / element_length;
    const double load_value = p.heat_generation_W_m3 * p.area_m2 * element_length / 2.0;

    std::vector<std::vector<double>> stiffness(node_count, std::vector<double>(node_count, 0.0));
    std::vector<double> load(node_count, 0.0);
    for (std::size_t element = 0; element < element_count; ++element) {
        const std::size_t left = element;
        const std::size_t right = element + 1;
        stiffness[left][left] += stiffness_factor;
        stiffness[left][right] -= stiffness_factor;
        stiffness[right][left] -= stiffness_factor;
        stiffness[right][right] += stiffness_factor;
        load[left] += load_value;
        load[right] += load_value;
    }

    apply_prescribed_temperature(stiffness, load, 0, p.left_temperature_C);
    apply_prescribed_temperature(stiffness, load, node_count - 1, p.right_temperature_C);
    const auto nodal_temperatures = solve_linear_system(std::move(stiffness), std::move(load));

    ThermalResult result;
    result.parameters = p;
    result.element_count = element_count;
    result.nodal_positions_m.resize(node_count);
    result.nodal_temperatures_C = nodal_temperatures;
    result.analytical_nodal_temperatures_C.resize(node_count);
    result.midpoint_positions_m.resize(element_count);
    result.fem_midpoint_temperatures_C.resize(element_count);
    result.analytical_midpoint_temperatures_C.resize(element_count);
    result.midpoint_absolute_errors_C.resize(element_count);
    for (std::size_t node = 0; node < node_count; ++node) {
        const double x = static_cast<double>(node) * element_length;
        result.nodal_positions_m[node] = x;
        result.analytical_nodal_temperatures_C[node] = analytical_temperature(p, x);
    }
    for (std::size_t element = 0; element < element_count; ++element) {
        const double midpoint = (result.nodal_positions_m[element] + result.nodal_positions_m[element + 1]) / 2.0;
        const double fem_midpoint = (result.nodal_temperatures_C[element] + result.nodal_temperatures_C[element + 1]) / 2.0;
        const double exact_midpoint = analytical_temperature(p, midpoint);
        const double error = std::abs(fem_midpoint - exact_midpoint);
        result.midpoint_positions_m[element] = midpoint;
        result.fem_midpoint_temperatures_C[element] = fem_midpoint;
        result.analytical_midpoint_temperatures_C[element] = exact_midpoint;
        result.midpoint_absolute_errors_C[element] = error;
        result.maximum_absolute_midpoint_error_C = std::max(result.maximum_absolute_midpoint_error_C, error);
    }
    return result;
}

} // namespace nexus::thermal
