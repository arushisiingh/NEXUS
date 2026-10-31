#include "core/thermal/thermal_solver.hpp"

#include <filesystem>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

using nexus::thermal::ThermalResult;

namespace {
void write_result_csv(const ThermalResult& result, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "x_m,fem_temperature_C,analytical_temperature_C\n" << std::setprecision(17);
    for (std::size_t i = 0; i < result.nodal_positions_m.size(); ++i) {
        out << result.nodal_positions_m[i] << ',' << result.nodal_temperatures_C[i] << ','
            << result.analytical_nodal_temperatures_C[i] << '\n';
    }
}

void write_comparison_csv(const ThermalResult& result, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "midpoint_x_m,fem_temperature_C,analytical_temperature_C,absolute_error_C\n" << std::setprecision(17);
    for (std::size_t i = 0; i < result.midpoint_positions_m.size(); ++i) {
        out << result.midpoint_positions_m[i] << ',' << result.fem_midpoint_temperatures_C[i] << ','
            << result.analytical_midpoint_temperatures_C[i] << ',' << result.midpoint_absolute_errors_C[i] << '\n';
    }
}

void write_convergence_csv(const std::vector<ThermalResult>& results, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "elements,element_length_m,maximum_midpoint_error_C\n" << std::setprecision(17);
    for (const auto& result : results) {
        out << result.element_count << ',' << result.parameters.length_m / result.element_count << ','
            << result.maximum_absolute_midpoint_error_C << '\n';
    }
}

void write_svg(const std::vector<ThermalResult>& results, const std::filesystem::path& path) {
    const auto& result = results.front();
    std::ofstream out(path);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"900\" height=\"430\" viewBox=\"0 0 900 430\">"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#07101a\"/><text x=\"45\" y=\"35\" fill=\"#e6eef6\" font-family=\"sans-serif\" font-size=\"20\">NEXUS Thermal Studio — FEM vs analytical</text>"
        << "<line x1=\"70\" y1=\"370\" x2=\"850\" y2=\"370\" stroke=\"#78909c\"/><line x1=\"70\" y1=\"70\" x2=\"70\" y2=\"370\" stroke=\"#78909c\"/>";
    auto x = [](double value) { return 70.0 + value * 780.0; };
    auto y = [](double value) { return 370.0 - (value - 10.0) / 100.0 * 300.0; };
    out << "<polyline fill=\"none\" stroke=\"#59c3df\" stroke-width=\"3\" points=\"";
    for (std::size_t i = 0; i < result.nodal_positions_m.size(); ++i) out << x(result.nodal_positions_m[i]) << ',' << y(result.nodal_temperatures_C[i]) << ' ';
    out << "\"/><polyline fill=\"none\" stroke=\"#f1c76b\" stroke-width=\"2\" stroke-dasharray=\"7 5\" points=\"";
    for (std::size_t i = 0; i < result.nodal_positions_m.size(); ++i) out << x(result.nodal_positions_m[i]) << ',' << y(result.analytical_nodal_temperatures_C[i]) << ' ';
    out << "\"/><text x=\"710\" y=\"90\" fill=\"#59c3df\" font-family=\"sans-serif\">FEM</text><text x=\"710\" y=\"112\" fill=\"#f1c76b\" font-family=\"sans-serif\">Analytical</text><text x=\"420\" y=\"410\" fill=\"#9bb0bb\" font-family=\"sans-serif\">Position x (m)</text><text x=\"15\" y=\"220\" fill=\"#9bb0bb\" font-family=\"sans-serif\" transform=\"rotate(-90 15 220)\">Temperature (°C)</text></svg>";
}

void write_convergence_svg(const std::vector<ThermalResult>& results, const std::filesystem::path& path) {
    std::ofstream out(path);
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"900\" height=\"430\" viewBox=\"0 0 900 430\">"
        << "<rect width=\"100%\" height=\"100%\" fill=\"#07101a\"/><text x=\"45\" y=\"35\" fill=\"#e6eef6\" font-family=\"sans-serif\" font-size=\"20\">NEXUS Thermal Studio — mesh convergence</text>"
        << "<line x1=\"80\" y1=\"370\" x2=\"850\" y2=\"370\" stroke=\"#78909c\"/><line x1=\"80\" y1=\"70\" x2=\"80\" y2=\"370\" stroke=\"#78909c\"/>";
    auto x = [](std::size_t index) { return 120.0 + static_cast<double>(index) * 220.0; };
    auto y = [](double error) { return 370.0 - (std::log10(error) + 2.0) / 3.0 * 280.0; };
    out << "<polyline fill=\"none\" stroke=\"#59c3df\" stroke-width=\"3\" points=\"";
    for (std::size_t i = 0; i < results.size(); ++i) out << x(i) << ',' << y(results[i].maximum_absolute_midpoint_error_C) << ' ';
    out << "\"/>";
    for (std::size_t i = 0; i < results.size(); ++i) {
        out << "<circle cx=\"" << x(i) << "\" cy=\"" << y(results[i].maximum_absolute_midpoint_error_C) << "\" r=\"5\" fill=\"#59c3df\"/>"
            << "<text x=\"" << x(i) - 12 << "\" y=\"390\" fill=\"#9bb0bb\" font-family=\"sans-serif\">" << results[i].element_count << "</text>";
    }
    out << "<text x=\"370\" y=\"415\" fill=\"#9bb0bb\" font-family=\"sans-serif\">Elements (log-scaled error axis)</text><text x=\"15\" y=\"220\" fill=\"#9bb0bb\" font-family=\"sans-serif\" transform=\"rotate(-90 15 220)\">Max midpoint error (°C)</text></svg>";
}
}

int main(int argc, char** argv) {
    const std::filesystem::path output_dir = argc > 1 ? argv[1] : "results/thermal";
    std::filesystem::create_directories(output_dir);
    nexus::thermal::ThermalParameters parameters;
    std::vector<ThermalResult> results;
    for (const std::size_t elements : {10U, 20U, 40U, 80U}) results.push_back(nexus::thermal::solve(parameters, elements));
    write_result_csv(results.back(), output_dir / "temperature_vs_position.csv");
    write_comparison_csv(results.back(), output_dir / "fem_analytical_comparison.csv");
    write_convergence_csv(results, output_dir / "mesh_convergence.csv");
    write_svg(results, output_dir / "temperature_comparison.svg");
    write_convergence_svg(results, output_dir / "mesh_convergence.svg");
    std::cout << "Generated thermal outputs in " << output_dir << "\n"
              << "N=80 maximum midpoint error: " << std::setprecision(17)
              << results.back().maximum_absolute_midpoint_error_C << " C\n";
}
