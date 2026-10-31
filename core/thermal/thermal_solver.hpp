#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace nexus::thermal {

struct ThermalParameters {
    double length_m = 1.0;
    double area_m2 = 1e-4;
    double conductivity_W_mK = 10.0;
    double heat_generation_W_m3 = 10000.0;
    double left_temperature_C = 100.0;
    double right_temperature_C = 20.0;
};

struct ThermalResult {
    ThermalParameters parameters;
    std::size_t element_count = 0;
    std::vector<double> nodal_positions_m;
    std::vector<double> nodal_temperatures_C;
    std::vector<double> analytical_nodal_temperatures_C;
    std::vector<double> midpoint_positions_m;
    std::vector<double> fem_midpoint_temperatures_C;
    std::vector<double> analytical_midpoint_temperatures_C;
    std::vector<double> midpoint_absolute_errors_C;
    double maximum_absolute_midpoint_error_C = 0.0;
};

void validate_parameters(const ThermalParameters& parameters, std::size_t element_count);
double analytical_temperature(const ThermalParameters& parameters, double x_m);
ThermalResult solve(const ThermalParameters& parameters, std::size_t element_count);

} // namespace nexus::thermal
